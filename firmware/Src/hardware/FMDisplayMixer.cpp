/*
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

#include "FMDisplayMixer.h"
#include "SynthState.h"
#include "TftDisplay.h"
#include "TftAlgo.h"

// To access comp values
#include "preenfm3.h"


// Mixer first Y line and HEIGHT
#define Y_MIXER           80
#define HEIGHT_MIXER_LINE 28


const char* outDisplay[] = { "1", "1-2", "  2", "3", "3-4", "  4", "5", "5-6", "  6" };
const char* compDisplay[]= { "Off", "Slow", "Medium", "Fast"};
const char* enableNames[] = { "Off", "On" };
const char* levelMeterWhere[] = { "Off", "Mix", "All" };
const char* scalaMapNames[] = { "Keybrd", "Continu" };
static const char* nullNames[] = { };

const char* midiWithNone [] = { "None", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"};
const char* midiWithAll [] = { "All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"};

// ======================================== MIXER ================================


const struct Pfm3MixerButtonState volumeButtonState = {
      "Volume", MIXER_VALUE_VOLUME,
        {0, 4, 401, DISPLAY_TYPE_FLOAT, nullNames},
};

const struct Pfm3MixerButtonState panButtonState = {
      "Stereo pan", MIXER_VALUE_PAN,
      {-63, 63, 127, DISPLAY_TYPE_INT, nullNames }
};


const struct Pfm3MixerButton volumeButton = {
  "Mix",
  2,
  { &volumeButtonState, &panButtonState }
};

const struct Pfm3MixerButtonState compButtonState = {
      "Compressor", MIXER_VALUE_COMPRESSOR,
        {0, 3, 4, DISPLAY_TYPE_STRINGS, compDisplay }
};


const struct Pfm3MixerButtonState outButtonState = {
      "Jack output", MIXER_VALUE_OUT,
        {0, 8, 9, DISPLAY_TYPE_STRINGS, outDisplay }
};


const struct Pfm3MixerButton outButton = {
  "Out",
  2,
  { &compButtonState, &outButtonState }
};


const struct Pfm3MixerButtonState voiceButtonState = {
      "Number of voices", MIXER_VALUE_NUMBER_OF_VOICES,
        {0, 16, 17, DISPLAY_TYPE_INT, nullNames}
};

const struct Pfm3MixerButton voiceButton = {
  "Voices",
  1,
  { &voiceButtonState }
};





// ==

const struct Pfm3MixerButtonState midiButtonState = {
      "Channel", MIXER_VALUE_MIDI_CHANNEL,
        {0, 16, 17, DISPLAY_TYPE_STRINGS, midiWithAll }
};

const struct Pfm3MixerButtonState rangeStartButtonState = {
      "First Note", MIXER_VALUE_MIDI_FIRST_NOTE,
        {0, 127, 128, DISPLAY_TYPE_INT, nullNames }
};

const struct Pfm3MixerButtonState rangeEndButtonState = {
      "Last Note", MIXER_VALUE_MIDI_LAST_NOTE,
        {0, 127, 128, DISPLAY_TYPE_INT, nullNames }
};

const struct Pfm3MixerButtonState rangeShiftButtonState = {
      "Note Shift", MIXER_VALUE_MIDI_SHIFT_NOTE,
        {-63, 64, 128, DISPLAY_TYPE_INT, nullNames }
};




const struct Pfm3MixerButton midiButton = {
  "Midi",
  4,
  { &midiButtonState, &rangeStartButtonState, &rangeEndButtonState, &rangeShiftButtonState  }
};


const struct Pfm3MixerButtonState scalaEnable = {
      "Enable", MIXER_VALUE_SCALA_ENABLE,
        {0, 1, 2, DISPLAY_TYPE_STRINGS, enableNames }
};


const struct Pfm3MixerButtonState scalaMap = {
      "Mapping", MIXER_VALUE_SCALA_MAPPING,
        {0, SCALA_MAPPING_NUMBER_OF_OPTIONS - 1, SCALA_MAPPING_NUMBER_OF_OPTIONS, DISPLAY_TYPE_STRINGS, scalaMapNames }
};

const struct Pfm3MixerButtonState scalaScale = {
      "Scale", MIXER_VALUE_SCALA_SCALE,
        {0, NUMBEROFSCALASCALEFILES-1, NUMBEROFSCALASCALEFILES, DISPLAY_TYPE_SCALA_SCALE, nullNames }
};

const struct Pfm3MixerButton scalaButton = {
  "Scala",
  3,
  { &scalaEnable, &scalaMap, &scalaScale }
};

// ==


#define NUMBER_OF_GLOBAL_SETTINGS 5
const struct Pfm3MixerButtonStateParam globalSettings[6] = {
   { 0, 16, 17, DISPLAY_TYPE_STRINGS, midiWithNone },
   { 0, 16, 17, DISPLAY_TYPE_STRINGS, midiWithNone },
   { 0, 1, 2, DISPLAY_TYPE_STRINGS, enableNames },
   { 420, 460, 401, DISPLAY_TYPE_FLOAT, nullNames },
   { 0, 2, 3, DISPLAY_TYPE_STRINGS, levelMeterWhere },
   { 430, 450, 102, DISPLAY_TYPE_FLOAT, nullNames }
};


const struct Pfm3MixerButtonState globalSettingsDetect = {
      "Options", MIXER_VALUE_GLOBAL_SETTINGS
};

const struct Pfm3MixerButton globalButton = {
  "Global",
  1,
  { &globalSettingsDetect}
};


const struct PfmMixerMenu mixerMenu = {
      { &volumeButton, &outButton, &voiceButton, &midiButton, &scalaButton, &globalButton }
};


/*
 * Class methods
 */

FMDisplayMixer::FMDisplayMixer() {
    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        valueChangedCounter_[k] = 0;
    }
}

void* FMDisplayMixer::getValuePointer(int valueType, int encoder) {
    void *valueP = 0;
    switch (valueType) {
        case MIXER_VALUE_OUT:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].out;
            break;
        case MIXER_VALUE_COMPRESSOR:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].compressorType;
            break;
        case MIXER_VALUE_PAN:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].pan;
            break;
        case MIXER_VALUE_VOLUME:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].volume;
            break;
        case MIXER_VALUE_NUMBER_OF_VOICES:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].numberOfVoices;
            break;
        case MIXER_VALUE_MIDI_CHANNEL:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].midiChannel;
            break;
        case MIXER_VALUE_MIDI_FIRST_NOTE:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].firstNote;
            break;
        case MIXER_VALUE_MIDI_LAST_NOTE:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].lastNote;
            break;
        case MIXER_VALUE_MIDI_SHIFT_NOTE:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].shiftNote;
            break;
        case MIXER_VALUE_SCALA_ENABLE:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].scalaEnable;
            break;
        case MIXER_VALUE_SCALA_MAPPING:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].scalaMapping;
            break;
        case MIXER_VALUE_SCALA_SCALE:
            valueP = (void*) &synthState_->mixerState.instrumentState_[encoder].scaleScaleNumber;
            break;
        case MIXER_VALUE_GLOBAL_SETTINGS:
            switch (encoder) {
                case 0:
                    valueP = (void*) &synthState_->mixerState.globalChannel_;
                    break;
                case 1:
                    valueP = (void*) &synthState_->mixerState.currentChannel_;
                    break;
                case 2:
                    valueP = (void*) &synthState_->mixerState.midiThru_;
                    break;
                case 3:
                    valueP = (void*) &synthState_->mixerState.tuning_;
                    break;
                case 4:
                    valueP = (void*) &synthState_->mixerState.levelMeterWhere_;
                    break;
                case 5:
                    break;
            }
            break;
    }
    return valueP;
}

void FMDisplayMixer::displayMixerValueInteger(int timbre, int x, int value) {
    tft_->setCursorInPixel(18 * TFT_BIG_CHAR_WIDTH, Y_MIXER + timbre * HEIGHT_MIXER_LINE);
    if (value >= 0 && value < 100 && timbre < 18) {
        tft_->print(' ');
    }
    if (value > -10 && value < 10) {
        tft_->print(' ');
    }
    tft_->print(value);
}

void FMDisplayMixer::displayMixerValue(int timbre) {
    tft_->setCharBackgroundColor(COLOR_BLACK);

    const Pfm3MixerButton *currentButton = mixerMenu.mixerButton[synthState_->fullState.mixerCurrentEdit];
    bool isGlobalSettings = (currentButton->state[0]->mixerValueType == MIXER_VALUE_GLOBAL_SETTINGS);

    const Pfm3MixerButtonStateParam *buttonStateParam;
    void *valueP;
    uint8_t mixerValueType;

    if (!isGlobalSettings) {
        int buttonState = synthState_->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + synthState_->fullState.mixerCurrentEdit];
        buttonStateParam = &currentButton->state[buttonState]->buttonState;
        mixerValueType = currentButton->state[buttonState]->mixerValueType;
    } else {
        if (timbre >= NUMBER_OF_GLOBAL_SETTINGS) {
            return;
        }

        mixerValueType = MIXER_VALUE_GLOBAL_SETTINGS;
        buttonStateParam = &globalSettings[timbre];
    }
    valueP = getValuePointer(mixerValueType, timbre);

    // Scale particular case
    if (!synthState_->mixerState.instrumentState_[timbre].scalaEnable
        && (mixerValueType == MIXER_VALUE_SCALA_MAPPING || mixerValueType == MIXER_VALUE_SCALA_SCALE)) {
        tft_->setCharColor(COLOR_GRAY);
        tft_->setCursorInPixel(18 * TFT_BIG_CHAR_WIDTH, Y_MIXER + timbre * HEIGHT_MIXER_LINE);
        tft_->print("N/A");
        return;
    }

    switch (buttonStateParam->displayType) {
        case DISPLAY_TYPE_SCALA_SCALE: {
            // displayMixerValueInteger(timbre, 18, (*((uint8_t*)valueP)));
            const char *scalaName = synthState_->getStorage()->getScalaFile()->getFile((*((uint8_t*) valueP)))->name;
            // memorize scala name
            // memcpy(synthState->mixerState.instrumentState[timbre].scalaScale, scalaName, 12); ??
            int dotIndex = 0;
            for (dotIndex = 0; dotIndex < 7 && scalaName[dotIndex] != '.'; dotIndex++);
            tft_->fillArea(160, Y_MIXER + timbre * HEIGHT_MIXER_LINE, 235-160, 20, COLOR_BLACK);
            tft_->setCursorInPixel(238 - dotIndex * TFT_SMALL_CHAR_WIDTH, Y_MIXER + timbre * HEIGHT_MIXER_LINE + 4);
            tft_->printSmallChars(scalaName, dotIndex);
            break;
        }
        case DISPLAY_TYPE_INT:

            if (unlikely(mixerValueType == MIXER_VALUE_PAN)) {
                // Stereo is 1,4,7
                int out = synthState_->mixerState.instrumentState_[timbre].out;
                out--;
                if ((out % 3) != 0) {
                    // Mono, no panning
                    tft_->setCharColor(COLOR_GRAY);
                    tft_->setCursorInPixel(20 * TFT_BIG_CHAR_WIDTH, Y_MIXER + timbre * HEIGHT_MIXER_LINE);
                    tft_->print('-');
                    return;
                }
            }

            displayMixerValueInteger(timbre, 18, (*((int8_t*) valueP)));
            break;
        case DISPLAY_TYPE_STRINGS: {
            const char *label = buttonStateParam->valueName[(int) (*((uint8_t*) valueP))];
            int len = getLength(label);
            tft_->fillArea(160, Y_MIXER + timbre * HEIGHT_MIXER_LINE, 235-160, 20, COLOR_BLACK);
            tft_->setCursorInPixel(238 - len * 11 , Y_MIXER + timbre * HEIGHT_MIXER_LINE);
            tft_->print(label, 7);
            break;
        }
        case DISPLAY_TYPE_FLOAT: {
            float toDisplay = *((float*) valueP);
            tft_->setCursorInPixel((toDisplay >= 100.0f ? 15 : 16) * TFT_BIG_CHAR_WIDTH, Y_MIXER + timbre * HEIGHT_MIXER_LINE);
            tft_->print(toDisplay);
            break;
        }
        default:
            break;
    }

}

void FMDisplayMixer::refreshMixerByStep(int currentTimbre, int &refreshStatus, int &endRefreshStatus) {
    uint8_t buttonState = synthState_->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + synthState_->fullState.mixerCurrentEdit];
    uint8_t mixerValueType = mixerMenu.mixerButton[synthState_->fullState.mixerCurrentEdit]->state[buttonState]->mixerValueType;
    bool isGlobalSettings = (mixerValueType == MIXER_VALUE_GLOBAL_SETTINGS);

    switch (refreshStatus) {
        case 20:
            tft_->pauseRefresh();
            tft_->clear();
            tft_->fillArea(0, 0, 240, 21, COLOR_DARK_GREEN);
            tft_->setCursorInPixel(2, 2);
            tft_->print("MIX ", COLOR_YELLOW, COLOR_DARK_GREEN);
            tft_->print(synthState_->mixerState.mixName_, COLOR_WHITE, COLOR_DARK_GREEN);
            break;
        case 19:
        case 18:
        case 17:
        case 16:
        case 15:
        case 14:
            // Display row : timbre number + preset name
            tft_->setCharBackgroundColor(COLOR_BLACK);
            tft_->setCursorInPixel(2, Y_MIXER + (19 - refreshStatus) * HEIGHT_MIXER_LINE);
            if (currentTimbre != (19 - refreshStatus)) {
                tft_->setCharColor(COLOR_LIGHT_GRAY);
            } else {
                tft_->setCharColor(COLOR_YELLOW);
            }
            tft_->print(20 - refreshStatus);

            tft_->setCursorInPixel(22, Y_MIXER + (19 - refreshStatus) * HEIGHT_MIXER_LINE);
            if (!isGlobalSettings) {
                tft_->print(synthState_->getTimbreName(19 - refreshStatus));
            } else {
                int line = 19 - refreshStatus;
                if (line < NUMBER_OF_GLOBAL_SETTINGS) {
                    refreshMixerRowGlobalOptions(19 - refreshStatus);
                }
            }
            break;
        case 12:
        case 11:
        case 10:
        case 9:
        case 8:
        case 7: {
            tft_->setCharColor(COLOR_WHITE);
            displayMixerValue(12 - refreshStatus);
            break;
        }
        case 6: {
            uint8_t buttonState = synthState_->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + synthState_->fullState.mixerCurrentEdit];
            const char *valueTitle = mixerMenu.mixerButton[synthState_->fullState.mixerCurrentEdit]->state[buttonState]->stateLabel;
            int len = getLength(valueTitle);
            tft_->setCursorInPixel(240 - len * TFT_BIG_CHAR_WIDTH, 50);
            tft_->setCharBackgroundColor(COLOR_BLACK);
            tft_->setCharColor(COLOR_GREEN);
            tft_->print(valueTitle);
            // NO BREAK ....
        }
        case 5:
        case 4:
        case 3:
        case 2:
        case 1: {
            uint8_t button = 6 - refreshStatus;

            const Pfm3MixerButton *currentButton = mixerMenu.mixerButton[button];

            tft_->drawButton(currentButton->buttonLabel, 270, 29, button, currentButton->numberOfStates,
                synthState_->fullState.mixerCurrentEdit == button ? synthState_->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + button] + 1 : 0,
                COLOR_DARK_GREEN);

            break;
        }
    }

    if (refreshStatus == endRefreshStatus) {
        endRefreshStatus = 0;
        refreshStatus = 0;
    }
}

void FMDisplayMixer::refreshMixerRowGlobalOptions(int row) {
    tft_->setCharColor(COLOR_LIGHT_GRAY);

    switch (row) {
        case 0:
            tft_->print("Global midi");
            break;
        case 1:
            tft_->print("Current midi");
            break;
        case 2:
            tft_->print("Midi thru");
            break;
        case 3:
            tft_->print("Tuning");
            break;
        case 4:
            tft_->print("Level meter");
            break;
        case 5:
            break;
    }
}

void FMDisplayMixer::newMixerValue(uint8_t mixerValue, uint8_t timbre, float oldValue, float newValue) {
    tft_->setCharBackgroundColor(COLOR_BLACK);
    if (oldValue != newValue) {
        tft_->setCharColor(COLOR_YELLOW);
    } else {
        tft_->setCharColor(COLOR_RED);
    }
    displayMixerValue(timbre);
    valueChangedCounter_[timbre] = 4;
}

void FMDisplayMixer::newMixerValueFromExternal(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) {
    const Pfm3MixerButton *currentButton = mixerMenu.mixerButton[synthState_->fullState.mixerCurrentEdit];
    int buttonState = synthState_->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + synthState_->fullState.mixerCurrentEdit];

    if (currentButton->state[buttonState]->mixerValueType == valueType) {
        newMixerValue(valueType, timbre, oldValue, newValue);
    }
}

void FMDisplayMixer::tempoClick() {
    static float lastVolume[NUMBER_OF_TIMBRES];
    static float lastGainReduction[NUMBER_OF_TIMBRES];

    for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {
        if (valueChangedCounter_[timbre] > 0) {
            valueChangedCounter_[timbre]--;
            if (valueChangedCounter_[timbre] == 0) {
                tft_->setCharBackgroundColor(COLOR_BLACK);
                tft_->setCharColor(COLOR_WHITE);
                displayMixerValue(timbre);
            }
        }
    }

    if (synthState_->mixerState.levelMeterWhere_ >= 1) {
        for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {
            if (synthState_->mixerState.instrumentState_[timbre].numberOfVoices > 0) {
                float volume = getCompInstrumentVolume(timbre);
                float gr = getCompInstrumentGainReduction(timbre);
                // gr tend to slower reach 0
                // Let'as accelerate when we don't want to draw the metter anymore
                if (gr > -.1f) {
                    gr = 0;
                }
                if ((volume != lastVolume[timbre] || gr != lastGainReduction[timbre])) {

                    lastVolume[timbre] = volume;
                    lastGainReduction[timbre] = gr;

                    bool isCompressed = synthState_->mixerState.instrumentState_[timbre].compressorType > 0;
                    tft_->drawLevelMetter(22, Y_MIXER + timbre * HEIGHT_MIXER_LINE + 21,
                        218, 3, 5, volume, isCompressed, gr);
                }
            }
        }
    }
}

void FMDisplayMixer::encoderTurned(int encoder, int ticks) {

    const Pfm3MixerButton *currentButton = mixerMenu.mixerButton[synthState_->fullState.mixerCurrentEdit];
    bool isGlobalSettings = (currentButton->state[0]->mixerValueType == MIXER_VALUE_GLOBAL_SETTINGS);
    bool isScalaButton = (currentButton->state[0]->mixerValueType == MIXER_VALUE_SCALA_ENABLE);

    const Pfm3MixerButtonStateParam *buttonStateParam;
    void *valueP;
    uint8_t mixerValueType;

    if (!isGlobalSettings) {
        int buttonState = synthState_->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + synthState_->fullState.mixerCurrentEdit];
        buttonStateParam = &currentButton->state[buttonState]->buttonState;
        mixerValueType = currentButton->state[buttonState]->mixerValueType;
    } else {
        if (encoder >= NUMBER_OF_GLOBAL_SETTINGS) {
            return;
        }
        mixerValueType = MIXER_VALUE_GLOBAL_SETTINGS;
        buttonStateParam = &globalSettings[encoder];
    }
    valueP = getValuePointer(mixerValueType, encoder);

//    uint8_t buttonState = synthState->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + synthState->fullState.mixerCurrentEdit];
//    uint8_t mixerValueType = currentButton->state[buttonState]->mixerValueType;
//    void* valueP = getValuePointer(mixerValueType, encoder);

    switch (buttonStateParam->displayType) {
        case DISPLAY_TYPE_INT:
        case DISPLAY_TYPE_STRINGS:
        case DISPLAY_TYPE_SCALA_SCALE: {
            int8_t oldValue = *((int8_t*) valueP);
            int newValue = oldValue + ticks;

            float maxValue;
            if (mixerValueType == MIXER_VALUE_NUMBER_OF_VOICES) {
                maxValue = MAX_NUMBER_OF_VOICES;
                for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
                    if (t != encoder) {
                        maxValue -= synthState_->mixerState.instrumentState_[t].numberOfVoices;
                    }
                }
            } else {
                maxValue = buttonStateParam->maxValue;
            }

            if (unlikely(newValue > maxValue)) {
                newValue = maxValue;
            } else if (unlikely(newValue < buttonStateParam->minValue)) {
                newValue = buttonStateParam->minValue;
            }

            if (oldValue != newValue) {
                if (isScalaButton) {
                    *((uint8_t*) valueP) = (uint8_t) newValue;
                    if (!synthState_->scalaSettingsChanged(encoder)) {
                        *((uint8_t*) valueP) = (uint8_t) oldValue;
                        newValue = oldValue;
                    }
                } else {
                    *((int8_t*) valueP) = (int8_t) newValue;
                }
            }
            synthState_->propagateNewMixerValue(mixerValueType, encoder, oldValue, newValue);

            break;
        }
        case DISPLAY_TYPE_FLOAT: {
            float oldValue = *((float*) valueP);
            float inc = (buttonStateParam->maxValue - buttonStateParam->minValue) / (buttonStateParam->numberOfValues - 1);
            float newValue = oldValue + ticks * inc;

            // Round to next hundredth
            int newValueInt =  newValue * 1000 + 3;
            newValueInt /= 10;
            newValue = (float)newValueInt / 100.0f;

            if (unlikely(newValue > buttonStateParam->maxValue)) {
                newValue = buttonStateParam->maxValue;
            } else if (unlikely(newValue < buttonStateParam->minValue)) {
                newValue = buttonStateParam->minValue;
            }

            if (oldValue != newValue) {
                *((float*) valueP) = (float) newValue;
            }
            synthState_->propagateNewMixerValue(mixerValueType, encoder, oldValue, newValue);
            break;
        }
        default:
            break;
    }
}

void FMDisplayMixer::buttonPressed(int button) {
    struct FullState *fullState = &synthState_->fullState;

    if (button >= BUTTON_PFM3_1 && button <= BUTTON_PFM3_6) {
        if (fullState->mixerCurrentEdit != button) {
            synthState_->propagateNewMixerEdit(fullState->mixerCurrentEdit, button);
            fullState->mixerCurrentEdit = button;
        } else {
            int numberOfStates = mixerMenu.mixerButton[synthState_->fullState.mixerCurrentEdit]->numberOfStates;

            if (numberOfStates > 1) {
                fullState->buttonState[BUTTONID_MIXER_FIRST_BUTTON + button]++;
                if (fullState->buttonState[BUTTONID_MIXER_FIRST_BUTTON + button] >= numberOfStates) {
                    fullState->buttonState[BUTTONID_MIXER_FIRST_BUTTON + button] = 0;
                }
                synthState_->propagateNewMixerEdit(button, button);
            }
        }
    }
}

