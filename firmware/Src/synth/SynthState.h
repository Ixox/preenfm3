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
#include "SynthParamListener.h"
#include "SynthMenuListener.h"
#include "Menu.h"
#include "Storage.h"
#include "MixerState.h"



enum {
    ENCODER_ENGINE_ALGO = 0,
    ENCODER_ENGINE_VELOCITY,
    ENCODER_ENGINE_PLAY_MODE,
    ENCODER_ENGINE_GLIDE_SPEED,
	ENCODER_NONE, // Define only once !
	ENCODER_INVISIBLE,
    ENCODER_MODULATOR_SYNC_LFO,
    ENCODER_MODULATOR_SYNC_STEPS
};


enum {
    ENCODER_ENGINE2_GLIDE_TYPE = 0,
    ENCODER_ENGINE2_UNISON_SPREAD,
    ENCODER_ENGINE2_UNISON_DETUNE,
    ENCODER_USED_FOR_PFM3_VERSION,
};


enum {
    ENCODER_ARPEGGIATOR_CLOCK = 0,
    ENCODER_ARPEGGIATOR_BPM,
    ENCODER_ARPEGGIATOR_DIRECTION,
    ENCODER_ARPEGGIATOR_OCTAVE
};

enum {
    ENCODER_ARPEGGIATOR_PATTERN = 0,
    ENCODER_ARPEGGIATOR_DIVISION,
    ENCODER_ARPEGGIATOR_DURATION,
    ENCODER_ARPEGGIATOR_LATCH
};

enum {
    ENCODER_EFFECT_TYPE = 0,
    ENCODER_EFFECT_PARAM1,
    ENCODER_EFFECT_PARAM2,
    ENCODER_EFFECT_PARAM3
};

enum {
    ENCODER_ENGINE_IM1 = 0,
    ENCODER_ENGINE_IM1_VELOCITY,
    ENCODER_ENGINE_IM2,
    ENCODER_ENGINE_IM2_VELOCITY
};
enum {
    ENCODER_ENGINE_IM3 = 0,
    ENCODER_ENGINE_IM3_VELOCITY,
    ENCODER_ENGINE_IM4,
    ENCODER_ENGINE_IM4_VELOCITY,
};

enum {
    ENCODER_ENGINE_IM5 = 0,
    ENCODER_ENGINE_IM5_VELOCITY,
    ENCODER_ENGINE_IM6,
    ENCODER_ENGINE_IM6_VELOCITY
};


enum {
    ENCODER_ENGINE_MIX1 = 0,
    ENCODER_ENGINE_PAN1,
    ENCODER_ENGINE_MIX2,
    ENCODER_ENGINE_PAN2,
};

enum {
    ENCODER_ENGINE_MIX3 = 0,
    ENCODER_ENGINE_PAN3,
    ENCODER_ENGINE_MIX4,
    ENCODER_ENGINE_PAN4
};

enum {
    ENCODER_ENGINE_MIX5 = 0,
    ENCODER_ENGINE_PAN5,
    ENCODER_ENGINE_MIX6,
    ENCODER_ENGINE_PAN6
};

enum {
    ENCODER_PERFORMANCE_CC1 = 0,
    ENCODER_PERFORMANCE_CC2,
    ENCODER_PERFORMANCE_CC3,
    ENCODER_PERFORMANCE_CC4
};

enum {
    ENCODER_PERFORMANCE_CC5 = 0,
    ENCODER_PERFORMANCE_CC6
};

enum {
    ENCODER_OSC_SHAP = 0,
    ENCODER_OSC_FTYPE,
    ENCODER_OSC_FREQ,
    ENCODER_OSC_FTUNE
};

enum {
    ENCODER_ENV_A = 0,
    ENCODER_ENV_D,
    ENCODER_ENV_S,
    ENCODER_ENV_R,
};


enum {
    ENCODER_ENV_A_CURVE = 0,
    ENCODER_ENV_D_CURVE,
    ENCODER_ENV_S_CURVE,
    ENCODER_ENV_R_CURVE
};


enum {
    ENCODER_MATRIX_SOURCE = 0,
    ENCODER_MATRIX_MUL,
    ENCODER_MATRIX_DEST1,
    ENCODER_MATRIX_DEST2,
};

enum {
    ENCODER_LFO_SHAPE = 0,
    ENCODER_LFO_FREQ,
    ENCODER_LFO_BIAS,
    ENCODER_LFO_KSYNC
};

enum {
    ENCODER_LFO_PHASE1 = 0,
    ENCODER_LFO_PHASE2,
    ENCODER_LFO_PHASE3
};

enum {
    ENCODER_LFO_ENV2_SILENCE = 0,
    ENCODER_LFO_ENV2_ATTACK,
    ENCODER_LFO_ENV2_RELEASE,
    ENCODER_LFO_ENV2_LOOP
};


enum {
    ENCODER_STEPSEQ_BPM = 0,
    ENCODER_STEPSEQ_GATE
};

enum {
    ENCODER_NOTECURVE_BEFORE = 0,
    ENCODER_NOTECURVE_BREAK_NOTE,
    ENCODER_NOTECURVE_AFTER
};



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

enum LfoType {
    LFO_SIN = 0,
    LFO_SAW,
    LFO_TRIANGLE,
    LFO_SQUARE,
    LFO_RANDOM,
    LFO_BROWNIAN,
    LFO_WANDERING,
    LFO_FLOW,
    LFO_TYPE_MAX
};

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

enum OscFrequencyType {
    OSC_FT_KEYBOARD = 0,
    OSC_FT_FIXE,
    OSC_FT_KEYHZ
};

enum OscEnv2Loop {
    LFO_ENV2_NOLOOP = 0,
    LFO_ENV2_LOOP_SILENCE,
    LFO_ENV2_LOOP_ATTACK
};

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

// Display information
struct FilterRowDisplay {
    const char* paramName[3];
};


struct ParameterRowDisplay {
    const char* rowName;
    const char* paramName[5];
    struct ParameterDisplay params[4];
};

// +1 for dummy oscillator sync
struct AllParameterRowsDisplay {
    struct ParameterRowDisplay* row[NUMBER_OF_ROWS + 1];
};


// Class define to allow initalization
enum EventType {
    MIDI_NOTE_OFF = 0x80,
    MIDI_NOTE_ON = 0x90,
    MIDI_POLY_AFTER_TOUCH = 0xA0,
    MIDI_CONTROL_CHANGE = 0xb0,
    MIDI_PROGRAM_CHANGE =0xc0,
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

enum EventState {
    MIDI_EVENT_WAITING = 0,
    MIDI_EVENT_IN_PROGRESS ,
    MIDI_EVENT_SYSEX ,
    MIDI_EVENT_COMPLETE
};





class Hexter;
class Timbre;
class MixerState;
class FMDisplayEditor;
class FMDisplayMixer;
class FMDisplayMenu;
class FMDisplaySequencer;


class SynthState: public EncodersListener {
public:
    SynthState();

    void setStorage(Storage* storage) {
        this->storage = storage;
    }
    void setHexter(Hexter* hexter) {
        this->hexter = hexter;
    }

    void setTimbres(Timbre* timbres) {
        this->timbres = timbres;
    }

    void init(FMDisplayMixer* displayMixer, FMDisplayEditor* dDisplayEditor, FMDisplayMenu* displayMenu, FMDisplaySequencer* displaySequencer);

    void encoderTurned(int encoder, int ticks);

    void buttonPressed(int button);
    void buttonLongPressed(int button);
    void twoButtonsPressed(int button1, int button2);
    void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button);
    void encoderTurnedForStepSequencer(int row, int encoder4, int encoder6, int ticks);
    void encoderTurnedForArpPattern(int row, int num, int ticks);

    void setNewStepValue(int timbre, int whichStepSeq, int step, int newValue);

    const MenuItem* getMenuBack();

    void setCurrentInstrument(int currentTimbre);

    void insertParamListener(SynthParamListener *listener) {
        if (firstParamListener != 0) {
            listener->nextListener = firstParamListener;
        }
        firstParamListener = listener;
    }

    void insertMenuListener(SynthMenuListener *listener) {
        if (firstMenuListener != 0) {
            listener->nextListener = firstMenuListener;
        }
        firstMenuListener = listener;
    }

    void propagateNewSynthMode() {
        propagateNoteOff();
        for (SynthMenuListener* listener = firstMenuListener; listener != 0; listener = listener->nextListener) {
            listener->newSynthMode(&fullState);
        }
    }

    void propagateNewPfm3Page() {
        propagateNoteOff();
        for (SynthMenuListener* listener = firstMenuListener; listener != 0; listener = listener->nextListener) {
            listener->newPfm3Page(&fullState);
        }
    }

    void propagateMenuBack(const MenuItem* oldMenuItem) {
        for (SynthMenuListener* listener = firstMenuListener; listener != 0; listener = listener->nextListener) {
            listener->menuBack(oldMenuItem, &fullState);
        }
    }

    void propagateNewMenuState() {
        for (SynthMenuListener* listener = firstMenuListener; listener != 0; listener = listener->nextListener) {
            listener->newMenuState(&fullState);
        }
    }

    void propagateNewMenuSelect() {
        for (SynthMenuListener* listener = firstMenuListener; listener != 0; listener = listener->nextListener) {
            listener->newMenuSelect(&fullState);
        }
    }

    void propagateNewMixerEdit(int oldButton, int newButton) {
        for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
            listener->newMixerEdit(oldButton, newButton);
        }
    }

    void propagateNewParamValue(int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue, float newValue) {
        for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
            listener->newParamValue(timbre, currentRow, encoder, param, oldValue, newValue);
        }
    }

    void propagateNewMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) {
        for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
            listener->newMixerValue(valueType, timbre, oldValue, newValue);
        }
    }

    void propagateNewPresetName(int timbre) {
        for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
            listener->newPresetName(timbre);
        }
    }

    void propagateNewParamValueFromExternal(int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue,
            float newValue) {
        for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
            listener->newParamValueFromExternal(timbre, currentRow, encoder, param, oldValue, newValue);
        }
    }

    void propagateNewMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue) {
        for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
            listener->newMixerValueFromExternal(timbre, mixerValueType, oldValue, newValue);
        }
    }

    void propagateBeforeNewParamsLoad(int timbre) {
        for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
            listener->beforeNewParamsLoad(timbre);
        }
    }

    bool canPlayNote() {
        return true;
    }

    void propagateNoteOff() {
        if (this->isPlayingNote && canPlayNote()) {
            for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
                listener->stopNote(playingTimbre, playingNote);
            }
            this->isPlayingNote = false;
        }
    }

    void propagateNoteOn(int shift) {
        if (canPlayNote()) {
            if (this->isPlayingNote && (this->playingNote == fullState.midiConfigValue[MIDICONFIG_TEST_NOTE] + shift)) {
                propagateNoteOff();
            } else {
                if (this->isPlayingNote) {
                    propagateNoteOff();
                }
                playingNote = fullState.midiConfigValue[MIDICONFIG_TEST_NOTE] + shift;
                playingTimbre = currentTimbre;
                for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
                    listener->playNote(playingTimbre, playingNote, fullState.midiConfigValue[MIDICONFIG_TEST_VELOCITY]);
                }
                this->isPlayingNote = true;
            }
        }
    }

    void storeTestNote() {
        this->storedIsPlayingNote = this->isPlayingNote;
        this->storedPlayingNote = this->playingNote;
        this->storedPlayingTimbre = this->playingTimbre;
    }

    void restoreTestNote() {
        if (this->storedIsPlayingNote) {
            for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
                listener->playNote(this->storedPlayingTimbre, this->storedPlayingNote, fullState.midiConfigValue[MIDICONFIG_TEST_VELOCITY]);
            }
            this->isPlayingNote = this->storedIsPlayingNote;
            this->playingTimbre = this->storedPlayingTimbre;
            this->playingNote = this->storedPlayingNote;
        }
    }

    void propagateAfterNewParamsLoad(int timbre);
    void propagateAfterNewMixerLoad();
    void propagateNewTimbre(int timbre);

    SynthEditMode getSynthMode() {
        return fullState.synthMode;
    }

    void newSysexBankReady();

    void tempoClick();

    void setParamsAndTimbre(struct OneSynthParams *newParams, int newCurrentTimbre);

    bool getIsPlayingNote() {
        return isPlayingNote;
    }
    void loadPreset(int timbre, PFM3File const *bank, int patchNumber, struct OneSynthParams* params);
    void loadNewPreset(int timbre);
    void loadDx7Patch(int timbre, PFM3File const *bank, int patchNumber, struct OneSynthParams* params);
    void loadMixer(PFM3File const *bank, int patchNumber);
    void loadPresetFromMidi(int timbre, int bank, int bankLSB, int patchNumber, struct OneSynthParams* params);

    bool newRandomizerValue(int encoder, int ticks);
    void randomizePreset();

    int getCurrentTimbre() {
        return currentTimbre;
    }

    char* getTimbreName(int t);
    uint8_t getTimbrePlayMode(int t);

    Storage* getStorage() {
        return storage;
    }

    bool scalaSettingsChanged(int timbre);

    struct OneSynthParams *params;
    struct OneSynthParams backupParams;
    MixerState mixerState;
    struct FullState fullState;
    char stepSelect[2];
    char patternSelect;

    const char* getSequenceName();

private:

    int currentTimbre;
    char operatorNumber;

    bool isPlayingNote;
    char playingNote;
    char playingTimbre;
    bool storedIsPlayingNote;
    char storedPlayingNote;
    char storedPlayingTimbre;

    // Done menu temporisation
    unsigned int doneClick;

    SynthParamListener* firstParamListener;
    SynthMenuListener* firstMenuListener;

    int getLength(const char *str) {
        int length = 0;
        for (const char *c = str; *c != '\0'; c++) {
            length++;
        }
        return length;
    }

    Storage* storage;
    Hexter* hexter;
    Timbre* timbres;

    // One Display class per main page
    FMDisplayMixer* displayMixer;
    FMDisplayMenu* displayMenu;
    FMDisplayEditor* displayEditor;
    FMDisplaySequencer* displaySequencer;
};

// Global structure used all over the code
extern struct AllParameterRowsDisplay allParameterRows;
extern struct FilterRowDisplay filterRowDisplay[];


#endif /* SYNTHSTATE_H_ */
