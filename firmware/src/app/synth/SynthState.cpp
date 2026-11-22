/*
 * Copyright 2013 Xavier Hosxe
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

#include "SynthState.h"
#include "Hexter.h"
#include "engine/pfm3/Pfm3ParameterRows.h"
#include "ScreenEditor.h"
#include "ScreenMenu.h"
#include "ScreenMixer.h"
#include "ScreenSequencer.h"
#include "Synth.h"
#include "Timbre.h"
#include "preenfm3.h"

extern Synth synth;
extern RNG_HandleTypeDef hrng;

static Pfm3Preset backupPfm3PresetStorage_ __attribute__((section(".ram_d3")));

SynthState::SynthState() {
    operatorNumber_ = 0;
    backupPfm3Preset_ = &backupPfm3PresetStorage_;

    // First default preset
    fullState_.synthMode = SYNTH_MODE_MIXER;
    fullState_.preenFMBankNumber = 0;
    fullState_.preenFMPresetNumber = 0;
    fullState_.preenFMMixerNumber = 0;
    fullState_.preenFMMixerPresetNumber = 0;
    fullState_.dx7BankNumber = 0;
    fullState_.dx7PresetNumber = 0;
    // Default pfm3 edit page
    fullState_.mainPage = -1;
    fullState_.editPage = 0;

    // https://en.wikipedia.org/wiki/C_(musical_note)
    // Frequency of note 60 C4 :
    fullState_.midiConfigValue[MIDICONFIG_USB] = 2;
    fullState_.midiConfigValue[MIDICONFIG_RECEIVES] = 3;
    fullState_.midiConfigValue[MIDICONFIG_SENDS] = 1;
    fullState_.midiConfigValue[MIDICONFIG_PROGRAM_CHANGE] = 1;
    fullState_.midiConfigValue[MIDICONFIG_TEST_NOTE] = 60;
    fullState_.midiConfigValue[MIDICONFIG_TEST_VELOCITY] = 120;
    fullState_.midiConfigValue[MIDICONFIG_ENCODER] = 1;
    fullState_.midiConfigValue[MIDICONFIG_ARPEGGIATOR_IN_PRESET] = 1;
    fullState_.midiConfigValue[MIDICONFIG_CPU_USAGE] = 0;
    fullState_.midiConfigValue[MIDICONFIG_TFT_BACKLIGHT] = 100;
    fullState_.midiConfigValue[MIDICONFIG_TFT_AUTO_REINIT] = 0;
    fullState_.midiConfigValue[MIDICONFIG_ENCODER_PUSH] = 0;
    fullState_.midiConfigValue[MIDICONFIG_REVERB_PARAMS] = 0;
    // Init randomizer values to 1
    fullState_.randomizer.Oper = 1;
    fullState_.randomizer.EnvT = 1;
    fullState_.randomizer.IM = 1;
    fullState_.randomizer.Modl = 1;

    // Mixer
    fullState_.mixerCurrentEdit = 0;
    fullState_.menuCurrentEdit = 0;

    for (int k = 0; k < 12; k++) {
        fullState_.name[k] = 0;
    }

    // edit with timbre 0
    currentTimbre_ = 0;
    stepSelect_[0] = 0;
    stepSelect_[1] = 0;
    patternSelect_ = 0;

    isPlayingNote_ = false;

    for (int row = 0; row < NUMBER_OF_ROWS; row++) {
        for (int param = 0; param < NUMBER_OF_ENCODERS_PFM2; param++) {
            struct ParameterDisplay* paramDisplay = &(allParameterRows.row[row]->params[param]);
            if (paramDisplay->numberOfValues > 1.0) {
                paramDisplay->incValue = ((paramDisplay->maxValue - paramDisplay->minValue) / (paramDisplay->numberOfValues - 1.0f));
            } else {
                paramDisplay->incValue = 0.0f;
            }
        }
    }

    // Init mixer state with default values
    char mixerStateChars[sizeof(mixerState_)];
    uint32_t size;
    mixerState_.getFullDefaultState(mixerStateChars, &size, 0);
    mixerState_.restoreFullState(mixerStateChars);

    for (int b = 0; b < NUMBER_OF_BUTTONIDS; b++) {
        fullState_.buttonState[b] = 0;
    }
    fullState_.operatorNumber = 0;
}

void SynthState::init(ScreenMixer* displayMixer, ScreenEditor* displayEditor, ScreenMenu* displayMenu, ScreenSequencer* displaySequencer) {
    this->displayMixer_ = displayMixer;
    this->displayEditor_ = displayEditor;
    this->displayMenu_ = displayMenu;
    this->displaySequencer_ = displaySequencer;

    mixerState_.fxBus_.init();
}

void SynthState::encoderTurnedForStepSequencer(int row, int encoder4, int encoder6, int ticks) {
    int whichStepSeq = row - ROW_LFOSEQ1;
    StepSequencerSteps* seqSteps = &((StepSequencerSteps*)(&pfm3Preset_->lfoSteps1))[whichStepSeq];

    if (encoder6 == 3) {
        int oldPos = stepSelect_[whichStepSeq];
        int newValue = stepSelect_[whichStepSeq] + (ticks > 0 ? 1 : -1);

        if (newValue > 15) {
            newValue = 0;
        } else if (newValue < 0) {
            newValue = 15;
        }

        stepSelect_[whichStepSeq] = newValue;
        propagateNewPfm3ParamValue(currentTimbre_, row, encoder4, (ParameterDisplay*)NULL, oldPos, stepSelect_[whichStepSeq]);

    } else if (encoder6 == 4) {
        char* step = &seqSteps->steps[(int)stepSelect_[whichStepSeq]];
        int oldValue = (int)(*step);

        int newValue = (*step) + ticks;

        if (newValue > 15) {
            newValue = 15;
        } else if (newValue < 0) {
            newValue = 0;
        }
        if ((*step) != newValue) {
            (*step) = newValue;
            propagateNewPfm3ParamValue(currentTimbre_, row, encoder4, (ParameterDisplay*)NULL, oldValue, (int)(*step));
        }
    }
}

void SynthState::encoderTurnedForArpPattern(int row, int encoder, int ticks) {
    if (encoder == 0) {
        // Encoder 0: move cursor
        int oldPos = patternSelect_;
        int newPos = oldPos;
        newPos += (ticks > 0 ? 1 : -1);

        if (newPos > 15) {
            newPos = 0;
        } else if (newPos < 0) {
            newPos = 15;
        }
        if (newPos != oldPos) {
            patternSelect_ = newPos;
            propagateNewPfm3ParamValue(currentTimbre_, row, encoder, (ParameterDisplay*)NULL, oldPos, patternSelect_);
        }

    } else if (encoder == 1) {
        // Change value(s)
        if (pfm3Preset_->engineArp2.pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT) {
            return;
        }
        arp_pattern_t pattern =
            pfm3Preset_->engineArpUserPatterns.patterns[(int)pfm3Preset_->engineArp2.pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT];
        const uint16_t oldMask = ARP_PATTERN_GETMASK(pattern);
        uint16_t newMask = oldMask;

        uint16_t bitsToModify = 0;
        switch (encoder) {
        case 1:
            bitsToModify = 0x1 << patternSelect_;
            break; // modify single note
        case 2:
            bitsToModify = 0x1111 << (patternSelect_ & 3);
            break; // modify all
        case 3:
            bitsToModify = 0xf << ((patternSelect_ >> 2) << 2);
            break; // modify entire bar
        }
        if (ticks > 0) {
            newMask |= bitsToModify;
        } else {
            newMask &= ~bitsToModify;
        }

        if (oldMask != newMask) {
            ARP_PATTERN_SETMASK(pattern, newMask);
            pfm3Preset_->engineArpUserPatterns.patterns[(int)pfm3Preset_->engineArp2.pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT] = pattern;
            propagateNewPfm3ParamValue(currentTimbre_, row, encoder, (ParameterDisplay*)NULL, oldMask, newMask);
        }
    }
}

void SynthState::twoButtonsPressed(int button1, int button2) {
    switch (button1) {
    case BUTTON_PFM3_MENU:
        switch (button2) {
        case BUTTON_PFM3_1:
            propagateNoteOn(-2);
            break;
        case BUTTON_PFM3_2:
            propagateNoteOn(0);
            break;
        case BUTTON_PFM3_3:
            propagateNoteOn(2);
            break;
        case BUTTON_PFM3_4:
            propagateNoteOn(-14);
            break;
        case BUTTON_PFM3_5:
            propagateNoteOn(-12);
            break;
        case BUTTON_PFM3_6:
            propagateNoteOn(-10);
            break;
        case BUTTON_PREVIOUS_INSTRUMENT: {
            SynthEditMode previousMode = fullState_.synthMode;
            fullState_.synthMode = SYNTH_MODE_REINIT_TFT;
            propagateNewPfm3Page();
            fullState_.synthMode = previousMode;
            propagateNewPfm3Page();
            break;
        }
        case BUTTON_PFM3_MIXER:
            break;
        }
        break;
    case BUTTON_NEXT_INSTRUMENT:
        if (button2 == BUTTON_PREVIOUS_INSTRUMENT) {
            propagateNoteOff();
            for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
                propagateBeforeNewParamsLoad(t);
            }
            propagateAfterNewMixerLoad();
        }
        break;
    case BUTTON_PREVIOUS_INSTRUMENT:
        if (button2 == BUTTON_NEXT_INSTRUMENT) {
            propagateNoteOff();
            for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
                propagateBeforeNewParamsLoad(t);
            }
            propagateAfterNewMixerLoad();
        }
        break;
    }

#ifdef PPMIMAGE_ENABLE
    // Screenshot !!
    if (button1 == BUTTON_PFM3_MENU && button2 == BUTTON_NEXT_INSTRUMENT) {
        storage_->getPPMImage()->saveImage();
        propagateNewPfm3Page();
    }
#endif

#ifdef DEBUG__KO
    if (button1 == BUTTON_LFO) {
        if (button2 == BUTTON_MATRIX) {
            synth.debugVoice();
        }
        if (button2 == BUTTON_BACK) {
            synth.showCycles();
        }
        if (button2 == BUTTON_MENUSELECT) {
            storage->getPatchBank()->testMemoryPreset();
        }
    }
#endif
}

void SynthState::encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) {
    if (button == BUTTON_NEXT_INSTRUMENT) {
        encoderTurned(encoder, ticks * 10);
        return;
    }

    if (fullState_.synthMode == SYNTH_MODE_EDITOR) {
        displayEditor_->encoderTurnedWhileButtonPressed(encoder, ticks, button);
    } else if (fullState_.synthMode == SYNTH_MODE_MIXER) {
        displayMixer_->encoderTurnedWhileButtonPressed(encoder, ticks, button);
    } else if (fullState_.synthMode == SYNTH_MODE_SEQUENCER) {
        displaySequencer_->encoderTurnedWhileButtonPressed(encoder, ticks, button);
    }
}

bool SynthState::newRandomizerValue(int encoder, int ticks) {
    int8_t oldValue = 0;
    int8_t newValue = 0;
    switch (encoder) {
    case 0:
        oldValue = fullState_.randomizer.Oper;
        fullState_.randomizer.Oper += (ticks > 0 ? 1 : -1);
        fullState_.randomizer.Oper = fullState_.randomizer.Oper > 3 ? 3 : fullState_.randomizer.Oper;
        fullState_.randomizer.Oper = fullState_.randomizer.Oper < 0 ? 0 : fullState_.randomizer.Oper;
        newValue = fullState_.randomizer.Oper;
        break;
    case 1:
    case 2:
        oldValue = fullState_.randomizer.EnvT;
        fullState_.randomizer.EnvT += (ticks > 0 ? 1 : -1);
        fullState_.randomizer.EnvT = fullState_.randomizer.EnvT > 3 ? 3 : fullState_.randomizer.EnvT;
        fullState_.randomizer.EnvT = fullState_.randomizer.EnvT < 0 ? 0 : fullState_.randomizer.EnvT;
        newValue = fullState_.randomizer.EnvT;
        break;
    case 3:
        oldValue = fullState_.randomizer.IM;
        fullState_.randomizer.IM += (ticks > 0 ? 1 : -1);
        fullState_.randomizer.IM = fullState_.randomizer.IM > 3 ? 3 : fullState_.randomizer.IM;
        fullState_.randomizer.IM = fullState_.randomizer.IM < 0 ? 0 : fullState_.randomizer.IM;
        newValue = fullState_.randomizer.IM;
        break;
    case 4:
    case 5:
        oldValue = fullState_.randomizer.Modl;
        fullState_.randomizer.Modl += (ticks > 0 ? 1 : -1);
        fullState_.randomizer.Modl = fullState_.randomizer.Modl > 3 ? 3 : fullState_.randomizer.Modl;
        fullState_.randomizer.Modl = fullState_.randomizer.Modl < 0 ? 0 : fullState_.randomizer.Modl;
        newValue = fullState_.randomizer.Modl;
        break;
    }
    return newValue != oldValue;
}

void SynthState::encoderTurned(int encoder, int ticks) {
    switch (fullState_.synthMode) {
    case SYNTH_MODE_EDITOR:
        displayEditor_->encoderTurned(encoder, ticks);
        break;
    case SYNTH_MODE_MENU:
        displayMenu_->encoderTurned(currentTimbre_, encoder, ticks);
        // Did we modify reverb visibility ?
        if (unlikely(fullState_.currentMenuItem->menuState == MENU_CONFIG_SETTINGS && encoder == 3 &&
                     fullState_.menuSelect == MIDICONFIG_REVERB_PARAMS)) {
            displayMixer_->setReverbParamVisible(fullState_.midiConfigValue[MIDICONFIG_REVERB_PARAMS] > 0);
        }
        break;
    case SYNTH_MODE_MIXER:
        displayMixer_->encoderTurned(encoder, ticks);
        break;
    case SYNTH_MODE_SEQUENCER: {
        if (encoder == 2) {
            setCurrentInstrument(ticks > 0 ? 0 : -1);
        }
        displaySequencer_->encoderTurned(currentTimbre_, encoder, ticks);
        break;
    }
    }
}

void SynthState::loadNewPreset(int timbre) {
    storeTestNote();
    propagateNoteOff();
    propagateBeforeNewParamsLoad(timbre);
    storage_->getPatchBank()->copyNewPreset(pfm3Preset_);
    propagateAfterNewParamsLoad(timbre);
    restoreTestNote();
}

void SynthState::loadPreset(int timbre, PFM3File const* bank, int patchNumber, struct Pfm3Preset* params) {
    storeTestNote();
    propagateNoteOff();
    propagateBeforeNewParamsLoad(timbre);

    bool loaded = storage_->getPatchBank()->loadPatch(bank, patchNumber, &timbres_[timbre]);

    if (loaded) {
        mixerState_.instrumentState_[timbre].engine = timbres_[timbre].getEngine();
    }

    propagateAfterNewParamsLoad(timbre);
    restoreTestNote();
}

void SynthState::savePreset(int timbre, PFM3File const* bank, int patchNumber, struct Pfm3Preset* params) {
    if (params != nullptr) {
        // The save dialog edits presetName in the Pfm3Preset mirror; keep the
        // persisted name in sync for every engine.
        timbres_[timbre].setPresetName(params->presetName);
    }
    storage_->getPatchBank()->savePatch(bank, patchNumber, &timbres_[timbre]);
}

void SynthState::loadDx7Patch(int timbre, PFM3File const* bank, int patchNumber, struct Pfm3Preset* params) {
    storeTestNote();
    propagateNoteOff();
    propagateBeforeNewParamsLoad(timbre);

    uint8_t* packed = storage_->getDX7SysexFile()->dx7LoadPatch(bank, patchNumber);
    if (packed != nullptr) {
        // DX7 sysex patches always switch the target timbre to DX7.
        mixerState_.instrumentState_[timbre].engine = SYNTH_ENGINE_DX7;
        timbres_[timbre].setEngine(SYNTH_ENGINE_DX7);

        // A DX7 sysex carries no arpeggiator/FX settings. Reset the shared
        // params (and the PFM3 mirror) so the new instrument does not inherit
        // the previous instrument's FX/arp.
        timbres_[timbre].applyPfm3Preset(preenMainPreset);

        timbres_[timbre].loadDx7Patch(packed);
        // Keep PFM3 params in sync for display/backward compatibility.
        hexter_->loadHexterPatch(packed, params);
        if (params != nullptr) {
            // The DX7 program name must also be the persisted name (preset
            // and mixer storage read the shared params).
            timbres_[timbre].setPresetName(params->presetName);
        }
    }

    propagateAfterNewParamsLoad(timbre);
    restoreTestNote();
}

void SynthState::loadMixer(PFM3File const* bank, int patchNumber) {
    propagateBeforeNewParamsLoad(currentTimbre_);
    storage_->getMixerBank()->loadMixer(bank, patchNumber);
    // Update and clean all timbres
    this->currentTimbre_ = 0;
    propagateNewTimbre(currentTimbre_);
    propagateAfterNewMixerLoad();
}

void SynthState::loadPresetFromMidi(int timbre, int bank, int bankLSB, int patchNumber, struct Pfm3Preset* params) {
    switch (bank) {
    case 0: {
        PFM3File const* bank = storage_->getPatchBank()->getFile(bankLSB);
        if (bank->fileType != FILE_EMPTY) {
            fullState_.preenFMBankNumber = bankLSB;
            fullState_.preenFMPresetNumber = patchNumber;
            loadPreset(timbre, bank, patchNumber, params);
        }
        break;
    }
    case 1: {
        PFM3File const* bank = storage_->getMixerBank()->getFile(bankLSB);
        if (bank->fileType != FILE_EMPTY) {
            fullState_.preenFMMixerNumber = bankLSB;
            fullState_.preenFMMixerPresetNumber = patchNumber;
            loadMixer(bank, patchNumber);
        }
        break;
    }
    case 2:
    case 3:
    case 4: {
        int dx7bank = bank - 2;
        PFM3File const* bank = storage_->getDX7SysexFile()->getFile(bankLSB + dx7bank * 128);
        if (bank->fileType != FILE_EMPTY) {
            fullState_.dx7BankNumber = bankLSB;
            fullState_.dx7PresetNumber = patchNumber;
            loadDx7Patch(timbre, bank, patchNumber, params);
        }
        break;
    }
    }
}

void SynthState::setCurrentInstrument(int value) {
    if (value == -1) {
        currentTimbre_ = currentTimbre_ - 1;
        if (unlikely(currentTimbre_ < 0)) {
            currentTimbre_ = NUMBER_OF_TIMBRES - 1;
        }
    } else if (value == 0) {
        currentTimbre_ = (currentTimbre_ + 1) % NUMBER_OF_TIMBRES;
    } else if (value <= (NUMBER_OF_TIMBRES + 1)) {
        currentTimbre_ = value - 1;
    } else {
        return;
    }
    propagateNewTimbre(currentTimbre_);
}

void SynthState::buttonLongPressed(int button) {
    switch (fullState_.synthMode) {
    case SYNTH_MODE_SEQUENCER:
        displaySequencer_->buttonLongPressed(currentTimbre_, button);
        break;
    case SYNTH_MODE_EDITOR:
        displayEditor_->buttonLongPressed(currentTimbre_, button);
        break;
    }
}

void SynthState::buttonPressed(int button) {
    SynthEditMode oldSynthMode = fullState_.synthMode;

    bool swithToMenuIfBackButtonPressed = true;

    if (fullState_.synthMode == SYNTH_MODE_EDITOR) {
        if (fullState_.mainPage != -1) {
            // Back must not go to MENU mode if not in the main page
            swithToMenuIfBackButtonPressed = false;
        }
        displayEditor_->buttonPressed(button);
    } else if (fullState_.synthMode == SYNTH_MODE_MENU) {
        displayMenu_->buttonPressed(currentTimbre_, button);
        swithToMenuIfBackButtonPressed = false;
    } else if (fullState_.synthMode == SYNTH_MODE_MIXER) {
        displayMixer_->buttonPressed(button);
    } else if (fullState_.synthMode == SYNTH_MODE_SEQUENCER) {
        if (displaySequencer_->getSequencerMode() != SEQ_MODE_NORMAL && displaySequencer_->getSequencerMode() != SEQ_MODE_STEP) {
            swithToMenuIfBackButtonPressed = false;
        }
        displaySequencer_->buttonPressed(currentTimbre_, button);
    }

    // Anywhere these are main functions !!!!
    switch (button) {
    case BUTTON_PFM3_EDIT:
        fullState_.synthMode = SYNTH_MODE_EDITOR;
        break;
    case BUTTON_PFM3_SEQUENCER:
        fullState_.synthMode = SYNTH_MODE_SEQUENCER;
        break;
    case BUTTON_PFM3_MIXER:
        fullState_.synthMode = SYNTH_MODE_MIXER;
        break;
    case BUTTON_PFM3_MENU:
        if (swithToMenuIfBackButtonPressed) {
            if (fullState_.synthModeBeforeMenu != SYNTH_MODE_MENU) {
                fullState_.synthModeBeforeMenu = fullState_.synthMode;
            }
            // Make sure to propagate synth mode
            oldSynthMode = SYNTH_MODE_EDITOR;
            fullState_.synthMode = SYNTH_MODE_MENU;
            fullState_.previousChoice = fullState_.previousMenuChoice.main;
            fullState_.currentMenuItem = MenuItemUtil::getMenuItem(MAIN_MENU);
            propagateNewSynthMode();
            return;
        }
        break;
    case BUTTON_PREVIOUS_INSTRUMENT:
        // select next instrument as current one
        if (fullState_.synthMode != SYNTH_MODE_MENU) {
            setCurrentInstrument(-1);
        }
        break;
    case BUTTON_NEXT_INSTRUMENT:
        // select next instrument as current one
        if (fullState_.synthMode != SYNTH_MODE_MENU) {
            setCurrentInstrument(0);
        }
        break;
    case BUTTON_ENCODER_1:
    case BUTTON_ENCODER_2:
    case BUTTON_ENCODER_3:
    case BUTTON_ENCODER_4:
    case BUTTON_ENCODER_5:
    case BUTTON_ENCODER_6:
        setCurrentInstrument(button - BUTTON_ENCODER_1 + 1);
        break;
    }

    if (oldSynthMode != fullState_.synthMode) {
        propagateNewSynthMode();
        return;
    }
}

void SynthState::propagateAfterNewParamsLoad(int timbre) {
    for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
        listener->afterNewParamsLoad(timbre);
    }
}

void SynthState::propagateAfterNewMixerLoad() {
    for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
        listener->afterNewMixerLoad();
    }
}

void SynthState::propagateNewTimbre(int timbre) {
    propagateNoteOff();
    for (SynthParamListener* listener = firstParamListener_; listener != 0; listener = listener->nextListener_) {
        listener->newTimbre(timbre);
    }
}

void SynthState::tempoClick() {
    if (fullState_.synthMode == SYNTH_MODE_MENU) {
        if (fullState_.currentMenuItem->menuType == MENUTYPE_TEMPORARY) {
            if (doneClick_ > 4) {
                fullState_.synthMode = fullState_.synthModeBeforeMenu;
                propagateNewSynthMode();
            }
            doneClick_++;
        }
    } else {
        doneClick_ = 0;
    }
}

void SynthState::setParamsAndTimbre(struct Pfm3Preset* newParams, int newCurrentTimbre) {
    this->pfm3Preset_ = newParams;
    this->currentTimbre_ = newCurrentTimbre;
}

/*
 * Randomizer
 */

int getRandomInt(int max) {
    uint32_t rnd;
    HAL_RNG_GenerateRandomNumber(&hrng, &rnd);
    return rnd % max;
}

float getRandomFloat(float min, float max) {
    uint32_t rnd;
    HAL_RNG_GenerateRandomNumber(&hrng, &rnd);
    float f = ((float)(rnd % 100000)) / 100000.0f;
    return f * (max - min) + min;
}

float getRandomShape(int operatorRandom) {
    int shape = getRandomInt(7);
    switch (operatorRandom) {
    case 1:
        if (shape > 2) {
            shape = 0;
        }
        break;
    case 2:
        shape = getRandomInt(8);
        if (shape == 6) { // Rand
            shape = 0;
        }
        break;
    case 3:
        break;
    }
    return shape;
}

float getRandomFrequencyType(int operatorRandom) {
    int freqType = getRandomInt(32);
    if (freqType > 1) { // Keyboard 5 times out of 6
        freqType = 0;
    }
    return freqType;
}

float getRandomFrequency(int operatorRandom) {
    float random1Frequency[] = {.5f, 1.0f, 2.0f, 4.0f};
    float random2Frequency[] = {.25, .5f, 1.0f, 1.5, 2.0f, 3.0f, 4.0f};
    float freq = 0;
    switch (operatorRandom) {
    case 1:
        freq = random1Frequency[getRandomInt(4)];
        break;
    case 2:
        freq = random2Frequency[getRandomInt(7)];
        break;
    case 3:
        freq = getRandomInt(24) * .25 + .25;
        break;
    }
    return freq;
}

float getFineTune(int operatorRandom) {
    float fineTune = 0;
    switch (operatorRandom) {
    case 2:
        fineTune = getRandomInt(10) * .01 - .05;
        if (fineTune < 0.03 || fineTune > 0.03) {
            fineTune = 0;
        }
        break;
    case 3:
        fineTune = getRandomInt(20) * .01 - .05;
        if (fineTune < 0.07 || fineTune > 0.07) {
            fineTune = 0;
        }
        break;
    }
    return fineTune;
}

void SynthState::randomizePreset() {
    int operatorRandom = fullState_.randomizer.Oper;
    int envelopeTypeRandom = fullState_.randomizer.EnvT;
    int imRandom = fullState_.randomizer.IM;
    int modulationRandom = fullState_.randomizer.Modl;

    // general

    pfm3Preset_->engine1.velocity = 8;

    if (operatorRandom > 0) {
        pfm3Preset_->engineMix1.mixOsc1 = getRandomFloat(0.6f, 1.0f);
        pfm3Preset_->engineMix1.mixOsc2 = getRandomFloat(0.6f, 1.0f);
        pfm3Preset_->engineMix2.mixOsc3 = getRandomFloat(0.6f, 1.0f);
        pfm3Preset_->engineMix2.mixOsc4 = getRandomFloat(0.6f, 1.0f);
        pfm3Preset_->engineMix3.mixOsc5 = getRandomFloat(0.6f, 1.0f);
        pfm3Preset_->engineMix3.mixOsc6 = getRandomFloat(0.6f, 1.0f);

        pfm3Preset_->engineMix1.panOsc1 = 0.0;
        pfm3Preset_->engineMix1.panOsc2 = getRandomFloat(-0.3f, 0.3f);
        pfm3Preset_->engineMix2.panOsc3 = getRandomFloat(-0.3f, 0.3f);
        pfm3Preset_->engineMix2.panOsc4 = getRandomFloat(-0.5f, 0.5f);
        pfm3Preset_->engineMix3.panOsc5 = getRandomFloat(-0.5f, 0.5f);
        pfm3Preset_->engineMix3.panOsc6 = getRandomFloat(-0.7f, 0.7f);

        pfm3Preset_->engine1.algo = getRandomInt(ALGO_END);

        for (int o = 0; o < 6; o++) {
            struct OscillatorParams* currentOsc = &((struct OscillatorParams*)&pfm3Preset_->osc1)[o];
            currentOsc->shape = getRandomShape(operatorRandom);
            currentOsc->frequencyMul = getRandomFrequency(operatorRandom);
            currentOsc->frequencyType = getRandomFrequencyType(operatorRandom);
            currentOsc->detune = getFineTune(operatorRandom);
        }

        // FX
        pfm3Preset_->effect1.param1 = getRandomFloat(0.2f, 0.8f);
        pfm3Preset_->effect1.param2 = getRandomFloat(0.2f, 0.8f);
        int effect = getRandomInt(15);
        if (effect == 1 || effect > 6) {
            pfm3Preset_->effect1.param3 = 1.0;
            pfm3Preset_->effect1.type = 0;
        } else {
            pfm3Preset_->effect1.param3 = 0.6;
            pfm3Preset_->effect1.type = effect;
        }
    }

    for (int e = 0; e < 6; e++) {
        struct EnvelopeParamsA* enva = &((struct EnvelopeParamsA*)&pfm3Preset_->env1Time)[e * 2];
        struct EnvelopeParamsB* envb = &((struct EnvelopeParamsB*)&pfm3Preset_->env1Level)[e * 2];

        switch (envelopeTypeRandom) {
        case 1:
            enva->attackLevel = 1.0;
            enva->attackTime = getRandomFloat(0, 0.3f);
            enva->decayLevel = getRandomFloat(0.5f, 1.0f);
            enva->decayTime = getRandomFloat(0.05, 0.5f);

            envb->sustainLevel = getRandomFloat(0.0f, 1.0f);
            envb->sustainTime = getRandomFloat(0.02, 1.0f);
            envb->releaseLevel = 0.0f;
            envb->releaseTime = getRandomFloat(0.2, 5.0f);
            ;

            break;
        case 2:
            enva->attackLevel = getRandomFloat(0.25f, 1.0f);
            enva->attackTime = getRandomFloat(0.5f, 3.0f);
            enva->decayLevel = getRandomFloat(0.5f, 1.0f);
            enva->decayTime = getRandomFloat(1.0f, 5.0f);

            envb->sustainLevel = getRandomFloat(0.0f, 1.0f);
            envb->sustainTime = getRandomFloat(2.0f, 5.0f);
            envb->releaseLevel = 0.0f;
            envb->releaseTime = getRandomFloat(1.0f, 8.0f);
            break;
        case 3:
            enva->attackLevel = getRandomFloat(0, 1.0f);
            enva->attackTime = getRandomFloat(0, 1.0f);
            enva->decayLevel = getRandomFloat(0, 1.0f);
            enva->decayTime = getRandomFloat(0, 1.0f);

            envb->sustainLevel = getRandomFloat(0, 1.0f);
            envb->sustainTime = getRandomFloat(0, 1.0f);
            envb->releaseLevel = getRandomFloat(0, 1.0f);
            envb->releaseTime = getRandomFloat(0, 4.0f);
            break;
        }
    }

    if (imRandom > 0) {
        struct EngineIm1* im1 = (struct EngineIm1*)&pfm3Preset_->engineIm1;
        struct EngineIm2* im2 = (struct EngineIm2*)&pfm3Preset_->engineIm2;
        struct EngineIm3* im3 = (struct EngineIm3*)&pfm3Preset_->engineIm3;

        float min = 0;
        float max = 0;
        float minVelo = 0;
        float maxVelo = 0;

        switch (imRandom) {
        case 1:
            min = 0.25f;
            max = 2.0f;
            minVelo = 0.2f;
            maxVelo = 1.0f;
            break;
        case 2:
            min = .5f;
            max = 3.0f;
            minVelo = 1.0f;
            maxVelo = 2.0f;
            break;
        case 3:
            min = 1.0f;
            max = 5.0f;
            minVelo = 1.0f;
            maxVelo = 4.0f;
            break;
        }
        im1->modulationIndex1 = getRandomFloat(min, max);
        im1->modulationIndexVelo1 = getRandomFloat(minVelo, maxVelo);
        im1->modulationIndex2 = getRandomFloat(min, max);
        im1->modulationIndexVelo2 = getRandomFloat(minVelo, maxVelo);
        im2->modulationIndex3 = getRandomFloat(min, max);
        im2->modulationIndexVelo3 = getRandomFloat(minVelo, maxVelo);
        im2->modulationIndex4 = getRandomFloat(min, max);
        im2->modulationIndexVelo4 = getRandomFloat(minVelo, maxVelo);
        im3->modulationIndex5 = getRandomFloat(min, max);
        im3->modulationIndexVelo5 = getRandomFloat(minVelo, maxVelo);
    }

    if (modulationRandom > 0) {

        pfm3Preset_->matrixRowState1.source = MATRIX_SOURCE_LFO1;
        pfm3Preset_->matrixRowState2.source = MATRIX_SOURCE_LFO1;
        pfm3Preset_->matrixRowState3.source = MATRIX_SOURCE_LFO2;
        pfm3Preset_->matrixRowState4.source = MATRIX_SOURCE_LFO2;
        pfm3Preset_->matrixRowState5.source = MATRIX_SOURCE_LFO3;
        pfm3Preset_->matrixRowState6.source = MATRIX_SOURCE_LFOENV1;
        pfm3Preset_->matrixRowState7.source = MATRIX_SOURCE_LFOENV2;
        pfm3Preset_->matrixRowState8.source = MATRIX_SOURCE_LFOSEQ1;
        pfm3Preset_->matrixRowState9.source = MATRIX_SOURCE_LFOSEQ2;

        pfm3Preset_->matrixRowState10.source = MATRIX_SOURCE_MODWHEEL;
        pfm3Preset_->matrixRowState10.mul = 2.0f;
        pfm3Preset_->matrixRowState10.dest1 = INDEX_ALL_MODULATION;

        pfm3Preset_->matrixRowState11.source = MATRIX_SOURCE_PITCHBEND;
        pfm3Preset_->matrixRowState11.mul = 1.0f;
        pfm3Preset_->matrixRowState11.dest1 = ALL_OSC_FREQ;

        pfm3Preset_->matrixRowState12.source = MATRIX_SOURCE_AFTERTOUCH;
        pfm3Preset_->matrixRowState12.mul = 1.0f;
        pfm3Preset_->matrixRowState12.dest1 = INDEX_MODULATION1;

        for (int m = 1; m <= 9; m++) {
            struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*)&pfm3Preset_->matrixRowState1)[m - 1];
            matrixRow->mul = 0;
            matrixRow->dest1 = 0;
        }

        float dest[] = {INDEX_ALL_MODULATION, OSC1_FREQ, ALL_PAN, OSC2_FREQ, INDEX_MODULATION1, PAN_OSC1, INDEX_ALL_MODULATION};
        for (int i = 0; i < 2; i++) {
            struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*)&pfm3Preset_->matrixRowState1)[getRandomInt(10)];
            matrixRow->mul = getRandomFloat(0.2f, 3.0f);
            matrixRow->dest1 = dest[getRandomInt(7)];
        }

        if (modulationRandom >= 2) {
            for (int i = 0; i < 3; i++) {
                struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*)&pfm3Preset_->matrixRowState1)[getRandomInt(10)];
                matrixRow->mul = getRandomFloat(1.0f, 3.0f);
                matrixRow->dest1 = getRandomInt(DESTINATION_MAX);
            }
        }

        if (modulationRandom >= 3) {
            for (int i = 0; i < 6; i++) {
                struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*)&pfm3Preset_->matrixRowState1)[getRandomInt(10)];
                float mm = getRandomFloat(2.0f, 5.0f);
                matrixRow->mul = getRandomFloat(-mm, mm);
                matrixRow->dest1 = getRandomInt(DESTINATION_MAX);
            }
        }

        for (int o = 0; o < 3; o++) {
            struct LfoParams* osc = &((struct LfoParams*)&pfm3Preset_->lfoOsc1)[o];
            osc->shape = getRandomInt(5);
            osc->freq = getRandomFloat(0.2, 3 + modulationRandom * 2);
            if (getRandomInt(4) > 1) {
                osc->bias = 0;
            } else {
                osc->bias = getRandomFloat(-1.0f, 1.0f);
            }
            // PAD
            if (envelopeTypeRandom == 2) {
                osc->keybRamp = getRandomFloat(0.0f, 4.0f);
            } else {
                osc->keybRamp = getRandomFloat(0.0f, 1.0f);
            }
        }
        for (int e = 0; e < 2; e++) {
            struct EnvelopeLfoParams* env = &((struct EnvelopeLfoParams*)&pfm3Preset_->lfoEnv1)[e];
            env->attack = getRandomFloat(0.05f, 1.0f);
            env->decay = getRandomFloat(0.05f, 1.0f);
            env->sustain = getRandomFloat(0.05f, 1.0f);
            if (e == 0) {
                env->release = getRandomFloat(0.05f, 1.0f);
            } else {
                pfm3Preset_->lfoEnv2.loop = getRandomInt(2) + 1;
            }
        }

        int bpm = getRandomInt(120) + 60;
        for (int s = 0; s < 2; s++) {
            struct StepSequencerParams* stepSeq = &((struct StepSequencerParams*)&pfm3Preset_->lfoSeq1)[s];
            stepSeq->bpm = bpm;
            stepSeq->gate = getRandomFloat(0.25, 1);

            struct StepSequencerSteps* steps = &((struct StepSequencerSteps*)&pfm3Preset_->lfoSteps1)[s];
            for (int k = 0; k < 16; k++) {
                if ((k % 4) == 0) {
                    steps->steps[k] = getRandomInt(5) + 11;
                } else {
                    steps->steps[k] = getRandomInt(10);
                }
            }
        }
    }
}

Timbre* SynthState::getTimbre(int timbre) {
    return &timbres_[timbre];
}

char* SynthState::getTimbreName(int t) {
    return timbres_[t].getPresetName();
}

uint8_t SynthState::getTimbrePlayMode(int t) {
    return timbres_[t].getPfm3Preset()->engine1.playMode;
}

/*
 * When a scala scale file does not exist anymore ?
 * If both number and name are wrong, scala is disable
 */
bool SynthState::scalaSettingsChanged(int timbre) {
    if (!mixerState_.instrumentState_[timbre].scalaEnable) {
        mixerState_.instrumentState_[timbre].scaleFrequencies = diatonicScaleFrequency;
    } else {
        const struct PFM3File* scalaFile = storage_->getScalaFile()->getFile(mixerState_.instrumentState_[timbre].scaleScaleNumber);
        if (scalaFile->fileType == FILE_EMPTY) {
            // Lest file
            return false;
        }
        // Let's fill scala Name (used in loadScalaScale)
        for (int c = 0; c < 12; c++) {
            mixerState_.instrumentState_[timbre].scalaScaleFileName[c] = scalaFile->name[c];
        }
        float* newScaleFrequencies = storage_->getScalaFile()->loadScalaScale(&mixerState_, timbre);
        if (newScaleFrequencies != 0) {
            mixerState_.instrumentState_[timbre].scaleFrequencies = newScaleFrequencies;
        } else {
            return false;
        }
    }
    return true;
}

const char* SynthState::getSequenceName() {
    return displaySequencer_->getSequenceName();
}
