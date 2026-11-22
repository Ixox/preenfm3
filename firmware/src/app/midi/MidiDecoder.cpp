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

#include "MidiData.h"
#include "MidiDecoder.h"
#include "RingBuffer.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

// Let's call it MidiOut despite USB spec
uint8_t usbMidiOutBuff[64];
uint8_t* usbMidiOutBuffWrt;

extern UART_HandleTypeDef huart1;

#define INV127 .00787401574803149606f
#define INV64  .015625f

// NRPN v2 command domain and error response (transport framing only).
#define NRPN_MSB_CONTROL    127
#define NRPN_RESPONSE_ERROR 66

static inline float ccToPercentFloat(unsigned char ccValue) {
    int percent = ((int)ccValue * 100 + 63) / 127;
    return (float)percent * 0.01f;
}

static inline float ccToDynamicsIndex(unsigned char ccValue) {
    return (ccValue < 7) ? (float)ccValue : (float)(((int)ccValue * 7) / 128);
}

static inline uint16_t percentFloatToNrpn(float value) {
    int percent = (int)(value * 100.0f + 0.5f);
    return (percent < 0) ? 0 : (uint16_t)percent;
}

RingBuffer<uint8_t, 64> usartBufferIn;
RingBuffer<uint8_t, 64> usartBufferOut;
RingBuffer<AsyncAction, 16> asyncActions;

// Let's have sysexBuffer in regular RAM.
#define SYSEX_BUFFER_SIZE 32
uint8_t sysexBuffer[SYSEX_BUFFER_SIZE];

MidiDecoder::MidiDecoder() {
    currentEventState_.eventState = MIDI_EVENT_WAITING;
    currentEventState_.index = 0;
    this->isExternalMidiClockStarted_ = false;
    this->midiClockCpt_ = 0;
    this->runningStatus_ = 0;
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        omniOn_[t] = false;
        bankNumber_[t] = 0;
        bankNumberLSB_[t] = 0;
    }

    for (int k = 0; k < 64; k++) {
        usbMidiOutBuff[k] = 0;
    }

    usbMidiOutBuffWrt = usbMidiOutBuff;
}

MidiDecoder::~MidiDecoder() {
}

void MidiDecoder::setSynth(Synth* synth) {
    this->synth_ = synth;
}

void MidiDecoder::setVisualInfo(VisualInfo* visualInfo) {
    this->visualInfo_ = visualInfo;
}

void MidiDecoder::newByte(unsigned char byte) {
    // Realtime first !
    if (byte >= 0xF8) {
        switch (byte) {
        case MIDI_CLOCK:
            this->midiClockCpt_++;
            this->synth_->midiTick(true);
            if (unlikely(this->midiClockCpt_ == 6)) {
                if (this->isExternalMidiClockStarted_) {
                    this->songPosition_++;
                }
                this->midiClockCpt_ = 0;
                this->synth_->midiClockSongPositionStep(this->songPosition_);

                if (unlikely((this->songPosition_ & 0x3) == 0)) {
                    this->visualInfo_->midiClock(true);
                }
                if (unlikely((this->songPosition_ & 0x3) == 0x2)) {
                    this->visualInfo_->midiClock(false);
                }
            }
            break;
        case MIDI_START:
            if (!this->isExternalMidiClockStarted_) {
                this->isExternalMidiClockStarted_ = true;
                this->songPosition_ = 0;
                this->midiClockCpt_ = 0;
                this->synth_->midiClockStart(true);
            }
            break;
        case MIDI_CONTINUE:
            if (!this->isExternalMidiClockStarted_) {
                this->isExternalMidiClockStarted_ = true;
                this->midiClockCpt_ = 0;
                this->synth_->midiClockContinue(this->songPosition_, true);
            }
            break;
        case MIDI_STOP:
            if (this->isExternalMidiClockStarted_) {
                this->isExternalMidiClockStarted_ = false;
                this->synth_->midiClockStop(true);
                this->visualInfo_->midiClock(false);
            }
            break;
        }
    } else {
        switch (currentEventState_.eventState) {
        case MIDI_EVENT_WAITING:
            if (unlikely(byte >= 0x80)) {
                // Running status is cleared later in newMEssageType() if byte >= 0xF0
                this->runningStatus_ = byte;
                newMessageType(byte);
            } else {
                // midi source use running status...
                if (this->runningStatus_ > 0) {
                    newMessageType(this->runningStatus_);
                    newMessageData(byte);
                }
            }
            break;
        case MIDI_EVENT_IN_PROGRESS:
            newMessageData(byte);
            break;
        case MIDI_EVENT_SYSEX:
            if (likely(currentEventState_.index < SYSEX_BUFFER_SIZE)) {
                sysexBuffer[currentEventState_.index++] = byte;
            }
            if (byte == MIDI_SYSEX_END) {
                if (currentEventState_.index < SYSEX_BUFFER_SIZE) {
                    // End of sysex =>Analyse Sysex
                    if (this->analyseSysexBuffer(sysexBuffer, currentEventState_.index) == 1) {
                        currentEventState_.eventState = MIDI_EVENT_COMPLETE;
                    }
                }
                currentEventState_.eventState = MIDI_EVENT_WAITING;
                currentEventState_.index = 0;
            }
            break;
        }
    }
}

void MidiDecoder::newMessageData(unsigned char byte) {
    currentEvent_.value[currentEventState_.index++] = byte;
    if (currentEventState_.index == currentEventState_.numberOfBytes) {
        midiEventReceived(currentEvent_);
        currentEventState_.eventState = MIDI_EVENT_WAITING;
        currentEventState_.index = 0;
    }
}

void MidiDecoder::newMessageType(unsigned char byte) {
    currentEvent_.eventType = (EventType)(byte & 0xf0);
    currentEvent_.channel = byte & 0x0f;

    switch (currentEvent_.eventType) {
    case MIDI_NOTE_OFF:
    case MIDI_NOTE_ON:
    case MIDI_CONTROL_CHANGE:
    case MIDI_PITCH_BEND:
    case MIDI_POLY_AFTER_TOUCH:
        currentEventState_.numberOfBytes = 2;
        currentEventState_.eventState = MIDI_EVENT_IN_PROGRESS;
        break;
    case MIDI_AFTER_TOUCH:
    case MIDI_PROGRAM_CHANGE:
        currentEventState_.numberOfBytes = 1;
        currentEventState_.eventState = MIDI_EVENT_IN_PROGRESS;
        break;
    case MIDI_SYSTEM_COMMON:
        this->runningStatus_ = 0;
        switch (byte) {
        case MIDI_SYSEX:
            currentEventState_.eventState = MIDI_EVENT_SYSEX;
            currentEventState_.index = 0;
            break;
        case MIDI_SONG_POSITION:
            currentEvent_.eventType = MIDI_SONG_POSITION;
            // Channel hack to make it accepted
            if (this->synthState_->mixerState_.instrumentState_[0].midiChannel == 0) {
                currentEvent_.channel = 0;
            } else {
                currentEvent_.channel = this->synthState_->mixerState_.instrumentState_[0].midiChannel - 1;
            }
            currentEventState_.numberOfBytes = 2;
            currentEventState_.eventState = MIDI_EVENT_IN_PROGRESS;
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
    bool isInst1MPE = this->synthState_->mixerState_.MPE_inst1_ > 0;

    if (unlikely(isInst1MPE)) {
        // Special MPE Case
        midiEventForInstrument1MPE(midiEvent);
    }

    if (unlikely(midiEvent.channel == (this->synthState_->mixerState_.globalChannel_ - 1))) {
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
    } else if (unlikely(midiEvent.channel == (this->synthState_->mixerState_.currentChannel_ - 1))) {
        // Midi current
        timbres[timbreIndex++] = currentTimbre_;
    } else {
        if (omniOn_[0] || this->synthState_->mixerState_.instrumentState_[0].midiChannel == 0 ||
            (this->synthState_->mixerState_.instrumentState_[0].midiChannel - 1) == midiEvent.channel) {
            if (likely(!isInst1MPE)) {
                timbres[timbreIndex++] = 0;
            }
        }
        if (omniOn_[1] || this->synthState_->mixerState_.instrumentState_[1].midiChannel == 0 ||
            (this->synthState_->mixerState_.instrumentState_[1].midiChannel - 1) == midiEvent.channel) {
            timbres[timbreIndex++] = 1;
        }
        if (omniOn_[2] || this->synthState_->mixerState_.instrumentState_[2].midiChannel == 0 ||
            (this->synthState_->mixerState_.instrumentState_[2].midiChannel - 1) == midiEvent.channel) {
            timbres[timbreIndex++] = 2;
        }
        if (omniOn_[3] || this->synthState_->mixerState_.instrumentState_[3].midiChannel == 0 ||
            (this->synthState_->mixerState_.instrumentState_[3].midiChannel - 1) == midiEvent.channel) {
            timbres[timbreIndex++] = 3;
        }
        if (omniOn_[4] || this->synthState_->mixerState_.instrumentState_[4].midiChannel == 0 ||
            (this->synthState_->mixerState_.instrumentState_[4].midiChannel - 1) == midiEvent.channel) {
            timbres[timbreIndex++] = 4;
        }
        if (omniOn_[5] || this->synthState_->mixerState_.instrumentState_[5].midiChannel == 0 ||
            (this->synthState_->mixerState_.instrumentState_[5].midiChannel - 1) == midiEvent.channel) {
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
            char note = midiEvent.value[0] + this->synthState_->mixerState_.instrumentState_[timbre].shiftNote;
            if (likely(midiEvent.value[0] >= this->synthState_->mixerState_.instrumentState_[timbre].firstNote &&
                       midiEvent.value[0] <= this->synthState_->mixerState_.instrumentState_[timbre].lastNote) &&
                note >= 0 && note <= 127) {
                this->synth_->noteOff(timbre, note);
            }
        }
        break;
    case MIDI_NOTE_ON:
        // Some keyboards send note-off this way
        for (int tk = 0; tk < timbreIndex; tk++) {
            int timbre = timbres[tk];
            char note = midiEvent.value[0] + this->synthState_->mixerState_.instrumentState_[timbre].shiftNote;
            if (likely(midiEvent.value[0] >= this->synthState_->mixerState_.instrumentState_[timbre].firstNote &&
                       midiEvent.value[0] <= this->synthState_->mixerState_.instrumentState_[timbre].lastNote && note >= 0 &&
                       note <= 127)) {
                if (midiEvent.value[1] == 0) {
                    this->synth_->noteOff(timbre, note);
                } else {
                    this->synth_->noteOn(timbre, note, midiEvent.value[1]);
                    visualInfo_->noteOn(timbre, true);
                }
            }
        }
        break;
    case MIDI_CONTROL_CHANGE: {
        // midiEventForInstrument1MPE already handles channel 0 when MPE is on.
        const bool mpeMaster = isInst1MPE && midiEvent.channel == 0;
        for (int tk = 0; tk < timbreIndex; tk++) {
            controlChange(timbres[tk], midiEvent, tk == 0 && !mpeMaster);
        }
        break;
    }
    case MIDI_POLY_AFTER_TOUCH:
        for (int tk = 0; tk < timbreIndex; tk++) {
            int timbre = timbres[tk];
            char note = midiEvent.value[0] + this->synthState_->mixerState_.instrumentState_[timbre].shiftNote;
            if (likely(note >= 0 && note <= 127)) {
                this->synth_->getTimbre(timbres[tk])->setMatrixPolyAfterTouch(note, INV127 * midiEvent.value[1]);
            }
        }
        break;
    case MIDI_AFTER_TOUCH:
        for (int tk = 0; tk < timbreIndex; tk++) {
            struct MidiData data = {MIDI_DATA_AFTERTOUCH, midiEvent.channel, 0, 0, 0, midiEvent.value[0]};
            this->synth_->getTimbre(timbres[tk])->midiData(data);
        }
        break;
    case MIDI_PITCH_BEND: {
        for (int tk = 0; tk < timbreIndex; tk++) {
            struct MidiData data = {
                MIDI_DATA_PITCHBEND, midiEvent.channel, 0, 0, 0, (uint16_t)((midiEvent.value[1] << 7) | midiEvent.value[0])};
            this->synth_->getTimbre(timbres[tk])->midiData(data);
        }
        break;
    }
    case MIDI_PROGRAM_CHANGE:
        if (this->synthState_->fullState_.midiConfigValue[MIDICONFIG_PROGRAM_CHANGE]) {
            AsyncAction newAction;
            newAction.fullBytes = 0l;
            newAction.action.actionType = LOAD_PRESET;
            for (int tk = 0; tk < timbreIndex; tk++) {
                newAction.action.timbre |= (1 << timbres[tk]);
            }
            newAction.action.param1 = bankNumber_[timbres[0]];
            newAction.action.param2 = bankNumberLSB_[timbres[0]];
            newAction.action.param3 = midiEvent.value[0];
            asyncActions.insert(newAction);
        }
        break;
    case MIDI_SONG_POSITION:
        this->songPosition_ = ((int)midiEvent.value[1] << 7) + midiEvent.value[0];
        this->synth_->midiClockSetSongPosition(this->songPosition_, true);
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
            controlChange(0, midiEvent, true);
        }
        case MIDI_PITCH_BEND: {
            // PITCH BEND in MPE mode we devide by 819.2 and not 8192 to have a x10 value
            struct MidiData data = {
                MIDI_DATA_PITCHBEND, midiEvent.channel, 0, 0, 0, (uint16_t)((midiEvent.value[1] << 7) | midiEvent.value[0])};
            this->synth_->getTimbre(0)->midiData(data);
            break;
        }
        case MIDI_AFTER_TOUCH: {
            struct MidiData data = {MIDI_DATA_AFTERTOUCH, midiEvent.channel, 0, 0, 0, midiEvent.value[0]};
            this->synth_->getTimbre(0)->midiData(data);
            break;
        }
        }
        return;
    }

    switch (midiEvent.eventType) {
    case MIDI_NOTE_OFF: {
        char note = midiEvent.value[0] + this->synthState_->mixerState_.instrumentState_[timbre].shiftNote;
        if (likely(midiEvent.value[0] >= this->synthState_->mixerState_.instrumentState_[timbre].firstNote &&
                   midiEvent.value[0] <= this->synthState_->mixerState_.instrumentState_[timbre].lastNote) &&
            note >= 0 && note <= 127) {
            this->synth_->getTimbre(0)->noteOffMPE(midiEvent.channel, note, midiEvent.value[1]);
        }
        break;
    }
    case MIDI_NOTE_ON: {
        char note = midiEvent.value[0] + this->synthState_->mixerState_.instrumentState_[timbre].shiftNote;
        if (likely(midiEvent.value[0] >= this->synthState_->mixerState_.instrumentState_[timbre].firstNote &&
                   midiEvent.value[0] <= this->synthState_->mixerState_.instrumentState_[timbre].lastNote && note >= 0 && note <= 127)) {
            if (midiEvent.value[1] == 0) {
                this->synth_->getTimbre(0)->noteOffMPE(midiEvent.channel, note, midiEvent.value[1]);
            } else {
                this->synth_->getTimbre(0)->noteOnMPE(midiEvent.channel, note, midiEvent.value[1]);
                visualInfo_->noteOn(0, true);
            }
        }
        break;
    }
    case MIDI_AFTER_TOUCH: {
        struct MidiData data = {MIDI_DATA_MPE_AFTERTOUCH, midiEvent.channel, 0, 0, 0, midiEvent.value[0]};
        this->synth_->getTimbre(0)->midiData(data);
        break;
    }
    case MIDI_CONTROL_CHANGE:
        switch (midiEvent.value[0]) {
        case CC_MPE_SLIDE_CC74: {
            struct MidiData data = {MIDI_DATA_MPE_SLIDE, midiEvent.channel, 0, 0, 0, midiEvent.value[1]};
            this->synth_->getTimbre(0)->midiData(data);
        } break;
        }
    case MIDI_PITCH_BEND: {
        struct MidiData data = {
            MIDI_DATA_MPE_PITCHBEND, midiEvent.channel, 0, 0, 0, (uint16_t)((midiEvent.value[1] << 7) | midiEvent.value[0])};
        this->synth_->getTimbre(0)->midiData(data);
        break;
    }
    }
}

void MidiDecoder::controlChange(int timbre, MidiEvent& midiEvent, bool ownsChannel) {
    int receives = this->synthState_->fullState_.midiConfigValue[MIDICONFIG_RECEIVES];

    // User CC are higher priority even if they hide some important preenfm3 CC
    for (int i = 0; i < 4; i++) {
        if (unlikely(this->synthState_->mixerState_.userCC_[i] == midiEvent.value[0])) {
            struct MidiData data = {MIDI_DATA_USER_CC, midiEvent.channel, midiEvent.value[0], midiEvent.value[1], 0, 0};
            this->synth_->getTimbre(timbre)->midiData(data);
            return;
        }
    }

    if (midiEvent.channel == this->synthState_->mixerState_.globalChannel_ - 1) {
        // treat global channel CC
        switch (midiEvent.value[0]) {
        case CC_MFX_RETURN:
            if (timbre == 0) {
                this->synth_->setNewMixerValueFromMidi(1, MIXER_VALUE_GLOBAL_SETTINGS_3, ccToPercentFloat(midiEvent.value[1]));
            }
            break;
        case CC_MFX_PRESET:
            if (timbre == 0) {
                this->synth_->setNewMixerValueFromMidi(0, MIXER_VALUE_GLOBAL_SETTINGS_3, (float)midiEvent.value[1]);
            }
            break;
        }
    }

    // the following one should always been treated...
    switch (midiEvent.value[0]) {
    case CC_BANK_SELECT:
        bankNumber_[timbre] = midiEvent.value[1];
        break;
    case CC_BANK_SELECT_LSB:
        bankNumberLSB_[timbre] = midiEvent.value[1];
        break;
    case CC_MODWHEEL: {
        struct MidiData data = {MIDI_DATA_CC, midiEvent.channel, midiEvent.value[0], midiEvent.value[1], 0, 0};
        this->synth_->getTimbre(timbre)->midiData(data);
        return;
    }
    case CC_BREATH: {
        struct MidiData data = {MIDI_DATA_CC, midiEvent.channel, midiEvent.value[0], midiEvent.value[1], 0, 0};
        this->synth_->getTimbre(timbre)->midiData(data);
        return;
    }
    case CC_ALL_NOTES_OFF:
        this->synth_->stopArpegiator(timbre);
        this->synth_->allNoteOff(timbre);
        break;
    case CC_ALL_SOUND_OFF:
        this->synth_->allSoundOff(timbre);
        break;
    case CC_HOLD_PEDAL:
        this->synth_->setHoldPedal(timbre, midiEvent.value[1]);
        break;
    case CC_OMNI_OFF:
        // Omni on && omni OFF only accepted on original channel
        if (this->synthState_->mixerState_.instrumentState_[timbre].midiChannel == midiEvent.channel) {
            this->synth_->stopArpegiator(timbre);
            this->synth_->allNoteOff(timbre);
            omniOn_[timbre] = false;
        }
        break;
    case CC_OMNI_ON:
        // Omni on && omni OFF only accepted on original channel
        if (this->synthState_->mixerState_.instrumentState_[timbre].midiChannel == midiEvent.channel) {
            this->synth_->stopArpegiator(timbre);
            this->synth_->allNoteOff(timbre);
            omniOn_[timbre] = true;
        }
        break;
    case CC_RESET:
        this->synth_->stopArpegiator(timbre);
        this->synth_->allNoteOff(timbre);
        this->runningStatus_ = 0;
        this->songPosition_ = 0;
        break;
    case CC_CURRENT_INSTRUMENT:
        this->synth_->setCurrentInstrument(midiEvent.value[1]);
        break;
    }

    // Receive CC enabled?
    if (receives & 0x01) {
        // Global mixer/MPE/sequencer CCs.
        switch (midiEvent.value[0]) {
        case CC_MIXER_VOLUME:
            this->synth_->setNewMixerValueFromMidi(timbre, MIXER_VALUE_VOLUME, (float)midiEvent.value[1] * INV127);
            break;
        case CC_MIXER_PAN:
            this->synth_->setNewMixerValueFromMidi(timbre, MIXER_VALUE_PAN, (float)midiEvent.value[1] - 63);
            break;
        case CC_MIXER_SEND:
            this->synth_->setNewMixerValueFromMidi(timbre, MIXER_VALUE_SEND, (float)midiEvent.value[1] * INV127);
            break;
        case CC_TIMBRE_DRYWET:
            this->synth_->setNewMixerValueFromMidi(timbre, MIXER_VALUE_SEND, ccToPercentFloat(midiEvent.value[1]));
            break;
        case CC_TIMBRE_DYNAMICS:
            this->synth_->setNewMixerValueFromMidi(timbre, MIXER_VALUE_COMPRESSOR, ccToDynamicsIndex(midiEvent.value[1]));
            break;
        case CC_MFX_RETURN:
            this->synth_->setNewMixerValueFromMidi(1, MIXER_VALUE_GLOBAL_SETTINGS_3, ccToPercentFloat(midiEvent.value[1]));
            break;
        case CC_MFX_PRESET:
            this->synth_->setNewMixerValueFromMidi(0, MIXER_VALUE_GLOBAL_SETTINGS_3, (float)midiEvent.value[1]);
            break;
        case CC_SEQ_START_ALL:
            this->synth_->setNewSeqValueFromMidi(timbre, SEQ_VALUE_PLAY_ALL, midiEvent.value[1]);
            break;
        case CC_SEQ_START_INST:
            this->synth_->setNewSeqValueFromMidi(timbre, SEQ_VALUE_PLAY_INST, midiEvent.value[1]);
            break;
        case CC_SEQ_RECORD_INST:
            this->synth_->setNewSeqValueFromMidi(timbre, SEQ_VALUE_RECORD_INST, midiEvent.value[1]);
            break;
        case CC_SEQ_TRANSPOSE:
            this->synth_->setNewSeqValueFromMidi(timbre, SEQ_VALUE_TRANSPOSE, midiEvent.value[1]);
            break;
        case CC_SEQ_SET_SEQUENCE:
            this->synth_->setNewSeqValueFromMidi(timbre, SEQ_VALUE_SEQUENCE_NUMBER, midiEvent.value[1]);
            break;
        default:
            break;
        }

        // Engine-owned parameter CCs.
        struct MidiData data = {MIDI_DATA_CC, midiEvent.channel, midiEvent.value[0], midiEvent.value[1], 0, 0};
        this->synth_->getTimbre(timbre)->midiData(data);
    }

    // Do we accept NRPN in the menu
    if (receives & 0x02) {
        switch (midiEvent.value[0]) {
        case 99:
            this->currentNrpn_[timbre].paramMSB = midiEvent.value[1];
            break;
        case 98:
            this->currentNrpn_[timbre].paramLSB = midiEvent.value[1];
            break;
        case 6:
            this->currentNrpn_[timbre].valueMSB = midiEvent.value[1];
            break;
        case 38:
            this->currentNrpn_[timbre].valueLSB = midiEvent.value[1];
            this->currentNrpn_[timbre].readyToSend = true;
            break;
        case 96:
            // nrpn increment
            if (this->currentNrpn_[timbre].valueLSB == 127) {
                this->currentNrpn_[timbre].valueLSB = 0;
                this->currentNrpn_[timbre].valueMSB++;
            } else {
                this->currentNrpn_[timbre].valueLSB++;
            }
            this->currentNrpn_[timbre].readyToSend = true;
            break;
        case 97:
            // nrpn decremenet
            if (this->currentNrpn_[timbre].valueLSB == 0) {
                this->currentNrpn_[timbre].valueLSB = 127;
                this->currentNrpn_[timbre].valueMSB--;
            } else {
                this->currentNrpn_[timbre].valueLSB--;
            }
            this->currentNrpn_[timbre].readyToSend = true;
            break;
        default:
            break;
        }

        if (this->currentNrpn_[timbre].readyToSend) {
            uint8_t paramMSB = this->currentNrpn_[timbre].paramMSB;
            uint8_t paramLSB = this->currentNrpn_[timbre].paramLSB;
            uint8_t valueMSB = this->currentNrpn_[timbre].valueMSB;
            uint8_t valueLSB = this->currentNrpn_[timbre].valueLSB;
            uint16_t value = (valueMSB << 7) + valueLSB;
            this->currentNrpn_[timbre].readyToSend = false;

            // Several timbres can share a MIDI channel (omni, global). The NRPN
            // is only applied by the timbre owning the channel; the others only
            // keep their parser state in sync.
            if (!ownsChannel) {
                return;
            }

            if (paramMSB == NRPN_MSB_CONTROL) {
                // Command-domain response: decoded by the engine from the main
                // loop (MIDI parsing runs in the audio ISR).
                AsyncAction asyncAction;
                asyncAction.fullBytes = 0l;
                asyncAction.action.actionType = SEND_NRPN_RESPONSE;
                asyncAction.action.timbre = (uint8_t)timbre;
                asyncAction.action.param1 = midiEvent.channel;
                asyncAction.action.param2 = paramMSB;
                asyncAction.action.param3 = paramLSB;
                asyncAction.action.param4 = valueMSB;
                asyncAction.action.param5 = valueLSB;
                asyncActions.insert(asyncAction);
            } else {
                struct MidiData data = {MIDI_DATA_NRPN, midiEvent.channel, 0, 0, (uint16_t)((paramMSB << 7) | paramLSB), value};
                if (!this->synth_->getTimbre(timbre)->midiData(data)) {
                    uint8_t error = this->synth_->getTimbre(timbre)->nrpnError();
                    if (error != 0xFF) {
                        // Single error frame: safe to send from the ISR.
                        sendNrpn(midiEvent.channel, (NRPN_MSB_CONTROL << 7) | NRPN_RESPONSE_ERROR, error);
                        sendMidiDin5Out();
                        sendMidiUsbOut();
                    }
                }
            }
        }
    }
}

void MidiDecoder::sendNrpn(uint8_t channel, uint16_t param, uint16_t value) {
    struct MidiEvent cc;
    cc.eventType = MIDI_CONTROL_CHANGE;
    cc.channel = channel;
    cc.value[0] = 99;
    cc.value[1] = (unsigned char)(param >> 7);
    writeMidiCCOut(&cc);
    cc.value[0] = 98;
    cc.value[1] = (unsigned char)(param & 0x7F);
    writeMidiCCOut(&cc);
    cc.value[0] = 6;
    cc.value[1] = (unsigned char)(value >> 7);
    writeMidiCCOut(&cc);
    cc.value[0] = 38;
    cc.value[1] = (unsigned char)(value & 0x7F);
    writeMidiCCOut(&cc);
}

void MidiDecoder::newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue) {
    // External MIDI changes are not sent back to MIDI.
}

void MidiDecoder::newMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) {
    int sendCCOrNRPN = this->synthState_->fullState_.midiConfigValue[MIDICONFIG_SENDS];
    if (sendCCOrNRPN != 1 && sendCCOrNRPN != 2) {
        return;
    }

    int channel;
    if (valueType == MIXER_VALUE_GLOBAL_SETTINGS_3) {
        channel = this->synthState_->mixerState_.globalChannel_ - 1;
        if (channel < 0) {
            channel = this->synthState_->mixerState_.instrumentState_[this->synthState_->getCurrentTimbre()].midiChannel - 1;
        }
    } else {
        channel = this->synthState_->mixerState_.instrumentState_[timbre].midiChannel - 1;
    }
    if (channel < 0) {
        channel = 0;
    }

    int cc = 0;
    int ccVal = 0;

    if (valueType == MIXER_VALUE_SEND) {
        cc = CC_TIMBRE_DRYWET;
        ccVal = percentFloatToNrpn(newValue);
    } else if (valueType == MIXER_VALUE_COMPRESSOR) {
        cc = CC_TIMBRE_DYNAMICS;
        ccVal = (int)(newValue + 0.1f);
    } else if (valueType == MIXER_VALUE_GLOBAL_SETTINGS_3) {
        if (timbre == 1) {
            cc = CC_MFX_RETURN;
            ccVal = percentFloatToNrpn(newValue);
        } else if (timbre == 0) {
            cc = CC_MFX_PRESET;
            ccVal = (int)(newValue + 0.1f);
        }
    }

    if (cc == 0) {
        return;
    }

    if (sendCCOrNRPN == 2) {
        struct MidiParamMessage message;
        if (synth_->getTimbre(timbre)->midiMixerFeedback(timbre, valueType, newValue, message)) {
            sendNrpn(channel, message.nrpn, message.nrpnValue);
            sendMidiDin5Out();
            sendMidiUsbOut();
        }
    } else {
        struct MidiEvent midiEvent;
        midiEvent.eventType = MIDI_CONTROL_CHANGE;
        midiEvent.channel = channel;
        midiEvent.value[0] = (unsigned char)cc;
        midiEvent.value[1] = (unsigned char)ccVal;
        writeMidiCCOut(&midiEvent);
        sendMidiDin5Out();
        sendMidiUsbOut();
    }
}

void MidiDecoder::newEngineParamValue(int timbre, int paramId, int context, float oldValue, float newValue) {
    struct MidiParamMessage message;
    if (this->synth_->getTimbre(timbre)->midiParamFeedback(paramId, context, newValue, message)) {
        sendMidiParamFeedback(timbre, message);
    }
}

void MidiDecoder::sendMidiParamFeedback(int timbre, const struct MidiParamMessage& message) {
    int channel = this->synthState_->mixerState_.instrumentState_[timbre].midiChannel - 1;

    struct MidiEvent cc;
    cc.eventType = MIDI_CONTROL_CHANGE;
    // Si channel = ALL envoie sur 1
    if (channel == -1) {
        channel = 0;
    }
    cc.channel = channel;

    if (message.isNrpn) {
        // NRPN is 4 control change
        cc.value[0] = 99;
        cc.value[1] = (unsigned char)(message.nrpn >> 7);
        writeMidiCCOut(&cc);
        cc.value[0] = 98;
        cc.value[1] = (unsigned char)(message.nrpn & 0x7F);
        writeMidiCCOut(&cc);
        cc.value[0] = 6;
        cc.value[1] = (unsigned char)(message.nrpnValue >> 7);
        writeMidiCCOut(&cc);
        cc.value[0] = 38;
        cc.value[1] = (unsigned char)(message.nrpnValue & 0x7F);
        writeMidiCCOut(&cc);

        sendMidiDin5Out();
        sendMidiUsbOut();
    } else {
        cc.value[0] = message.cc;
        cc.value[1] = message.ccValue;

        if (lastSentCC_.value[1] != cc.value[1] || lastSentCC_.value[0] != cc.value[0] || lastSentCC_.channel != cc.channel) {
            writeMidiCCOut(&cc);
            sendMidiDin5Out();
            sendMidiUsbOut();
            lastSentCC_ = cc;
        }
    }
}

/** Only send control change */
void MidiDecoder::writeMidiCCOut(struct MidiEvent* toSend) {

    // We don't send midi to both USB and USART at the same time...
    if (this->synthState_->fullState_.midiConfigValue[MIDICONFIG_USB] == USBMIDI_IN_AND_OUT) {
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
    if (usbMidiOutBuffWrt != usbMidiOutBuff && this->synthState_->fullState_.midiConfigValue[MIDICONFIG_USB] == USBMIDI_IN_AND_OUT) {

        USBD_LL_Transmit(&hUsbDeviceFS, MIDI_IN_EP, usbMidiOutBuff, usbMidiOutBuffWrt - usbMidiOutBuff);
        // go back to begining of buffer
        usbMidiOutBuffWrt = usbMidiOutBuff;
    }
}

/*
 * Allow to conserve same MIDI nrpn even when order are changed
 */
uint8_t MidiDecoder::analyseSysexBuffer(uint8_t* sysexBuffer, uint16_t size) {
    if (sysexBuffer[0] == 0x7d) {
        switch (sysexBuffer[1]) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            this->synth_->setNewMixerValueFromMidi(sysexBuffer[1] - 1, MIXER_VALUE_VOLUME, (float)sysexBuffer[2] * INV127);
            return 1;
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
                if (((1 << t) & asyncAction.action.timbre) > 0) {
                    synth_->loadPresetFromMidi(t, asyncAction.action.param1, asyncAction.action.param2, asyncAction.action.param3);
                }
            }
            break;
        case SEND_NRPN_RESPONSE: {
            Timbre* timbre = this->synth_->getTimbre(asyncAction.action.timbre);
            uint8_t channel = asyncAction.action.param1;
            uint16_t address = (uint16_t)((asyncAction.action.param2 << 7) | asyncAction.action.param3);
            uint16_t value = (uint16_t)((asyncAction.action.param4 << 7) | asyncAction.action.param5);

            timbre->startNrpnResponse(channel, address, value);
            const struct MidiParamMessage* message;
            while ((message = timbre->getNextNrpn()) != nullptr) {
                if (!message->isNrpn) {
                    continue;
                }
                sendNrpn(channel, message->nrpn, message->nrpnValue);
                sendMidiDin5Out();
                sendMidiUsbOutIfBufferFull();
                while (usartBufferOut.getCount() > 0) {
                }
            }
            sendMidiUsbOut();
            break;
        }
        }
    }
}
