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

#include "FMDisplayMenu.h"
#include "SynthState.h"
#include "TftDisplay.h"
#include "version.h"

extern int getLength(const char *str);

const char* enveloppeRandomizer[] = {
    " -- ",
    "perc",
    "pad ",
    "rand" };
const char* otherRandomizer[] = {
    " -- ",
    "soft",
    "medi",
    "high" };

void FMDisplayMenu::init(SynthState* synthState, TftDisplay* tft, Storage* storage) {
    this->synthState = synthState;
    this->tft = tft;
    this->storage = storage;
    this->previousConfigSetting = 255;
    this->menuStateRow = 3;
}

void FMDisplayMenu::refreshMenuByStep(int currentTimbre, int refreshStatus, int menuRow) {

    // We just pressed the menu button, pre select the first menu
    // Because of "return" in this method we must have the test here
    // We do that when refreshStatus ==6 so that we get the correct button


    // It was not a good idea, it makes things more complicated
    //    if (refreshStatus == 6 && previousSynthMode > 0) {
    //        switch (previousSynthMode) {
    //        case SYNTH_MODE_MIXER:
    //            buttonPressed(currentTimbre, BUTTON_PFM3_1);
    //            break;
    //        case SYNTH_MODE_EDIT_PFM3:
    //            buttonPressed(currentTimbre, BUTTON_PFM3_2);
    //            break;
    //        case SYNTH_MODE_SEQUENCER:
    //            buttonPressed(currentTimbre, BUTTON_PFM3_3);
    //            break;
    //        }
    //        previousSynthMode = -1;
    //    }


    switch (refreshStatus) {
    case 20:
        tft->pauseRefresh();
        tft->clear();
        tft->fillArea(0, 0, 240, 21, COLOR_DARK_RED);
        tft->setCursorInPixel(2, 2);
        tft->print("M E N U", COLOR_YELLOW, COLOR_DARK_RED);

        tft->setCharBackgroundColor(COLOR_BLACK);
        tft->setCharColor(COLOR_LIGHT_GRAY);
        tft->setCursorInPixel(240 - 7 * PFM3_FIRMWARE_VERSION_STRLEN, 25);
        tft->printSmallChars(PFM3_FIRMWARE_VERSION);
        break;

    case 19: {

        switch (this->synthState->fullState.currentMenuItem->menuState) {
        case MENU_MIXER_SAVE_ENTER_NAME:
        case MENU_PRESET_SAVE_ENTER_NAME:
        case MENU_SEQUENCER_SAVE_ENTER_NAME:
            tft->setCursor(6, menuRow);
            for (int k = 0; k < 12; k++) {
                tft->print(this->synthState->fullState.name[k]);
            }
            break;
        case MENU_SD_CREATE_SEQUENCE_FILE:
        case MENU_SD_CREATE_MIXER_FILE:
        case MENU_SD_CREATE_PRESET_FILE:
        case MENU_SD_RENAME_MIXER_ENTER_NAME:
        case MENU_SD_RENAME_PRESET_ENTER_NAME:
        case MENU_SD_RENAME_SEQUENCE_ENTER_NAME:
            tft->setCursor(6, menuRow);
            for (int k = 0; k < 8; k++) {
                tft->print(this->synthState->fullState.name[k]);
            }
            break;
        case MENU_DEFAULT_SAVE:
            tft->setCursor(1, menuRow);
            tft->print("Save to default ?");
            break;
        case MENU_DEFAULT_DELETE:
            tft->setCursor(1, menuRow);
            tft->print("Reset default ?");
            break;
        default:
            break;
        }
        break;
    }
    case 6:
    case 5:
    case 4:
    case 3:
    case 2:
    case 1: {
        int button = 6 - refreshStatus;

        switch (this->synthState->fullState.currentMenuItem->menuType) {

        case MENUTYPE_WITHSUBMENU:
            if (button < this->synthState->fullState.currentMenuItem->maxValue) {
                const char* name = MenuItemUtil::getMenuItem(
                    this->synthState->fullState.currentMenuItem->subMenu[button])->name;
                tft->drawButton(name, 270, 29, button, 0, 1, COLOR_DARK_RED);
                return;
            }
            break;
        case MENUTYPE_ENTERNAME:
            switch (button) {
            case 0:
                tft->drawButton("Save", 270, 29, button, 0, 1, COLOR_DARK_RED);
                return;
            case 1:
                tft->drawSimpleButton("' '", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
                return;
            case 2:
                tft->drawSimpleButton("'0'", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
                return;
            case 3:
                tft->drawSimpleButton("'A'", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
                return;
            case 4:
                tft->drawSimpleButton("'a'", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
                return;
            }
            // Then draw Load button 0 => so no break
        case MENUTYPE_FILESELECT_LOAD:
        case MENUTYPE_FILESELECT_SAVE:
            if (button == 0) {
                tft->drawButton(
                    this->synthState->fullState.currentMenuItem->menuType == MENUTYPE_FILESELECT_LOAD ?
                        "Load" : "Select", 270, 29, button, 0, 1, COLOR_DARK_RED);
                return;
            }
            break;
        case MENUTYPE_RANDOMIZER:
            switch (button) {
            case 0:
                tft->drawButton("Apply", 270, 29, button, 0, 1, COLOR_DARK_RED);
                return;
            case 1:
                tft->drawSimpleButton("Rand", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
                return;
            }
            break;
        case MENUTYPE_SETTINGS:
            if (button == 0) {
                tft->drawButton("Save", 270, 29, button, 0, 1, COLOR_DARK_RED);
                return;
            }
            break;
        case MENUTYPE_CONFIRM:
            if (button == 0) {
                // Default Empty button
                tft->drawSimpleButton("Sure?", 270, 29, button, COLOR_YELLOW, COLOR_DARK_RED);
                return;
            }
            break;
        default:
            break;
        }

        if (button == 5) {
        	switch (this->synthState->fullState.currentMenuItem->menuState) {
        	case MAIN_MENU:
        	case MENU_MIXER:
        	case MENU_SEQUENCER:
        	case MENU_PRESET:
        	case MENU_DEFAULT: {
                const char* name = MenuItemUtil::getMenuItem(
                    this->synthState->fullState.currentMenuItem->subMenu[this->synthState->fullState.previousChoice])->name;
                tft->drawSimpleBorderButton(name, 270, 29, button, COLOR_LIGHT_GRAY, COLOR_DARK_RED);
                break;
        	}
        	default:
                // Default Empty button
                tft->drawSimpleButton("", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
        		break;
        	}
        } else {
            // Default Empty button
        	tft->drawButton("", 270, 29, button, 0, 1, COLOR_DARK_RED);
        }

        break;
    }
    }
}

void FMDisplayMenu::encoderTurned(int currentTimbre, int encoder, int ticks) {
    struct FullState* fullState = &this->synthState->fullState;

    switch (fullState->currentMenuItem->menuState) {
    case MENU_CONFIG_SETTINGS:
        if (encoder == 3) {
            changePresetSelect(&fullState->midiConfigValue[fullState->menuSelect], ticks,
                midiConfig[fullState->menuSelect].maxValue - 1);

            if (fullState->menuSelect == MIDICONFIG_TFT_BACKLIGHT) {
                uint8_t tft_bl =  fullState->midiConfigValue[MIDICONFIG_TFT_BACKLIGHT];
#if defined(LQFP100) || defined(LQFP100_OLD)
                TIM3->CCR4 = tft_bl < 10 ? 10 : tft_bl;
#endif
#if defined(LQFP144)
                TIM1->CCR2 = tft_bl < 10 ? 10 : tft_bl;
#endif
            }
        } else if (encoder == 0) {
            changePresetSelect(&fullState->menuSelect, ticks, MIDICONFIG_SIZE);
        }
        break;
    case MENU_PRESET_LOAD_SELECT:
        if (encoder == 0) {
            // Did we really change bank?
            if (changePresetSelect(&fullState->preenFMBankNumber, ticks, (int) fullState->currentMenuItem->maxValue - 1)) {
                fullState->preenFMPresetNumber = 0;
                fullState->preenFMBank = synthState->getStorage()->getPatchBank()->getFile(
                    fullState->preenFMBankNumber);
                synthState->loadPreset(currentTimbre, fullState->preenFMBank, fullState->preenFMPresetNumber,
                    synthState->params);
            }
        } else if (encoder == 3) {
            if (changePresetSelect(&fullState->preenFMPresetNumber, ticks, 127)) {
                synthState->loadPreset(currentTimbre, fullState->preenFMBank, fullState->preenFMPresetNumber,
                    synthState->params);
            }
        }
        break;
    case MENU_PRESET_LOAD_SELECT_DX7_BANK:
        if (encoder == 0) {
            // Did we really change bank?
            if (changePresetSelect(&fullState->dx7BankNumber, ticks, (int) fullState->currentMenuItem->maxValue - 1)) {
                fullState->preenFMBank = synthState->getStorage()->getDX7SysexFile()->getFile(fullState->dx7BankNumber);
                fullState->dx7PresetNumber = 0;
                synthState->loadDx7Patch(currentTimbre, fullState->preenFMBank, fullState->dx7PresetNumber,
                    synthState->params);
            }
        } else if (encoder == 3) {
            if (changePresetSelect(&fullState->dx7PresetNumber, ticks, 31)) {
                synthState->loadDx7Patch(currentTimbre, fullState->preenFMBank, fullState->dx7PresetNumber,
                    synthState->params);
            }
        }
        break;
    case MENU_SD_RENAME_MIXER_SELECT_FILE:
    case MENU_MIXER_SAVE_SELECT:
    case MENU_MIXER_LOAD_SELECT:
        if (encoder == 0) {
            // Did we really change bank?
            if (changePresetSelect(&fullState->preenFMMixerNumber, ticks, (int) fullState->currentMenuItem->maxValue - 1)) {
                fullState->preenFMMixerPresetNumber = 0;
                fullState->preenFMMixer = synthState->getStorage()->getMixerBank()->getFile(
                    fullState->preenFMMixerNumber);
            }
        } else if (encoder == 3) {
            changePresetSelect(&fullState->preenFMMixerPresetNumber, ticks, NUMBER_OF_MIXERS_PER_BANK -1);
        }
        break;
    case MENU_SD_RENAME_PRESET_SELECT_FILE:
    case MENU_PRESET_SAVE_SELECT:
        if (encoder == 0) {
            // Did we really change bank?
            if (changePresetSelect(&fullState->preenFMBankNumber, ticks, (int) fullState->currentMenuItem->maxValue - 1)) {
                fullState->preenFMPresetNumber = 0;
                fullState->preenFMBank = synthState->getStorage()->getPatchBank()->getFile(
                    fullState->preenFMBankNumber);
            }
        } else if (encoder == 3) {
            changePresetSelect(&fullState->preenFMPresetNumber, ticks, 127);
        }
        break;
    case MENU_SD_RENAME_SEQUENCE_SELECT_FILE:
    case MENU_SEQUENCER_SAVE_SELECT:
    case MENU_SEQUENCER_LOAD_SELECT:
        if (encoder == 0) {
            // Did we really change bank?
            if (changePresetSelect(&fullState->preenFMSeqBankNumber, ticks, (int) fullState->currentMenuItem->maxValue - 1)) {
                fullState->preenFMSeqSequencerNumber = 0;
                fullState->preenFMSequence = synthState->getStorage()->getSequenceBank()->getFile(
                    fullState->preenFMSeqBankNumber);
            }
        } else if (encoder == 3) {
            changePresetSelect(&fullState->preenFMSeqSequencerNumber, ticks, NUMBER_OF_SEQUENCES_PER_BANK - 1);
        }
        break;
    case MENU_SD_CREATE_PRESET_FILE:
    case MENU_SD_CREATE_MIXER_FILE:
    case MENU_SD_CREATE_SEQUENCE_FILE:
    case MENU_SD_RENAME_MIXER_ENTER_NAME:
    case MENU_SD_RENAME_PRESET_ENTER_NAME:
    case MENU_SD_RENAME_SEQUENCE_ENTER_NAME:
        if (encoder == 0) {
            changePresetSelect(&fullState->menuSelect, ticks, 7);
        } else if (encoder == 3) {
            changeCharSelect(&fullState->name[fullState->menuSelect], ticks);
        }
        break;
    case MENU_MIXER_SAVE_ENTER_NAME:
    case MENU_PRESET_SAVE_ENTER_NAME:
    case MENU_SEQUENCER_SAVE_ENTER_NAME:
        if (encoder == 0) {
            changePresetSelect(&fullState->menuSelect, ticks, 12);
        } else if (encoder == 3) {
            changeCharSelect(&fullState->name[fullState->menuSelect], ticks);
        }
        break;
    case MENU_PRESET_RANDOMIZER:
        if (synthState->newRandomizerValue(encoder, ticks)) {
            // Only propagate if value really changed
            synthState->propagateNewMenuSelect();
        }
        return;
        break;
    default:
        break;
    }

    synthState->propagateNewMenuSelect();
}

bool FMDisplayMenu::changeCharSelect(char *valueToChange, int ticks) {
    int oldPresetSelect = *valueToChange;
    int newPresetSelect = oldPresetSelect + ticks;
    if (newPresetSelect > 127) {
        newPresetSelect = 127;
    } else if (newPresetSelect < 32) {
        newPresetSelect = 32;
    }
    *valueToChange = (char) newPresetSelect;
    return oldPresetSelect != newPresetSelect;
}


bool FMDisplayMenu::changePresetSelect(uint8_t *valueToChange, int ticks, int max) {
    int oldPresetSelect = *valueToChange;
    int newPresetSelect = oldPresetSelect + ticks;
    if (newPresetSelect > max) {
        newPresetSelect = max;
    } else if (newPresetSelect < 0) {
        newPresetSelect = 0;
    }
    *valueToChange = (uint8_t) newPresetSelect;
    return oldPresetSelect != newPresetSelect;
}

bool FMDisplayMenu::changePresetSelect(char *valueToChange, int ticks, int max) {
    int oldPresetSelect = *valueToChange;
    int newPresetSelect = oldPresetSelect + ticks;
    if (newPresetSelect > max) {
        newPresetSelect = max;
    } else if (newPresetSelect < 0) {
        newPresetSelect = 0;
    }
    *valueToChange = (char) newPresetSelect;
    return oldPresetSelect != newPresetSelect;
}

bool FMDisplayMenu::changePresetSelect(uint16_t *valueToChange, int ticks, int max) {
    int oldPresetSelect = *valueToChange;
    int newPresetSelect = oldPresetSelect + ticks;
    if (newPresetSelect > max) {
        newPresetSelect = max;
    } else if (newPresetSelect < 0) {
        newPresetSelect = 0;
    }
    *valueToChange = (uint16_t) newPresetSelect;
    return oldPresetSelect != newPresetSelect;
}

bool FMDisplayMenu::changePresetSelect(uint32_t *valueToChange, int ticks, int max) {
    int oldPresetSelect = *valueToChange;
    int newPresetSelect = oldPresetSelect + ticks;
    if (newPresetSelect > max) {
        newPresetSelect = max;
    } else if (newPresetSelect < 0) {
        newPresetSelect = 0;
    }
    *valueToChange = (uint32_t) newPresetSelect;
    return oldPresetSelect != newPresetSelect;
}

void FMDisplayMenu::buttonPressed(int currentTimbre, int button) {
    struct FullState* fullState = &this->synthState->fullState;
    int32_t length;

    if (button == BUTTON_PFM3_MENU) {
        if (fullState->currentMenuItem->menuState != MAIN_MENU) {
            const MenuItem* oldMenuItem = fullState->currentMenuItem;
            fullState->currentMenuItem = getMenuBack(currentTimbre);
            updatePreviousChoice(fullState->currentMenuItem->menuState);
            synthState->propagateMenuBack(oldMenuItem);
        }
        return;
    }

    if (button < BUTTON_PFM3_1 || button > BUTTON_PFM3_6) {
        return;
    }

    // Do not do anythin in the following case
    if (fullState->currentMenuItem->menuType == MENUTYPE_FILESELECT_SAVE
    		|| fullState->currentMenuItem->menuType == MENUTYPE_FILESELECT_LOAD) {
    	// Seq
        if (fullState->currentMenuItem->menuState == MENU_SEQUENCER_SAVE_SELECT
        		&& fullState->preenFMSequence->fileType != FILE_OK) {
        	return;
        }
        if (fullState->currentMenuItem->menuState == MENU_SEQUENCER_LOAD_SELECT
        		&& fullState->preenFMSequence->fileType == FILE_EMPTY) {
        	return;
        }
    	// Mixer
        if (fullState->currentMenuItem->menuState == MENU_MIXER_SAVE_SELECT
        		&& fullState->preenFMMixer->fileType != FILE_OK) {
        	return;
        }
        if (fullState->currentMenuItem->menuState == MENU_MIXER_LOAD_SELECT
        		&& fullState->preenFMMixer->fileType == FILE_EMPTY) {
        	return;
        }
    	// Preset
        if (fullState->currentMenuItem->menuState == MENU_PRESET_SAVE_SELECT
        		&& fullState->preenFMBank->fileType != FILE_OK) {
        	return;
        }
        if (fullState->currentMenuItem->menuState == MENU_PRESET_LOAD_SELECT
        		&& fullState->preenFMBank->fileType == FILE_EMPTY) {
        	return;
        }
    }

    // Special treatment for Randomizer
    if (unlikely(fullState->currentMenuItem->menuState == MENU_PRESET_RANDOMIZER)) {
        if (button == 1) {
            synthState->storeTestNote();
            synthState->propagateNoteOff();
            synthState->propagateBeforeNewParamsLoad(currentTimbre);
            synthState->randomizePreset();
            synthState->propagateAfterNewParamsLoad(currentTimbre);
            synthState->restoreTestNote();
            return;
        }
    }

    // Enter name ?
    if (fullState->currentMenuItem->menuType == MENUTYPE_ENTERNAME && button >= 1 && button <= 4) {
        char staticChars[4] = {
            ' ',
            '0',
            'A',
            'a'
        };
        fullState->name[fullState->menuSelect] = staticChars[button - 1];
        synthState->propagateNewMenuSelect();
        return;
    }

    // Next menu
    MenuState oldMenu = fullState->currentMenuItem->menuState;

    if (button == 5) {
        // restore previous choice
		switch (oldMenu) {
			case MAIN_MENU:
				button = fullState->previousMenuChoice.main;
				break;
			case MENU_MIXER:
				button = fullState->previousMenuChoice.mixer;
				break;
			case MENU_PRESET:
				button = fullState->previousMenuChoice.preset;
				break;
			case MENU_SEQUENCER:
				button = fullState->previousMenuChoice.sequencer;
				break;
			case MENU_DEFAULT:
				button = fullState->previousMenuChoice.deflt;
				break;
			default:
				break;
		}
    }

    // Don't do anything if no button
    int maxButton;
    switch (fullState->currentMenuItem->menuType) {
    case MENUTYPE_WITHSUBMENU:
        maxButton = fullState->currentMenuItem->maxValue - 1;
        break;
    case MENUTYPE_RANDOMIZER:
        maxButton = 1;
        break;
    case MENUTYPE_TEMPORARY:
        maxButton = -1;
        break;
    default:
    	// only button 1
        maxButton = 0;
        break;
    }
    if (button > maxButton) {
    	return;
    }

    if (button < 5) {
        // store previous choice
        switch (oldMenu) {
            case MAIN_MENU:
                fullState->previousMenuChoice.main = button;
                break;
            case MENU_MIXER:
                fullState->previousMenuChoice.mixer = button;
                break;
            case MENU_PRESET:
                fullState->previousMenuChoice.preset = button;
                break;
            case MENU_SEQUENCER:
                fullState->previousMenuChoice.sequencer = button;
                break;
            case MENU_DEFAULT:
                fullState->previousMenuChoice.deflt = button;
                break;
            default:
                break;
        }
    }

    const MenuItem* nextMenu = MenuItemUtil::getMenuItem(fullState->currentMenuItem->subMenu[button]);


    // If Previous state was the following we have some action to do
    if (nextMenu->menuState == MENU_DONE) {
        switch (oldMenu) {
        case MENU_PRESET_SAVE_ENTER_NAME:
            // Let save the preset !
            for (length = 12; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--);
            for (int k = 0; k < length; k++) {
                synthState->params->presetName[k] = fullState->name[k];
            }
            synthState->params->presetName[length] = '\0';
            synthState->getStorage()->getPatchBank()->savePatch(fullState->preenFMBank, fullState->preenFMPresetNumber, synthState->params);
            break;
        case MENU_MIXER_SAVE_ENTER_NAME:
            // Let save the mixer !
            for (length = 12; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--);
            fullState->name[length] = '\0';
            synthState->getStorage()->getMixerBank()->saveMixer(fullState->preenFMMixer, fullState->preenFMMixerPresetNumber, fullState->name);
            break;
        case MENU_SEQUENCER_SAVE_ENTER_NAME:
            // Let save the sequence !
            for (length = 12; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--);
            fullState->name[length] = '\0';
            synthState->getStorage()->getSequenceBank()->saveSequence(fullState->preenFMSequence, fullState->preenFMSeqSequencerNumber, fullState->name);
            break;
        case MENU_MIXER_LOAD_SELECT:
            synthState->loadMixer(fullState->preenFMMixer, fullState->preenFMMixerPresetNumber);
            break;
        case MENU_SEQUENCER_LOAD_SELECT:
            synthState->getStorage()->getSequenceBank()->loadSequence(fullState->preenFMSequence, fullState->preenFMSeqSequencerNumber);
            break;
        case MENU_CONFIG_SETTINGS:
            // Let save the preset !
            synthState->getStorage()->getConfigurationFile()->saveConfig(fullState->midiConfigValue);
            break;
        case MENU_SD_CREATE_MIXER_FILE:
            // Must create the mixer here....
            for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--);
            length += copyStringAt(fullState->name, length, ".mix" );
            fullState->name[length] = '\0';

            if (!synthState->getStorage()->getMixerBank()->nameExists(fullState->name)) {
                fullState->currentMenuItem = MenuItemUtil::getMenuItem(MENU_IN_PROGRESS);
                // display "Wait"
                synthState->propagateNewMenuSelect();
                synthState->getStorage()->getMixerBank()->createMixerBank(fullState->name);
            } else {
                nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
            }
            break;
        case MENU_SD_CREATE_SEQUENCE_FILE:
            // Must create the sequence file here....
            for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--);
            length += copyStringAt(fullState->name, length, ".seq" );
            fullState->name[length] = '\0';

            if (!synthState->getStorage()->getSequenceBank()->nameExists(fullState->name)) {
                fullState->currentMenuItem = MenuItemUtil::getMenuItem(MENU_IN_PROGRESS);
                // display "Wait"
                synthState->propagateNewMenuSelect();
                synthState->getStorage()->getSequenceBank()->createSequenceFile(fullState->name);
            } else {
                nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
            }
            break;
        case MENU_SD_CREATE_PRESET_FILE:
            // Must create the bank here....
            for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--);
            length += copyStringAt(fullState->name, length, ".bnk" );
            fullState->name[length] = '\0';

            if (!synthState->getStorage()->getPatchBank()->nameExists(fullState->name)) {
                fullState->currentMenuItem = MenuItemUtil::getMenuItem(MENU_IN_PROGRESS);
                // display "Wait"
                synthState->propagateNewMenuSelect();
                synthState->getStorage()->getPatchBank()->createPatchBank(fullState->name);
            } else {
                nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
            }
            break;
        case MENU_SD_RENAME_MIXER_ENTER_NAME:

            for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--);
            length += copyStringAt(fullState->name, length, ".mix" );
            fullState->name[length] = '\0';

            if (synthState->getStorage()->getMixerBank()->renameFile(fullState->preenFMMixer, fullState->name) > 0) {
                nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
            }
            break;
        case MENU_SD_RENAME_PRESET_ENTER_NAME:

            for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--);
            length += copyStringAt(fullState->name, length, ".bnk" );
            fullState->name[length] = '\0';

            if (synthState->getStorage()->getPatchBank()->renameFile(fullState->preenFMBank, fullState->name) > 0) {
                nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
            }
            break;
        case MENU_SD_RENAME_SEQUENCE_ENTER_NAME:
            for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--);
            length += copyStringAt(fullState->name, length, ".seq" );
            fullState->name[length] = '\0';

            if (synthState->getStorage()->getSequenceBank()->renameFile(fullState->preenFMSequence, fullState->name) > 0) {
                nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
            }
            break;
            break;
        case MENU_DEFAULT_SAVE:
            synthState->getStorage()->getMixerBank()->saveDefaultMixerAndSeq();
            break;
        case MENU_DEFAULT_DELETE:
            synthState->getStorage()->getMixerBank()->removeDefaultMixer();
            break;
        case MENU_DEFAULT_LOAD:
            synthState->getStorage()->getMixerBank()->loadDefaultMixerAndSeq();
            break;
        default:
            break;

        }
    }

    // Action depending on Next menu
    updatePreviousChoice(nextMenu->menuState);

    switch (nextMenu->menuState) {
    case MENU_PRESET_LOAD_SELECT:
        copySynthParams((char*) synthState->params, (char*) &synthState->backupParams);
        fullState->preenFMBank = synthState->getStorage()->getPatchBank()->getFile(fullState->preenFMBankNumber);
        synthState->loadPreset(currentTimbre, fullState->preenFMBank, fullState->preenFMPresetNumber,
            synthState->params);
        break;
    case MENU_PRESET_LOAD_SELECT_DX7_BANK:
        copySynthParams((char*) synthState->params, (char*) &synthState->backupParams);
        fullState->preenFMBank = synthState->getStorage()->getDX7SysexFile()->getFile(fullState->dx7BankNumber);
        synthState->loadDx7Patch(currentTimbre, fullState->preenFMBank, fullState->dx7PresetNumber, synthState->params);
        break;
    case MENU_SD_RENAME_MIXER_SELECT_FILE:
    case MENU_MIXER_LOAD_SELECT:
    case MENU_MIXER_SAVE_SELECT:
        fullState->preenFMMixer = synthState->getStorage()->getMixerBank()->getFile(fullState->preenFMMixerNumber);
        break;
    case MENU_SD_RENAME_PRESET_SELECT_FILE:
    case MENU_PRESET_SAVE_SELECT:
        fullState->preenFMBank = synthState->getStorage()->getPatchBank()->getFile(fullState->preenFMBankNumber);
        break;
    case MENU_SD_RENAME_SEQUENCE_SELECT_FILE:
    case MENU_SEQUENCER_SAVE_SELECT:
    case MENU_SEQUENCER_LOAD_SELECT:
        fullState->preenFMSequence = synthState->getStorage()->getSequenceBank()->getFile(fullState->preenFMSeqBankNumber);
        break;
    case MENU_PRESET_SAVE_ENTER_NAME:
        copyAndTransformString(fullState->name, synthState->params->presetName);
        break;
    case MENU_MIXER_SAVE_ENTER_NAME:
        copyAndTransformString(fullState->name,
            synthState->mixerState.mixName);
        break;
    case MENU_SEQUENCER_SAVE_ENTER_NAME:
        copyAndTransformString(fullState->name,
        		synthState->getSequenceName());
        break;
    case MENU_SD_RENAME_PRESET_ENTER_NAME:
        if (fullState->preenFMBank->fileType == FILE_OK) {
            copyAndTransformString(fullState->name, fullState->preenFMBank->name);
            moveExtensionAtTheEnd(fullState->name);
        } else {
            nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
        }
        break;
    case MENU_SD_RENAME_MIXER_ENTER_NAME:
        if (fullState->preenFMMixer->fileType == FILE_OK) {
            copyAndTransformString(fullState->name, fullState->preenFMMixer->name);
            moveExtensionAtTheEnd(fullState->name);
        } else {
            nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
        }
        break;
    case MENU_SD_RENAME_SEQUENCE_ENTER_NAME:
        if (fullState->preenFMSequence->fileType == FILE_OK) {
            copyAndTransformString(fullState->name, fullState->preenFMSequence->name);
            moveExtensionAtTheEnd(fullState->name);
        } else {
            nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
        }
        break;
    case MENU_PRESET_RANDOMIZER:
        copySynthParams((char*) synthState->params, (char*) &synthState->backupParams);
        break;
    case MENU_SD_CREATE_PRESET_FILE:
        copyAndTransformString(fullState->name, "Bank");
        break;
    case MENU_SD_CREATE_MIXER_FILE:
        copyAndTransformString(fullState->name, "Mixer");
        break;
    case MENU_SD_CREATE_SEQUENCE_FILE:
        copyAndTransformString(fullState->name, "Seq");
        break;
    default:
        break;
    }

    fullState->currentMenuItem = nextMenu;
    synthState->propagateNewMenuState();
    synthState->propagateNewMenuSelect();

}

void FMDisplayMenu::updatePreviousChoice(uint8_t menuState) {
    struct FullState* fullState = &this->synthState->fullState;

	switch (menuState) {
	case MAIN_MENU:
		fullState->previousChoice = fullState->previousMenuChoice.main;
		break;
	case MENU_MIXER:
		fullState->previousChoice = fullState->previousMenuChoice.mixer;
		break;
	case MENU_PRESET:
		fullState->previousChoice = fullState->previousMenuChoice.preset;
		break;
	case MENU_SEQUENCER:
		fullState->previousChoice = fullState->previousMenuChoice.sequencer;
		break;
	case MENU_DEFAULT:
		fullState->previousChoice = fullState->previousMenuChoice.deflt;
		break;
	default:
		break;
	}
}



const MenuItem* FMDisplayMenu::getMenuBack(int currentTimbre) {
    struct FullState* fullState = &this->synthState->fullState;

    const MenuItem* rMenuItem = 0;

    // default menuSelect value
    fullState->menuSelect = 0; // MenuItemUtil::getParentMenuSelect(fullState->currentMenuItem->menuState);

    switch (fullState->currentMenuItem->menuState) {
    case MENU_PRESET_RANDOMIZER:
        // Put back original preset
        synthState->propagateNoteOff();
        synthState->propagateBeforeNewParamsLoad(currentTimbre);
        copySynthParams((char*) &synthState->backupParams, (char*) synthState->params);
        synthState->propagateAfterNewParamsLoad(currentTimbre);
        break;
    case MENU_PRESET_LOAD_SELECT:
    case MENU_PRESET_LOAD_SELECT_DX7_BANK:
        synthState->propagateNoteOff();
        synthState->propagateBeforeNewParamsLoad(currentTimbre);
        // Put back original preset
        copySynthParams((char*) &synthState->backupParams, (char*) synthState->params);
        synthState->propagateAfterNewParamsLoad(currentTimbre);
        break;
    default:
        break;
    }
    rMenuItem = MenuItemUtil::getParentMenuItem(fullState->currentMenuItem->menuState);
    return rMenuItem;
}

void FMDisplayMenu::moveExtensionAtTheEnd(char *fileName) {
    int length = getLength(fileName);
    int extSize = 0;
    char extension[8];
    do {
        length--;
        extSize++;
    } while (fileName[length] != '.');

    for (int i = 0; i < extSize; i++) {
        extension[i] = fileName[length + i];
    }

    for (int i = 0; i < extSize; i++) {
        fileName[8 + i] = extension[i];
    }

    while (length < 8) {
        fileName[length++] = ' ';
    }
}

void FMDisplayMenu::copyAndTransformString(char *dest, const char* source) {
    for (int k = 0; k < 12; k++) {
        dest[k] = ' ';
    }
    for (int k = 0; k < 12 && source[k] != 0; k++) {
        dest[k] = source[k];
    }
}

int FMDisplayMenu::copyStringAt(char *dest, int offset, const char *source) {
    int o = 0;
    while (source[o] != '\0') {
        dest[offset + o] = source[o];
        o++;
    }
    return o;
}


void FMDisplayMenu::copySynthParams(char* source, char* dest) {
    for (uint32_t k = 0; k < sizeof(struct OneSynthParams); k++) {
        dest[k] = source[k];
    }
}

void FMDisplayMenu::newMenuState(FullState* fullState) {
    tft->pauseRefresh();

    if (MenuItemUtil::getParentMenuItem(this->synthState->fullState.currentMenuItem->menuState)->menuState == MAIN_MENU) {
        menuStateRow = 1;
    }

    menuStateRow++;

    tft->setCharBackgroundColor(COLOR_BLACK);
    tft->setCharColor(COLOR_RED);

    tft->setCursor((menuStateRow - 1), menuStateRow);
    tft->print(this->synthState->fullState.currentMenuItem->name);
    switch (this->synthState->fullState.currentMenuItem->menuState) {
    case MENU_PRESET_LOAD_SELECT:
        tft->setCharColor(COLOR_YELLOW);
        tft->print(" in I");
        tft->print(this->synthState->getCurrentTimbre() + 1);
        tft->setCharColor(COLOR_RED);
        break;
    case MENU_PRESET_SAVE_SELECT:
    case MENU_PRESET_RANDOMIZER:
        tft->print(" ");
        tft->print(this->synthState->params->presetName);
        break;
    default:
        break;
    }
    displayMenuState(fullState);
}

void FMDisplayMenu::displayMenuState(FullState* fullState) {

    tft->setCharBackgroundColor(COLOR_BLACK);
    tft->setCharColor(COLOR_GRAY);
    switch (this->synthState->fullState.currentMenuItem->menuState) {
    case MENU_PRESET_LOAD_SELECT:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - bank");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("4 - preset");
        break;
    case MENU_PRESET_SAVE_SELECT:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - save in file");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("4 - save in preset");
        break;
    case MENU_SEQUENCER_SAVE_SELECT:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - save in file");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("4 - save in sequence");
        break;
    case MENU_SEQUENCER_LOAD_SELECT:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - load from file");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("4 - sequence");
        break;
    case MENU_MIXER_LOAD_SELECT:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - mixer file");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("4 - mixer");
        break;
    case MENU_MIXER_SAVE_SELECT:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - save in mixer file");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("4 - save in mixer");
        break;
    case MENU_PRESET_LOAD_SELECT_DX7_BANK:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - dx7 sysex file");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("4 - preset");
        break;
    case MENU_PRESET_SAVE_ENTER_NAME:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("    save in file");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - move    4 - change char");
        break;
    case MENU_MIXER_SAVE_ENTER_NAME:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("    save in mixer file");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - move    4 - change char");
        break;
    case MENU_SEQUENCER_SAVE_ENTER_NAME:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("    save in sequence file");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - move    4 - change char");
        break;
    case MENU_PRESET_RANDOMIZER:
        tft->setCharBackgroundColor(COLOR_BLACK);
        tft->setCharColor(COLOR_GRAY);
        tft->setCursor(1, 7);
        tft->print("Operator  Envelope");
        tft->setCursor(1, 9);
        tft->print("Indices   Modulation");
        break;
    case MENU_CONFIG_SETTINGS:
        previousConfigSetting = 255;
        break;
    case MENU_SD_CREATE_PRESET_FILE:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 7 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("    Create preset bank");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - move    4 - change char");
        break;
    case MENU_SD_CREATE_MIXER_FILE:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 7 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("    Create mixer bank");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - move    4 - change char");
        break;
    case MENU_SD_CREATE_SEQUENCE_FILE:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 7 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("    Create sequence file");
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - move    4 - change char");
        break;
    case MENU_SD_RENAME_MIXER_ENTER_NAME:
    case MENU_SD_RENAME_PRESET_ENTER_NAME:
    case MENU_SD_RENAME_SEQUENCE_ENTER_NAME:
        tft->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft->printSmallChars("1 - move    4 - change char");
        break;
    default:
        break;
    }
}

void FMDisplayMenu::newMenuSelect(FullState* fullState) {

    int length;

    switch (fullState->currentMenuItem->menuState) {
    case MENU_PRESET_LOAD_SELECT_DX7_BANK:
        displayBankSelect(fullState->dx7BankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY),
            fullState->preenFMBank->name);
        if (fullState->preenFMBank->fileType != FILE_EMPTY) {
            displayPatchSelect(fullState->dx7PresetNumber, this->synthState->params->presetName);
        } else {
            displayPatchSelect(0, 0);
        }
        break;
    case MENU_PRESET_LOAD_SELECT:
        displayBankSelect(fullState->preenFMBankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY),
            fullState->preenFMBank->name);
        if (fullState->preenFMBank->fileType != FILE_EMPTY) {
            displayPatchSelect(fullState->preenFMPresetNumber, this->synthState->params->presetName);
        } else {
            displayPatchSelect(0, 0);
        }
        break;
    case MENU_MIXER_LOAD_SELECT:
    case MENU_MIXER_SAVE_SELECT:
        displayBankSelect(fullState->preenFMMixerNumber, (fullState->preenFMMixer->fileType != FILE_EMPTY),
            fullState->preenFMMixer->name);
        if (fullState->preenFMMixer->fileType != FILE_EMPTY) {
            displayPatchSelect(fullState->preenFMMixerPresetNumber,
                storage->getMixerBank()->loadMixerName(fullState->preenFMMixer,
                    fullState->preenFMMixerPresetNumber));
        } else {
            displayPatchSelect(0, 0);
        }
        break;
    case MENU_ERROR:
    case MENU_CANCEL:
        tft->setCharColor(COLOR_RED);
        goto displayStateMessage;
    case MENU_IN_PROGRESS:
    case MENU_DONE:
        tft->setCharColor(COLOR_YELLOW);
        displayStateMessage:
        tft->setCharBackgroundColor(COLOR_BLACK);
        tft->setCharColor(COLOR_YELLOW);
        tft->setCursor(8, 12);
        tft->print(fullState->currentMenuItem->name);
        tft->print("   ");
        break;
    case MENU_PRESET_SAVE_SELECT:
        displayBankSelect(fullState->preenFMBankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY),
            fullState->preenFMBank->name);
        if (fullState->preenFMBank->fileType != FILE_EMPTY) {
            displayPatchSelect(fullState->preenFMPresetNumber,
                storage->getPatchBank()->loadPatchName(fullState->preenFMBank, fullState->preenFMPresetNumber));
        } else {
            displayPatchSelect(0, 0);
        }
        break;
    case MENU_SEQUENCER_SAVE_SELECT:
        displayBankSelect(fullState->preenFMSeqBankNumber, (fullState->preenFMSequence->fileType == FILE_OK),
            fullState->preenFMSequence->name);
        if (fullState->preenFMSequence->fileType == FILE_OK) {
            displayPatchSelect(fullState->preenFMSeqSequencerNumber,
                storage->getSequenceBank()->loadSequenceName(fullState->preenFMSequence, fullState->preenFMSeqSequencerNumber));
        } else {
            displayPatchSelect(0, 0);
        }
        break;
    case MENU_SEQUENCER_LOAD_SELECT:
        displayBankSelect(fullState->preenFMSeqBankNumber, (fullState->preenFMSequence->fileType != FILE_EMPTY),
            fullState->preenFMSequence->name);
        if (fullState->preenFMSequence->fileType != FILE_EMPTY) {
            displayPatchSelect(fullState->preenFMSeqSequencerNumber,
                storage->getSequenceBank()->loadSequenceName(fullState->preenFMSequence, fullState->preenFMSeqSequencerNumber));
        } else {
            displayPatchSelect(0, 0);
        }
        break;

    case MENU_SD_CREATE_PRESET_FILE:
    case MENU_SD_CREATE_MIXER_FILE:
    case MENU_SD_CREATE_SEQUENCE_FILE:
        length = 8;
        goto displayChars;

    case MENU_SD_RENAME_MIXER_ENTER_NAME:
    case MENU_SD_RENAME_SEQUENCE_ENTER_NAME:
    case MENU_SD_RENAME_PRESET_ENTER_NAME:
    case MENU_MIXER_SAVE_ENTER_NAME:
    case MENU_PRESET_SAVE_ENTER_NAME:
    case MENU_SEQUENCER_SAVE_ENTER_NAME:
        length = 12;

        displayChars:
        tft->setCharBackgroundColor(COLOR_BLACK);
        tft->setCursor(4, 9);
        for (int k = 0; k < length; k++) {
            if (likely(k != fullState->menuSelect)) {
                tft->setCharColor(COLOR_WHITE);
                tft->print(fullState->name[k]);
            } else {
                tft->setCharColor(COLOR_RED);
                if (unlikely(fullState->name[fullState->menuSelect] == ' ')) {
                    tft->print('_');
                } else {
                    tft->print(fullState->name[fullState->menuSelect]);
                }
            }
        }
        break;
    case MENU_PRESET_RANDOMIZER:
        tft->setCharBackgroundColor(COLOR_BLACK);
        tft->setCharColor(COLOR_WHITE);
        tft->setCursor(3, 8);
        tft->print(otherRandomizer[(int) fullState->randomizer.Oper]);
        tft->setCursor(13, 8);
        tft->print(enveloppeRandomizer[(int) fullState->randomizer.EnvT]);
        tft->setCursor(3, 10);
        tft->print(otherRandomizer[(int) fullState->randomizer.IM]);
        tft->setCursor(13, 10);
        tft->print(otherRandomizer[(int) fullState->randomizer.Modl]);
        break;
    case MENU_CONFIG_SETTINGS:
        if (previousConfigSetting != fullState->menuSelect) {
            tft->pauseRefresh();
            tft->fillArea(0, 160, 240, 40, COLOR_BLACK);

            tft->setCursor(1, 8);
            tft->setCharColor(COLOR_GRAY);
            tft->printSmallChar('1');

            tft->setCharColor(COLOR_WHITE);
            tft->setCursor(2, 8);
            tft->print(midiConfig[fullState->menuSelect].title);

            tft->setCursor(1, 9);
            tft->setCharColor(COLOR_GRAY);
            tft->printSmallChar('2');
        } else {
            tft->fillArea(30, 180, 210, 20, COLOR_BLACK);
        }
        tft->setCursor(2, 9);
        tft->setCharColor(COLOR_YELLOW);
        if (midiConfig[fullState->menuSelect].valueName != NULL) {
            tft->print(midiConfig[fullState->menuSelect].valueName[fullState->midiConfigValue[fullState->menuSelect]]);
        } else {
            tft->print((int) fullState->midiConfigValue[fullState->menuSelect]);
        }
        if (previousConfigSetting != fullState->menuSelect) {
            tft->restartRefreshTft();
            previousConfigSetting = fullState->menuSelect;
        }
        break;

    case MENU_SD_RENAME_PRESET_SELECT_FILE:
        displayBankSelect(fullState->preenFMBankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY),
            fullState->preenFMBank->name);
        break;
    case MENU_SD_RENAME_MIXER_SELECT_FILE:
        displayBankSelect(fullState->preenFMMixerNumber, (fullState->preenFMMixer->fileType != FILE_EMPTY),
            fullState->preenFMMixer->name);
        break;
    case MENU_SD_RENAME_SEQUENCE_SELECT_FILE:
        displayBankSelect(fullState->preenFMSeqBankNumber, (fullState->preenFMSequence->fileType != FILE_EMPTY),
            fullState->preenFMSequence->name);
        break;
    default:
        break;
    }
}

void FMDisplayMenu::menuBack(const MenuItem* oldMenuItem, FullState* fullState) {

    int y = menuStateRow * TFT_BIG_CHAR_HEIGHT;
    tft->fillArea(0, y, 240, 260 - y, COLOR_BLACK);

    menuStateRow--;

    displayMenuState(fullState);
    newMenuSelect(fullState);
}

void FMDisplayMenu::displayBankSelect(int bankNumber, bool usable, const char* name) {

    bankNumber++;

    tft->setCharBackgroundColor(COLOR_BLACK);
    tft->setCharColor(usable ? COLOR_WHITE : COLOR_GRAY);
    tft->setCursor(0, 7);
    if (bankNumber < 100) {
        tft->print(' ');
    }
    if (bankNumber < 10) {
        tft->print(' ');
    }
    tft->print(bankNumber);
    tft->print(' ');
    tft->print(name);
    tft->printSpaceTillEndOfLine();
}

void FMDisplayMenu::displayPatchSelect(int presetNumber, const char* name) {
    presetNumber++;

    if (name != 0) {
        tft->setCharBackgroundColor(COLOR_BLACK);
        tft->setCharColor(COLOR_WHITE);
        tft->setCursor(0, 9);
        if (presetNumber < 100) {
            tft->print(' ');
        }
        if (presetNumber < 10) {
            tft->print(' ');
        }
        tft->print(presetNumber);
        tft->print(' ');
        tft->print(name);
        tft->printSpaceTillEndOfLine();
    } else {
        tft->fillArea(0, 180, 240, 20, COLOR_BLACK);
    }
}

void FMDisplayMenu::printScalaFrequency(float value) {
    int integer = (int) (value + .0005f);
    tft->print(integer);
    tft->print('.');
    value -= integer;
    tft->print((int) (value * 10 + .0005f));
}

void FMDisplayMenu::setPreviousSynthMode(int synthModeBeforeMenu) {
    this->previousSynthMode = synthModeBeforeMenu;
}
