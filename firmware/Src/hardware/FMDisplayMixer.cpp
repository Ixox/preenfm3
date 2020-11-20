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

const char* outDisplay[] = { "1", "1-2", "  2", "3", "3-4", "  4", "5", "5-6", "  6" };
const char* compDisplay[]= { "No", "Slow", "Medi", "Fast"};
const char* enableNames[] = { "Off", "On" };
const char* scalaMapNames[] = { "Keyb", "Cont" };
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
      "Midi Channel", MIXER_VALUE_MIDI_CHANNEL,
        {0, 16, 17, DISPLAY_TYPE_STRINGS, midiWithAll }
};

const struct Pfm3MixerButtonState rangeStartButtonState = {
      "First Midi Note", MIXER_VALUE_MIDI_FIRST_NOTE,
        {0, 127, 128, DISPLAY_TYPE_INT, nullNames }
};

const struct Pfm3MixerButtonState rangeEndButtonState = {
      "Last Midi Note", MIXER_VALUE_MIDI_LAST_NOTE,
        {0, 127, 128, DISPLAY_TYPE_INT, nullNames }
};

const struct Pfm3MixerButtonState rangeShiftButtonState = {
      "Midi Note Shift", MIXER_VALUE_MIDI_SHIFT_NOTE,
        {-63, 64, 128, DISPLAY_TYPE_INT, nullNames }
};




const struct Pfm3MixerButton midiButton = {
  "Midi",
  4,
  { &midiButtonState, &rangeStartButtonState, &rangeEndButtonState, &rangeShiftButtonState  }
};


const struct Pfm3MixerButtonState scalaEnable = {
      "Scala Enable", MIXER_VALUE_SCALA_ENABLE,
        {0, 1, 2, DISPLAY_TYPE_STRINGS, enableNames }
};


const struct Pfm3MixerButtonState scalaMap = {
      "Scala Mapping", MIXER_VALUE_SCALA_MAPPING,
        {0, SCALA_MAPPING_NUMBER_OF_OPTIONS - 1, SCALA_MAPPING_NUMBER_OF_OPTIONS, DISPLAY_TYPE_STRINGS, scalaMapNames }
};

const struct Pfm3MixerButtonState scalaScale = {
      "Scala Scale", MIXER_VALUE_SCALA_SCALE,
        {0, NUMBEROFSCALASCALEFILES-1, NUMBEROFSCALASCALEFILES, DISPLAY_TYPE_SCALA_SCALE, nullNames }
};

const struct Pfm3MixerButton scalaButton = {
  "Scala",
  3,
  { &scalaEnable, &scalaMap, &scalaScale }
};

// ==


#define NUMBER_OF_GLOBAL_SETTINGS 4
const struct Pfm3MixerButtonStateParam globalSettings[6] = {
   { 0, 16, 17, DISPLAY_TYPE_STRINGS, midiWithNone },
   { 0, 16, 17, DISPLAY_TYPE_STRINGS, midiWithNone },
   { 0, 1, 2, DISPLAY_TYPE_STRINGS, enableNames },
   { 420, 460, 401, DISPLAY_TYPE_FLOAT, nullNames },
   { 430, 450, 102, DISPLAY_TYPE_FLOAT, nullNames },
   { 430, 450, 102, DISPLAY_TYPE_FLOAT, nullNames }
};


const struct Pfm3MixerButtonState globalSettingsDetect = {
      "Global Options", MIXER_VALUE_GLOBAL_SETTINGS
};

const struct Pfm3MixerButton globalButton = {
  "Global",
  1,
  { &globalSettingsDetect}
};

// =

const struct PfmMixerMenu mixerMenu = {
      { &volumeButton, &outButton, &voiceButton, &midiButton, &scalaButton, &globalButton }
};



FMDisplayMixer::FMDisplayMixer() {
    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        valueChangedCounter[k] = 0;
    }
}


void* FMDisplayMixer::getValuePointer(int valueType, int encoder) {
    void* valueP = 0;
    switch (valueType) {
    case MIXER_VALUE_OUT:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].out;
        break;
    case MIXER_VALUE_COMPRESSOR:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].compressorType;
        break;
    case MIXER_VALUE_PAN:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].pan;
        break;
    case MIXER_VALUE_VOLUME:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].volume;
        break;
    case MIXER_VALUE_NUMBER_OF_VOICES:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].numberOfVoices;
        break;
    case MIXER_VALUE_MIDI_CHANNEL:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].midiChannel;
        break;
    case MIXER_VALUE_MIDI_FIRST_NOTE:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].firstNote;
        break;
    case MIXER_VALUE_MIDI_LAST_NOTE:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].lastNote;
        break;
    case MIXER_VALUE_MIDI_SHIFT_NOTE:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].shiftNote;
        break;
    case MIXER_VALUE_SCALA_ENABLE:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].scalaEnable;
        break;
    case MIXER_VALUE_SCALA_MAPPING:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].scalaMapping;
        break;
    case MIXER_VALUE_SCALA_SCALE:
        valueP = (void *) &this->synthState->mixerState.instrumentState[encoder].scaleScaleNumber;
        break;
    case MIXER_VALUE_GLOBAL_SETTINGS:
        switch (encoder) {
        case 0:
            valueP = (void *) &this->synthState->mixerState.globalChannel;
            break;
        case 1:
            valueP = (void *) &this->synthState->mixerState.currentChannel;
            break;
        case 2:
            valueP = (void *) &this->synthState->mixerState.midiThru;
            break;
        case 3:
            valueP = (void *) &this->synthState->mixerState.tuning;
            break;
        case 4:
            break;
        case 5:
            break;
        }
        break;
    }
    return valueP;
}

void FMDisplayMixer::displayMixerValueInteger(int timbre, int x, int value) {
    tft->setCursorInPixel(18 * TFT_BIG_CHAR_WIDTH, Y_MIXER + timbre * HEIGHT_MIXER_LINE);
    if (value >= 0 && value < 100 && timbre < 18) {
        tft->print(' ');
    }
    if (value > -10 && value < 10) {
        tft->print(' ');
    }
    tft->print(value);
}


void FMDisplayMixer::displayMixerValue(int timbre) {
    tft->setCharBackgroundColor(COLOR_BLACK);

    const Pfm3MixerButton* currentButton = mixerMenu.mixerButton[this->synthState->fullState.mixerCurrentEdit];
    bool isGlobalSettings = (currentButton->state[0]->mixerValueType == MIXER_VALUE_GLOBAL_SETTINGS);

    const Pfm3MixerButtonStateParam *buttonStateParam;
    void* valueP;
    uint8_t mixerValueType;

    if (!isGlobalSettings) {
        int buttonState = this->synthState->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + this->synthState->fullState.mixerCurrentEdit];
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
    if (!this->synthState->mixerState.instrumentState[timbre].scalaEnable
        && (mixerValueType == MIXER_VALUE_SCALA_MAPPING || mixerValueType == MIXER_VALUE_SCALA_SCALE)) {
        tft->setCharColor(COLOR_GRAY);
        tft->setCursorInPixel(18 * TFT_BIG_CHAR_WIDTH, Y_MIXER + timbre * HEIGHT_MIXER_LINE);
        tft->print("N/A");
        return;
    }


    switch (buttonStateParam->displayType) {
    case DISPLAY_TYPE_SCALA_SCALE: {
        // displayMixerValueInteger(timbre, 18, (*((uint8_t*)valueP)));
        const char * scalaName = this->synthState->getStorage()->getScalaFile()->getFile((*((uint8_t*)valueP)))->name;
        // memorize scala name
        // memcpy(this->synthState->mixerState.instrumentState[timbre].scalaScale, scalaName, 12); ??
        int len = getLength(scalaName);
        tft->setCursorInPixel(9 * TFT_BIG_CHAR_WIDTH, Y_MIXER + timbre * HEIGHT_MIXER_LINE);
        for (int s = 0; s < 13 - len; s++) {
            tft->print(' ');
        }
        for (int l = 0; l < len; l++) {
            tft->print(scalaName[l]);
        }


        break;
    }
    case DISPLAY_TYPE_INT:

        if (unlikely(mixerValueType == MIXER_VALUE_PAN)) {
            // Stereo is 1,4,7
            int out = this->synthState->mixerState.instrumentState[timbre].out;
            out--;
            if ((out % 3) != 0) {
                // Mono, no panning
                tft->setCharColor(COLOR_GRAY);
                tft->setCursorInPixel(20 * TFT_BIG_CHAR_WIDTH, Y_MIXER + timbre * HEIGHT_MIXER_LINE);
                tft->print('-');
                return;
            }
        }

        displayMixerValueInteger(timbre, 18, (*((int8_t*)valueP)));
        break;
    case DISPLAY_TYPE_STRINGS: {
        const char * label = buttonStateParam->valueName[(int)(*((uint8_t*)valueP))];
        int len = getLength(label);
        tft->setCursorInPixel(17 * TFT_BIG_CHAR_WIDTH, Y_MIXER + timbre * HEIGHT_MIXER_LINE);
        for (int s = 0; s < 4 - len; s++) {
            tft->print(' ');
        }
        tft->print(label);
        break;
    }
    case DISPLAY_TYPE_FLOAT: {
        float toDisplay = *((float*)valueP);
        tft->setCursorInPixel((toDisplay >= 100.0f ? 15 : 16) * TFT_BIG_CHAR_WIDTH, Y_MIXER + timbre * HEIGHT_MIXER_LINE);
        tft->print(toDisplay);
        break;
    }
    default:
        break;
    }

}

void FMDisplayMixer::refreshMixerByStep(int currentTimbre, int &refreshStatus, int &endRefreshStatus) {
    uint8_t buttonState = this->synthState->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + this->synthState->fullState.mixerCurrentEdit];
    uint8_t mixerValueType = mixerMenu.mixerButton[this->synthState->fullState.mixerCurrentEdit]->state[buttonState]->mixerValueType;
    bool isGlobalSettings = (mixerValueType == MIXER_VALUE_GLOBAL_SETTINGS);

    switch (refreshStatus) {
    case 20:
        tft->pauseRefresh();
        tft->clear();
        tft->fillArea(0, 0, 240, 21, COLOR_DARK_GREEN);
        tft->setCursorInPixel(2, 2);
        tft->print("MIX ", COLOR_YELLOW, COLOR_DARK_GREEN);
        tft->print(synthState->mixerState.mixName, COLOR_WHITE, COLOR_DARK_GREEN);
        break;
    case 19:
    case 18:
    case 17:
    case 16:
    case 15:
    case 14:
        // Display row : timbre number + preset name
        tft->setCharBackgroundColor(COLOR_BLACK);
        tft->setCursorInPixel(0, Y_MIXER + (19 - refreshStatus) * HEIGHT_MIXER_LINE);
        if ((currentTimbre != (19 - refreshStatus)) || isGlobalSettings)  {
            tft->setCharColor(COLOR_GRAY);
        } else {
            tft->setCharColor(COLOR_YELLOW);
        }
        tft->print(20 - refreshStatus);
        tft->setCharColor(COLOR_GRAY);

        if (!isGlobalSettings) {
            tft->print('-');
            tft->setCharColor(COLOR_GREEN);

            // scala scale file is a particular case
            // We don't display preset name to display scala scales
            if (mixerValueType != MIXER_VALUE_SCALA_SCALE) {
                tft->print(synthState->getTimbreName(19 - refreshStatus));
            }
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
        tft->setCharColor(COLOR_WHITE);
        displayMixerValue(12 - refreshStatus);
        break;
    }
    case 6: {
        uint8_t buttonState = this->synthState->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + this->synthState->fullState.mixerCurrentEdit];
        const char* valueTitle = mixerMenu.mixerButton[this->synthState->fullState.mixerCurrentEdit]->state[buttonState]->stateLabel;
        int len = getLength(valueTitle);
        tft->setCursorInPixel(240 - len * TFT_BIG_CHAR_WIDTH, 50);
        tft->setCharColor(COLOR_GREEN);
        tft->print(valueTitle);
        // NO BREAK ....
    }
    case 5:
    case 4:
    case 3:
    case 2:
    case 1: {
        uint8_t button = 6 - refreshStatus;

        const Pfm3MixerButton* currentButton = mixerMenu.mixerButton[button];

        tft->drawButton(currentButton->buttonLabel, 270, 29, button,
                currentButton->numberOfStates,
                synthState->fullState.mixerCurrentEdit == button ?
                        this->synthState->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + button] + 1 : 0,
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
    tft->setCharColor(COLOR_GRAY);
    tft->print(' ');


    switch (row) {
    case 0:
        tft->print("Global midi");
        break;
    case 1:
        tft->print("Current midi");
        break;
    case 2:
        tft->print("Midi thru");
        break;
    case 3:
        tft->print("Tuning");
        break;
    case 4:
        break;
    case 5:
        break;
    }
}



void FMDisplayMixer::newMixerValue(uint8_t mixerValue, uint8_t timbre, float oldValue, float newValue) {
    tft->setCharBackgroundColor(COLOR_BLACK);
    if (oldValue != newValue) {
        tft->setCharColor(COLOR_YELLOW);
    } else {
        tft->setCharColor(COLOR_RED);
    }
    displayMixerValue(timbre);
    valueChangedCounter[timbre] = 4;
}


void FMDisplayMixer::newMixerValueFromExternal(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) {
    const Pfm3MixerButton* currentButton = mixerMenu.mixerButton[this->synthState->fullState.mixerCurrentEdit];
    int buttonState = this->synthState->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + this->synthState->fullState.mixerCurrentEdit];

    if (currentButton->state[buttonState]->mixerValueType == valueType) {
        newMixerValue(valueType, timbre, oldValue, newValue);
    }
}


void FMDisplayMixer::tempoClick() {
    for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {
        if (valueChangedCounter[timbre] > 0) {
            valueChangedCounter[timbre]--;
            if (valueChangedCounter[timbre] == 0) {
                tft->setCharBackgroundColor(COLOR_BLACK);
                tft->setCharColor(COLOR_WHITE);
                displayMixerValue(timbre);
            }
        }

        if (this->synthState->mixerState.instrumentState[timbre].numberOfVoices > 0) {
            float volume = getCompInstrumentVolume(timbre);
            float gr = getCompInstrumentGainReduction(timbre);

            if (volume != lastVolume[timbre] || gr != lastGainReduction[timbre]) {
                // gr is negative
                int pixelPerDb = 5;
                float maxX = 240 + (volume + gr) * pixelPerDb;
                if (maxX < 0) {
                    maxX = 0;
                }
                tft->fillArea(0, Y_MIXER + timbre * HEIGHT_MIXER_LINE + 20, 240 , 5 , COLOR_BLACK);
                uint8_t visuColo = COLOR_GREEN;
                uint8_t visuColorAfterThresh = COLOR_LIGHT_GREEN;

                if (this->synthState->mixerState.instrumentState[timbre].compressorType > 0) {
                    // Saturation ?
                    if (maxX > 240) {
                        maxX = 240;
                        visuColo = COLOR_RED;
                        visuColorAfterThresh  = COLOR_RED ;
                    }

                    // With compressor
                    int afterThresh = 0;
                    int threashold = 12;
                    int threshInPixel = 240 - threashold * pixelPerDb;
                    if (maxX > threshInPixel) {
                        afterThresh = (maxX - threshInPixel);
                        maxX = threshInPixel;
                    }
                    tft->fillArea(0, Y_MIXER + timbre * HEIGHT_MIXER_LINE + 20, maxX , 5 , visuColo);
                    if (afterThresh > 0) {
                        tft->fillArea(threshInPixel +1 , Y_MIXER + timbre * HEIGHT_MIXER_LINE + 20, afterThresh , 5 , visuColorAfterThresh);
                        tft->fillArea(threshInPixel, Y_MIXER + timbre * HEIGHT_MIXER_LINE + 20, 1,  5, COLOR_BLACK);
                    } else {
                        if (maxX > 0) {
                            tft->fillArea(threshInPixel, Y_MIXER + timbre * HEIGHT_MIXER_LINE + 20, 1,  5, COLOR_LIGHT_GRAY);
                        }
                    }

                    if (gr < -.5) {
                        tft->fillArea(0, Y_MIXER + timbre * HEIGHT_MIXER_LINE + 20, - gr * 3 , 5 , COLOR_ORANGE);
                        tft->fillArea(- gr * 3 + 1, Y_MIXER + timbre * HEIGHT_MIXER_LINE + 20, 1 , 5 , COLOR_BLACK);
                    }
                } else {
                    // Without compressor
                    if (maxX > 240) {
                        maxX = 240;
                        visuColo = COLOR_RED;
                    }
                    tft->fillArea(0, Y_MIXER + timbre * HEIGHT_MIXER_LINE + 20, maxX , 5 , visuColo);
                }
            }

            lastVolume[timbre] = volume;
            lastGainReduction[timbre] = gr;

        }
    }
}




void FMDisplayMixer::encoderTurned(int encoder, int ticks) {

    const Pfm3MixerButton* currentButton = mixerMenu.mixerButton[this->synthState->fullState.mixerCurrentEdit];
    bool isGlobalSettings = (currentButton->state[0]->mixerValueType == MIXER_VALUE_GLOBAL_SETTINGS);
    bool isScalaButton = (currentButton->state[0]->mixerValueType == MIXER_VALUE_SCALA_ENABLE);

    const Pfm3MixerButtonStateParam *buttonStateParam;
    void* valueP;
    uint8_t mixerValueType;

    if (!isGlobalSettings) {
        int buttonState = this->synthState->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + this->synthState->fullState.mixerCurrentEdit];
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

//    uint8_t buttonState = this->synthState->fullState.buttonState[BUTTONID_MIXER_FIRST_BUTTON + this->synthState->fullState.mixerCurrentEdit];
//    uint8_t mixerValueType = currentButton->state[buttonState]->mixerValueType;
//    void* valueP = getValuePointer(mixerValueType, encoder);

    switch (buttonStateParam->displayType) {
    case DISPLAY_TYPE_INT:
    case DISPLAY_TYPE_STRINGS:
    case DISPLAY_TYPE_SCALA_SCALE:
    {
        int8_t oldValue = *((int8_t*) valueP);
        int newValue = oldValue + ticks;

        float maxValue;
        if (mixerValueType == MIXER_VALUE_NUMBER_OF_VOICES) {
            maxValue = MAX_NUMBER_OF_VOICES;
            for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
                if (t != encoder) {
                    maxValue -= this->synthState->mixerState.instrumentState[t].numberOfVoices;
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
                *((uint8_t*)valueP) = (uint8_t)newValue;
                if (!synthState->scalaSettingsChanged(encoder)) {
                    *((uint8_t*)valueP) = (uint8_t)oldValue;
                    newValue = oldValue;
                }
            } else {
                *((int8_t*)valueP) = (int8_t)newValue;
            }
        }
        synthState->propagateNewMixerValue(mixerValueType, encoder, oldValue, newValue);

        break;
    }
    case DISPLAY_TYPE_FLOAT: {
        float oldValue = *((float*)valueP);
        float inc = (buttonStateParam->maxValue - buttonStateParam->minValue) / (buttonStateParam->numberOfValues -1);
        float newValue = oldValue + ticks * inc;

        if (unlikely(newValue > buttonStateParam->maxValue)) {
            newValue = buttonStateParam->maxValue;
        } else if (unlikely(newValue < buttonStateParam->minValue)) {
            newValue = buttonStateParam->minValue;
        }

        if (oldValue != newValue) {
            *((float*)valueP) = (float)newValue;
        }
        synthState->propagateNewMixerValue(mixerValueType, encoder, oldValue, newValue);
        break;
    }
    default:
        break;
    }
}

void FMDisplayMixer::buttonPressed(int button) {
    struct FullState* fullState = &this->synthState->fullState;

    if (button >= BUTTON_PFM3_1 && button <= BUTTON_PFM3_6) {
        if (fullState->mixerCurrentEdit != button) {
            synthState->propagateNewMixerEdit(fullState->mixerCurrentEdit, button);
            fullState->mixerCurrentEdit = button;
        } else {
            int numberOfStates = mixerMenu.mixerButton[this->synthState->fullState.mixerCurrentEdit]->numberOfStates;

            if (numberOfStates > 1) {
                fullState->buttonState[BUTTONID_MIXER_FIRST_BUTTON + button]++;
                if (fullState->buttonState[BUTTONID_MIXER_FIRST_BUTTON + button] >= numberOfStates) {
                    fullState->buttonState[BUTTONID_MIXER_FIRST_BUTTON + button] = 0;
                }
                synthState->propagateNewMixerEdit(button, button);
            }
        }
    }
}


