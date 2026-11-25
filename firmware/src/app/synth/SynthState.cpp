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
#include "ScreenEditor.h"
#include "ScreenMenu.h"
#include "ScreenMixer.h"
#include "ScreenSequencer.h"
#include "Synth.h"
#include "Timbre.h"
#include "preenfm3.h"

extern Synth synth;
extern RNG_HandleTypeDef hrng;

SynthState::SynthState() {
    operatorNumber_ = 0;

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

    // https://en.wikipedia.org/wiki/C_(musical_note)
    // Frequency of note 60 C4 :
    fullState_.midiConfigValue[MIDICONFIG_USB] = 2;
    fullState_.midiConfigValue[MIDICONFIG_USB_AUDIO] = 1;
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

    isPlayingNote_ = false;

    // Init mixer state with default values
    char mixerStateChars[sizeof(mixerState_)];
    uint32_t size;
    mixerState_.getFullDefaultState(mixerStateChars, &size, 0);
    mixerState_.restoreFullState(mixerStateChars);

    for (int b = 0; b < NUMBER_OF_BUTTONIDS; b++) {
        fullState_.buttonState[b] = 0;
    }
}

void SynthState::init(ScreenMixer* displayMixer, ScreenEditor* displayEditor, ScreenMenu* displayMenu, ScreenSequencer* displaySequencer) {
    this->displayMixer_ = displayMixer;
    this->displayEditor_ = displayEditor;
    this->displayMenu_ = displayMenu;
    this->displaySequencer_ = displaySequencer;

    mixerState_.fxBus_.init();
}

void SynthState::twoButtonsPressed(int button1, int button2) {
    switch (button1) {
    case BUTTON_SWITCH_MENU:
        switch (button2) {
        case BUTTON_SWITCH_1:
            propagateNoteOn(-2);
            break;
        case BUTTON_SWITCH_2:
            propagateNoteOn(0);
            break;
        case BUTTON_SWITCH_3:
            propagateNoteOn(2);
            break;
        case BUTTON_SWITCH_4:
            propagateNoteOn(-14);
            break;
        case BUTTON_SWITCH_5:
            propagateNoteOn(-12);
            break;
        case BUTTON_SWITCH_6:
            propagateNoteOn(-10);
            break;
        case BUTTON_PREVIOUS_INSTRUMENT: {
            SynthEditMode previousMode = fullState_.synthMode;
            fullState_.synthMode = SYNTH_MODE_REINIT_TFT;
            propagateNewScreenPage();
            fullState_.synthMode = previousMode;
            propagateNewScreenPage();
            break;
        }
        case BUTTON_SWITCH_MIXER:
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
    if (button1 == BUTTON_SWITCH_MENU && button2 == BUTTON_NEXT_INSTRUMENT) {
        storage_->getPPMImage()->saveImage();
        propagateNewScreenPage();
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
    default:
        break;
    }
}

void SynthState::configValueChanged() {
    displayMixer_->setReverbParamVisible(fullState_.midiConfigValue[MIDICONFIG_REVERB_PARAMS] > 0);
}

void SynthState::loadNewPreset(int timbre) {
    storeTestNote();
    propagateNoteOff();
    propagateBeforeNewParamsLoad(timbre);
    timbres_[timbre].loadNewPreset();
    propagateAfterNewParamsLoad(timbre);
    restoreTestNote();
}

void SynthState::loadPreset(int timbre, PFM3File const* bank, int patchNumber) {
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

void SynthState::savePreset(int timbre, PFM3File const* bank, int patchNumber) {
    storage_->getPatchBank()->savePatch(bank, patchNumber, &timbres_[timbre]);
}

void SynthState::loadDx7Patch(int timbre, PFM3File const* bank, int patchNumber) {
    storeTestNote();
    propagateNoteOff();
    propagateBeforeNewParamsLoad(timbre);

    uint8_t* packed = storage_->getDX7SysexFile()->dx7LoadPatch(bank, patchNumber);
    if (packed != nullptr) {
        // DX7 sysex patches always switch the target timbre to DX7.
        mixerState_.instrumentState_[timbre].engine = SYNTH_ENGINE_DX7;
        timbres_[timbre].setEngine(SYNTH_ENGINE_DX7);
        timbres_[timbre].loadDx7Patch(packed);
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

void SynthState::loadPresetFromMidi(int timbre, int bank, int bankLSB, int patchNumber) {
    switch (bank) {
    case 0: {
        PFM3File const* bank = storage_->getPatchBank()->getFile(bankLSB);
        if (bank->fileType != FILE_EMPTY) {
            fullState_.preenFMBankNumber = bankLSB;
            fullState_.preenFMPresetNumber = patchNumber;
            loadPreset(timbre, bank, patchNumber);
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
            loadDx7Patch(timbre, bank, patchNumber);
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
    default:
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
    case BUTTON_SWITCH_EDIT:
        fullState_.synthMode = SYNTH_MODE_EDITOR;
        break;
    case BUTTON_SWITCH_SEQUENCER:
        fullState_.synthMode = SYNTH_MODE_SEQUENCER;
        break;
    case BUTTON_SWITCH_MIXER:
        fullState_.synthMode = SYNTH_MODE_MIXER;
        break;
    case BUTTON_SWITCH_MENU:
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

void SynthState::slowTick() {
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

void SynthState::randomizePreset() {
    timbres_[currentTimbre_].randomizePreset(fullState_.randomizer.Oper, fullState_.randomizer.EnvT, fullState_.randomizer.IM,
                                             fullState_.randomizer.Modl);
}

Timbre* SynthState::getTimbre(int timbre) {
    return &timbres_[timbre];
}

char* SynthState::getTimbreName(int t) {
    return timbres_[t].getPresetName();
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
