/*
 * Copyright 2013 Xavier Hosxe
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

#ifndef MIDIDECODER_H_
#define MIDIDECODER_H_

#include "SynthStateAware.h"
#include "Synth.h"
#include "RingBuffer.h"
#include "VisualInfo.h"
#include "Storage.h"

// number of external control change
#define NUMBER_OF_ECC 4

struct MidiEventState {
    EventState eventState;
    uint8_t numberOfBytes;
    uint16_t index;
};

struct MidiEvent {
    unsigned char channel;
    EventType eventType;
    unsigned char value[2];
};

enum AllControlChange {
    CC_BANK_SELECT = 0,
    CC_MODWHEEL = 1,
    CC_BREATH = 2,
    CC_MIXER_VOLUME = 7,
    CC_MIXER_PAN = 10,
    CC_MIXER_SEND = 11,
    CC_UNISON_DETUNE = 13,
    CC_UNISON_SPREAD = 14,
    CC_ALGO = 16,
    CC_IM1,
    CC_IM2,
    CC_IM3,
    CC_IM4,
    CC_IM5,
    CC_MIX1 = 22,
    CC_PAN1,
    CC_MIX2,
    CC_PAN2,
    CC_MIX3,
    CC_PAN3,
    CC_MIX4,
    CC_PAN4,
    CC_IM_FEEDBACK = 30,
    CC_BANK_SELECT_LSB = 32,
    CC_MFX_PRESET = 40,
    CC_MFX_PREDELAYTIME,
    CC_MFX_PREDELAYMIX,
    CC_MFX_INPUTTILT,
    CC_MFX_MOD_SPEED,
    CC_MFX_MOD_DEPTH,
    CC_MATRIXROW1_MUL = 46,
    CC_MATRIXROW2_MUL,
    CC_MATRIXROW3_MUL,
    CC_MATRIXROW4_MUL,
    CC_OSC1_FREQ,
    CC_OSC2_FREQ,
    CC_OSC3_FREQ,
    CC_OSC4_FREQ,
    CC_OSC5_FREQ,
    CC_OSC6_FREQ,
    CC_LFO1_FREQ,
    CC_LFO2_FREQ,
    CC_LFO3_FREQ,
    CC_LFO_ENV2_SILENCE,
    CC_STEPSEQ5_GATE,
    CC_STEPSEQ6_GATE,
    CC_ENV_ATK_ALL_MODULATOR = 62,
    CC_ENV_REL_ALL_MODULATOR,
    CC_HOLD_PEDAL = 64,
    CC_ENV_ATK_OP1,
    CC_FILTER_TYPE = 70,
    CC_FILTER_PARAM1,
    CC_FILTER_PARAM2,
    CC_FILTER_GAIN,
    CC_MPE_SLIDE_CC74,
    CC_ENV_ATK_OP2,
    CC_ENV_ATK_OP3,
    CC_ENV_ATK_OP4,
    CC_ENV_ATK_OP5,
    CC_ENV_ATK_OP6,
    CC_ENV_ATK_ALL = 80,
    CC_ENV_REL_ALL,
    CC_ENV_REL_OP1,
    CC_ENV_REL_OP2,
    CC_ENV_REL_OP3,
    CC_ENV_REL_OP4,
    CC_ENV_REL_OP5,
    CC_ENV_REL_OP6,
    CC_LFO1_PHASE, // 88
    CC_LFO2_PHASE,
    CC_LFO3_PHASE,
    CC_LFO1_BIAS,
    CC_LFO2_BIAS,
    CC_LFO3_BIAS,
    CC_LFO1_SHAPE,
    CC_LFO2_SHAPE,
    CC_LFO3_SHAPE,
    CC_ARP_CLOCK = 100,
    CC_ARP_DIRECTION,
    CC_ARP_OCTAVE,
    CC_ARP_PATTERN,
    CC_ARP_DIVISION,
    CC_ARP_DURATION,
    CC_MATRIX_SOURCE_CC1 = 115,
    CC_MATRIX_SOURCE_CC2,
    CC_MATRIX_SOURCE_CC3,
    CC_MATRIX_SOURCE_CC4,
    CC_CURRENT_INSTRUMENT,
    CC_ALL_SOUND_OFF = 120,
    CC_ALL_NOTES_OFF = 123,
    CC_OMNI_OFF = 124,
    CC_OMNI_ON,
    CC_RESET = 127
};

struct Nrpn {
    unsigned char paramLSB;
    unsigned char paramMSB;
    unsigned char valueLSB;
    unsigned char valueMSB;
    bool readyToSend;
};


enum ActionType {
    LOAD_PRESET,
    SEND_PATCH_AS_NRPN
};

struct AsyncActionDetail {
    uint8_t actionType;
    uint8_t timbre;
    uint8_t param1;
    uint8_t param2;
    uint8_t param3;
    uint8_t param4;
    uint8_t param5;
    uint8_t param6;
};

struct AsyncAction {
    union {
        struct AsyncActionDetail action;
        uint64_t fullBytes;
    };
};

class MidiDecoder: public SynthParamListener, public SynthStateAware {
public:
    MidiDecoder();
    virtual ~MidiDecoder();
    void setStorage(Storage* storage) {
        this->storage = storage;
    }

    void newByte(unsigned char byte);
    void newMessageType(unsigned char byte);
    void newMessageData(unsigned char byte);
    void midiEventReceived(MidiEvent& midiEvent);
    void midiEventForInstrument1MPE(MidiEvent& midiEvent);
    void controlChange(int timbre, MidiEvent& midiEvent);
    void decodeNrpn(int timbre);
    void setSynth(Synth* synth);
    void setVisualInfo(VisualInfo* visualInfo);

    void newParamValueFromExternal(int timbre, int currentrow, int encoder, ParameterDisplay* param, float oldValue, float newValue);
    void newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue);
    void newParamValue(int timbre, int currentrow, int encoder, ParameterDisplay* param, float oldValue, float newValue);
    void newMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) {
    }
    void newMixerEdit(int oldButton, int newButton) {
    }

    void beforeNewParamsLoad(int timbre) {
    }
    void afterNewParamsLoad(int timbre) {
    }
    void afterNewMixerLoad() {
    }
    void showAlgo() {
    }
    void showIMInformation() {
    }

    void writeMidiCCOut(struct MidiEvent *toSend);
    void sendMidiDin5Out();
    void sendMidiUsbOut();
    void sendMidiUsbOutIfBufferFull();
    void playNote(int timbre, char note, char velocity) {
    }
    void stopNote(int timbre, char note) {
    }
    void newTimbre(int timbre) {
        currentTimbre = timbre;
    }
    void sendCurrentPatchAsNrpns(int timbre);

    // Firmware 2.00
    // Phase LFO1/3 added not at the right place so nrpm and params row are now
    // unlinked for compatibility reason....
    int getNrpnRowFromParamRow(int paramRow);
    int getParamRowFromNrpnRow(int nrpmRow);

    // Some actions must not be called from the audio thread
    void processAsyncActions();
private:
    uint8_t analyseSysexBuffer(uint8_t *sysexBuffer, uint16_t size);

    struct MidiEventState currentEventState;
    struct MidiEvent currentEvent;
    Synth* synth;
    VisualInfo *visualInfo;
    Storage* storage;
    int currentTimbre;
    struct MidiEvent toSend;
    struct MidiEvent lastSentCC;
    struct Nrpn currentNrpn[NUMBER_OF_TIMBRES];
    bool omniOn[NUMBER_OF_TIMBRES];
    unsigned char runningStatus;

    // Midi Clock
    bool isExternalMidiClockStarted;
    int midiClockCpt;
    int songPosition;

    // int bank number
    char bankNumber[NUMBER_OF_TIMBRES];
    char bankNumberLSB[NUMBER_OF_TIMBRES];

};

#endif /* MIDIDECODER_H_ */
