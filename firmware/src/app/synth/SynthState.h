/*
 * Copyright 2019 Xavier Hosxe
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

#ifndef SYNTHSTATE_H_
#define SYNTHSTATE_H_

#include "Common.h"
#include "EncodersListener.h"
#include "Menu.h"
#include "MixerState.h"
#include "ScreenTypes.h"
#include "Storage.h"
#include "SynthMenuListener.h"
#include "SynthParamListener.h"

typedef unsigned char uchar;

enum OscShape {
    OSC_SHAPE_SIN = 0,
    OSC_SHAPE_SAW,
    OSC_SHAPE_SQUARE,
    OSC_SHAPE_SIN_SQUARE,
    OSC_SHAPE_SIN_ZERO,
    OSC_SHAPE_SIN_POS,
    OSC_SHAPE_RAND,
    OSC_SHAPE_OFF,
    OSC_SHAPE_USER1,
    OSC_SHAPE_USER2,
    OSC_SHAPE_USER3,
    OSC_SHAPE_USER4,
    OSC_SHAPE_USER5,
    OSC_SHAPE_USER6,
    OSC_SHAPE_LAST
};

enum LfoType { LFO_SIN = 0, LFO_SAW, LFO_TRIANGLE, LFO_SQUARE, LFO_RANDOM, LFO_BROWNIAN, LFO_WANDERING, LFO_FLOW, LFO_TYPE_MAX };

// const char* midiNoteCurves[] =  { "Flat", "+Ln ", "+Ln2", "+Exp", "-Ln ", "-Ln2", "-Exp" };

enum MidiNoteCurve {
    MIDI_NOTE_CURVE_FLAT = 0,
    MIDI_NOTE_CURVE_LINEAR,
    MIDI_NOTE_CURVE_LINEAR2,
    MIDI_NOTE_CURVE_EXP,
    MIDI_NOTE_CURVE_M_LINEAR,
    MIDI_NOTE_CURVE_M_LINEAR2,
    MIDI_NOTE_CURVE_M_EXP,
    MIDI_NOTE_CURVE_MAX
};

enum OscFrequencyType { OSC_FT_KEYBOARD = 0, OSC_FT_FIXE, OSC_FT_KEYHZ };

enum OscEnv2Loop { LFO_ENV2_NOLOOP = 0, LFO_ENV2_LOOP_SILENCE, LFO_ENV2_LOOP_ATTACK };

enum CurveType {
    CURVE_TYPE_EXP = 0,
    CURVE_TYPE_LIN,
    CURVE_TYPE_LOG,
    CURVE_TYPE_USER1,
    CURVE_TYPE_USER2,
    CURVE_TYPE_USER3,
    CURVE_TYPE_USER4,
    CURVE_TYPE_MAX
};

enum FILTER_TYPE {
    FILTER_OFF = 0,
    FILTER_MIXER,
    FILTER_LP,
    FILTER_HP,
    FILTER_BASS,
    FILTER_BP,
    FILTER_CRUSHER,
    FILTER_LP2,
    FILTER_HP2,
    FILTER_BP2,
    FILTER_LP3,
    FILTER_HP3,
    FILTER_BP3,
    FILTER_PEAK,
    FILTER_NOTCH,
    FILTER_BELL,
    FILTER_LOWSHELF,
    FILTER_HIGHSHELF,
    FILTER_LPHP,
    FILTER_BPds,
    FILTER_LPWS,
    FILTER_TILT,
    FILTER_STEREO,
    FILTER_SAT,
    FILTER_SIGMOID,
    FILTER_FOLD,
    FILTER_WRAP,
    FILTER_XOR,
    FILTER_TEXTURE1,
    FILTER_TEXTURE2,
    FILTER_LPXOR,
    FILTER_LPXOR2,
    FILTER_LPSIN,
    FILTER_HPSIN,
    FILTER_QUADNOTCH,
    FILTER_AP4,
    FILTER_AP4B,
    FILTER_AP4D,
    FILTER_ORYX,
    FILTER_ORYX2,
    FILTER_ORYX3,
    FILTER_18DB,
    FILTER_LADDER,
    FILTER_LADDER2,
    FILTER_DIOD,
    FILTER_KRMG,
    FILTER_TEEBEE,
    FILTER_SVFLH,
    FILTER_CRUSH2,
    FILTER_LAST
};

enum FILTER2_TYPE {
    FILTER2_OFF = 0,
    FILTER2_FLANGE,
    FILTER2_DIMENSION,
    FILTER2_CHORUS,
    FILTER2_WIDE,
    FILTER2_DOUBLER,
    FILTER2_TRIPLER,
    FILTER2_BODE,
    FILTER2_DELAYCRUNCH,
    FILTER2_PINGPONG,
    FILTER2_DIFFUSER,
    FILTER2_GRAIN1,
    FILTER2_GRAIN2,
    FILTER2_STEREO_BP,
    FILTER2_LAST
};

// Display information
struct FilterRowDisplay {
    const char* paramName[3];
};

// Class define to allow initalization
enum EventType {
    MIDI_NOTE_OFF = 0x80,
    MIDI_NOTE_ON = 0x90,
    MIDI_POLY_AFTER_TOUCH = 0xA0,
    MIDI_CONTROL_CHANGE = 0xb0,
    MIDI_PROGRAM_CHANGE = 0xc0,
    MIDI_AFTER_TOUCH = 0xd0,
    MIDI_PITCH_BEND = 0xe0,
    MIDI_SYSTEM_COMMON = 0xf0,
    MIDI_SYSEX = 0xf0,
    MIDI_SONG_POSITION = 0xf2,
    MIDI_START = 0xfa,
    MIDI_CLOCK = 0xf8,
    MIDI_CONTINUE = 0xfb,
    MIDI_STOP = 0xfc,
    MIDI_SYSEX_END = 0xf7
};

enum EventState { MIDI_EVENT_WAITING = 0, MIDI_EVENT_IN_PROGRESS, MIDI_EVENT_SYSEX, MIDI_EVENT_COMPLETE };

class Timbre;
class MixerState;
class ScreenEditor;
class ScreenMixer;
class ScreenMenu;
class ScreenSequencer;

class SynthState : public EncodersListener {
public:
    SynthState();

    void setStorage(Storage* storage) {
        this->storage_ = storage;
    }
    void setTimbres(Timbre* timbres) {
        this->timbres_ = timbres;
    }

    Timbre* getTimbre(int timbre);

    void init(ScreenMixer* displayMixer, ScreenEditor* dDisplayEditor, ScreenMenu* displayMenu, ScreenSequencer* displaySequencer);

    void encoderTurned(int encoder, int ticks);
    void configValueChanged();

    void buttonPressed(int button);
    void buttonLongPressed(int button);
    void twoButtonsPressed(int button1, int button2);
    void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button);

    void setNewStepValue(int timbre, int whichStepSeq, int step, int newValue);

    const MenuItem* getMenuBack();

    void setCurrentInstrument(int currentTimbre);

    void insertParamListener(SynthParamListener* listener) {
        if (firstParamListener_ != 0) {
            listener->nextListener_ = firstParamListener_;
        }
        firstParamListener_ = listener;
    }

    void insertMenuListener(SynthMenuListener* listener) {
        if (firstMenuListener_ != 0) {
            listener->nextListener_ = firstMenuListener_;
        }
        firstMenuListener_ = listener;
    }

    void propagateNewSynthMode() {
        propagateNoteOff();
        for (SynthMenuListener* listener = firstMenuListener_; listener != 0; listener = listener->nextListener_) {
            listener->newSynthMode(&fullState_);
        }
    }

    void propagateNewScreenPage() {
        propagateNoteOff();
        for (SynthMenuListener* listener = firstMenuListener_; listener != 0; listener = listener->nextListener_) {
            listener->newScreenPage(&fullState_);
        }
    }

    void propagateMenuBack(const MenuItem* oldMenuItem) {
        for (SynthMenuListener* listener = firstMenuListener_; listener != 0; listener = listener->nextListener_) {
            listener->menuBack(oldMenuItem, &fullState_);
        }
    }

    void propagateNewMenuState() {
        for (SynthMenuListener* listener = firstMenuListener_; listener != 0; listener = listener->nextListener_) {
            listener->newMenuState(&fullState_);
        }
    }

    void propagateNewMenuSelect() {
        for (SynthMenuListener* listener = firstMenuListener_; listener != 0; listener = listener->nextListener_) {
            listener->newMenuSelect(&fullState_);
        }
    }

    void propagateNewMixerEdit(int oldButton, int newButton) {
        for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
            listener->newMixerEdit(oldButton, newButton);
        }
    }

    void propagateNewEngineParamValue(int timbre, int paramId, int context, float oldValue, float newValue) {
        for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
            listener->newEngineParamValue(timbre, paramId, context, oldValue, newValue);
        }
    }

    void propagateNewEngineParamValueFromExternal(int timbre, int paramId, int context, float oldValue, float newValue) {
        for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
            listener->newEngineParamValueFromExternal(timbre, paramId, context, oldValue, newValue);
        }
    }

    void propagateNewMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) {
        for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
            listener->newMixerValue(valueType, timbre, oldValue, newValue);
        }
    }

    void propagateNewPresetName(int timbre) {
        for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
            listener->newPresetName(timbre);
        }
    }

    void propagateNewMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue) {
        for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
            listener->newMixerValueFromExternal(timbre, mixerValueType, oldValue, newValue);
        }
    }

    void propagateBeforeNewParamsLoad(int timbre) {
        for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
            listener->beforeNewParamsLoad(timbre);
        }
    }

    bool canPlayNote() {
        return true;
    }

    void propagateNoteOff() {
        if (this->isPlayingNote_ && canPlayNote()) {
            for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
                listener->stopNote(playingTimbre_, playingNote_);
            }
            this->isPlayingNote_ = false;
        }
    }

    void propagateNoteOn(int shift) {
        if (canPlayNote()) {
            if (this->isPlayingNote_ && (this->playingNote_ == fullState_.midiConfigValue[MIDICONFIG_TEST_NOTE] + shift)) {
                propagateNoteOff();
            } else {
                if (this->isPlayingNote_) {
                    propagateNoteOff();
                }
                playingNote_ = fullState_.midiConfigValue[MIDICONFIG_TEST_NOTE] + shift;
                playingTimbre_ = currentTimbre_;
                for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
                    listener->playNote(playingTimbre_, playingNote_, fullState_.midiConfigValue[MIDICONFIG_TEST_VELOCITY]);
                }
                this->isPlayingNote_ = true;
            }
        }
    }

    void storeTestNote() {
        this->storedIsPlayingNote_ = this->isPlayingNote_;
        this->storedPlayingNote_ = this->playingNote_;
        this->storedPlayingTimbre_ = this->playingTimbre_;
    }

    void restoreTestNote() {
        if (this->storedIsPlayingNote_) {
            for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
                listener->playNote(this->storedPlayingTimbre_, this->storedPlayingNote_,
                                   fullState_.midiConfigValue[MIDICONFIG_TEST_VELOCITY]);
            }
            this->isPlayingNote_ = this->storedIsPlayingNote_;
            this->playingTimbre_ = this->storedPlayingTimbre_;
            this->playingNote_ = this->storedPlayingNote_;
        }
    }

    void propagateAfterNewParamsLoad(int timbre);
    void propagateAfterNewMixerLoad();
    void propagateNewTimbre(int timbre);

    SynthEditMode getSynthMode() {
        return fullState_.synthMode;
    }

    void newSysexBankReady();

    void slowTick();

    void setCurrentTimbre(int timbre) {
        currentTimbre_ = timbre;
    }

    bool getIsPlayingNote() {
        return isPlayingNote_;
    }
    void loadPreset(int timbre, PFM3File const* bank, int patchNumber);
    void savePreset(int timbre, PFM3File const* bank, int patchNumber);
    void loadNewPreset(int timbre);
    void loadDx7Patch(int timbre, PFM3File const* bank, int patchNumber);
    void loadMixer(PFM3File const* bank, int patchNumber);
    void loadPresetFromMidi(int timbre, int bank, int bankLSB, int patchNumber);

    bool newRandomizerValue(int encoder, int ticks);
    void randomizePreset();

    int getCurrentTimbre() {
        return currentTimbre_;
    }

    char* getTimbreName(int t);

    Storage* getStorage() {
        return storage_;
    }

    bool scalaSettingsChanged(int timbre);

    MixerState mixerState_;
    struct FullState fullState_;

    const char* getSequenceName();

private:
    int currentTimbre_;
    char operatorNumber_;

    bool isPlayingNote_;
    char playingNote_;
    char playingTimbre_;
    bool storedIsPlayingNote_;
    char storedPlayingNote_;
    char storedPlayingTimbre_;

    // Done menu temporisation
    unsigned int doneClick_;

    SynthParamListener* firstParamListener_;
    SynthMenuListener* firstMenuListener_;

    int getLength(const char* str) {
        int length = 0;
        for (const char* c = str; *c != '\0'; c++) {
            length++;
        }
        return length;
    }

    Storage* storage_;
    Timbre* timbres_;

    // One Display class per main page
    ScreenMixer* displayMixer_;
    ScreenMenu* displayMenu_;
    ScreenEditor* displayEditor_;
    ScreenSequencer* displaySequencer_;
};

#endif /* SYNTHSTATE_H_ */
