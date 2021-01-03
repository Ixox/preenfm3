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

const char *enveloppeRandomizer[] = { " -- ", "perc", "pad ", "rand" };
const char *otherRandomizer[] = { " -- ", "soft", "medi", "high" };

void FMDisplayMenu::init(SynthState *synthState, TftDisplay *tft, Storage *storage) {
    synthState_ = synthState;
    tft_ = tft;
    storage_ = storage;
    previousConfigSetting_ = 255;
    menuStateRow_ = 3;
}

void FMDisplayMenu::refreshMenuByStep(int currentTimbre, int refreshStatus) {

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
            tft_->pauseRefresh();
            tft_->clear();
            tft_->fillArea(0, 0, 240, 21, COLOR_DARK_RED);
            tft_->setCursorInPixel(2, 2);
            tft_->print("M E N U", COLOR_YELLOW, COLOR_DARK_RED);

            tft_->setCharBackgroundColor(COLOR_BLACK);
            tft_->setCharColor(COLOR_LIGHT_GRAY);
            tft_->setCursorInPixel(240 - 7 * PFM3_FIRMWARE_VERSION_STRLEN, 25);
            tft_->printSmallChars(PFM3_FIRMWARE_VERSION);

#ifdef DEBUG
            tft_->setCharColor(COLOR_RED);
            tft_->setCursorInPixel(5, 25);
            tft_->printSmallChars("DEBUG");
#endif
            break;
        case 6:
        case 5:
        case 4:
        case 3:
        case 2:
        case 1: {
            int button = 6 - refreshStatus;

            switch (synthState_->fullState.currentMenuItem->menuType) {

                case MENUTYPE_WITHSUBMENU:
                    if (button < synthState_->fullState.currentMenuItem->maxValue) {
                        const char *name = MenuItemUtil::getMenuItem(synthState_->fullState.currentMenuItem->subMenu[button])->name;
                        tft_->drawButton(name, 270, 29, button, 0, 1, COLOR_DARK_RED);
                        return;
                    }
                    break;
                case MENUTYPE_ENTERNAME:
                    switch (button) {
                        case 0:
                            tft_->drawButton("Save", 270, 29, button, 0, 1, COLOR_DARK_RED);
                            return;
                        case 1:
                            tft_->drawSimpleButton("' '", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
                            return;
                        case 2:
                            tft_->drawSimpleButton("'0'", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
                            return;
                        case 3:
                            tft_->drawSimpleButton("'A'", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
                            return;
                        case 4:
                            tft_->drawSimpleButton("'a'", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
                            return;
                        case 5:
                            tft_->drawSimpleButton(">", 270, 29, button, COLOR_GRAY, COLOR_DARK_RED);
                            return;
                    }
                    // Then draw Load button 0 => so no break
                case MENUTYPE_FILESELECT_LOAD:
                case MENUTYPE_FILESELECT_SAVE:
                    if (button == 0) {
                        tft_->drawButton(synthState_->fullState.currentMenuItem->menuType == MENUTYPE_FILESELECT_LOAD ? "Load" : "Select", 270, 29, button, 0,
                            1, COLOR_DARK_RED);
                        return;
                    }
                    break;
                case MENUTYPE_RANDOMIZER:
                    switch (button) {
                        case 0:
                            tft_->drawButton("Apply", 270, 29, button, 0, 1, COLOR_DARK_RED);
                            return;
                        case 1:
                            tft_->drawSimpleButton("Rand", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
                            return;
                    }
                    break;
                case MENUTYPE_SETTINGS:
                    if (button == 0) {
                        tft_->drawButton("Save", 270, 29, button, 0, 1, COLOR_DARK_RED);
                        return;
                    }
                    break;
                case MENUTYPE_CONFIRM:
                    if (button == 0) {
                        // Default Empty button
                        tft_->drawSimpleButton("Yes", 270, 29, button, COLOR_YELLOW, COLOR_DARK_RED);
                        return;
                    }
                    break;
                default:
                    break;
            }

            if (button == 5) {
                switch (synthState_->fullState.currentMenuItem->menuState) {
                    case MAIN_MENU:
                    case MENU_MIXER:
                    case MENU_SEQUENCER:
                    case MENU_PRESET:
                    case MENU_DEFAULT_SEQUENCER:
                    case MENU_DEFAULT_MIXER: {
                        const char *name =
                            MenuItemUtil::getMenuItem(synthState_->fullState.currentMenuItem->subMenu[synthState_->fullState.previousChoice])->name;
                        tft_->drawSimpleBorderButton(name, 270, 29, button, COLOR_LIGHT_GRAY, COLOR_DARK_RED);
                        break;
                    }
                    default:
                        // Default Empty button
                        tft_->drawSimpleButton("", 270, 29, button, COLOR_RED, COLOR_DARK_RED);
                        break;
                }
            } else {
                // Default Empty button
                tft_->drawButton("", 270, 29, button, 0, 1, COLOR_DARK_RED);
            }

            break;
        }
    }
}

void FMDisplayMenu::encoderTurned(int currentTimbre, int encoder, int ticks) {
    struct FullState *fullState = &synthState_->fullState;

    switch (fullState->currentMenuItem->menuState) {
        case MENU_CONFIG_SETTINGS:
            if (encoder == 3) {
                changePresetSelect(&fullState->midiConfigValue[fullState->menuSelect], ticks, midiConfig[fullState->menuSelect].maxValue - 1);
            } else if (encoder == 0) {
                changePresetSelect(&fullState->menuSelect, ticks, MIDICONFIG_SIZE);
            }
            break;
        case MENU_PRESET_LOAD_SELECT:
            if (encoder == 0) {
                // Did we really change bank?
                if (changePresetSelect(&fullState->preenFMBankNumber, ticks, (int) fullState->currentMenuItem->maxValue - 1)) {
                    fullState->preenFMPresetNumber = 0;
                    fullState->preenFMBank = synthState_->getStorage()->getPatchBank()->getFile(fullState->preenFMBankNumber);
                    synthState_->loadPreset(currentTimbre, fullState->preenFMBank, fullState->preenFMPresetNumber, synthState_->params);
                }
            } else if (encoder == 3) {
                if (changePresetSelect(&fullState->preenFMPresetNumber, ticks, 127)) {
                    synthState_->loadPreset(currentTimbre, fullState->preenFMBank, fullState->preenFMPresetNumber, synthState_->params);
                }
            }
            break;
        case MENU_PRESET_LOAD_SELECT_DX7_BANK:
            if (encoder == 0) {
                // Did we really change bank?
                if (changePresetSelect(&fullState->dx7BankNumber, ticks, (int) fullState->currentMenuItem->maxValue - 1)) {
                    fullState->preenFMBank = synthState_->getStorage()->getDX7SysexFile()->getFile(fullState->dx7BankNumber);
                    fullState->dx7PresetNumber = 0;
                    synthState_->loadDx7Patch(currentTimbre, fullState->preenFMBank, fullState->dx7PresetNumber, synthState_->params);
                }
            } else if (encoder == 3) {
                if (changePresetSelect(&fullState->dx7PresetNumber, ticks, 31)) {
                    synthState_->loadDx7Patch(currentTimbre, fullState->preenFMBank, fullState->dx7PresetNumber, synthState_->params);
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
                    fullState->preenFMMixer = synthState_->getStorage()->getMixerBank()->getFile(fullState->preenFMMixerNumber);
                }
            } else if (encoder == 3) {
                changePresetSelect(&fullState->preenFMMixerPresetNumber, ticks, NUMBER_OF_MIXERS_PER_BANK - 1);
            }
            break;
        case MENU_SD_RENAME_PRESET_SELECT_FILE:
        case MENU_PRESET_SAVE_SELECT:
            if (encoder == 0) {
                // Did we really change bank?
                if (changePresetSelect(&fullState->preenFMBankNumber, ticks, (int) fullState->currentMenuItem->maxValue - 1)) {
                    fullState->preenFMPresetNumber = 0;
                    fullState->preenFMBank = synthState_->getStorage()->getPatchBank()->getFile(fullState->preenFMBankNumber);
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
                    fullState->preenFMSequence = synthState_->getStorage()->getSequenceBank()->getFile(fullState->preenFMSeqBankNumber);
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
                changePresetSelect(&fullState->menuSelect, ticks, 11);
            } else if (encoder == 3) {
                changeCharSelect(&fullState->name[fullState->menuSelect], ticks);
            }
            break;
        case MENU_PRESET_RANDOMIZER:
            if (synthState_->newRandomizerValue(encoder, ticks)) {
                // Only propagate if value really changed
                synthState_->propagateNewMenuSelect();
            }
            return;
            break;
        default:
            break;
    }

    synthState_->propagateNewMenuSelect();
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
    struct FullState *fullState = &synthState_->fullState;
    int32_t length;

    if (button == BUTTON_PFM3_MENU) {
        if (fullState->currentMenuItem->menuState != MAIN_MENU) {
            const MenuItem *oldMenuItem = fullState->currentMenuItem;
            fullState->currentMenuItem = getMenuBack(currentTimbre);
            updatePreviousChoice(fullState->currentMenuItem->menuState);
            synthState_->propagateMenuBack(oldMenuItem);
        }
        return;
    }

    if (button < BUTTON_PFM3_1 || button > BUTTON_PFM3_6) {
        return;
    }

    // Do not do anythin in the following case
    if (fullState->currentMenuItem->menuType == MENUTYPE_FILESELECT_SAVE || fullState->currentMenuItem->menuType == MENUTYPE_FILESELECT_LOAD) {
        // Seq
        if (fullState->currentMenuItem->menuState == MENU_SEQUENCER_SAVE_SELECT && fullState->preenFMSequence->fileType != FILE_OK) {
            return;
        }
        if (fullState->currentMenuItem->menuState == MENU_SEQUENCER_LOAD_SELECT && fullState->preenFMSequence->fileType == FILE_EMPTY) {
            return;
        }
        // Mixer
        if (fullState->currentMenuItem->menuState == MENU_MIXER_SAVE_SELECT && fullState->preenFMMixer->fileType != FILE_OK) {
            return;
        }
        if (fullState->currentMenuItem->menuState == MENU_MIXER_LOAD_SELECT && fullState->preenFMMixer->fileType == FILE_EMPTY) {
            return;
        }
        // Preset
        if (fullState->currentMenuItem->menuState == MENU_PRESET_SAVE_SELECT && fullState->preenFMBank->fileType != FILE_OK) {
            return;
        }
        if (fullState->currentMenuItem->menuState == MENU_PRESET_LOAD_SELECT && fullState->preenFMBank->fileType == FILE_EMPTY) {
            return;
        }
    }

    // Special treatment for Randomizer
    if (unlikely(fullState->currentMenuItem->menuState == MENU_PRESET_RANDOMIZER)) {
        if (button == 1) {
            synthState_->storeTestNote();
            synthState_->propagateNoteOff();
            synthState_->propagateBeforeNewParamsLoad(currentTimbre);
            synthState_->randomizePreset();
            synthState_->propagateAfterNewParamsLoad(currentTimbre);
            synthState_->restoreTestNote();
            return;
        }
    }

    // Enter name ?
    if (fullState->currentMenuItem->menuType == MENUTYPE_ENTERNAME) {
        if (button >= 1 && button <= 4) {
            char staticChars[4] = { ' ', '0', 'A', 'a' };
            fullState->name[fullState->menuSelect] = staticChars[button - 1];
            synthState_->propagateNewMenuSelect();
            return;
        } else if (button == 5) {
            if (fullState->menuSelect < 11) {
                fullState->menuSelect++;
				fullState->name[fullState->menuSelect] = fullState->name[fullState->menuSelect-1];
            }
            synthState_->propagateNewMenuSelect();
            return;
        }
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
            case MENU_DEFAULT_MIXER:
                button = fullState->previousMenuChoice.defltMix;
                break;
            case MENU_DEFAULT_SEQUENCER:
                button = fullState->previousMenuChoice.defltSeq;
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
            case MENU_DEFAULT_MIXER:
                fullState->previousMenuChoice.defltMix = button;
                break;
            case MENU_DEFAULT_SEQUENCER:
                fullState->previousMenuChoice.defltSeq = button;
                break;
            default:
                break;
        }
    }

    const MenuItem *nextMenu = MenuItemUtil::getMenuItem(fullState->currentMenuItem->subMenu[button]);

    // If Previous state was the following we have some action to do
    if (nextMenu->menuState == MENU_DONE) {
        switch (oldMenu) {
            case MENU_PRESET_SAVE_ENTER_NAME:
                // Let save the preset !
                for (length = 12; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                    ;
                for (int k = 0; k < length; k++) {
                    synthState_->params->presetName[k] = fullState->name[k];
                }
                synthState_->params->presetName[length] = '\0';
                synthState_->getStorage()->getPatchBank()->savePatch(fullState->preenFMBank, fullState->preenFMPresetNumber, synthState_->params);
                break;
            case MENU_MIXER_SAVE_ENTER_NAME:
                // Let save the mixer !
                for (length = 12; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                    ;
                fullState->name[length] = '\0';
                synthState_->getStorage()->getMixerBank()->saveMixer(fullState->preenFMMixer, fullState->preenFMMixerPresetNumber, fullState->name);
                break;
            case MENU_SEQUENCER_SAVE_ENTER_NAME:
                // Let save the sequence !
                for (length = 12; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                    ;
                fullState->name[length] = '\0';
                synthState_->getStorage()->getSequenceBank()->saveSequence(fullState->preenFMSequence, fullState->preenFMSeqSequencerNumber, fullState->name);
                break;
            case MENU_MIXER_LOAD_SELECT:
                synthState_->loadMixer(fullState->preenFMMixer, fullState->preenFMMixerPresetNumber);
                break;
            case MENU_SEQUENCER_LOAD_SELECT:
                synthState_->getStorage()->getSequenceBank()->loadSequence(fullState->preenFMSequence, fullState->preenFMSeqSequencerNumber);
                break;
            case MENU_CONFIG_SETTINGS:
                // Let save the preset !
                synthState_->getStorage()->getConfigurationFile()->saveConfig(fullState->midiConfigValue);
                break;
            case MENU_SD_CREATE_MIXER_FILE:
                // Must create the mixer here....
                for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                    ;
                length += copyStringAt(fullState->name, length, ".mix");
                fullState->name[length] = '\0';

                if (!synthState_->getStorage()->getMixerBank()->nameExists(fullState->name)) {
                    fullState->currentMenuItem = MenuItemUtil::getMenuItem(MENU_IN_PROGRESS);
                    // display "Wait"
                    synthState_->propagateNewMenuSelect();
                    synthState_->getStorage()->getMixerBank()->createMixerBank(fullState->name);
                } else {
                    nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
                }
                break;
            case MENU_SD_CREATE_SEQUENCE_FILE:
                // Must create the sequence file here....
                for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                    ;
                length += copyStringAt(fullState->name, length, ".seq");
                fullState->name[length] = '\0';

                if (!synthState_->getStorage()->getSequenceBank()->nameExists(fullState->name)) {
                    fullState->currentMenuItem = MenuItemUtil::getMenuItem(MENU_IN_PROGRESS);
                    // display "Wait"
                    synthState_->propagateNewMenuSelect();
                    synthState_->getStorage()->getSequenceBank()->createSequenceFile(fullState->name);
                } else {
                    nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
                }
                break;
            case MENU_SD_CREATE_PRESET_FILE:
                // Must create the bank here....
                for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                    ;
                length += copyStringAt(fullState->name, length, ".bnk");
                fullState->name[length] = '\0';

                if (!synthState_->getStorage()->getPatchBank()->nameExists(fullState->name)) {
                    fullState->currentMenuItem = MenuItemUtil::getMenuItem(MENU_IN_PROGRESS);
                    // display "Wait"
                    synthState_->propagateNewMenuSelect();
                    synthState_->getStorage()->getPatchBank()->createPatchBank(fullState->name);
                } else {
                    nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
                }
                break;
            case MENU_SD_RENAME_MIXER_ENTER_NAME:

                for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                    ;
                length += copyStringAt(fullState->name, length, ".mix");
                fullState->name[length] = '\0';

                if (synthState_->getStorage()->getMixerBank()->renameFile(fullState->preenFMMixer, fullState->name) > 0) {
                    nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
                }
                break;
            case MENU_SD_RENAME_PRESET_ENTER_NAME:

                for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                    ;
                length += copyStringAt(fullState->name, length, ".bnk");
                fullState->name[length] = '\0';

                if (synthState_->getStorage()->getPatchBank()->renameFile(fullState->preenFMBank, fullState->name) > 0) {
                    nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
                }
                break;
            case MENU_SD_RENAME_SEQUENCE_ENTER_NAME:
                for (length = 8; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                    ;
                length += copyStringAt(fullState->name, length, ".seq");
                fullState->name[length] = '\0';

                if (synthState_->getStorage()->getSequenceBank()->renameFile(fullState->preenFMSequence, fullState->name) > 0) {
                    nextMenu = MenuItemUtil::getMenuItem(MENU_ERROR);
                }
                break;
                break;
            case MENU_DEFAULT_MIXER_SAVE:
                synthState_->getStorage()->getMixerBank()->saveDefaultMixer();
                break;
            case MENU_DEFAULT_MIXER_DELETE:
                synthState_->getStorage()->getMixerBank()->removeDefaultMixer();
                break;
            case MENU_DEFAULT_MIXER_LOAD:
                synthState_->getStorage()->getMixerBank()->loadDefaultMixer();
                break;
            case MENU_DEFAULT_SEQUENCER_SAVE:
                synthState_->getStorage()->getSequenceBank()->saveDefaultSequence();
                break;
            case MENU_DEFAULT_SEQUENCER_DELETE:
                synthState_->getStorage()->getSequenceBank()->removeDefaultSequence();
                break;
            case MENU_DEFAULT_SEQUENCER_LOAD:
                synthState_->getStorage()->getSequenceBank()->loadDefaultSequence();
                break;
            case MENU_PRESET_NEW:
                synthState_->loadNewPreset(currentTimbre);
                break;
            default:
                break;

        }
    }

    // Action depending on Next menu
    updatePreviousChoice(nextMenu->menuState);

    switch (nextMenu->menuState) {
        case MENU_PRESET_LOAD_SELECT:
            copySynthParams((char*) synthState_->params, (char*) &synthState_->backupParams);
            fullState->preenFMBank = synthState_->getStorage()->getPatchBank()->getFile(fullState->preenFMBankNumber);
            synthState_->loadPreset(currentTimbre, fullState->preenFMBank, fullState->preenFMPresetNumber, synthState_->params);
            break;
        case MENU_PRESET_LOAD_SELECT_DX7_BANK:
            copySynthParams((char*) synthState_->params, (char*) &synthState_->backupParams);
            fullState->preenFMBank = synthState_->getStorage()->getDX7SysexFile()->getFile(fullState->dx7BankNumber);
            synthState_->loadDx7Patch(currentTimbre, fullState->preenFMBank, fullState->dx7PresetNumber, synthState_->params);
            break;
        case MENU_SD_RENAME_MIXER_SELECT_FILE:
        case MENU_MIXER_LOAD_SELECT:
        case MENU_MIXER_SAVE_SELECT:
            fullState->preenFMMixer = synthState_->getStorage()->getMixerBank()->getFile(fullState->preenFMMixerNumber);
            break;
        case MENU_SD_RENAME_PRESET_SELECT_FILE:
        case MENU_PRESET_SAVE_SELECT:
            fullState->preenFMBank = synthState_->getStorage()->getPatchBank()->getFile(fullState->preenFMBankNumber);
            break;
        case MENU_SD_RENAME_SEQUENCE_SELECT_FILE:
        case MENU_SEQUENCER_SAVE_SELECT:
        case MENU_SEQUENCER_LOAD_SELECT:
            fullState->preenFMSequence = synthState_->getStorage()->getSequenceBank()->getFile(fullState->preenFMSeqBankNumber);
            break;
        case MENU_PRESET_SAVE_ENTER_NAME:
            copyAndTransformString(fullState->name, synthState_->params->presetName);
            break;
        case MENU_MIXER_SAVE_ENTER_NAME:
            copyAndTransformString(fullState->name, synthState_->mixerState.mixName_);
            break;
        case MENU_SEQUENCER_SAVE_ENTER_NAME:
            copyAndTransformString(fullState->name, synthState_->getSequenceName());
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
            copySynthParams((char*) synthState_->params, (char*) &synthState_->backupParams);
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
    synthState_->propagateNewMenuState();
    synthState_->propagateNewMenuSelect();

}

void FMDisplayMenu::updatePreviousChoice(uint8_t menuState) {
    struct FullState *fullState = &synthState_->fullState;

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
        case MENU_DEFAULT_MIXER:
            fullState->previousChoice = fullState->previousMenuChoice.defltMix;
            break;
        case MENU_DEFAULT_SEQUENCER:
            fullState->previousChoice = fullState->previousMenuChoice.defltSeq;
            break;
        default:
            break;
    }
}

const MenuItem* FMDisplayMenu::getMenuBack(int currentTimbre) {
    struct FullState *fullState = &synthState_->fullState;

    const MenuItem *rMenuItem = 0;

    // default menuSelect value
    fullState->menuSelect = 0; // MenuItemUtil::getParentMenuSelect(fullState->currentMenuItem->menuState);

    switch (fullState->currentMenuItem->menuState) {
        case MENU_PRESET_RANDOMIZER:
            // Put back original preset
            synthState_->propagateNoteOff();
            synthState_->propagateBeforeNewParamsLoad(currentTimbre);
            copySynthParams((char*) &synthState_->backupParams, (char*) synthState_->params);
            synthState_->propagateAfterNewParamsLoad(currentTimbre);
            break;
        case MENU_PRESET_LOAD_SELECT:
        case MENU_PRESET_LOAD_SELECT_DX7_BANK:
            synthState_->propagateNoteOff();
            synthState_->propagateBeforeNewParamsLoad(currentTimbre);
            // Put back original preset
            copySynthParams((char*) &synthState_->backupParams, (char*) synthState_->params);
            synthState_->propagateAfterNewParamsLoad(currentTimbre);
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

void FMDisplayMenu::copyAndTransformString(char *dest, const char *source) {
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

void FMDisplayMenu::copySynthParams(char *source, char *dest) {
    for (uint32_t k = 0; k < sizeof(struct OneSynthParams); k++) {
        dest[k] = source[k];
    }
}

void FMDisplayMenu::newMenuState(FullState *fullState) {
    tft_->pauseRefresh();

    if (MenuItemUtil::getParentMenuItem(synthState_->fullState.currentMenuItem->menuState)->menuState == MAIN_MENU) {
        menuStateRow_ = 1;
    }

    menuStateRow_++;

    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_RED);

    tft_->setCursor((menuStateRow_ - 1), menuStateRow_);
    tft_->print(synthState_->fullState.currentMenuItem->name);
    switch (synthState_->fullState.currentMenuItem->menuState) {
        case MENU_PRESET_LOAD_SELECT:
            tft_->setCharColor(COLOR_YELLOW);
            tft_->print(" in I");
            tft_->print(synthState_->getCurrentTimbre() + 1);
            tft_->setCharColor(COLOR_RED);
            break;
        case MENU_PRESET_SAVE_SELECT:
        case MENU_PRESET_RANDOMIZER:
            tft_->print(" ");
            tft_->print(synthState_->params->presetName);
            break;

        case MENU_DEFAULT_MIXER_LOAD:
            tft_->setCharColor(COLOR_LIGHT_GRAY);
            tft_->setCursor(0, 12);
            tft_->print("Load default Mixer");
            break;
        case MENU_DEFAULT_MIXER_SAVE:
            tft_->setCharColor(COLOR_LIGHT_GRAY);
            tft_->setCursor(0, 12);
            tft_->print("Save default Mixer");
            break;
        case MENU_DEFAULT_MIXER_DELETE:
            tft_->setCharColor(COLOR_LIGHT_GRAY);
            tft_->setCursor(0, 12);
            tft_->print("Clear default Mixer");
            break;
        case MENU_DEFAULT_SEQUENCER_LOAD:
            tft_->setCharColor(COLOR_LIGHT_GRAY);
            tft_->setCursor(0, 12);
            tft_->print("Load default Sequencer");
            break;
        case MENU_DEFAULT_SEQUENCER_SAVE:
            tft_->setCharColor(COLOR_LIGHT_GRAY);
            tft_->setCursor(0, 12);
            tft_->print("Save default Sequencer");
            break;
        case MENU_DEFAULT_SEQUENCER_DELETE:
            tft_->setCharColor(COLOR_LIGHT_GRAY);
            tft_->setCursor(0, 12);
            tft_->print("Clear deflt Sequencer");
            break;

        default:
            break;
    }
    displayMenuState(fullState);
}

void FMDisplayMenu::displayMenuState(FullState *fullState) {

    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_GRAY);
    switch (synthState_->fullState.currentMenuItem->menuState) {
        case MENU_PRESET_LOAD_SELECT:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - bank");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("4 - preset");
            break;
        case MENU_PRESET_SAVE_SELECT:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - save in file");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("4 - save in preset");
            break;
        case MENU_SEQUENCER_SAVE_SELECT:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - save in file");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("4 - save in sequence");
            break;
        case MENU_SEQUENCER_LOAD_SELECT:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - load from file");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("4 - sequence");
            break;
        case MENU_MIXER_LOAD_SELECT:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - mixer file");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("4 - mixer");
            break;
        case MENU_MIXER_SAVE_SELECT:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - save in mixer file");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("4 - save in mixer");
            break;
        case MENU_PRESET_LOAD_SELECT_DX7_BANK:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - dx7 sysex file");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("4 - preset");
            break;
        case MENU_PRESET_SAVE_ENTER_NAME:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("    save in file");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - move    4 - change char");
            break;
        case MENU_MIXER_SAVE_ENTER_NAME:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("    save in mixer file");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - move    4 - change char");
            break;
        case MENU_SEQUENCER_SAVE_ENTER_NAME:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("    save in sequence file");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - move    4 - change char");
            break;
        case MENU_PRESET_RANDOMIZER:
            tft_->setCharBackgroundColor(COLOR_BLACK);
            tft_->setCharColor(COLOR_GRAY);
            tft_->setCursor(1, 7);
            tft_->print("Operator  Envelope");
            tft_->setCursor(1, 9);
            tft_->print("Indices   Modulation");
            break;
        case MENU_CONFIG_SETTINGS:
            previousConfigSetting_ = 255;
            break;
        case MENU_SD_CREATE_PRESET_FILE:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 7 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("    Create preset bank");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - move    4 - change char");
            break;
        case MENU_SD_CREATE_MIXER_FILE:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 7 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("    Create mixer bank");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - move    4 - change char");
            break;
        case MENU_SD_CREATE_SEQUENCE_FILE:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 7 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("    Create sequence file");
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - move    4 - change char");
            break;
        case MENU_SD_RENAME_MIXER_ENTER_NAME:
        case MENU_SD_RENAME_PRESET_ENTER_NAME:
        case MENU_SD_RENAME_SEQUENCE_ENTER_NAME:
            tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
            tft_->printSmallChars("1 - move    4 - change char");
            break;
        default:
            break;
    }
}

void FMDisplayMenu::newMenuSelect(FullState *fullState) {

    int length;

    switch (fullState->currentMenuItem->menuState) {
        case MENU_PRESET_LOAD_SELECT_DX7_BANK:
            displayBankSelect(fullState->dx7BankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY), fullState->preenFMBank->name);
            if (fullState->preenFMBank->fileType != FILE_EMPTY) {
                displayPatchSelect(fullState->dx7PresetNumber, synthState_->params->presetName);
            } else {
                displayPatchSelect(0, 0);
            }
            break;
        case MENU_PRESET_LOAD_SELECT:
            displayBankSelect(fullState->preenFMBankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY), fullState->preenFMBank->name);
            if (fullState->preenFMBank->fileType != FILE_EMPTY) {
                displayPatchSelect(fullState->preenFMPresetNumber, synthState_->params->presetName);
            } else {
                displayPatchSelect(0, 0);
            }
            break;
        case MENU_MIXER_LOAD_SELECT:
        case MENU_MIXER_SAVE_SELECT:
            displayBankSelect(fullState->preenFMMixerNumber, (fullState->preenFMMixer->fileType != FILE_EMPTY), fullState->preenFMMixer->name);
            if (fullState->preenFMMixer->fileType != FILE_EMPTY) {
                displayPatchSelect(fullState->preenFMMixerPresetNumber,
                    storage_->getMixerBank()->loadMixerName(fullState->preenFMMixer, fullState->preenFMMixerPresetNumber));
            } else {
                displayPatchSelect(0, 0);
            }
            break;
        case MENU_ERROR:
        case MENU_CANCEL:
            tft_->setCharColor(COLOR_RED);
            goto displayStateMessage;
        case MENU_IN_PROGRESS:
        case MENU_DONE:
            tft_->setCharColor(COLOR_YELLOW);
displayStateMessage: tft_->fillArea(0, 12 * TFT_BIG_CHAR_HEIGHT, 240, TFT_BIG_CHAR_HEIGHT, COLOR_BLACK);
            tft_->setCharBackgroundColor(COLOR_BLACK);
            tft_->setCharColor(COLOR_YELLOW);
            tft_->setCursor(8, 12);
            tft_->print(fullState->currentMenuItem->name);
            break;
        case MENU_PRESET_SAVE_SELECT:
            displayBankSelect(fullState->preenFMBankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY), fullState->preenFMBank->name);
            if (fullState->preenFMBank->fileType != FILE_EMPTY) {
                displayPatchSelect(fullState->preenFMPresetNumber,
                    storage_->getPatchBank()->loadPatchName(fullState->preenFMBank, fullState->preenFMPresetNumber));
            } else {
                displayPatchSelect(0, 0);
            }
            break;
        case MENU_SEQUENCER_SAVE_SELECT:
            displayBankSelect(fullState->preenFMSeqBankNumber, (fullState->preenFMSequence->fileType == FILE_OK), fullState->preenFMSequence->name);
            if (fullState->preenFMSequence->fileType == FILE_OK) {
                displayPatchSelect(fullState->preenFMSeqSequencerNumber,
                    storage_->getSequenceBank()->loadSequenceName(fullState->preenFMSequence, fullState->preenFMSeqSequencerNumber));
            } else {
                displayPatchSelect(0, 0);
            }
            break;
        case MENU_SEQUENCER_LOAD_SELECT:
            displayBankSelect(fullState->preenFMSeqBankNumber, (fullState->preenFMSequence->fileType != FILE_EMPTY), fullState->preenFMSequence->name);
            if (fullState->preenFMSequence->fileType != FILE_EMPTY) {
                displayPatchSelect(fullState->preenFMSeqSequencerNumber,
                    storage_->getSequenceBank()->loadSequenceName(fullState->preenFMSequence, fullState->preenFMSeqSequencerNumber));
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

displayChars: tft_->setCharBackgroundColor(COLOR_BLACK);
            tft_->setCursor(4, 9);
            for (int k = 0; k < length; k++) {
                if (likely(k != fullState->menuSelect)) {
                    tft_->setCharColor(COLOR_WHITE);
                    tft_->print(fullState->name[k]);
                } else {
                    tft_->setCharColor(COLOR_RED);
                    if (unlikely(fullState->name[fullState->menuSelect] == ' ')) {
                        tft_->print('_');
                    } else {
                        tft_->print(fullState->name[fullState->menuSelect]);
                    }
                }
            }
            break;
        case MENU_PRESET_RANDOMIZER:
            tft_->setCharBackgroundColor(COLOR_BLACK);
            tft_->setCharColor(COLOR_WHITE);
            tft_->setCursor(3, 8);
            tft_->print(otherRandomizer[(int) fullState->randomizer.Oper]);
            tft_->setCursor(13, 8);
            tft_->print(enveloppeRandomizer[(int) fullState->randomizer.EnvT]);
            tft_->setCursor(3, 10);
            tft_->print(otherRandomizer[(int) fullState->randomizer.IM]);
            tft_->setCursor(13, 10);
            tft_->print(otherRandomizer[(int) fullState->randomizer.Modl]);
            break;
        case MENU_CONFIG_SETTINGS:
            if (previousConfigSetting_ != fullState->menuSelect) {
                tft_->pauseRefresh();
                tft_->fillArea(0, 160, 240, 40, COLOR_BLACK);

                tft_->setCursor(1, 8);
                tft_->setCharColor(COLOR_GRAY);
                tft_->printSmallChar('1');

                tft_->setCharColor(COLOR_WHITE);
                tft_->setCursor(2, 8);
                tft_->print(midiConfig[fullState->menuSelect].title);

                tft_->setCursor(1, 9);
                tft_->setCharColor(COLOR_GRAY);
                tft_->printSmallChar('4');
            } else {
                tft_->fillArea(30, 180, 210, 20, COLOR_BLACK);
            }
            tft_->setCursor(2, 9);
            tft_->setCharColor(COLOR_YELLOW);
            if (midiConfig[fullState->menuSelect].valueName != NULL) {
                tft_->print(midiConfig[fullState->menuSelect].valueName[fullState->midiConfigValue[fullState->menuSelect]]);
            } else {
                tft_->print((int) fullState->midiConfigValue[fullState->menuSelect]);
            }
            if (previousConfigSetting_ != fullState->menuSelect) {
                tft_->restartRefreshTft();
                previousConfigSetting_ = fullState->menuSelect;
            }
            break;

        case MENU_SD_RENAME_PRESET_SELECT_FILE:
            displayBankSelect(fullState->preenFMBankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY), fullState->preenFMBank->name);
            break;
        case MENU_SD_RENAME_MIXER_SELECT_FILE:
            displayBankSelect(fullState->preenFMMixerNumber, (fullState->preenFMMixer->fileType != FILE_EMPTY), fullState->preenFMMixer->name);
            break;
        case MENU_SD_RENAME_SEQUENCE_SELECT_FILE:
            displayBankSelect(fullState->preenFMSeqBankNumber, (fullState->preenFMSequence->fileType != FILE_EMPTY), fullState->preenFMSequence->name);
            break;
        default:
            break;
    }
}

void FMDisplayMenu::menuBack(const MenuItem *oldMenuItem, FullState *fullState) {

    int y = menuStateRow_ * TFT_BIG_CHAR_HEIGHT;
    tft_->fillArea(0, y, 240, 260 - y, COLOR_BLACK);

    menuStateRow_--;

    displayMenuState(fullState);
    newMenuSelect(fullState);
}

void FMDisplayMenu::displayBankSelect(int bankNumber, bool usable, const char *name) {

    bankNumber++;

    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(usable ? COLOR_WHITE : COLOR_GRAY);
    tft_->setCursor(0, 7);
    if (bankNumber < 100) {
        tft_->print(' ');
    }
    if (bankNumber < 10) {
        tft_->print(' ');
    }
    tft_->print(bankNumber);
    tft_->print(' ');
    tft_->print(name);
    tft_->printSpaceTillEndOfLine();
}

void FMDisplayMenu::displayPatchSelect(int presetNumber, const char *name) {
    presetNumber++;

    if (name != 0) {
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(COLOR_WHITE);
        tft_->setCursor(0, 9);
        if (presetNumber < 100) {
            tft_->print(' ');
        }
        if (presetNumber < 10) {
            tft_->print(' ');
        }
        tft_->print(presetNumber);
        tft_->print(' ');
        tft_->print(name);
        tft_->printSpaceTillEndOfLine();
    } else {
        tft_->fillArea(0, 180, 240, 20, COLOR_BLACK);
    }
}

void FMDisplayMenu::printScalaFrequency(float value) {
    int integer = (int) (value + .0005f);
    tft_->print(integer);
    tft_->print('.');
    value -= integer;
    tft_->print((int) (value * 10 + .0005f));
}

void FMDisplayMenu::setPreviousSynthMode(int synthModeBeforeMenu) {
    previousSynthMode_ = synthModeBeforeMenu;
}
