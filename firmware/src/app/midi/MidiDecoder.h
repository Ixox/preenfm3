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

#include "MidiControlChange.h"
#include "RingBuffer.h"
#include "Storage.h"
#include "Synth.h"
#include "SynthStateAware.h"
#include "VisualInfo.h"

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

struct Nrpn {
    unsigned char paramLSB;
    unsigned char paramMSB;
    unsigned char valueLSB;
    unsigned char valueMSB;
    bool readyToSend;
};

enum ActionType { LOAD_PRESET, SEND_NRPN_RESPONSE, SIMULATE_UI };

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

class MidiDecoder : public SynthParamListener, public SynthStateAware {
public:
    MidiDecoder();
    virtual ~MidiDecoder();
    void setStorage(Storage* storage) {
        this->storage_ = storage;
    }

    void newByte(unsigned char byte);
    void newMessageType(unsigned char byte);
    void newMessageData(unsigned char byte);
    void midiEventReceived(MidiEvent& midiEvent);
    void midiEventForInstrument1MPE(MidiEvent& midiEvent);
    void controlChange(int timbre, MidiEvent& midiEvent, bool ownsChannel);
    void setSynth(Synth* synth);
    void setVisualInfo(VisualInfo* visualInfo);

    void newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue);
    void newEngineParamValue(int timbre, int paramId, int context, float oldValue, float newValue);
    void newMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue);
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

    void writeMidiCCOut(struct MidiEvent* toSend);
    void sendMidiDin5Out();
    void sendMidiUsbOut();
    void sendMidiUsbOutIfBufferFull();
    void playNote(int timbre, char note, char velocity) {
    }
    void stopNote(int timbre, char note) {
    }
    void newTimbre(int timbre) {
        currentTimbre_ = timbre;
    }

    // Some actions must not be called from the audio thread
    void processAsyncActions();

private:
    void sendMidiParamFeedback(int timbre, const struct MidiParamMessage& message);
    void sendNrpn(uint8_t channel, uint16_t param, uint16_t value);
    uint8_t analyseSysexBuffer(uint8_t* sysexBuffer, uint16_t size);

    struct MidiEventState currentEventState_;
    struct MidiEvent currentEvent_;
    Synth* synth_;
    VisualInfo* visualInfo_;
    Storage* storage_;
    int currentTimbre_;
    struct MidiEvent toSend_;
    struct MidiEvent lastSentCC_;
    struct Nrpn currentNrpn_[NUMBER_OF_TIMBRES];
    bool omniOn_[NUMBER_OF_TIMBRES];
    unsigned char runningStatus_;

    // Midi Clock
    bool isExternalMidiClockStarted_;
    int midiClockCpt_;
    int songPosition_;

    // int bank number
    char bankNumber_[NUMBER_OF_TIMBRES];
    char bankNumberLSB_[NUMBER_OF_TIMBRES];
};

#endif /* MIDIDECODER_H_ */
