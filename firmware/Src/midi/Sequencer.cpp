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
    getFullDefaultState(actionBuffer, &stateSize);
    setFullState((uint8_t*)actions);

    for (uint32_t i = 0; i < sizeof(actions); i++) {
        actionBuffer[i] = 0;
    }

    for (uint32_t i = 0; i < NUMBER_OF_STEP_SEQUENCES; i++) {
        for (int s = 0; s < 256; s++) {
            stepNotes[i][s].full = 0l;
        }
    }
    reset(false);
}

Sequencer::~Sequencer() {
}

void Sequencer::setSynth(Synth* synth) {
    this->synth = synth;
}

void Sequencer::setDisplaySequencer(FMDisplaySequencer* displaySequencer) {
    this->displaySequencer = displaySequencer;
}

void Sequencer::reset(bool synthNoteOff) {
    this->millisTimer = 0;
    this->current16bitTimer = 0;
    this->previousCurrent16bitTimer = MAX_TIME;
    this->stepNumberOfNotesOn = 0;

    for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
        if (synthNoteOff) {
            synth->allNoteOff(i);
        }

        this->nextActionIndex[i] = i * 2 + 1;
        this->previousActionIndex[i] = i * 2;
        this->seqActivated[i] = false;
        this->lastInstrument16bitTimer[i] = -1;

        actions[i * 2].when = 0;
        actions[i * 2].actionType = SEQ_ACTION_NONE;
        actions[i * 2].nextIndex = i * 2 + 1;

        actions[i * 2 + 1].when = instrumentTimerMask[i];
        actions[i * 2 + 1].actionType = SEQ_ACTION_NONE;
        actions[i * 2 + 1].nextIndex = i * 2;
    }

    this->lastFreeAction = NUMBER_OF_TIMBRES * 2;
}

void Sequencer::setNumberOfBars(int instrument, uint8_t bars) {
    if (bars < 1 || bars > 4 || bars == 3) {
        return;
    }
    // set instrumentTimerMask to last number
    instrumentTimerMask[instrument] = (uint16_t)((MAX_PER_BEAT + 1)* 4 * bars - 1);
    // update the last action
    actions[instrument * 2 + 1].when = instrumentTimerMask[instrument];

    // action timer need to be recalculated from scratch
    nextActionTimerOutOfSync[instrument] = true;
}


bool Sequencer::setSeqActivated(uint8_t instrument) {
    bool newActivated = (actions[instrument * 2].nextIndex != instrument * 2 + 1);
    if (unlikely(newActivated != seqActivated[instrument])) {
        seqActivated[instrument] = newActivated;
        return true;
    }
    return false;
}



void Sequencer::setExternalClock(bool enable) {
    externalClock = enable;
}


void Sequencer::stop() {
    running = false;
    for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
        synth->allNoteOff(i);
    }
}

void Sequencer::setMuted(uint8_t instrument, bool mute) {
    muted[instrument] = mute;
    if (mute) {
        synth->allNoteOff(instrument);
    }
}

void Sequencer::toggleMuted(uint8_t instrument) {
    if (!muted[instrument]) {
        synth->allNoteOff(instrument);
    }
    muted[instrument] = !muted[instrument];
}



void Sequencer::setTempo(float newTempo) {
    this->tempo = newTempo;
    this->oneBitInMillis = 60.0f * 1000.0f / newTempo / (MAX_PER_BEAT + 1);
    this->millisInBits = 1 / this->oneBitInMillis;
    // Main sequence always 4 bars (16 beats)
    this->sequenceDuration = 60 * 16 * 1000 / this->tempo ;
}


void Sequencer::start() {
    running = true;
    if (!externalClock && current16bitTimer == 0 && millisTimer == 0) {
        precount = 1023;
    }
}

void Sequencer::onMidiContinue(int songPosition) {
    if (externalClock) {
        if (!extMidiRunning) {
            extMidiRunning = true;
            displaySequencer->refresh(17, 17);
        }
    }
}

void Sequencer::onMidiStart() {
    if (externalClock) {
        if (!extMidiRunning) {
            rewind();
            extMidiRunning = true;
            displaySequencer->refresh(17, 17);
        }
    }
}

void Sequencer::onMidiStop() {
    if (externalClock) {
        if (extMidiRunning) {
            extMidiRunning = false;
            for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
                synth->allNoteOff(i);
            }
            displaySequencer->refresh(17, 17);
        }
    }
}


void Sequencer::onMidiClock() {
    if (externalClock && running) {
        // We're called 24 times per beat
        midiClockTimer += (256.0f / 24.0f);
        if (midiClockTimer > 4096.0f) { // 4 (measures) * 4 (beats) * 256
            midiClockTimer -= 4096.0f;
        }
        tic(midiClockTimer);
    }
}


void Sequencer::ticMillis() {
    // if external clock we don't do anything here
    if (externalClock) {
        return;
    }

    uint16_t counter = this->millisTimer * this->millisInBits;
    // if !running we must call tic for the precount
    tic(counter);
    if (likely(running)) {
        this->millisTimer  = (this->millisTimer + 1) % sequenceDuration;
    }
}


void Sequencer::midiClockSetSongPosition(int songPosition) {
    int midiClockTimerInt = songPosition * 4;
    midiClockTimerInt = midiClockTimerInt % 4096;

    this->midiClockTimer = midiClockTimerInt;
}


void Sequencer::tic(uint16_t counter) {

    if (!running)  {
        return;
    }

    if (unlikely(precount > 0)) {
        this->precount -=  this->millisInBits;
        current16bitTimer = this->precount;
        if ((this->current16bitTimer & 0x300) != lastBeat) {
            lastBeat = this->current16bitTimer & 0x300;
            displaySequencer->displayBeat();
        }
        if (likely(precount > 0)) {
            return;
        }
        rewind();
        previousCurrent16bitTimer = 1;
        lastBeat = 1;
    }

    this->current16bitTimer = counter;


    // at 120 BPM we're called (.5 / 16 = 0.031ms) 31 times before a new current16bitTimer value
    if (likely(previousCurrent16bitTimer == current16bitTimer)) {
        return;
    } else {
        previousCurrent16bitTimer = current16bitTimer;
    }

    //
    if ((this->current16bitTimer & 0x300) != lastBeat) {
        lastBeat = this->current16bitTimer & 0x300;
        displaySequencer->displayBeat();
        HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_SET);
        ledTimer = HAL_GetTick();
    } else if (unlikely(HAL_GetTick() - ledTimer > 100)){
        // Led remains on during 1/10th of a sec
        HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_RESET);
        ledTimer = 0Xffffffff;
    }


    for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {

        // Do we have at least one note for this instrument ?
        if (!seqActivated[i] && !stepActivated[instrumentStepSeq[i]]) {
            // if not go to next instrument
            continue;
        }

        int instrument16bitTimer = this->current16bitTimer & instrumentTimerMask[i];

        // Did we just changed the number of bars ?
        // Update nextActionIndex to catch up with the new instrument timer
        if (nextActionTimerOutOfSync[i]) {
            nextActionTimerOutOfSync[i] = false;
            resyncNextAction(i, instrument16bitTimer);
        }

        // instrument16bitTimer loop to 0
        if (unlikely(lastInstrument16bitTimer[i] > instrument16bitTimer)) {
            processActionBetwen(i, lastInstrument16bitTimer[i], instrumentTimerMask[i]);
            //  loop for regular iteration
            lastInstrument16bitTimer[i] = 0;
        }

        // process all events until now
        processActionBetwen(i, lastInstrument16bitTimer[i], instrument16bitTimer);

        lastInstrument16bitTimer[i] = instrument16bitTimer;
    }

}

void Sequencer::resyncNextAction(int instrument, uint16_t newInstrumentTimer) {
    // update lastInstrument16bitTimer[i]
    if (externalClock) {
        lastInstrument16bitTimer[instrument] = newInstrumentTimer - (256.0f / 24.0f);
    } else {
        lastInstrument16bitTimer[instrument] = newInstrumentTimer - this->millisInBits;
    }
    if (lastInstrument16bitTimer[instrument] < 0) {
        lastInstrument16bitTimer[instrument] += 4096;
    } else if (lastInstrument16bitTimer[instrument] >= 4096) {
        lastInstrument16bitTimer[instrument] -= 4096;
    }

    nextActionIndex[instrument] = instrument * 2;
    while (actions[nextActionIndex[instrument]].when < lastInstrument16bitTimer[instrument]) {
        nextActionIndex[instrument] = actions[nextActionIndex[instrument]].nextIndex;
    }
}

void Sequencer::processActionBetwen(int instrument, uint16_t startTimer, uint16_t endTimer) {

    if (stepActivated[instrumentStepSeq[instrument]] && !muted[instrument]) {
        uint8_t currentIndex = endTimer >> 4;
        if (instrumentStepIndex[instrument] != currentIndex) {
        	int seqNumber = instrumentStepSeq[instrument];
            instrumentStepIndex[instrument] = currentIndex;
            uint16_t newUnique = stepNotes[seqNumber][instrumentStepIndex[instrument]].unique;

            if (instrumentStepLastUnique[instrument] != newUnique || currentIndex == 0) {
                instrumentStepLastUnique[instrument] = newUnique;

                uint8_t previousIndex = (currentIndex - 1) & (instrumentTimerMask[instrument] >> 4);
                if ((stepNotes[seqNumber][currentIndex].full & 0xffffff00) == 0l) {
                    if ((stepNotes[seqNumber][previousIndex].full & 0xffffff00) != 0l) {
                        // Note off
                        for (int n = 0; stepNotes[seqNumber][previousIndex].values[3 + n] != 0 && n < 6; n++) {
                            synth->noteOffFromSequencer(instrument, stepNotes[seqNumber][previousIndex].values[3 + n]);
                        }
                    }
                } else {
                    // Note off
                    if ((stepNotes[seqNumber][previousIndex].full & 0xffffff00) != 0l) {
                        for (int n = 0; stepNotes[seqNumber][previousIndex].values[3 + n] != 0 && n < 6; n++) {
                            synth->noteOffFromSequencer(instrument, stepNotes[seqNumber][previousIndex].values[3 + n]);
                        }
                    }
                    // Note on
                    for (int n = 0; stepNotes[seqNumber][currentIndex].values[3 + n] != 0 && n < 6; n++) {
                        synth->noteOnFromSequencer(instrument, stepNotes[seqNumber][currentIndex].values[3 + n],
                            stepNotes[seqNumber][currentIndex].values[2]);
                    }
                }
            }
        }
    }

    // Play to the end before looping
    // We test activated as we can clear or apply step seq in the middle of this loop
    if (seqActivated[instrument]) {
        while (actions[nextActionIndex[instrument]].when >= startTimer && actions[nextActionIndex[instrument]].when <= endTimer && seqActivated[instrument]) {
            // actions[nextActionIndex];
            if (!muted[instrument]) {
                switch (actions[nextActionIndex[instrument]].actionType) {
                case SEQ_ACTION_NOTE:
                    if (actions[nextActionIndex[instrument]].param2 > 0) {
                        synth->noteOnFromSequencer(instrument, actions[nextActionIndex[instrument]].param1, actions[nextActionIndex[instrument]].param2);
                    } else {
                        synth->noteOffFromSequencer(instrument, actions[nextActionIndex[instrument]].param1);
                    }
                    break;
                }
            }

            previousActionIndex[instrument] = nextActionIndex[instrument];
            nextActionIndex[instrument] = actions[nextActionIndex[instrument]].nextIndex;

            if (actions[nextActionIndex[instrument]].when > instrumentTimerMask[instrument]) {
                // We have events after the instrument bar limit, we must loop now
                previousActionIndex[instrument] = instrument * 2;
                nextActionIndex[instrument] = instrument * 2;
            }
        }
    }
}



void Sequencer::clear(uint8_t instrument) {

    if (!isSeqActivated(instrument)) {
        return;
    }

    // Clear instrument
    seqActivated[instrument] = false;

    this->nextActionIndex[instrument] = instrument * 2 + 1;
    this->previousActionIndex[instrument] = instrument * 2;

    actions[instrument * 2].nextIndex = instrument * 2 + 1;

    bool allInactive = true;
    for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
        if (isSeqActivated(i)) {
            allInactive = false;
        }
    }
    if (allInactive) {
        // Nothing to do
        this->lastFreeAction = NUMBER_OF_TIMBRES * 2;
        return;
    }

    uint16_t index = actions[instrument * 2].nextIndex;
    uint16_t end = instrument * 2 + 1;

    while (index != end) {
        actions[index].actionType = SEQ_ACTION_NONE;
        index = actions[index].nextIndex;
    }

    for (int i = (NUMBER_OF_TIMBRES * 2); i < lastFreeAction; i++) {
        if (actions[i].actionType == SEQ_ACTION_NONE) {
            // Move next one to this index : i
            int movedFromIndex = 0;
            for (int j = lastFreeAction - 1; j > i; j--) {
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
                    if (nextActionIndex[instrument] == movedFromIndex) {
                        nextActionIndex[instrument] = i;
                        break;
                    }
                }
                // Update previous index
                // Can be in the first indexes !
                for (int j = 0; j < lastFreeAction; j++) {
                    if (actions[j].nextIndex == movedFromIndex) {
                        // update index
                        actions[j].nextIndex = i;
                        break;
                    }
                }
            }
        }
    }
    for (int i = (NUMBER_OF_TIMBRES * 2); i < lastFreeAction; i++) {
        if (actions[i].actionType == SEQ_ACTION_NONE) {
            lastFreeAction = i;
            break;
        }
    }

    synth->allNoteOff(instrument);

}



// From main loop thread
// Different lower priority thread than tic() above
// We save in the stepCurrentInstrument !
void Sequencer::insertNote(uint8_t instrument, uint8_t note, uint8_t velocity) {
    if (stepMode) {
        if (velocity > 0) {
            if (stepNumberOfNotesOn < 5) {
                if (stepNumberOfNotesOn == 0){
                	tmpStepValue.values[2] = velocity;
                }
                tmpStepValue.values[3 + stepNumberOfNotesOn] = note;
            }
            stepNumberOfNotesOn ++;

        } else {
            stepNumberOfNotesOn --;
            if (stepNumberOfNotesOn == 0) {
                displaySequencer->newNoteEntered(instrument);
            }
        }
        return;
    }

    if (!isRunning() || precount > 0 || !isRecording(instrument)) {
        return;
    }

    if (lastFreeAction < SEQ_ACTION_SIZE) {
        SeqMidiAction* newAction = &actions[lastFreeAction];
        newAction->when = this->current16bitTimer & instrumentTimerMask[instrument];
        newAction->actionType = SEQ_ACTION_NOTE;
        newAction->param1 = note;
        newAction->param2 = velocity;
        newAction->nextIndex = actions[previousActionIndex[instrument]].nextIndex;
        actions[previousActionIndex[instrument]].nextIndex = lastFreeAction;

        previousActionIndex[instrument] = lastFreeAction;

        lastFreeAction++;

        if (unlikely(setSeqActivated(instrument))) {
            displaySequencer->refreshActivated();
        } else if (unlikely((lastFreeAction & 0xF) == 0)) {
            displaySequencer->refreshMemory();
        }
    }
}


/*
 * Returns true if more thant one note
 */
bool Sequencer::stepRecordNotes(int instrument, int stepCursor, int stepSize) {
    bool moreThanOneNote;
	int seqNumber = instrumentStepSeq[instrument];

    for (int s = stepCursor; (s < (stepCursor + stepSize)) && (s < 256); s++) {
        stepNotes[seqNumber][s].full = tmpStepValue.full;
        stepNotes[seqNumber][s].unique = stepUniqueValue[instrument];
    }
    stepUniqueValue[instrument]++;
    moreThanOneNote = (tmpStepValue.values[4] > 0);
    tmpStepValue.full = 0l;
    stepNumberOfNotesOn = 0;

    if (unlikely(!stepActivated[seqNumber])) {
    	stepActivated[seqNumber] = true;
    }
    return moreThanOneNote;
}

void Sequencer::stepClearPart(int instrument, int stepCursor, int stepSize) {
	int seqNumber = instrumentStepSeq[instrument];
    for (int s = stepCursor; (s < (stepCursor + stepSize)) && (s < 255); s++) {
        stepNotes[seqNumber][s].full = 0l;
        stepNotes[seqNumber][s].unique = stepUniqueValue[instrument];
    }
    stepUniqueValue[instrument]++;
    tmpStepValue.full = 0l;
    stepNumberOfNotesOn = 0;
}


StepSeqValue* Sequencer::stepGetSequence(int instrument) {
	int seqNumber = instrumentStepSeq[instrument];
    return stepNotes[seqNumber];
}

void Sequencer::stepClearAll(int instrument) {
	int seqNumber = instrumentStepSeq[instrument];
    synth->allNoteOff(instrument);
    for (int s = 0; s < 255; s++) {
        stepNotes[seqNumber][s].full = 0;
    }
    stepUniqueValue[instrument] = 1;
    stepNumberOfNotesOn = 0;
    stepActivated[seqNumber] = false;
}


/*
 * Used to create empty bank
 */
void Sequencer::getFullDefaultState(uint8_t* buffer, uint32_t *size) {
    int index = 0;
    buffer[index++] = (uint8_t)SEQ_CURRENT_VERSION;
    // sequenceName
    const char defautSequenceName[13] = "First Seq\0\0\0";
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
        buffer[index++] = sequenceName[s];
    }
    buffer[index++] = (uint8_t)(isExternalClockEnabled() ? 1 : 0);
    *((float*)&buffer[index]) = tempo;
    index+=sizeof(float);
    *((uint16_t*)&buffer[index]) = lastFreeAction;
    index+=sizeof(uint16_t);
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        *((uint16_t*)&buffer[index]) = stepUniqueValue[t];
        index += sizeof(uint16_t);
        *((uint16_t*)&buffer[index]) = instrumentTimerMask[t];
        index += sizeof(uint16_t);
        buffer[index++] = (seqActivated[t] ? 1 : 0);
        buffer[index++] = (recording[t] ? 1 : 0);
        buffer[index++] = (muted[t] ? 1 : 0);
        buffer[index++] = instrumentStepSeq[t];
    }

    for (int s = 0; s < NUMBER_OF_STEP_SEQUENCES; s++) {
    	buffer[index++] = (stepActivated[s] ? 1 : 0);
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
        sequenceName[s] = buffer[index++];
    }
    setExternalClock(buffer[index++] == 1);

    // Needed to load from mem to float !
    uint8_t tempoUint8[4];
    for (int i = 0; i < 4; i++) {
        tempoUint8[i] = buffer[index++];
    }
    float newTempo = * ((float*)tempoUint8);
    setTempo(newTempo);

    lastFreeAction = *((uint16_t*)&buffer[index]) ;
    index+=sizeof(uint16_t);
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        stepUniqueValue[t] = *((uint16_t*)(&buffer[index]));
        index+=sizeof(uint16_t);
        instrumentTimerMask[t] = *((uint16_t*)(&buffer[index]));
        index+=sizeof(uint16_t);
        seqActivated[t]  = (buffer[index++] == 1);
        stepActivated[t]  = (buffer[index++] == 1);
        recording[t]  = (buffer[index++] == 1);
        muted[t]  = (buffer[index++] == 1);
        // init default Value
        instrumentStepSeq[t] = t;
    }
}

void Sequencer::loadStateVersion2(uint8_t* buffer) {
    uint32_t index = 0;
    index++; // VERSION
    for (int s = 0; s < 12; s++) {
        sequenceName[s] = buffer[index++];
    }
    setExternalClock(buffer[index++] == 1);

    // Needed to load from mem to float !
    uint8_t tempoUint8[4];
    for (int i = 0; i < 4; i++) {
        tempoUint8[i] = buffer[index++];
    }
    float newTempo = * ((float*)tempoUint8);
    setTempo(newTempo);

    lastFreeAction = *((uint16_t*)&buffer[index]) ;
    index+=sizeof(uint16_t);
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        stepUniqueValue[t] = *((uint16_t*)(&buffer[index]));
        index+=sizeof(uint16_t);
        instrumentTimerMask[t] = *((uint16_t*)(&buffer[index]));
        index+=sizeof(uint16_t);
        seqActivated[t]  = (buffer[index++] == 1);
        recording[t]  = (buffer[index++] == 1);
        muted[t]  = (buffer[index++] == 1);
        instrumentStepSeq[t] = buffer[index++];
    }
    for (int s = 0; s < NUMBER_OF_STEP_SEQUENCES; s++) {
        stepActivated[s]  = (buffer[index++] == 1);
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

void Sequencer::setSequenceName(const char* newName) {
    for (int n = 0; n < 12; n++) {
        sequenceName[n] = newName[n];
    }
    sequenceName[12] = 0;
}
