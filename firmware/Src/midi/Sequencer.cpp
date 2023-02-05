/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <dot> hosxe (at) g m a i l <dot> com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Sequencer.h>
#include <RingBuffer.h>
#include "Synth.h"
#include "FMDisplaySequencer.h"

__attribute__((section(".ram_d3"))) SeqMidiAction actions[SEQ_ACTION_SIZE];
__attribute__((section(".ram_d3"))) StepSeqValue stepNotes[NUMBER_OF_STEP_SEQUENCES][256];


Sequencer::Sequencer() {
    uint32_t stateSize;
    uint8_t* actionBuffer =  (uint8_t*)actions;
    // Use actions as a temporary buffer
    getFullDefaultState(actionBuffer, &stateSize, 0);
    setFullState((uint8_t*)actions);

    for (uint32_t i = 0; i < sizeof(actions); i++) {
        actionBuffer[i] = 0;
    }

    for (uint32_t i = 0; i < NUMBER_OF_STEP_SEQUENCES; i++) {
        for (int s = 0; s < 256; s++) {
            stepNotes[i][s].full = 0l;
        }
    }
    for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
        transpose_[i] = 0;
    }
    reset(false);
}

Sequencer::~Sequencer() {
}

void Sequencer::setSynth(Synth* synth) {
    synth_ = synth;
}

void Sequencer::setDisplaySequencer(FMDisplaySequencer* displaySequencer) {
    displaySequencer_ = displaySequencer;
}

void Sequencer::reset(bool synthNoteOff) {
    millisTimer_ = 0;
    current16bitTimer_ = 0;
    previousCurrent16bitTimer_ = MAX_TIME;
    stepNumberOfNotesOn_ = 0;

    for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
        if (synthNoteOff) {
            synth_->stopArpegiator(i);
            synth_->allNoteOff(i);
        }

        nextActionIndex_[i] = i * 2 + 1;
        previousActionIndex_[i] = i * 2;
        seqActivated_[i] = false;
        lastInstrument16bitTimer_[i] = -1;

        actions[i * 2].when = 0;
        actions[i * 2].actionType = SEQ_ACTION_NONE;
        actions[i * 2].nextIndex = i * 2 + 1;

        actions[i * 2 + 1].when = instrumentTimerMask_[i];
        actions[i * 2 + 1].actionType = SEQ_ACTION_NONE;
        actions[i * 2 + 1].nextIndex = i * 2;
    }

    lastFreeAction_ = NUMBER_OF_TIMBRES * 2;
    midiTickCounterLastSent_ = 0;
}

void Sequencer::setNumberOfBars(int instrument, uint8_t bars) {
    if (bars < 1 || bars > 4 || bars == 3) {
        return;
    }
    // set instrumentTimerMask to last number
    instrumentTimerMask_[instrument] = (uint16_t)((MAX_PER_BEAT + 1)* 4 * bars - 1);
    // update the last action
    actions[instrument * 2 + 1].when = instrumentTimerMask_[instrument];

    // action timer need to be recalculated from scratch
    nextActionTimerOutOfSync_[instrument] = true;
}


bool Sequencer::setSeqActivated(uint8_t instrument) {
    bool newActivated = (actions[instrument * 2].nextIndex != instrument * 2 + 1);
    if (unlikely(newActivated != seqActivated_[instrument])) {
        seqActivated_[instrument] = newActivated;
        return true;
    }
    return false;
}

void Sequencer::setExternalClock(bool enable) {
    externalClock_ = enable;
}


void Sequencer::stop() {
    running_ = false;
    for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
        synth_->stopArpegiator(i);
        synth_->allNoteOff(i);
    }
    if (!externalClock_) {
        synth_->midiClockStop(false);
    }
}

void Sequencer::setMuted(uint8_t instrument, bool mute) {
    muted_[instrument] = mute;
    if (mute) {
        synth_->stopArpegiator(instrument);
        synth_->allNoteOff(instrument);
    }
}

void Sequencer::toggleMuted(uint8_t instrument) {
    if (!muted_[instrument]) {
        synth_->stopArpegiator(instrument);
        synth_->allNoteOff(instrument);
    }
    muted_[instrument] = !muted_[instrument];
}

void Sequencer::setTranspose(uint8_t instrument, int16_t newValue) {
    if (likely(newValue > -48 && newValue < 48 && transpose_[instrument] != newValue)) {
        synth_->stopArpegiator(instrument);
        synth_->allNoteOff(instrument);
        transpose_[instrument] = newValue;
    }
}

void Sequencer::setTempo(float newTempo) {
    tempo_ = newTempo;
    oneBitInMillis_ = 60.0f * 1000.0f / newTempo / (MAX_PER_BEAT + 1);
    millisInBits_ = 1 / oneBitInMillis_;
    // Main sequence always 4 bars (16 beats)
    sequenceDuration_ = 60 * 16 * 1000 / tempo_ ;
    // We must have sent 4 *4 * 24 (384) midi ticks at the end of sequenceDuration
    midiTickCounterInc_ = 384.0f / sequenceDuration_;
}


void Sequencer::start() {
    running_ = true;
    if (!externalClock_ && current16bitTimer_ == 0 && millisTimer_ == 0) {
        precount_ = 1023;
    } else if (!externalClock_) {
        synth_->midiClockContinue(songPosition_, false);
    }
}

void Sequencer::onMidiContinue(int songPosition) {
    if (externalClock_) {
        if (!extMidiRunning_) {
            extMidiRunning_ = true;
            displaySequencer_->refresh(17, 17);
        }
    }
}

void Sequencer::onMidiStart() {
    if (externalClock_) {
        if (!extMidiRunning_) {
            rewind();
            extMidiRunning_ = true;
            displaySequencer_->refresh(17, 17);
        }
    }
}

void Sequencer::onMidiStop() {
    if (externalClock_) {
        if (extMidiRunning_) {
            extMidiRunning_ = false;
            for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
                synth_->stopArpegiator(i);
                synth_->allNoteOff(i);
            }
            displaySequencer_->refresh(17, 17);
        }
    }
}


void Sequencer::onMidiClock() {
    if (externalClock_ && running_) {
        // We're called 24 times per beat
        midiClockTimer_ += (256.0f / 24.0f);
        if (midiClockTimer_ > 4096.0f) { // 4 (measures) * 4 (beats) * 256
            midiClockTimer_ -= 4096.0f;
        }
        mainSequencerTic(midiClockTimer_);
    }
}


void Sequencer::ticMillis() {
    // if external clock we don't do anything here
    if (externalClock_) {
        return;
    }

    uint16_t counter = millisTimer_ * millisInBits_;

    // Main sequencer step
    mainSequencerTic(counter);

    // Change counters and send tic information to synch
    if (likely(running_)) {
        millisTimer_++;

        // Midi tick and song position
        midiTickCounter_ += midiTickCounterInc_;

        // We reach the end of the 4 bars sequence
        if (unlikely(millisTimer_ >=  sequenceDuration_)) {
            millisTimer_ = 0;
            songPosition_ = 0;
            midiTickCounter_ = 0.0f;
        }

        // New midi information to send ?
        int midiTickCounterInt = midiTickCounter_;
        if (unlikely(midiTickCounterInt != midiTickCounterLastSent_)) {
            midiTickCounterLastSent_ = midiTickCounterInt;
            synth_->midiTick(false);

            if (unlikely((midiTickCounterInt % 6) == 0)) {
                songPosition_++;
                synth_->midiClockSongPositionStep(songPosition_);

            }
        }
    }
}


void Sequencer::midiClockSetSongPosition(int songPosition) {
    int midiClockTimerInt = songPosition * 4;
    midiClockTimerInt = midiClockTimerInt % 4096;

    midiClockTimer_ = midiClockTimerInt;
}


void Sequencer::mainSequencerTic(uint16_t counter) {

    if (!running_)  {
        return;
    }

    if (unlikely(precount_ > 0)) {
        precount_ -=  millisInBits_;
        current16bitTimer_ = precount_;
        if ((current16bitTimer_ & 0x300) != lastBeat_) {
            lastBeat_ = current16bitTimer_ & 0x300;
            displaySequencer_->displayBeat();
        }
        if (likely(precount_ > 0)) {
            return;
        }
        rewind();
        counter = 0;
        previousCurrent16bitTimer_ = 1;
        lastBeat_ = 1;
        // Midi clock real start
        if (!externalClock_) {
            songPosition_ = 0;
            synth_->midiClockStart(false);
        }
        for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
            lastInstrument16bitTimer_[i] = 0;
        }
    }

    current16bitTimer_ = counter;


    // at 120 BPM we're called (.5 / 16 = 0.031ms) 31 times before a new current16bitTimer value
    if (likely(previousCurrent16bitTimer_ == current16bitTimer_)) {
        return;
    } else {
        previousCurrent16bitTimer_ = current16bitTimer_;
    }

    //
    if ((current16bitTimer_ & 0x300) != lastBeat_) {
        lastBeat_ = current16bitTimer_ & 0x300;
        displaySequencer_->displayBeat();
        HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_SET);
        ledTimer_ = HAL_GetTick();
    } else if (unlikely(HAL_GetTick() - ledTimer_ > 100)){
        // Led remains on during 1/10th of a sec
        HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_RESET);
        ledTimer_ = 0Xffffffff;
    }


    for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {

        // Do we have at least one note for this instrument ?
        if (!seqActivated_[i] && !stepActivated_[instrumentStepSeq_[i]]) {
            // if not go to next instrument
            continue;
        }

        int instrument16bitTimer = current16bitTimer_ & instrumentTimerMask_[i];

        // Did we just changed the number of bars ?
        // Update nextActionIndex to catch up with the new instrument timer
        if (nextActionTimerOutOfSync_[i]) {
            nextActionTimerOutOfSync_[i] = false;
            resyncNextAction(i, instrument16bitTimer);
        }

        // instrument16bitTimer loop to 0
        if (unlikely(lastInstrument16bitTimer_[i] > instrument16bitTimer)) {
            processActionBetwen(i, lastInstrument16bitTimer_[i], instrumentTimerMask_[i]);
            //  loop for regular iteration
            lastInstrument16bitTimer_[i] = 0;
        }

        // process all events until now
        processActionBetwen(i, lastInstrument16bitTimer_[i], instrument16bitTimer);

        lastInstrument16bitTimer_[i] = instrument16bitTimer;
    }

}

void Sequencer::resyncNextAction(int instrument, uint16_t newInstrumentTimer) {
    // update lastInstrument16bitTimer[i]
    if (externalClock_) {
        lastInstrument16bitTimer_[instrument] = newInstrumentTimer - (256.0f / 24.0f);
    } else {
        lastInstrument16bitTimer_[instrument] = newInstrumentTimer - millisInBits_;
    }
    if (lastInstrument16bitTimer_[instrument] < 0) {
        lastInstrument16bitTimer_[instrument] += 4096;
    } else if (lastInstrument16bitTimer_[instrument] >= 4096) {
        lastInstrument16bitTimer_[instrument] -= 4096;
    }

    nextActionIndex_[instrument] = instrument * 2;
    while (actions[nextActionIndex_[instrument]].when < lastInstrument16bitTimer_[instrument]) {
        nextActionIndex_[instrument] = actions[nextActionIndex_[instrument]].nextIndex;
    }
}

void Sequencer::processActionBetwen(int instrument, uint16_t startTimer, uint16_t endTimer) {

    if (stepActivated_[instrumentStepSeq_[instrument]] && !muted_[instrument]) {
        uint8_t currentIndex = endTimer >> 4;
        if (instrumentStepIndex_[instrument] != currentIndex) {
        	int seqNumber = instrumentStepSeq_[instrument];
            instrumentStepIndex_[instrument] = currentIndex;
            uint16_t newUnique = stepNotes[seqNumber][instrumentStepIndex_[instrument]].unique;

            if (instrumentStepLastUnique_[instrument] != newUnique || currentIndex == 0) {
                instrumentStepLastUnique_[instrument] = newUnique;

                uint8_t previousIndex = (currentIndex - 1) & (instrumentTimerMask_[instrument] >> 4);
                if ((stepNotes[seqNumber][currentIndex].full & 0xffffff00) == 0l) {
                    if ((stepNotes[seqNumber][previousIndex].full & 0xffffff00) != 0l) {
                        // Note off
                        for (int n = 0; stepNotes[seqNumber][previousIndex].values[3 + n] != 0 && n < 6; n++) {
                            synth_->noteOffFromSequencer(instrument, stepNotes[seqNumber][previousIndex].values[3 + n] + transpose_[instrument]);
                        }
                    }
                } else {
                    // Note off
                    if ((stepNotes[seqNumber][previousIndex].full & 0xffffff00) != 0l) {
                        for (int n = 0; stepNotes[seqNumber][previousIndex].values[3 + n] != 0 && n < 6; n++) {
                            synth_->noteOffFromSequencer(instrument, stepNotes[seqNumber][previousIndex].values[3 + n] + transpose_[instrument]);
                        }
                    }
                    // Note on
                    for (int n = 0; stepNotes[seqNumber][currentIndex].values[3 + n] != 0 && n < 6; n++) {
                        synth_->noteOnFromSequencer(instrument, stepNotes[seqNumber][currentIndex].values[3 + n]  + transpose_[instrument],
                            stepNotes[seqNumber][currentIndex].values[2]);
                    }
                }
            }
        }
    }

    // Play to the end before looping
    // We test activated as we can clear or apply step seq in the middle of this loop
    if (seqActivated_[instrument]) {
        while (actions[nextActionIndex_[instrument]].when >= startTimer && actions[nextActionIndex_[instrument]].when <= endTimer && seqActivated_[instrument]) {
            // actions[nextActionIndex];
            if (!muted_[instrument]) {
                switch (actions[nextActionIndex_[instrument]].actionType) {
                case SEQ_ACTION_NOTE:
                    if (actions[nextActionIndex_[instrument]].param2 > 0) {
                        synth_->noteOnFromSequencer(instrument, actions[nextActionIndex_[instrument]].param1  + transpose_[instrument] , actions[nextActionIndex_[instrument]].param2);
                    } else {
                        synth_->noteOffFromSequencer(instrument, actions[nextActionIndex_[instrument]].param1  + transpose_[instrument]);
                    }
                    break;
                }
            }

            previousActionIndex_[instrument] = nextActionIndex_[instrument];
            nextActionIndex_[instrument] = actions[nextActionIndex_[instrument]].nextIndex;

            if (actions[nextActionIndex_[instrument]].when > instrumentTimerMask_[instrument]) {
                // We have events after the instrument bar limit, we must loop now
                previousActionIndex_[instrument] = instrument * 2;
                nextActionIndex_[instrument] = instrument * 2;
            }
        }
    }
}



void Sequencer::clear(uint8_t instrument) {

    if (!isSeqActivated(instrument)) {
        return;
    }

    // Clear instrument
    seqActivated_[instrument] = false;

    nextActionIndex_[instrument] = instrument * 2 + 1;
    previousActionIndex_[instrument] = instrument * 2;

    actions[instrument * 2].nextIndex = instrument * 2 + 1;

    bool allInactive = true;
    for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
        if (isSeqActivated(i)) {
            allInactive = false;
        }
    }
    if (allInactive) {
        // Nothing to do
        lastFreeAction_ = NUMBER_OF_TIMBRES * 2;
        return;
    }

    uint16_t index = actions[instrument * 2].nextIndex;
    uint16_t end = instrument * 2 + 1;

    while (index != end) {
        actions[index].actionType = SEQ_ACTION_NONE;
        index = actions[index].nextIndex;
    }

    for (int i = (NUMBER_OF_TIMBRES * 2); i < lastFreeAction_; i++) {
        if (actions[i].actionType == SEQ_ACTION_NONE) {
            // Move next one to this index : i
            int movedFromIndex = 0;
            for (int j = lastFreeAction_ - 1; j > i; j--) {
                if (actions[j].actionType != SEQ_ACTION_NONE) {
                    // move j to i
                    actions[i].when = actions[j].when;
                    actions[i].actionType = actions[j].actionType;
                    actions[i].param1 = actions[j].param1;
                    actions[i].param2 = actions[j].param2;
                    actions[i].nextIndex = actions[j].nextIndex;
                    movedFromIndex = j;
                    actions[j].actionType = SEQ_ACTION_NONE;
                    break;
                }
            }
            if (movedFromIndex == 0) {
                // only SEQ_ACTION_CLEAR so we can finished
                break;
            } else {
                // Update nextActionIndex if needed
                for (int instrument = 0; instrument < NUMBER_OF_TIMBRES; instrument ++) {
                    if (nextActionIndex_[instrument] == movedFromIndex) {
                        nextActionIndex_[instrument] = i;
                        break;
                    }
                }
                // Update previous index
                // Can be in the first indexes !
                for (int j = 0; j < lastFreeAction_; j++) {
                    if (actions[j].nextIndex == movedFromIndex) {
                        // update index
                        actions[j].nextIndex = i;
                        break;
                    }
                }
            }
        }
    }
    for (int i = (NUMBER_OF_TIMBRES * 2); i < lastFreeAction_; i++) {
        if (actions[i].actionType == SEQ_ACTION_NONE) {
            lastFreeAction_ = i;
            break;
        }
    }

    synth_->allNoteOff(instrument);

}



// From main loop thread
// Different lower priority thread than tic() above
// We save in the stepCurrentInstrument !
void Sequencer::insertNote(uint8_t instrument, uint8_t note, uint8_t velocity) {
    if (stepMode_) {
        if (velocity > 0) {
            if (stepNumberOfNotesOn_ < 5) {
                if (stepNumberOfNotesOn_ == 0){
                	tmpStepValue_.values[2] = velocity;
                }
                tmpStepValue_.values[3 + stepNumberOfNotesOn_] = note;
            }
            stepNumberOfNotesOn_ ++;

        } else {
            // Can be < 0 when we swap between normal and step mode while playuing
            stepNumberOfNotesOn_ --;
            if (stepNumberOfNotesOn_ == 0) {
                displaySequencer_->newNoteEntered(instrument);
            }
            if (stepNumberOfNotesOn_ < 0) {
                stepNumberOfNotesOn_ = 0;
            }
        }
        return;
    }

    if (!isRunning() || precount_ > 0 || !isRecording(instrument)) {
        return;
    }

    if (likely(lastFreeAction_ < SEQ_ACTION_SIZE)) {
        SeqMidiAction* newAction = &actions[lastFreeAction_];
        newAction->when = current16bitTimer_ & instrumentTimerMask_[instrument];
        newAction->actionType = SEQ_ACTION_NOTE;
        newAction->param1 = note;
        newAction->param2 = velocity;
        newAction->nextIndex = actions[previousActionIndex_[instrument]].nextIndex;
        actions[previousActionIndex_[instrument]].nextIndex = lastFreeAction_;

        previousActionIndex_[instrument] = lastFreeAction_;

        lastFreeAction_++;

        if (unlikely(setSeqActivated(instrument))) {
            displaySequencer_->refreshActivated();
        } else if (unlikely((lastFreeAction_ & 0xF) == 0)) {
            displaySequencer_->refreshMemory();
        }
    }
}


/*
 * Returns true if more than one note
 */
bool Sequencer::stepRecordNotes(int instrument, int stepCursor, int stepSize) {
    bool moreThanOneNote;
	int seqNumber = instrumentStepSeq_[instrument];

    for (int s = stepCursor; (s < (stepCursor + stepSize)) && (s < 256); s++) {
        stepNotes[seqNumber][s].full = tmpStepValue_.full;
        stepNotes[seqNumber][s].unique = stepUniqueValue_[instrument];
    }
    stepUniqueValue_[instrument]++;
    moreThanOneNote = (tmpStepValue_.values[4] > 0);
    tmpStepValue_.full = 0l;
    stepNumberOfNotesOn_ = 0;

    if (unlikely(!stepActivated_[seqNumber])) {
    	stepActivated_[seqNumber] = true;
    }
    return moreThanOneNote;
}

void Sequencer::stepClearPart(int instrument, int stepCursor, int stepSize) {
	int seqNumber = instrumentStepSeq_[instrument];
    for (int s = stepCursor; (s < (stepCursor + stepSize)) && (s < 255); s++) {
        stepNotes[seqNumber][s].full = 0l;
        stepNotes[seqNumber][s].unique = stepUniqueValue_[instrument];
    }
    stepUniqueValue_[instrument]++;
    tmpStepValue_.full = 0l;
    stepNumberOfNotesOn_ = 0;
}


StepSeqValue* Sequencer::stepGetSequence(int instrument) {
	int seqNumber = instrumentStepSeq_[instrument];
    return stepNotes[seqNumber];
}

void Sequencer::stepClearAll(int instrument) {
	int seqNumber = instrumentStepSeq_[instrument];
    synth_->stopArpegiator(instrument);
	synth_->allNoteOff(instrument);
    for (int s = 0; s < 255; s++) {
        stepNotes[seqNumber][s].full = 0;
    }
    stepUniqueValue_[instrument] = 1;
    stepNumberOfNotesOn_ = 0;
    stepActivated_[seqNumber] = false;
}


/*
 * Used to create empty bank
 */
void Sequencer::getFullDefaultState(uint8_t* buffer, uint32_t *size, uint8_t seqNumber) {
    int index = 0;
    buffer[index++] = (uint8_t)SEQ_CURRENT_VERSION;
    // sequenceName
    char defautSequenceName[13] = "Seq \0\0\0\0\0\0\0";
    uint8_t dizaine = seqNumber / 10;
    defautSequenceName[4] = (char)('0' + dizaine);
    defautSequenceName[5] = (char)('0' + (seqNumber - dizaine * 10));

    for (int s = 0; s < 12; s++) {
        buffer[index++] = defautSequenceName[s];
    }
    // isExternalClockEnabled
    buffer[index++] = (uint8_t)1;
    // tempo
    *((float*)&buffer[index]) = 90.0f;
    index+=sizeof(float);
    // Last free action
    *((uint16_t*)&buffer[index]) = 12;
    index+=sizeof(uint16_t);

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        // stepUniqueValue
        *((uint16_t*)&buffer[index]) = 1;
        index += sizeof(uint16_t);
        // instrumentTimerMask
        *((uint16_t*)&buffer[index]) = 4095;
        index += sizeof(uint16_t);
        // seqActivated
        buffer[index++] = 0;
        // recording
        buffer[index++] = 0;
        // muted
        buffer[index++] = 0;
        // instrument Step seq
        buffer[index++] = t;
    }

    for (int t = 0; t < NUMBER_OF_STEP_SEQUENCES; t++) {
		// stepActivated
		buffer[index++] = 0;
    }

    *size = index;
}


void Sequencer::getFullState(uint8_t* buffer, uint32_t *size) {
    int index = 0;
    buffer[index++] = (uint8_t)SEQ_CURRENT_VERSION;
    for (int s = 0; s < 12; s++) {
        buffer[index++] = sequenceName_[s];
    }
    buffer[index++] = (uint8_t)(isExternalClockEnabled() ? 1 : 0);
    *((float*)&buffer[index]) = tempo_;
    index+=sizeof(float);
    *((uint16_t*)&buffer[index]) = lastFreeAction_;
    index+=sizeof(uint16_t);
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        *((uint16_t*)&buffer[index]) = stepUniqueValue_[t];
        index += sizeof(uint16_t);
        *((uint16_t*)&buffer[index]) = instrumentTimerMask_[t];
        index += sizeof(uint16_t);
        buffer[index++] = (seqActivated_[t] ? 1 : 0);
        buffer[index++] = (recording_[t] ? 1 : 0);
        buffer[index++] = (muted_[t] ? 1 : 0);
        buffer[index++] = instrumentStepSeq_[t];
    }

    for (int s = 0; s < NUMBER_OF_STEP_SEQUENCES; s++) {
    	buffer[index++] = (stepActivated_[s] ? 1 : 0);
    }

    *size = index;
}

void Sequencer::setFullState(uint8_t* buffer) {
    SEQ_VERSION version = (SEQ_VERSION)buffer[0];
    switch (version) {
    case SEQ_VERSION1:
        loadStateVersion1(buffer);
        break;
    case SEQ_VERSION2:
        loadStateVersion2(buffer);
        break;
    }
}

void Sequencer::loadStateVersion1(uint8_t* buffer) {
    uint32_t index = 0;
    index++; // VERSION
    for (int s = 0; s < 12; s++) {
        sequenceName_[s] = buffer[index++];
    }
    setExternalClock(buffer[index++] == 1);

    // Needed to load from mem to float !
    uint8_t tempoUint8[4];
    for (int i = 0; i < 4; i++) {
        tempoUint8[i] = buffer[index++];
    }
    float newTempo = * ((float*)tempoUint8);
    setTempo(newTempo);

    lastFreeAction_ = *((uint16_t*)&buffer[index]) ;
    index+=sizeof(uint16_t);
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        stepUniqueValue_[t] = *((uint16_t*)(&buffer[index]));
        index+=sizeof(uint16_t);
        instrumentTimerMask_[t] = *((uint16_t*)(&buffer[index]));
        index+=sizeof(uint16_t);
        seqActivated_[t]  = (buffer[index++] == 1);
        stepActivated_[t]  = (buffer[index++] == 1);
        recording_[t]  = (buffer[index++] == 1);
        muted_[t]  = (buffer[index++] == 1);
        // init default Value
        instrumentStepSeq_[t] = t;
    }
}

void Sequencer::loadStateVersion2(uint8_t* buffer) {
    uint32_t index = 0;
    index++; // VERSION
    for (int s = 0; s < 12; s++) {
        sequenceName_[s] = buffer[index++];
    }
    setExternalClock(buffer[index++] == 1);

    // Needed to load from mem to float !
    uint8_t tempoUint8[4];
    for (int i = 0; i < 4; i++) {
        tempoUint8[i] = buffer[index++];
    }
    float newTempo = * ((float*)tempoUint8);
    setTempo(newTempo);

    lastFreeAction_ = *((uint16_t*)&buffer[index]) ;
    index+=sizeof(uint16_t);
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        stepUniqueValue_[t] = *((uint16_t*)(&buffer[index]));
        index+=sizeof(uint16_t);
        instrumentTimerMask_[t] = *((uint16_t*)(&buffer[index]));
        index+=sizeof(uint16_t);
        seqActivated_[t]  = (buffer[index++] == 1);
        recording_[t]  = (buffer[index++] == 1);
        muted_[t]  = (buffer[index++] == 1);
        instrumentStepSeq_[t] = buffer[index++];
    }
    for (int s = 0; s < NUMBER_OF_STEP_SEQUENCES; s++) {
        stepActivated_[s]  = (buffer[index++] == 1);
    }
}


char* Sequencer::getSequenceNameInBuffer(char* buffer) {
    SEQ_VERSION version = (SEQ_VERSION)buffer[0];
    switch (version) {
    case SEQ_VERSION1:
        return buffer + 1;
    case SEQ_VERSION2:
        return buffer + 1;
    }
    return "##";
}

void Sequencer::setInstrumentStepSeq(int instrument, int index) {
    synth_->stopArpegiator(instrument);
    synth_->allNoteOff(instrument);
    instrumentStepSeq_[instrument] = (uint8_t)index;
}


void Sequencer::setSequenceName(const char* newName) {
    for (int n = 0; n < 12; n++) {
        sequenceName_[n] = newName[n];
    }
    sequenceName_[12] = 0;
}


void Sequencer::setNewSeqValueFromMidi(uint8_t timbre, uint8_t seqValue, uint8_t newValue) {
    switch (seqValue) {
    case SEQ_VALUE_PLAY_ALL:
        if (newValue > 0) {
            start();
        } else  {
            stop();
        }
        break;
    case SEQ_VALUE_PLAY_INST:
        setMuted(timbre, newValue == 0);
        break;
    case SEQ_VALUE_RECORD_INST:
        setRecording(timbre, newValue > 0);
        break;
    case SEQ_VALUE_SEQUENCE_NUMBER:
        setInstrumentStepSeq(timbre, newValue % NUMBER_OF_STEP_SEQUENCES);
        break;
    case SEQ_VALUE_TRANSPOSE:
        setTranspose(timbre, newValue - 64);
        break;
    };
    displaySequencer_->sequencerWasUpdated(timbre, seqValue, newValue);
}


uint64_t Sequencer::getStepData(int instrument, int stepCursor) {
    int seqNumber = instrumentStepSeq_[instrument];
    return stepNotes[seqNumber][stepCursor].full;
}



void Sequencer::changeCurrentNote(int instrument, int stepCursor, int stepSize, int ticks) {
    int seqNumber = instrumentStepSeq_[instrument];

    // if no note we create a note if everything is empty bellow
    if (stepNotes[seqNumber][stepCursor].values[3] == 0) {
        createNewNoteIfEmpty(instrument, stepCursor, stepSize);
        return;
    }

    // Don't change if some notes are out of [0-127]
    for (int n = 3; n <= 7; n++) {
        // note 2-5 can be null
        if (stepNotes[seqNumber][stepCursor].values[n] == 0) {
            continue;
        }
        int note = stepNotes[seqNumber][stepCursor].values[n] + ticks;
        // 0 is considered as no note so we don't want 0
        if (note <= 0 || note > 127) {
            return;
        }
    }
    for (int s = stepCursor; (s < (stepCursor + stepSize)) && (s < 256); s++) {
        for (int n = 3; n <= 7; n++) {
            if (stepNotes[seqNumber][s].values[n] != 0) {
                stepNotes[seqNumber][s].values[n] += ticks;
            }
        }
    }
    if (!createNewNoteIfNeeded(instrument, stepCursor, stepSize)) {
        displaySequencer_->updateCurrentData();
        displaySequencer_->refresh(12, 12);
    }
}

void Sequencer::changeCurrentVelocity(int instrument, int stepCursor, int stepSize, int ticks) {
    int seqNumber = instrumentStepSeq_[instrument];

    // return if no note
    if (stepNotes[seqNumber][stepCursor].values[3] == 0) {
        return;
    }

    // Must be int for >127 and negative test
    for (int s = stepCursor; (s < (stepCursor + stepSize)) && (s < 256); s++) {
        if (stepNotes[seqNumber][s].values[2] != 0) {
            int newValue = stepNotes[seqNumber][s].values[2] + ticks;
            if (newValue > 127) {
                newValue = 127;
            } else if (newValue < 1) {
                // No zero so that it can be edited
                newValue = 1;
            }
            stepNotes[seqNumber][s].values[2] = newValue;
        }
    }
    if (!createNewNoteIfNeeded(instrument, stepCursor, stepSize)) {
        displaySequencer_->updateCurrentData();
        displaySequencer_->refresh(11, 11);
    }
}


/*
 * We check limit
 * If everything is the same, we're in the middle of a bigger note -> yes we must create a new note (new uniqueId)
 */
bool Sequencer::createNewNoteIfNeeded(int instrument, int stepCursor, int stepSize) {
    bool createNewNote = false;
    int seqNumber = instrumentStepSeq_[instrument];
    uint16_t startUniqueId = stepNotes[seqNumber][stepCursor].unique;

    // Before cursor
    if (stepCursor > 0) {
        if (stepNotes[seqNumber][stepCursor - 1].unique == startUniqueId) {
            createNewNote = true;;
        }
    }

    // After cursor
    if ((stepCursor + stepSize) <= 255) {
        if (stepNotes[seqNumber][stepCursor + stepSize].unique == startUniqueId) {
            createNewNote = true;
        }
    }

    // All steps must be the same
    // Only test if it's currently true;
    if (createNewNote) {
        for (int s = stepCursor; (s < (stepCursor + stepSize)) && (s < 256); s++) {
            if (stepNotes[seqNumber][s].unique != startUniqueId) {
                return false;
            }
        }
    }


    if (createNewNote) {
        int uniqueId = stepUniqueValue_[instrument];
        for (int s = stepCursor; (s < (stepCursor + stepSize)) && (s < 256); s++) {
            stepNotes[seqNumber][s].unique = uniqueId;
        }
        stepUniqueValue_[instrument]++;
        displaySequencer_->refreshStepSeq();
    }
    return createNewNote;
}

/*
 * We check that everything is empty bellow the cursor
 * If empty we create a 64/100 note
 */
bool Sequencer::createNewNoteIfEmpty(int instrument, int stepCursor, int stepSize) {
    int seqNumber = instrumentStepSeq_[instrument];

    // All steps must be empty
    for (int s = stepCursor; (s < (stepCursor + stepSize)) && (s < 256); s++) {
        if (stepNotes[seqNumber][s].unique != 0) {
            return false;
        }
    }

    // Create new Note
    StepSeqValue newNote;
    newNote.unique = stepUniqueValue_[instrument];
    newNote.values[2] = 100;
    newNote.values[3] = 64;
    for (int s = stepCursor; (s < (stepCursor + stepSize)) && (s < 256); s++) {
        stepNotes[seqNumber][s] = newNote;

    }
    stepUniqueValue_[instrument]++;
    displaySequencer_->refreshStepSeq();
    return true;
}


