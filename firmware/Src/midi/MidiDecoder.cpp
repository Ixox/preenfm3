/*
 * Copyright 2013
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
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

extern "C" {
#include "usbd_midi.h"
}

#include "MidiDecoder.h"
#include "RingBuffer.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

// Let's call it MidiOut despite USB spec
uint8_t usbMidiOutBuff[64];
uint8_t *usbMidiOutBuffWrt;

extern UART_HandleTypeDef huart1;

#define INV127 .00787401574803149606f
#define INV64  .015625f

RingBuffer<uint8_t, 64> usartBufferIn;
RingBuffer<uint8_t, 64> usartBufferOut;
RingBuffer<AsyncAction, 16> asyncActions;


// Let's have sysexBuffer in regular RAM.
#define SYSEX_BUFFER_SIZE 32
uint8_t sysexBuffer[SYSEX_BUFFER_SIZE];


MidiDecoder::MidiDecoder() {
    currentEventState.eventState = MIDI_EVENT_WAITING;
    currentEventState.index = 0;
    this->isExternalMidiClockStarted = false;
    this->midiClockCpt = 0;
    this->runningStatus = 0;
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        omniOn[t] = false;
        bankNumber[t] = 0;
        bankNumberLSB[t] = 0;
    }

    for (int k = 0; k < 64; k++) {
        usbMidiOutBuff[k] = 0;
    }

    usbMidiOutBuffWrt = usbMidiOutBuff;
}

MidiDecoder::~MidiDecoder() {
}

void MidiDecoder::setSynth(Synth* synth) {
    this->synth = synth;
}

void MidiDecoder::setVisualInfo(VisualInfo *visualInfo) {
    this->visualInfo = visualInfo;
}

void MidiDecoder::newByte(unsigned char byte) {
    // Realtime first !
    if (byte >= 0xF8) {
        switch (byte) {
        case MIDI_CLOCK:
            this->midiClockCpt++;
            this->synth->midiTick(true);
            if (unlikely(this->midiClockCpt == 6)) {
                if (this->isExternalMidiClockStarted) {
                    this->songPosition++;
                }
                this->midiClockCpt = 0;
                this->synth->midiClockSongPositionStep(this->songPosition);

                if (unlikely((this->songPosition & 0x3) == 0)) {
                    this->visualInfo->midiClock(true);
                }
                if (unlikely((this->songPosition & 0x3) == 0x2)) {
                    this->visualInfo->midiClock(false);
                }
            }
            break;
        case MIDI_START:
            if (!this->isExternalMidiClockStarted) {
                this->isExternalMidiClockStarted = true;
                this->songPosition = 0;
                this->midiClockCpt = 0;
                this->synth->midiClockStart(true);
            }
            break;
        case MIDI_CONTINUE:
            if (!this->isExternalMidiClockStarted) {
                this->isExternalMidiClockStarted = true;
                this->midiClockCpt = 0;
                this->synth->midiClockContinue(this->songPosition, true);
            }
            break;
        case MIDI_STOP:
            if (this->isExternalMidiClockStarted) {
                this->isExternalMidiClockStarted = false;
                this->synth->midiClockStop(true);
                this->visualInfo->midiClock(false);
            }
            break;
        }
    } else {
        switch (currentEventState.eventState) {
        case MIDI_EVENT_WAITING:
            if (unlikely(byte >= 0x80)) {
                // Running status is cleared later in newMEssageType() if byte >= 0xF0
                this->runningStatus = byte;
                newMessageType(byte);
            } else {
                // midi source use running status...
                if (this->runningStatus > 0) {
                    newMessageType(this->runningStatus);
                    newMessageData(byte);
                }
            }
            break;
        case MIDI_EVENT_IN_PROGRESS:
            newMessageData(byte);
            break;
        case MIDI_EVENT_SYSEX:
            if (likely(currentEventState.index < SYSEX_BUFFER_SIZE)) {
                sysexBuffer[currentEventState.index++] = byte;
            }
            if (byte == MIDI_SYSEX_END) {
                if (currentEventState.index < SYSEX_BUFFER_SIZE) {
                    // End of sysex =>Analyse Sysex
                    if (this->analyseSysexBuffer(sysexBuffer, currentEventState.index) == 1) {
                        currentEventState.eventState = MIDI_EVENT_COMPLETE;
                    }
                }
                currentEventState.eventState = MIDI_EVENT_WAITING;
                currentEventState.index = 0;
            }
            break;
        }
    }
}

void MidiDecoder::newMessageData(unsigned char byte) {
    currentEvent.value[currentEventState.index++] = byte;
    if (currentEventState.index == currentEventState.numberOfBytes) {
        midiEventReceived(currentEvent);
        currentEventState.eventState = MIDI_EVENT_WAITING;
        currentEventState.index = 0;
    }
}

void MidiDecoder::newMessageType(unsigned char byte) {
    currentEvent.eventType = (EventType) (byte & 0xf0);
    currentEvent.channel = byte & 0x0f;

    switch (currentEvent.eventType) {
    case MIDI_NOTE_OFF:
    case MIDI_NOTE_ON:
    case MIDI_CONTROL_CHANGE:
    case MIDI_PITCH_BEND:
    case MIDI_POLY_AFTER_TOUCH:
        currentEventState.numberOfBytes = 2;
        currentEventState.eventState = MIDI_EVENT_IN_PROGRESS;
        break;
    case MIDI_AFTER_TOUCH:
    case MIDI_PROGRAM_CHANGE:
        currentEventState.numberOfBytes = 1;
        currentEventState.eventState = MIDI_EVENT_IN_PROGRESS;
        break;
    case MIDI_SYSTEM_COMMON:
        this->runningStatus = 0;
        switch (byte) {
        case MIDI_SYSEX:
            currentEventState.eventState = MIDI_EVENT_SYSEX;
            currentEventState.index = 0;
            break;
        case MIDI_SONG_POSITION:
            currentEvent.eventType = MIDI_SONG_POSITION;
            // Channel hack to make it accepted
            if (this->synthState_->mixerState.instrumentState_[0].midiChannel == 0) {
                currentEvent.channel = 0;
            } else {
                currentEvent.channel = this->synthState_->mixerState.instrumentState_[0].midiChannel - 1;
            }
            currentEventState.numberOfBytes = 2;
            currentEventState.eventState = MIDI_EVENT_IN_PROGRESS;
            break;
        }
        break;
    default:
        // Nothing to do...
        break;
    }
}



void MidiDecoder::midiEventReceived(MidiEvent& midiEvent) {
    int timbreIndex = 0;
    int timbres[6];
    bool isInst1MPE = this->synthState_->mixerState.MPE_inst1_ > 0;

    if (unlikely(isInst1MPE)) {
    	// Special MPE Case
    	midiEventForInstrument1MPE(midiEvent);
    }

    if (unlikely(midiEvent.channel == (this->synthState_->mixerState.globalChannel_ - 1))) {
        // Midi global => all timbres
    	// add instrument 1 if not MPE
    	if (likely(!isInst1MPE)) {
    			timbres[timbreIndex++] = 0;
    	}
        timbres[timbreIndex++] = 1;
        timbres[timbreIndex++] = 2;
        timbres[timbreIndex++] = 3;
        timbres[timbreIndex++] = 4;
        timbres[timbreIndex++] = 5;
    } else if (unlikely(midiEvent.channel == (this->synthState_->mixerState.currentChannel_ - 1))) {
        // Midi current
        timbres[timbreIndex++] = currentTimbre;
    } else {
        if (omniOn[0] || this->synthState_->mixerState.instrumentState_[0].midiChannel == 0
                || (this->synthState_->mixerState.instrumentState_[0].midiChannel - 1) == midiEvent.channel) {
        	if (likely(!isInst1MPE)) {
        		timbres[timbreIndex++] = 0;
        	}
        }
        if (omniOn[1] || this->synthState_->mixerState.instrumentState_[1].midiChannel == 0
                || (this->synthState_->mixerState.instrumentState_[1].midiChannel - 1) == midiEvent.channel) {
            timbres[timbreIndex++] = 1;
        }
        if (omniOn[2] || this->synthState_->mixerState.instrumentState_[2].midiChannel == 0
                || (this->synthState_->mixerState.instrumentState_[2].midiChannel - 1) == midiEvent.channel) {
            timbres[timbreIndex++] = 2;
        }
        if (omniOn[3] || this->synthState_->mixerState.instrumentState_[3].midiChannel == 0
                || (this->synthState_->mixerState.instrumentState_[3].midiChannel - 1) == midiEvent.channel) {
            timbres[timbreIndex++] = 3;
        }
        if (omniOn[4] || this->synthState_->mixerState.instrumentState_[4].midiChannel == 0
                || (this->synthState_->mixerState.instrumentState_[4].midiChannel - 1) == midiEvent.channel) {
            timbres[timbreIndex++] = 4;
        }
        if (omniOn[5] || this->synthState_->mixerState.instrumentState_[5].midiChannel == 0
                || (this->synthState_->mixerState.instrumentState_[5].midiChannel - 1) == midiEvent.channel) {
            timbres[timbreIndex++] = 5;
        }
    }

    if (timbreIndex == 0) {
        // No accurate channel
        return;
    }

    switch (midiEvent.eventType) {
    case MIDI_NOTE_OFF:
        for (int tk = 0; tk < timbreIndex; tk++) {
            int timbre = timbres[tk];
            char note = midiEvent.value[0] + this->synthState_->mixerState.instrumentState_[timbre].shiftNote;
            if (likely(
                    midiEvent.value[0] >= this->synthState_->mixerState.instrumentState_[timbre].firstNote
                    && midiEvent.value[0] <= this->synthState_->mixerState.instrumentState_[timbre].lastNote)
                    && note >= 0 && note <= 127) {
                this->synth->noteOff(timbre, note);
            }
        }
        break;
    case MIDI_NOTE_ON:
        // Some keyboards send note-off this way
        for (int tk = 0; tk < timbreIndex; tk++) {
            int timbre = timbres[tk];
            char note = midiEvent.value[0] + this->synthState_->mixerState.instrumentState_[timbre].shiftNote;
            if (likely(
                    midiEvent.value[0] >= this->synthState_->mixerState.instrumentState_[timbre].firstNote
                    && midiEvent.value[0] <= this->synthState_->mixerState.instrumentState_[timbre].lastNote
                    && note >= 0 && note <= 127)) {
                if (midiEvent.value[1] == 0) {
                    this->synth->noteOff(timbre, note);
                } else {
                    this->synth->noteOn(timbre, note, midiEvent.value[1]);
                    visualInfo->noteOn(timbre, true);
                }
            }
        }
        break;
    case MIDI_CONTROL_CHANGE:
        for (int tk = 0; tk < timbreIndex; tk++) {
            controlChange(timbres[tk], midiEvent);
        }
        break;
    case MIDI_POLY_AFTER_TOUCH:
        for (int tk = 0; tk < timbreIndex; tk++) {
            int timbre = timbres[tk];
            char note = midiEvent.value[0] + this->synthState_->mixerState.instrumentState_[timbre].shiftNote;
            if (likely(note >= 0 && note <= 127)) {
                this->synth->getTimbre(timbres[tk])->setMatrixPolyAfterTouch(note, INV127 * midiEvent.value[1]);
            }
        }
        break;
    case MIDI_AFTER_TOUCH:
        for (int tk = 0; tk < timbreIndex; tk++) {
            this->synth->getTimbre(timbres[tk])->setMatrixSource(MATRIX_SOURCE_AFTERTOUCH, INV127 * midiEvent.value[0]);
        }
        break;
    case MIDI_PITCH_BEND: {
        int pb = ((int) midiEvent.value[1] << 7) + (int) midiEvent.value[0] - 8192;
        for (int tk = 0; tk < timbreIndex; tk++) {
            this->synth->getTimbre(timbres[tk])->setMatrixSource(MATRIX_SOURCE_PITCHBEND, (float) pb * .00012207031250000000f);
        }
        break;
    }
    case MIDI_PROGRAM_CHANGE:
        if (this->synthState_->fullState.midiConfigValue[MIDICONFIG_PROGRAM_CHANGE]) {
            AsyncAction newAction;
            newAction.fullBytes = 0l;
            newAction.action.actionType = LOAD_PRESET;
            for (int tk = 0; tk < timbreIndex; tk++) {
                newAction.action.timbre |= (1 << timbres[tk]);
            }
            newAction.action.param1 = bankNumber[timbres[0]];
            newAction.action.param2 = bankNumberLSB[timbres[0]];
            newAction.action.param3 = midiEvent.value[0];
            asyncActions.insert(newAction);
        }
        break;
    case MIDI_SONG_POSITION:
        this->songPosition = ((int) midiEvent.value[1] << 7) + midiEvent.value[0];
        this->synth->midiClockSetSongPosition(this->songPosition, true);
        break;
    }
}


void MidiDecoder::midiEventForInstrument1MPE(MidiEvent& midiEvent) {

	// MPE only available on instrument
	int timbre = 0;

	if (midiEvent.channel == 0) {
		// special case for global midi
    	// CC74 is per voice, the rest is only on channel 0
		switch (midiEvent.eventType) {
	    case MIDI_CONTROL_CHANGE: {
			controlChange(0, midiEvent);
	    }
		case MIDI_PITCH_BEND: {
			int pb = ((int) midiEvent.value[1] << 7) + (int) midiEvent.value[0] - 8192;
			// PITCH BEND in MPE mode we devide by 819.2 and not 8192 to have a x10 value
			this->synth->getTimbre(0)->setMatrixSource(MATRIX_SOURCE_PITCHBEND, (float) pb * .00012207031250000000f);
			break;
		}
	    case MIDI_AFTER_TOUCH: {
	        this->synth->getTimbre(0)->setMatrixSource(MATRIX_SOURCE_AFTERTOUCH, INV127 * midiEvent.value[0]);
	        break;
	    }
		}
		return;
	}


	switch (midiEvent.eventType) {
    case MIDI_NOTE_OFF: {
		char note = midiEvent.value[0] + this->synthState_->mixerState.instrumentState_[timbre].shiftNote;
		if (likely(
				midiEvent.value[0] >= this->synthState_->mixerState.instrumentState_[timbre].firstNote
				&& midiEvent.value[0] <= this->synthState_->mixerState.instrumentState_[timbre].lastNote)
				&& note >= 0 && note <= 127) {
			this->synth->getTimbre(0)->noteOffMPE(midiEvent.channel, note, midiEvent.value[1]);
		}
        break;
    }
    case MIDI_NOTE_ON: {
		char note = midiEvent.value[0] + this->synthState_->mixerState.instrumentState_[timbre].shiftNote;
		if (likely(
				midiEvent.value[0] >= this->synthState_->mixerState.instrumentState_[timbre].firstNote
				&& midiEvent.value[0] <= this->synthState_->mixerState.instrumentState_[timbre].lastNote
				&& note >= 0 && note <= 127)) {
			if (midiEvent.value[1] == 0) {
				this->synth->getTimbre(0)->noteOffMPE(midiEvent.channel, note, midiEvent.value[1]);
			} else {
				this->synth->getTimbre(0)->noteOnMPE(midiEvent.channel, note, midiEvent.value[1]);
				visualInfo->noteOn(0, true);
			}
		}
        break;
    }
    case MIDI_AFTER_TOUCH: {
        this->synth->getTimbre(0)->setMatrixSourceMPE(midiEvent.channel, MATRIX_SOURCE_AFTERTOUCH_MPE, INV127 * midiEvent.value[0]);
        break;
    }
    case MIDI_CONTROL_CHANGE:
		switch (midiEvent.value[0]) {
		case CC_MPE_SLIDE_CC74:
			this->synth->getTimbre(0)->setMatrixSourceMPE(midiEvent.channel, MATRIX_SOURCE_MPESLIDE, INV127 * midiEvent.value[1]);
		}
        break;
    case MIDI_PITCH_BEND:
		int pb = ((int) midiEvent.value[1] << 7) + (int) midiEvent.value[0] - 8192;
		this->synth->getTimbre(0)->setMatrixSourceMPE(midiEvent.channel, MATRIX_SOURCE_PITCHBEND_MPE, (float) pb * .00012207031250000000f);
        break;
	}
}



void MidiDecoder::controlChange(int timbre, MidiEvent& midiEvent) {
    int receives = this->synthState_->fullState.midiConfigValue[MIDICONFIG_RECEIVES];

    // User CC are higher priority even if they hide some important preenfm3 CC
    if (unlikely(this->synthState_->mixerState.userCC_[0] == midiEvent.value[0])) {
        this->synth->getTimbre(timbre)->setMatrixSource(MATRIX_SOURCE_USER_CC1, INV127 * midiEvent.value[1]);
        return;
    }
    if (unlikely(this->synthState_->mixerState.userCC_[1] == midiEvent.value[0])) {
        this->synth->getTimbre(timbre)->setMatrixSource(MATRIX_SOURCE_USER_CC2, INV127 * midiEvent.value[1]);
        return;
    }
    if (unlikely(this->synthState_->mixerState.userCC_[2] == midiEvent.value[0])) {
        this->synth->getTimbre(timbre)->setMatrixSource(MATRIX_SOURCE_USER_CC3, INV127 * midiEvent.value[1]);
        return;
    }
    if (unlikely(this->synthState_->mixerState.userCC_[3] == midiEvent.value[0])) {
        this->synth->getTimbre(timbre)->setMatrixSource(MATRIX_SOURCE_USER_CC4, INV127 * midiEvent.value[1]);
        return;
    }

    if (midiEvent.channel == this->synthState_->mixerState.globalChannel_ - 1) {
        // treat global channel CC
        switch (midiEvent.value[0])
        {
        case CC_MFX_PRESET:
            this->synthState_->mixerState.reverbPreset_ = midiEvent.value[1];
            this->synthState_->mixerState.fxBus_.nextPresetNum = midiEvent.value[1];
            this->synthState_->mixerState.fxBus_.slowParamChange();
            break;
        case CC_MFX_PREDELAYTIME:
            this->synthState_->mixerState.fxBus_.masterfxConfig[GLOBALFX_PREDELAYTIME] = INV127 * midiEvent.value[1];
            this->synthState_->mixerState.fxBus_.slowParamChange();
            break;
        case CC_MFX_PREDELAYMIX:
            this->synthState_->mixerState.fxBus_.masterfxConfig[GLOBALFX_PREDELAYMIX] = INV127 * midiEvent.value[1];
            this->synthState_->mixerState.fxBus_.slowParamChange();
            break;
        case CC_MFX_INPUTTILT:
            this->synthState_->mixerState.fxBus_.masterfxConfig[GLOBALFX_INPUTBASE] = INV127 * midiEvent.value[1];
            this->synthState_->mixerState.fxBus_.slowParamChange();
            break;
        case CC_MFX_MOD_SPEED:
            this->synthState_->mixerState.fxBus_.masterfxConfig[GLOBALFX_LFOSPEED] = INV127 * midiEvent.value[1];
            this->synthState_->mixerState.fxBus_.slowParamChange();
            break;
        case CC_MFX_MOD_DEPTH:
            this->synthState_->mixerState.fxBus_.masterfxConfig[GLOBALFX_LFODEPTH] = INV127 * midiEvent.value[1];
            this->synthState_->mixerState.fxBus_.slowParamChange();
            break;
        }
    }
    
    // the following one should always been treated...
    switch (midiEvent.value[0]) {
    case CC_BANK_SELECT:
        bankNumber[timbre] = midiEvent.value[1];
        break;
    case CC_BANK_SELECT_LSB:
        bankNumberLSB[timbre] = midiEvent.value[1];
        break;
    case CC_MODWHEEL:
        this->synth->getTimbre(timbre)->setMatrixSource(MATRIX_SOURCE_MODWHEEL, INV127 * midiEvent.value[1]);
        break;
    case CC_BREATH:
        this->synth->getTimbre(timbre)->setMatrixSource(MATRIX_SOURCE_BREATH, INV127 * midiEvent.value[1]);
        break;
    case CC_ALL_NOTES_OFF:
        this->synth->stopArpegiator(timbre);
        this->synth->allNoteOff(timbre);
        break;
    case CC_ALL_SOUND_OFF:
        this->synth->allSoundOff(timbre);
        break;
    case CC_HOLD_PEDAL:
        this->synth->setHoldPedal(timbre, midiEvent.value[1]);
        break;
    case CC_OMNI_OFF:
        // Omni on && omni OFF only accepted on original channel
        if (this->synthState_->mixerState.instrumentState_[timbre].midiChannel == midiEvent.channel) {
            this->synth->stopArpegiator(timbre);
            this->synth->allNoteOff(timbre);
            omniOn[timbre] = false;
        }
        break;
    case CC_OMNI_ON:
        // Omni on && omni OFF only accepted on original channel
        if (this->synthState_->mixerState.instrumentState_[timbre].midiChannel == midiEvent.channel) {
            this->synth->stopArpegiator(timbre);
            this->synth->allNoteOff(timbre);
            omniOn[timbre] = true;
        }
        break;
    case CC_RESET:
        this->synth->stopArpegiator(timbre);
        this->synth->allNoteOff(timbre);
        this->runningStatus = 0;
        this->songPosition = 0;
        break;
    case CC_CURRENT_INSTRUMENT:
        this->synth->setCurrentInstrument(midiEvent.value[1]);
        break;
    }

    // Receive CC enabled?
    if (receives & 0x01) {
        switch (midiEvent.value[0]) {
        case CC_ALGO:
            this->synth->setNewValueFromMidi(timbre, ROW_ENGINE, ENCODER_ENGINE_ALGO, (float) midiEvent.value[1]);
            break;
        case CC_IM1:
        case CC_IM2:
            this->synth->setNewValueFromMidi(timbre, ROW_MODULATION1, ENCODER_ENGINE_IM1 + (midiEvent.value[0] - CC_IM1) * 2,
                    (float) midiEvent.value[1] * .1f);
            break;
        case CC_IM3:
        case CC_IM4:
            this->synth->setNewValueFromMidi(timbre, ROW_MODULATION2, ENCODER_ENGINE_IM3 + (midiEvent.value[0] - CC_IM3) * 2,
                    (float) midiEvent.value[1] * .1f);
            break;
        case CC_IM5:
            this->synth->setNewValueFromMidi(timbre, ROW_MODULATION3, ENCODER_ENGINE_IM5,
                    (float) midiEvent.value[1] * .1f);
            break;
        case CC_IM_FEEDBACK:
            this->synth->setNewValueFromMidi(timbre, ROW_MODULATION3, ENCODER_ENGINE_IM6,
                    (float) midiEvent.value[1] * INV127);
            break;
        case CC_MIX1:
        case CC_MIX2:
            this->synth->setNewValueFromMidi(timbre, ROW_OSC_MIX1, ENCODER_ENGINE_MIX1 + (midiEvent.value[0] - CC_MIX1),
                    (float) midiEvent.value[1] * .00787401574803149606f);
            break;
        case CC_MIX3:
        case CC_MIX4:
            this->synth->setNewValueFromMidi(timbre, ROW_OSC_MIX2, ENCODER_ENGINE_MIX3 + (midiEvent.value[0] - CC_MIX3),
                    (float) midiEvent.value[1] * .00787401574803149606f);
            break;
        case CC_PAN1:
        case CC_PAN2:
            this->synth->setNewValueFromMidi(timbre, ROW_OSC_MIX1, ENCODER_ENGINE_PAN1 + (midiEvent.value[0] - CC_PAN1),
                    (float) midiEvent.value[1] * .00787401574803149606f * 2.0f - 1.0f);
            break;
        case CC_PAN3:
        case CC_PAN4:
            this->synth->setNewValueFromMidi(timbre, ROW_OSC_MIX2, ENCODER_ENGINE_PAN3 + (midiEvent.value[0] - CC_PAN3),
                    (float) midiEvent.value[1] * .00787401574803149606f * 2.0f - 1.0f);
            break;
        case CC_OSC1_FREQ:
        case CC_OSC2_FREQ:
        case CC_OSC3_FREQ:
        case CC_OSC4_FREQ:
        case CC_OSC5_FREQ:
        case CC_OSC6_FREQ:
            this->synth->setNewValueFromMidi(timbre, ROW_OSC1 + midiEvent.value[0] - CC_OSC1_FREQ, ENCODER_OSC_FREQ,
                    (float) midiEvent.value[1] * .0833333333333333f);
            break;
        case CC_MATRIXROW1_MUL:
        case CC_MATRIXROW2_MUL:
        case CC_MATRIXROW3_MUL:
        case CC_MATRIXROW4_MUL:
            // cc.value[1] = newValue * 5.0f + 50.1f;
            this->synth->setNewValueFromMidi(timbre, ROW_MATRIX1 + midiEvent.value[0] - CC_MATRIXROW1_MUL, ENCODER_MATRIX_MUL,
                    (float) midiEvent.value[1] * .2f - 10.0f);
            break;
        case CC_LFO1_FREQ:
        case CC_LFO2_FREQ:
        case CC_LFO3_FREQ:
            // cc.value[1] = newValue * 5.0f + .1f;
            this->synth->setNewValueFromMidi(timbre, ROW_LFOOSC1 + midiEvent.value[0] - CC_LFO1_FREQ, ENCODER_LFO_FREQ,
                    (float) midiEvent.value[1] * .2f);
            break;
        case CC_LFO_ENV2_SILENCE:
            this->synth->setNewValueFromMidi(timbre, ROW_LFOENV2, ENCODER_LFO_ENV2_SILENCE, (float) midiEvent.value[1] * .125f);
            break;
        case CC_STEPSEQ5_GATE:
        case CC_STEPSEQ6_GATE:
            // cc.value[1] = newValue * 100.0f + .1f;
            this->synth->setNewValueFromMidi(timbre, ROW_LFOSEQ1 + midiEvent.value[0] - CC_STEPSEQ5_GATE, ENCODER_STEPSEQ_GATE,
                    (float) midiEvent.value[1] * .01f);
            break;
        case CC_MATRIX_SOURCE_CC1:
        case CC_MATRIX_SOURCE_CC2:
        case CC_MATRIX_SOURCE_CC3:
        case CC_MATRIX_SOURCE_CC4:
            // cc.value[1] = newValue * 5.0f + 50.1f;
            this->synth->setNewValueFromMidi(timbre, ROW_PERFORMANCE1, midiEvent.value[0] - CC_MATRIX_SOURCE_CC1,
                    (float) (midiEvent.value[1] - 64) * INV127 * 2.0);
            break;

            break;
        case CC_FILTER_TYPE:
            this->synth->setNewValueFromMidi(timbre, ROW_EFFECT, ENCODER_EFFECT_TYPE, (float) midiEvent.value[1]);
            break;
        case CC_FILTER_PARAM1:
        case CC_FILTER_PARAM2:
            this->synth->setNewValueFromMidi(timbre, ROW_EFFECT, midiEvent.value[0] - CC_FILTER_PARAM1 + 1,
                    (float) midiEvent.value[1] * INV127);
            break;
        case CC_FILTER_GAIN:
            this->synth->setNewValueFromMidi(timbre, ROW_EFFECT, midiEvent.value[0] - CC_FILTER_PARAM1 + 1,
                    (float) midiEvent.value[1] * .01f);
            break;
        case CC_ENV_ATK_OP1:
            this->synth->setNewValueFromMidi(timbre, ROW_ENV1_TIME, ENCODER_ENV_A,
                    (float)midiEvent.value[1] * .01562500000000000000f);
            break;
        case CC_ENV_ATK_OP2:
        case CC_ENV_ATK_OP3:
        case CC_ENV_ATK_OP4:
        case CC_ENV_ATK_OP5:
        case CC_ENV_ATK_OP6:
            this->synth->setNewValueFromMidi(timbre, ROW_ENV2_TIME + (midiEvent.value[0] - CC_ENV_ATK_OP2)* 2, ENCODER_ENV_A,
            		(float) midiEvent.value[1] * .01562500000000000000f);
            break;
        case CC_ENV_ATK_ALL:
        case CC_ENV_ATK_ALL_MODULATOR: {
            int currentAlgo = (int) this->synthState_->params->engine1.algo;
            int type = midiEvent.value[0] == CC_ENV_ATK_ALL ? OPERATOR_CARRIER : OPERATOR_MODULATOR;
            for (int e = 0; e < NUMBER_OF_OPERATORS; e++) {
                if (algoOpInformation[currentAlgo][e] == type) {
                    this->synth->setNewValueFromMidi(timbre, ROW_ENV1_TIME + e * 2, ENCODER_ENV_A,
                            (float) midiEvent.value[1] * .01562500000000000000f);
                }
            }
            break;
        }
        case CC_ENV_REL_OP1:
        case CC_ENV_REL_OP2:
        case CC_ENV_REL_OP3:
        case CC_ENV_REL_OP4:
        case CC_ENV_REL_OP5:
        case CC_ENV_REL_OP6:
            this->synth->setNewValueFromMidi(timbre, ROW_ENV1_LEVEL + (midiEvent.value[0] - CC_ENV_REL_OP1) * 2, ENCODER_ENV_R,
                    (float) midiEvent.value[1] * .03125000000000000000f);
            break;
        case CC_ENV_REL_ALL_MODULATOR:
        case CC_ENV_REL_ALL: {
            int currentAlgo = (int) this->synthState_->params->engine1.algo;
            int type = midiEvent.value[0] == CC_ENV_REL_ALL ? OPERATOR_CARRIER : OPERATOR_MODULATOR;
            for (int e = 0; e < NUMBER_OF_OPERATORS; e++) {
                if (algoOpInformation[currentAlgo][e] == type) {
                    this->synth->setNewValueFromMidi(timbre, ROW_ENV1_LEVEL + e * 2, ENCODER_ENV_R,
                            (float) midiEvent.value[1] * .03125000000000000000f);
                }
            }
            break;
        }
        case CC_LFO1_PHASE:
        case CC_LFO2_PHASE:
        case CC_LFO3_PHASE:
            this->synth->setNewValueFromMidi(timbre, ROW_LFOPHASES, ENCODER_LFO_PHASE1 + midiEvent.value[0] - CC_LFO1_PHASE,
                    (float) midiEvent.value[1] * .01f);
            break;
        case CC_LFO1_BIAS:
        case CC_LFO2_BIAS:
        case CC_LFO3_BIAS:
            this->synth->setNewValueFromMidi(timbre, ROW_LFOOSC1 + midiEvent.value[0] - CC_LFO1_BIAS, ENCODER_LFO_BIAS,
                    (float) midiEvent.value[1] * .01f);
            break;
        case CC_LFO1_SHAPE:
        case CC_LFO2_SHAPE:
        case CC_LFO3_SHAPE:
            this->synth->setNewValueFromMidi(timbre, ROW_LFOOSC1 + midiEvent.value[0] - CC_LFO1_SHAPE, ENCODER_LFO_SHAPE,
                    (float) midiEvent.value[1]);
            break;
        case CC_ARP_CLOCK:
            this->synth->setNewValueFromMidi(timbre, ROW_ARPEGGIATOR1, ENCODER_ARPEGGIATOR_CLOCK, (float) midiEvent.value[1]);
            break;
        case CC_ARP_DIRECTION:
            this->synth->setNewValueFromMidi(timbre, ROW_ARPEGGIATOR1, ENCODER_ARPEGGIATOR_DIRECTION, (float) midiEvent.value[1]);
            break;
        case CC_ARP_OCTAVE:
            this->synth->setNewValueFromMidi(timbre, ROW_ARPEGGIATOR1, ENCODER_ARPEGGIATOR_OCTAVE, (float) midiEvent.value[1]);
            break;
        case CC_ARP_PATTERN:
            this->synth->setNewValueFromMidi(timbre, ROW_ARPEGGIATOR2, ENCODER_ARPEGGIATOR_PATTERN, (float) midiEvent.value[1]);
            break;
        case CC_ARP_DIVISION:
            this->synth->setNewValueFromMidi(timbre, ROW_ARPEGGIATOR2, ENCODER_ARPEGGIATOR_DIVISION, (float) midiEvent.value[1]);
            break;
        case CC_ARP_DURATION:
            this->synth->setNewValueFromMidi(timbre, ROW_ARPEGGIATOR2, ENCODER_ARPEGGIATOR_DURATION, (float) midiEvent.value[1]);
            break;
        case CC_MIXER_VOLUME:
            this->synth->setNewMixerValueFromMidi(timbre, MIXER_VALUE_VOLUME, (float) midiEvent.value[1] * INV127);
            break;
        case CC_MIXER_PAN:
            this->synth->setNewMixerValueFromMidi(timbre, MIXER_VALUE_PAN, (float) midiEvent.value[1] - 63);
            break;
        case CC_MIXER_SEND:
            this->synth->setNewMixerValueFromMidi(timbre, MIXER_VALUE_SEND, (float) midiEvent.value[1] * INV127);
            break;
        case CC_MPE_SLIDE_CC74:
        	// No CC74 on global midi channel
        	if (!this->synthState_->mixerState.MPE_inst1_ > 0 || timbre != 0) {
        		this->synth->getTimbre(timbre)->setMatrixSource(MATRIX_SOURCE_MPESLIDE, INV127 * midiEvent.value[1]);
        	}
            break;
        case CC_UNISON_DETUNE:
            this->synth->setNewValueFromMidi(timbre, ROW_ENGINE2, ENCODER_ENGINE2_UNISON_DETUNE,
                    ((float)midiEvent.value[1]) * INV64 - 1.0f);
            break;
        case CC_UNISON_SPREAD:
            this->synth->setNewValueFromMidi(timbre, ROW_ENGINE2, ENCODER_ENGINE2_UNISON_SPREAD,
                    (float)midiEvent.value[1] * INV127);
        case CC_SEQ_START_ALL:
            this->synth->setNewSeqValueFromMidi(timbre, SEQ_VALUE_PLAY_ALL, midiEvent.value[1]);
            break;
        case CC_SEQ_START_INST:
            this->synth->setNewSeqValueFromMidi(timbre, SEQ_VALUE_PLAY_INST, midiEvent.value[1]);
            break;
        case CC_SEQ_RECORD_INST:
            this->synth->setNewSeqValueFromMidi(timbre, SEQ_VALUE_RECORD_INST, midiEvent.value[1]);
            break;
        case CC_SEQ_TRANSPOSE:
            this->synth->setNewSeqValueFromMidi(timbre, SEQ_VALUE_TRANSPOSE, midiEvent.value[1]);
            break;
        case CC_SEQ_SET_SEQUENCE:
            this->synth->setNewSeqValueFromMidi(timbre, SEQ_VALUE_SEQUENCE_NUMBER, midiEvent.value[1]);
            break;

        }
    }

    // Do we accept NRPN in the menu
    if (receives & 0x02) {
        switch (midiEvent.value[0]) {
        case 99:
            this->currentNrpn[timbre].paramMSB = midiEvent.value[1];
            break;
        case 98:
            this->currentNrpn[timbre].paramLSB = midiEvent.value[1];
            break;
        case 6:
            this->currentNrpn[timbre].valueMSB = midiEvent.value[1];
            break;
        case 38:
            this->currentNrpn[timbre].valueLSB = midiEvent.value[1];
            this->currentNrpn[timbre].readyToSend = true;
            break;
        case 96:
            // nrpn increment
            if (this->currentNrpn[timbre].valueLSB == 127) {
                this->currentNrpn[timbre].valueLSB = 0;
                this->currentNrpn[timbre].valueMSB++;
            } else {
                this->currentNrpn[timbre].valueLSB++;
            }
            this->currentNrpn[timbre].readyToSend = true;
            break;
        case 97:
            // nrpn decremenet
            if (this->currentNrpn[timbre].valueLSB == 0) {
                this->currentNrpn[timbre].valueLSB = 127;
                this->currentNrpn[timbre].valueMSB--;
            } else {
                this->currentNrpn[timbre].valueLSB--;
            }
            this->currentNrpn[timbre].readyToSend = true;
            break;
        default:
            break;
        }

        if (this->currentNrpn[timbre].readyToSend) {
            decodeNrpn(timbre);
            this->currentNrpn[timbre].readyToSend = false;
        }
    }
}

void MidiDecoder::sendCurrentPatchAsNrpns(int timbre) {
    struct MidiEvent cc;

    OneSynthParams * paramsToSend = this->synth->getTimbre(timbre)->getParamRaw();

    cc.eventType = MIDI_CONTROL_CHANGE;
    // Si channel = ALL envoie sur 1
    int channel = this->synthState_->mixerState.instrumentState_[timbre].midiChannel - 1;
    if (channel == -1) {
        channel = 0;
    }
    cc.channel = channel;

    // Send the title
    for (int k = 0; k < 12; k++) {
        int valueToSend = paramsToSend->presetName[k];
        cc.value[0] = 99;
        cc.value[1] = 1;
        writeMidiCCOut(&cc);
        cc.value[0] = 98;
        cc.value[1] = 100 + k;
        writeMidiCCOut(&cc);
        cc.value[0] = 6;
        cc.value[1] = (unsigned char) (valueToSend >> 7);
        writeMidiCCOut(&cc);
        cc.value[0] = 38;
        cc.value[1] = (unsigned char) (valueToSend & 0x7f);
        writeMidiCCOut(&cc);

        sendMidiDin5Out();
        sendMidiUsbOutIfBufferFull();
        // Wait for midi (USART) to be all sent
        while (usartBufferOut.getCount() > 0) {
        }
    }

    // flush all parameters
    for (int currentrow = 0; currentrow < NUMBER_OF_ROWS; currentrow++) {
        for (int encoder = 0; encoder < NUMBER_OF_ENCODERS_PFM2; encoder++) {
            struct ParameterDisplay* param = &allParameterRows.row[currentrow]->params[encoder];
            float floatValue = ((float*) paramsToSend)[currentrow * NUMBER_OF_ENCODERS_PFM2 + encoder];

            int valueToSend;

            if (param->displayType == DISPLAY_TYPE_FLOAT || param->displayType == DISPLAY_TYPE_FLOAT_OSC_FREQUENCY
                    || param->displayType == DISPLAY_TYPE_FLOAT_LFO_FREQUENCY || param->displayType == DISPLAY_TYPE_LFO_KSYN) {
                valueToSend = (floatValue - param->minValue) * 100.0f + .1f;
            } else {
                valueToSend = floatValue + .1f;
            }
            // MSB / LSB
            int paramNumber = getNrpnRowFromParamRow(currentrow) * NUMBER_OF_ENCODERS_PFM2 + encoder;
            // Value to send must be positive

            // NRPN is 4 control change
            cc.value[0] = 99;
            cc.value[1] = (unsigned char) (paramNumber >> 7);
            writeMidiCCOut(&cc);
            cc.value[0] = 98;
            cc.value[1] = (unsigned char) (paramNumber & 0x7F);
            writeMidiCCOut(&cc);
            cc.value[0] = 6;
            cc.value[1] = (unsigned char) (valueToSend >> 7);
            writeMidiCCOut(&cc);
            cc.value[0] = 38;
            cc.value[1] = (unsigned char) (valueToSend & 0x7F);
            writeMidiCCOut(&cc);

            sendMidiDin5Out();
            sendMidiUsbOutIfBufferFull();
            // Wait for midi (USART) to be all sent
            while (usartBufferOut.getCount() > 0) {
            }
        }
    }

    // Step Seq
    for (int whichStepSeq = 0; whichStepSeq < 2; whichStepSeq++) {
        for (int step = 0; step < 16; step++) {
            cc.value[0] = 99;
            cc.value[1] = whichStepSeq + 2;
            writeMidiCCOut(&cc);
            cc.value[0] = 98;
            cc.value[1] = step;
            writeMidiCCOut(&cc);
            cc.value[0] = 6;
            cc.value[1] = 0;
            writeMidiCCOut(&cc);
            cc.value[0] = 38;
            StepSequencerSteps * seqSteps = &((StepSequencerSteps *) (&paramsToSend->lfoSteps1))[whichStepSeq];
            cc.value[1] = seqSteps->steps[step];
            writeMidiCCOut(&cc);

            sendMidiDin5Out();
            sendMidiUsbOutIfBufferFull();
            // Wait for midi (USART) to be all sent
            while (usartBufferOut.getCount() > 0) {
            }
        }
    }

    // send Usb Anyway
    sendMidiUsbOut();
}

void MidiDecoder::decodeNrpn(int timbre) {
    if (this->currentNrpn[timbre].paramMSB < 2) {
        unsigned int index = (this->currentNrpn[timbre].paramMSB << 7) + this->currentNrpn[timbre].paramLSB;
        float value = (this->currentNrpn[timbre].valueMSB << 7) + this->currentNrpn[timbre].valueLSB;
        unsigned int row = getParamRowFromNrpnRow(index / NUMBER_OF_ENCODERS_PFM2);
        unsigned int encoder = index % NUMBER_OF_ENCODERS_PFM2;

        struct ParameterDisplay* param = &(allParameterRows.row[row]->params[encoder]);

        if (row < NUMBER_OF_ROWS) {
            if (param->displayType == DISPLAY_TYPE_FLOAT || param->displayType == DISPLAY_TYPE_FLOAT_OSC_FREQUENCY
                    || param->displayType == DISPLAY_TYPE_FLOAT_LFO_FREQUENCY || param->displayType == DISPLAY_TYPE_LFO_KSYN) {
                value = value * .01f + param->minValue;
            }

            this->synth->setNewValueFromMidi(timbre, row, encoder, value);
        } else if (index >= 228 && index < 240) {
            this->synth->setNewSymbolInPresetName(timbre, index - 228, (char)value);
            if (index == 239) {
                this->synthState_->propagateNewPresetName(timbre);
            }
        }
    } else if (this->currentNrpn[timbre].paramMSB < 4) {
        unsigned int whichStepSeq = this->currentNrpn[timbre].paramMSB - 2;
        unsigned int step = this->currentNrpn[timbre].paramLSB;
        unsigned int value = this->currentNrpn[timbre].valueLSB;

        this->synth->setNewStepValueFromMidi(timbre, whichStepSeq, step, value);
    } else if (this->currentNrpn[timbre].paramMSB == 127 && this->currentNrpn[timbre].paramLSB == 127) {
        AsyncAction asyncAction;
        asyncAction.fullBytes = 0l;
        asyncAction.action.actionType = SEND_PATCH_AS_NRPN;
        asyncAction.action.timbre = timbre;
        asyncActions.insert(asyncAction);
    }
}

void MidiDecoder::newParamValueFromExternal(int timbre, int currentrow, int encoder, ParameterDisplay* param, float oldValue,
        float newValue) {
    // Nothing to do
}

void MidiDecoder::newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue) {
    // Nothing to do
}

void MidiDecoder::newParamValue(int timbre, int currentrow, int encoder, ParameterDisplay* param, float oldValue, float newValue) {
    int sendCCOrNRPN = this->synthState_->fullState.midiConfigValue[MIDICONFIG_SENDS];
    int channel = this->synthState_->mixerState.instrumentState_[timbre].midiChannel -1;

    struct MidiEvent cc;
    cc.eventType = MIDI_CONTROL_CHANGE;
    // Si channel = ALL envoie sur 1
    if (channel == -1) {
        channel = 0;
    }
    cc.channel = channel;

    // Do we send NRPN ?
    if (sendCCOrNRPN == 2) {
        // Special case for Step sequencer
        if ((currentrow == ROW_LFOSEQ1 || currentrow == ROW_LFOSEQ2) && encoder >= 2) {
            if (encoder == 2) {
                return;
            }
            if (encoder == 3) {
                // We modify a step value, we want to send that
                int currentStepSeq = currentrow - ROW_LFOSEQ1;
                // NRPN is 4 control change
                cc.value[0] = 99;
                cc.value[1] = currentStepSeq + 2;
                writeMidiCCOut(&cc);
                cc.value[0] = 98;
                cc.value[1] = this->synthState_->stepSelect[currentStepSeq];
                writeMidiCCOut(&cc);
                cc.value[0] = 6;
                cc.value[1] = 0;
                writeMidiCCOut(&cc);
                cc.value[0] = 38;
                cc.value[1] = (unsigned char) newValue;
                writeMidiCCOut(&cc);

                sendMidiDin5Out();
                sendMidiUsbOut();
            }
        } else {
            int valueToSend;

            if (param->displayType == DISPLAY_TYPE_FLOAT || param->displayType == DISPLAY_TYPE_FLOAT_OSC_FREQUENCY
                    || param->displayType == DISPLAY_TYPE_FLOAT_LFO_FREQUENCY || param->displayType == DISPLAY_TYPE_LFO_KSYN) {
                valueToSend = (newValue - param->minValue) * 100.0f + .1f;
            } else {
                valueToSend = newValue + .1f;
            }
            // MSB / LSB
            int paramNumber = getNrpnRowFromParamRow(currentrow) * NUMBER_OF_ENCODERS_PFM2 + encoder;
            // Value to send must be positive

            // NRPN is 4 control change
            cc.value[0] = 99;
            cc.value[1] = (unsigned char) (paramNumber >> 7);
            writeMidiCCOut(&cc);
            cc.value[0] = 98;
            cc.value[1] = (unsigned char) (paramNumber & 0x7F);
            writeMidiCCOut(&cc);
            cc.value[0] = 6;
            cc.value[1] = (unsigned char) (valueToSend >> 7);
            writeMidiCCOut(&cc);
            cc.value[0] = 38;
            cc.value[1] = (unsigned char) (valueToSend & 0x7F);
            writeMidiCCOut(&cc);

            sendMidiDin5Out();
            sendMidiUsbOut();
        }
    }

    // Do we send control change
    if (sendCCOrNRPN == 1) {
        cc.value[0] = 0;

        switch (currentrow) {
        case ROW_ENGINE:
            if (encoder == ENCODER_ENGINE_ALGO) {
                cc.value[1] = newValue;
                cc.value[0] = CC_ALGO;
            }
            break;
        case ROW_MODULATION1:
            if ((encoder & 0x1) == 0) {
                cc.value[1] = newValue * 10 + .001f;
                cc.value[0] = CC_IM1 + (encoder >> 1);
            }
            break;
        case ROW_MODULATION2:
            if ((encoder & 0x1) == 0) {
                cc.value[1] = newValue * 10 + .001f;
                cc.value[0] = CC_IM3 + encoder;
            }
            break;
        case ROW_MODULATION3:
            if ((encoder & 0x1) == 0) {
                cc.value[1] = newValue * 10 + .001f;
                cc.value[0] = CC_IM5 + encoder;
            }
            break;
        case ROW_OSC_MIX1:
            cc.value[0] = CC_MIX1 + encoder;
            if ((encoder & 0x1) == 0) {
                // mix
                cc.value[1] = newValue * 100.0f + .1f;
            } else {
                cc.value[1] = (newValue + 1.0f) * 50.0f + .1f;
            }
            break;
        case ROW_OSC_MIX2:
            cc.value[0] = CC_MIX3 + encoder;
            if ((encoder & 0x1) == 0) {
                // mix
                cc.value[1] = newValue * 100.0f + .1f;
            } else {
                cc.value[1] = (newValue + 1.0f) * 50.0f + .1f;
            }
            break;
        case ROW_OSC_FIRST ... ROW_OSC_LAST:
            if (encoder == ENCODER_OSC_FREQ) {
                cc.value[0] = CC_OSC1_FREQ + (currentrow - ROW_OSC_FIRST);
                cc.value[1] = newValue * 12.0f + .1f;
            }
            break;
        case ROW_MATRIX_FIRST ... ROW_MATRIX4:
            if (encoder == ENCODER_MATRIX_MUL) {
                cc.value[0] = CC_MATRIXROW1_MUL + currentrow - ROW_MATRIX_FIRST;
                cc.value[1] = newValue * 5.0f + 50.1f;
            }
            break;
        case ROW_LFO_FIRST ... ROW_LFOOSC3:
            switch (encoder) {
            case ENCODER_LFO_SHAPE:
                cc.value[0] = CC_LFO1_SHAPE + (currentrow - ROW_LFO_FIRST);
                cc.value[1] = newValue + .1f;
                break;
            case ENCODER_LFO_FREQ:
                cc.value[0] = CC_LFO1_FREQ + (currentrow - ROW_LFO_FIRST);
                cc.value[1] = newValue * 5.0f + .1f;
                break;
            case ENCODER_LFO_BIAS:
                cc.value[0] = CC_LFO1_BIAS + (currentrow - ROW_LFO_FIRST);
                cc.value[1] = newValue * 100.0f + .1f;
                break;
            }
            break;
        case ROW_LFOENV2:
            if (encoder == ENCODER_LFO_ENV2_SILENCE) {
                cc.value[0] = CC_LFO_ENV2_SILENCE;
                cc.value[1] = newValue * 8.0f + .1f;
            }
            break;
        case ROW_LFOSEQ1 ... ROW_LFOSEQ2:
            if (encoder == ENCODER_STEPSEQ_GATE) {
                cc.value[0] = CC_STEPSEQ5_GATE + (currentrow - ROW_LFOSEQ1);
                cc.value[1] = newValue * 100.0f + .1f;
            }
            break;
        case ROW_EFFECT:
            if (encoder == ENCODER_EFFECT_TYPE) {
                cc.value[0] = CC_FILTER_TYPE;
                cc.value[1] = newValue + .1f;
            } else {
                cc.value[0] = CC_FILTER_PARAM1 + encoder - 1;
                if (encoder == ENCODER_EFFECT_PARAM3) {
                    cc.value[1] = newValue * 100.0f + .1f;
                } else {
                    cc.value[1] = newValue * 128.0f + .1f;
                }
            }
            break;
        case ROW_ENV1_TIME:
            if (encoder == ENCODER_ENV_A) {
                cc.value[0] = CC_ENV_ATK_OP1;
                cc.value[1] = newValue * 64.0f + .1f;
            }
            break;
        case ROW_ENV2_TIME:
        case ROW_ENV3_TIME:
        case ROW_ENV4_TIME:
        case ROW_ENV5_TIME:
        case ROW_ENV6_TIME:
            if (encoder == ENCODER_ENV_A) {
                cc.value[0] = CC_ENV_ATK_OP2 + ((currentrow - ROW_ENV2_TIME) >> 1);
                cc.value[1] = newValue * 64.0f + .1f;
            }
            break;
        case ROW_ENV1_LEVEL:
        case ROW_ENV2_LEVEL:
        case ROW_ENV3_LEVEL:
        case ROW_ENV4_LEVEL:
        case ROW_ENV5_LEVEL:
        case ROW_ENV6_LEVEL:
            if (encoder == ENCODER_ENV_R) {
                cc.value[0] = CC_ENV_REL_OP1 + ((currentrow - ROW_ENV1_LEVEL) >> 1);
                cc.value[1] = newValue * 32.0f + .1f;
            }
            break;
        case ROW_LFOPHASES:
            cc.value[0] = CC_LFO1_PHASE + encoder;
            cc.value[1] = newValue * 100.0f + .1f;
            break;
        case ROW_ARPEGGIATOR1:
            switch (encoder) {
            case ENCODER_ARPEGGIATOR_CLOCK:
                cc.value[0] = CC_ARP_CLOCK;
                cc.value[1] = newValue + .1f;
                break;
            case ENCODER_ARPEGGIATOR_DIRECTION:
                cc.value[0] = CC_ARP_DIRECTION;
                cc.value[1] = newValue + .1f;
                break;
            case ENCODER_ARPEGGIATOR_OCTAVE:
                cc.value[0] = CC_ARP_OCTAVE;
                cc.value[1] = newValue + .1f;
                break;
            }
            break;
        case ROW_ARPEGGIATOR2:
            switch (encoder) {
            case ENCODER_ARPEGGIATOR_PATTERN:
                cc.value[0] = CC_ARP_PATTERN;
                cc.value[1] = newValue + .1f;
                break;
            case ENCODER_ARPEGGIATOR_DIVISION:
                cc.value[0] = CC_ARP_DIVISION;
                cc.value[1] = newValue + .1f;
                break;
            case ENCODER_ARPEGGIATOR_DURATION:
                cc.value[0] = CC_ARP_DURATION;
                cc.value[1] = newValue + .1f;
                break;
            }
        }

        if (cc.value[0] != 0) {
            // CC limited to 127
            if (cc.value[1] > 127) {
                cc.value[1] = 127;
            }

            if (lastSentCC.value[1] != cc.value[1] || lastSentCC.value[0] != cc.value[0] || lastSentCC.channel != cc.channel) {
                writeMidiCCOut(&cc);
                sendMidiDin5Out();
                sendMidiUsbOut();
                lastSentCC = cc;
            }
        }
    }
}

/** Only send control change */
void MidiDecoder::writeMidiCCOut(struct MidiEvent *toSend) {

    // We don't send midi to both USB and USART at the same time...
    if (this->synthState_->fullState.midiConfigValue[MIDICONFIG_USB] == USBMIDI_IN_AND_OUT) {
        // usbBuf[0] = [number of cable on 4 bits] [event type on 4 bites]
        *usbMidiOutBuffWrt++ = 0x00 | (toSend->eventType >> 4);
        *usbMidiOutBuffWrt++ = toSend->eventType + toSend->channel;
        *usbMidiOutBuffWrt++ = toSend->value[0];
        *usbMidiOutBuffWrt++ = toSend->value[1];

    }

    usartBufferOut.insert(toSend->eventType + toSend->channel);
    usartBufferOut.insert(toSend->value[0]);
    usartBufferOut.insert(toSend->value[1]);
}

void MidiDecoder::sendMidiDin5Out() {
    // Enable interupt to send Midi buffer :
    SET_BIT(huart1.Instance->CR1, USART_CR1_TXEIE_TXFNFIE);
}


void MidiDecoder::sendMidiUsbOutIfBufferFull() {
	if ((usbMidiOutBuffWrt - usbMidiOutBuff) == 64) {
		sendMidiUsbOut();
	}
}


void MidiDecoder::sendMidiUsbOut() {
    if (usbMidiOutBuffWrt != usbMidiOutBuff && this->synthState_->fullState.midiConfigValue[MIDICONFIG_USB] == USBMIDI_IN_AND_OUT) {

        USBD_LL_Transmit(&hUsbDeviceFS, MIDI_IN_EP, usbMidiOutBuff, usbMidiOutBuffWrt - usbMidiOutBuff);
        // go back to begining of buffer
        usbMidiOutBuffWrt = usbMidiOutBuff;
    }
}


int MidiDecoder::getNrpnRowFromParamRow(int paramRow) {
    switch (paramRow) {
    case ROW_LFOPHASES:
        // Move after ROW_LFOSEQ2
        return paramRow + 4;
    case ROW_LFOENV1:
    case ROW_LFOENV2:
    case ROW_LFOSEQ1:
    case ROW_LFOSEQ2:
        // Move before ROW_LFOPHASE
        return paramRow - 1;
    default:
        return paramRow;
    }
}

int MidiDecoder::getParamRowFromNrpnRow(int nrpmRow) {
    // Move back row
    switch (nrpmRow) {
    case ROW_LFOPHASES + 4:
        return ROW_LFOPHASES;
    case ROW_LFOENV1 - 1:
    case ROW_LFOENV2 - 1:
    case ROW_LFOSEQ1 - 1:
    case ROW_LFOSEQ2 - 1:
        return nrpmRow + 1;
    default:
        return nrpmRow;
    }
    return nrpmRow;
}

uint8_t MidiDecoder::analyseSysexBuffer(uint8_t *sysexBuffer, uint16_t size) {
    if (sysexBuffer[0] == 0x7d) {
        switch (sysexBuffer[1]) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            this->synth->setNewMixerValueFromMidi(sysexBuffer[1] - 1, MIXER_VALUE_VOLUME, (float) sysexBuffer[2] * INV127);
            return 1 ;
        }
    }
    return 0;
}


/**
 * Here we process the actions that cannot be executed inside the main midi loop
 */
void MidiDecoder::processAsyncActions() {
    while (asyncActions.getCount() > 0) {
        AsyncAction asyncAction = asyncActions.remove();
        switch (asyncAction.action.actionType) {
            case LOAD_PRESET:
                for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
                    if (((1 << t) & asyncAction.action.timbre)  > 0) {
                        synth->loadPreenFMPatchFromMidi(t, asyncAction.action.param1, asyncAction.action.param2, asyncAction.action.param3);
                    }
                }
                break;
            case SEND_PATCH_AS_NRPN:
                sendCurrentPatchAsNrpns(asyncAction.action.timbre);
                break;
        }
    }
}

