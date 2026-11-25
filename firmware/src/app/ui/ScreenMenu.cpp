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

#include "ScreenMenu.h"
#include "PersistenceMagic.h"
#include "PreenFMFileType.h"
#include "SynthState.h"
#include "TftDisplay.h"
#include "Timbre.h"
#include "version.h"

extern int getLength(const char* str);

struct TimbreBackup {
    SynthEngineType engine;
    uint32_t magic;
    uint32_t size;
    uint8_t blob[ALIGNED_PATCH_SIZE - 5];
};

static TimbreBackup backupTimbre_ __attribute__((section(".ram_d3"), aligned(4)));

const char* enveloppeRandomizer[] = {" -- ", "perc", "pad ", "rand"};
const char* otherRandomizer[] = {" -- ", "soft", "medi", "high"};

#define CONFIG_VALUE_AREA_Y 64
#define CONFIG_VALUE_AREA_H 52
#define CONFIG_TITLE_Y      66
#define CONFIG_VALUE_Y      80
#define CONFIG_LIST_Y       120
#define CONFIG_ROW_H        20
#define CONFIG_VISIBLE_ROWS 7
#define CONFIG_AREA_H       196

void ScreenMenu::init(SynthState* synthState, TftDisplay* tft, Storage* storage) {
    synthState_ = synthState;
    tft_ = tft;
    storage_ = storage;
    menuStateRow_ = 3;

    configScrollOffset_ = 0;
    configPrevSelect_ = 0xFF;
    configPrevOffset_ = 0;
    configPrevValue_ = 0;
    configPrevEditMode_ = false;
    configPrevDirty_ = false;
    configEditMode_ = false;
    configEditOldValue_ = 0;
    configEditValue_ = 0;
    configBaselineValid_ = false;
    configPendingClear_ = false;
    configPendingRows_ = 0;
    configPendingValue_ = false;
}

void ScreenMenu::backupTimbre(int timbre) {
    Timbre* t = synthState_->getTimbre(timbre);
    backupTimbre_.engine = t->getEngine();
    backupTimbre_.magic = t->getStorageMagic();
    backupTimbre_.size = t->getStorageBlob(backupTimbre_.blob, sizeof(backupTimbre_.blob));
}

bool ScreenMenu::restoreTimbre(int timbre) {
    if (backupTimbre_.size == 0) {
        return false;
    }
    Timbre* t = synthState_->getTimbre(timbre);
    if (!t->loadStorageBlob(backupTimbre_.blob, backupTimbre_.size, backupTimbre_.engine, backupTimbre_.magic)) {
        return false;
    }
    synthState_->mixerState_.instrumentState_[timbre].engine = (uint8_t)t->getEngine();
    return true;
}

void ScreenMenu::refreshMenuByStep(int currentTimbre, int refreshStatus) {

    // Config list is drawn one small chunk per step so that the 128-entry
    // TFT action queue never overflows.
    if (synthState_->fullState_.currentMenuItem->menuState == MENU_CONFIG_SETTINGS && refreshStatus >= 1 && refreshStatus <= 6) {
        if (configPendingClear_) {
            tft_->fillArea(0, CONFIG_VALUE_AREA_Y, 240, CONFIG_AREA_H, COLOR_BLACK);
            configPendingClear_ = false;
            if (refreshStatusP_ != nullptr) {
                *refreshStatusP_ = 7;
            }
            return;
        }
        if (configPendingRows_ != 0) {
            uint8_t row = 0;
            while ((configPendingRows_ & (1 << row)) == 0) {
                row++;
            }
            configPendingRows_ &= (uint8_t)~(1 << row);
            drawConfigRowTitle(row);
            drawConfigRowValue(row);
            if (refreshStatusP_ != nullptr) {
                *refreshStatusP_ = 7;
            }
            return;
        }
        if (configPendingValue_) {
            drawConfigValueLine();
            configPendingValue_ = false;
            if (refreshStatusP_ != nullptr) {
                *refreshStatusP_ = 7;
            }
            return;
        }
    }

    // We just pressed the menu button, pre select the first menu
    // Because of "return" in this method we must have the test here
    // We do that when refreshStatus ==6 so that we get the correct button

    switch (refreshStatus) {
    case 20:
        tft_->pauseRefresh();
        tft_->clear();
        tft_->fillArea(0, 0, 240, 21, COLOR_RED_DARK);
        tft_->setCursorInPixel(2, 2);
        tft_->print("M E N U", COLOR_YELLOW, COLOR_RED_DARK);

        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(COLOR_GRAY_LIGHT);
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

        switch (synthState_->fullState_.currentMenuItem->menuType) {

        case MENUTYPE_WITHSUBMENU:
            if (button < synthState_->fullState_.currentMenuItem->maxValue) {
                const char* name = MenuItemUtil::getMenuItem(synthState_->fullState_.currentMenuItem->subMenu[button])->name;
                tft_->drawButton(name, 270, 29, button, 0, 1, COLOR_RED_DARK);
                return;
            }
            break;
        case MENUTYPE_ENTERNAME:
            switch (button) {
            case 0:
                tft_->drawButton("Save", 270, 29, button, 0, 1, COLOR_RED_DARK);
                return;
            case 1:
                tft_->drawSimpleButton("' '", 270, 29, button, COLOR_RED, COLOR_RED_DARK);
                return;
            case 2:
                tft_->drawSimpleButton("'0'", 270, 29, button, COLOR_RED, COLOR_RED_DARK);
                return;
            case 3:
                tft_->drawSimpleButton("'A'", 270, 29, button, COLOR_RED, COLOR_RED_DARK);
                return;
            case 4:
                tft_->drawSimpleButton("'a'", 270, 29, button, COLOR_RED, COLOR_RED_DARK);
                return;
            case 5:
                tft_->drawSimpleButton(">", 270, 29, button, COLOR_GRAY, COLOR_RED_DARK);
                return;
            }
            // Then draw Load button 0 => so no break
        case MENUTYPE_FILESELECT_LOAD:
        case MENUTYPE_FILESELECT_SAVE:
            if (button == 0) {
                tft_->drawButton(synthState_->fullState_.currentMenuItem->menuType == MENUTYPE_FILESELECT_LOAD ? "Load" : "Select", 270, 29,
                                 button, 0, 1, COLOR_RED_DARK);
                return;
            }
            break;
        case MENUTYPE_RANDOMIZER:
            switch (button) {
            case 0:
                tft_->drawButton("Apply", 270, 29, button, 0, 1, COLOR_RED_DARK);
                return;
            case 1:
                tft_->drawSimpleButton("Rand", 270, 29, button, COLOR_RED, COLOR_RED_DARK);
                return;
            }
            break;
        case MENUTYPE_SETTINGS:
            switch (button) {
            case BUTTON_SWITCH_1:
                tft_->drawButton("Edit", 270, 29, button, 0, 0, COLOR_RED_DARK,
                                 (!configEditMode_ && configIsEditable((uint8_t)synthState_->fullState_.menuSelect)) ? COLOR_WHITE
                                                                                                                     : COLOR_GRAY_DARK,
                                 COLOR_RED);
                return;
            case BUTTON_SWITCH_2:
                if (configEditMode_) {
                    tft_->drawButton("OK", 270, 29, button, 0, 0, COLOR_RED_DARK, COLOR_WHITE, COLOR_GRAY);
                } else {
                    tft_->drawButton("", 270, 29, button, 0, 0, COLOR_RED_DARK);
                }
                return;
            case BUTTON_SWITCH_3:
                if (configEditMode_) {
                    tft_->drawButton("Cancel", 270, 29, button, 0, 0, COLOR_RED_DARK, COLOR_WHITE, COLOR_GRAY);
                } else {
                    tft_->drawButton("", 270, 29, button, 0, 0, COLOR_RED_DARK);
                }
                return;
            case BUTTON_SWITCH_4:
                tft_->drawButton("", 270, 29, button, 0, 0, COLOR_RED_DARK);
                return;
            case BUTTON_SWITCH_5:
                tft_->drawButton("Back", 270, 29, button, 0, 0, COLOR_RED_DARK, COLOR_WHITE, COLOR_RED);
                return;
            case BUTTON_SWITCH_6:
                tft_->drawButton("Save", 270, 29, button, 0, 0, COLOR_RED_DARK, configIsDirty() ? COLOR_WHITE : COLOR_GRAY_DARK, COLOR_RED);
                return;
            }
            return;
        case MENUTYPE_CONFIRM:
            if (button == 0) {
                // Default Empty button
                tft_->drawSimpleButton("Yes", 270, 29, button, COLOR_YELLOW, COLOR_RED_DARK);
                return;
            }
            break;
        default:
            break;
        }

        if (button == 5) {
            switch (synthState_->fullState_.currentMenuItem->menuState) {
            case MAIN_MENU:
            case MENU_MIXER:
            case MENU_SEQUENCER:
            case MENU_PRESET:
            case MENU_DEFAULT_SEQUENCER:
            case MENU_DEFAULT_MIXER: {
                const char* name =
                    MenuItemUtil::getMenuItem(synthState_->fullState_.currentMenuItem->subMenu[synthState_->fullState_.previousChoice])
                        ->name;
                tft_->drawSimpleBorderButton(name, 270, 29, button, COLOR_GRAY_LIGHT, COLOR_RED_DARK);
                break;
            }
            default:
                // Default Empty button
                tft_->drawSimpleButton("", 270, 29, button, COLOR_RED, COLOR_RED_DARK);
                break;
            }
        } else {
            // Default Empty button
            tft_->drawButton("", 270, 29, button, 0, 1, COLOR_RED_DARK);
        }

        break;
    }
    }
}

void ScreenMenu::encoderTurned(int currentTimbre, int encoder, int ticks) {
    struct FullState* fullState = &synthState_->fullState_;

    switch (fullState->currentMenuItem->menuState) {
    case MENU_CONFIG_SETTINGS:
        if (encoder == 0) {
            if (configEditMode_) {
                int max = (int)midiConfig[fullState->menuSelect].maxValue - 1;
                int newValue = (int)configEditValue_ + ticks;
                if (newValue < 0) {
                    newValue = 0;
                } else if (newValue > max) {
                    newValue = max;
                }
                if (newValue != (int)configEditValue_) {
                    configEditValue_ = (uint8_t)newValue;
                    // USB settings only apply after a reboot: keep them staged.
                    if (fullState->menuSelect != MIDICONFIG_USB && fullState->menuSelect != MIDICONFIG_USB_AUDIO) {
                        fullState->midiConfigValue[fullState->menuSelect] = (uint8_t)newValue;
                        applyConfigSideEffects((uint8_t)fullState->menuSelect);
                    }
                    synthState_->propagateNewMenuSelect();
                }
            } else {
                if (changePresetSelect(&fullState->menuSelect, ticks, MIDICONFIG_SIZE)) {
                    synthState_->propagateNewMenuSelect();
                }
            }
        }
        return;
    case MENU_PRESET_LOAD_SELECT:
        if (encoder == 0) {
            // Did we really change bank?
            if (changePresetSelect(&fullState->preenFMBankNumber, ticks, (int)fullState->currentMenuItem->maxValue - 1)) {
                fullState->preenFMPresetNumber = 0;
                fullState->preenFMBank = synthState_->getStorage()->getPatchBank()->getFile(fullState->preenFMBankNumber);
                synthState_->loadPreset(currentTimbre, fullState->preenFMBank, fullState->preenFMPresetNumber);
            }
        } else if (encoder == 3) {
            if (changePresetSelect(&fullState->preenFMPresetNumber, ticks, 127)) {
                synthState_->loadPreset(currentTimbre, fullState->preenFMBank, fullState->preenFMPresetNumber);
            }
        }
        break;
    case MENU_PRESET_LOAD_SELECT_DX7_BANK:
        if (encoder == 0) {
            // Did we really change bank?
            if (changePresetSelect(&fullState->dx7BankNumber, ticks, (int)fullState->currentMenuItem->maxValue - 1)) {
                fullState->preenFMBank = synthState_->getStorage()->getDX7SysexFile()->getFile(fullState->dx7BankNumber);
                fullState->dx7PresetNumber = 0;
                synthState_->loadDx7Patch(currentTimbre, fullState->preenFMBank, fullState->dx7PresetNumber);
            }
        } else if (encoder == 3) {
            if (changePresetSelect(&fullState->dx7PresetNumber, ticks, 31)) {
                synthState_->loadDx7Patch(currentTimbre, fullState->preenFMBank, fullState->dx7PresetNumber);
            }
        }
        break;
    case MENU_SD_RENAME_MIXER_SELECT_FILE:
    case MENU_MIXER_SAVE_SELECT:
    case MENU_MIXER_LOAD_SELECT:
        if (encoder == 0) {
            // Did we really change bank?
            if (changePresetSelect(&fullState->preenFMMixerNumber, ticks, (int)fullState->currentMenuItem->maxValue - 1)) {
                fullState->preenFMMixerPresetNumber = 0;
                fullState->preenFMMixer = synthState_->getStorage()->getMixerBank()->getFile(fullState->preenFMMixerNumber);
                synthState_->propagateAfterNewMixerLoad();
            }
        } else if (encoder == 3) {
            changePresetSelect(&fullState->preenFMMixerPresetNumber, ticks, NUMBER_OF_MIXERS_PER_BANK - 1);
        }
        break;
    case MENU_SD_RENAME_PRESET_SELECT_FILE:
    case MENU_PRESET_SAVE_SELECT:
        if (encoder == 0) {
            // Did we really change bank?
            if (changePresetSelect(&fullState->preenFMBankNumber, ticks, (int)fullState->currentMenuItem->maxValue - 1)) {
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
            if (changePresetSelect(&fullState->preenFMSeqBankNumber, ticks, (int)fullState->currentMenuItem->maxValue - 1)) {
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

bool ScreenMenu::changeCharSelect(char* valueToChange, int ticks) {
    int oldPresetSelect = *valueToChange;
    int newPresetSelect = oldPresetSelect + ticks;
    if (newPresetSelect > 127) {
        newPresetSelect = 127;
    } else if (newPresetSelect < 32) {
        newPresetSelect = 32;
    }
    *valueToChange = (char)newPresetSelect;
    return oldPresetSelect != newPresetSelect;
}

bool ScreenMenu::changePresetSelect(uint8_t* valueToChange, int ticks, int max) {
    int oldPresetSelect = *valueToChange;
    int newPresetSelect = oldPresetSelect + ticks;
    if (newPresetSelect > max) {
        newPresetSelect = max;
    } else if (newPresetSelect < 0) {
        newPresetSelect = 0;
    }
    *valueToChange = (uint8_t)newPresetSelect;
    return oldPresetSelect != newPresetSelect;
}

bool ScreenMenu::changePresetSelect(char* valueToChange, int ticks, int max) {
    int oldPresetSelect = *valueToChange;
    int newPresetSelect = oldPresetSelect + ticks;
    if (newPresetSelect > max) {
        newPresetSelect = max;
    } else if (newPresetSelect < 0) {
        newPresetSelect = 0;
    }
    *valueToChange = (char)newPresetSelect;
    return oldPresetSelect != newPresetSelect;
}

bool ScreenMenu::changePresetSelect(uint16_t* valueToChange, int ticks, int max) {
    int oldPresetSelect = *valueToChange;
    int newPresetSelect = oldPresetSelect + ticks;
    if (newPresetSelect > max) {
        newPresetSelect = max;
    } else if (newPresetSelect < 0) {
        newPresetSelect = 0;
    }
    *valueToChange = (uint16_t)newPresetSelect;
    return oldPresetSelect != newPresetSelect;
}

bool ScreenMenu::changePresetSelect(uint32_t* valueToChange, int ticks, int max) {
    int oldPresetSelect = *valueToChange;
    int newPresetSelect = oldPresetSelect + ticks;
    if (newPresetSelect > max) {
        newPresetSelect = max;
    } else if (newPresetSelect < 0) {
        newPresetSelect = 0;
    }
    *valueToChange = (uint32_t)newPresetSelect;
    return oldPresetSelect != newPresetSelect;
}

void ScreenMenu::buttonPressed(int currentTimbre, int button) {
    struct FullState* fullState = &synthState_->fullState_;
    int32_t length;

    if (button == BUTTON_SWITCH_MENU) {
        if (fullState->currentMenuItem->menuState != MAIN_MENU) {
            const MenuItem* oldMenuItem = fullState->currentMenuItem;
            fullState->currentMenuItem = getMenuBack(currentTimbre);
            updatePreviousChoice(fullState->currentMenuItem->menuState);
            synthState_->propagateMenuBack(oldMenuItem);
        }
        return;
    }

    if (fullState->currentMenuItem->menuState == MENU_CONFIG_SETTINGS && button >= BUTTON_SWITCH_1 && button <= BUTTON_SWITCH_6) {
        handleConfigButton(currentTimbre, button);
        return;
    }

    if (button < BUTTON_SWITCH_1 || button > BUTTON_SWITCH_6) {
        return;
    }

    // Do not do anythin in the following case
    if (fullState->currentMenuItem->menuType == MENUTYPE_FILESELECT_SAVE ||
        fullState->currentMenuItem->menuType == MENUTYPE_FILESELECT_LOAD) {
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
            char staticChars[4] = {' ', '0', 'A', 'a'};
            fullState->name[fullState->menuSelect] = staticChars[button - 1];
            synthState_->propagateNewMenuSelect();
            return;
        } else if (button == 5) {
            if (fullState->menuSelect < 11) {
                fullState->menuSelect++;
                fullState->name[fullState->menuSelect] = fullState->name[fullState->menuSelect - 1];
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

    const MenuItem* nextMenu = MenuItemUtil::getMenuItem(fullState->currentMenuItem->subMenu[button]);

    // If Previous state was the following we have some action to do
    if (nextMenu->menuState == MENU_DONE) {
        switch (oldMenu) {
        case MENU_PRESET_SAVE_ENTER_NAME:
            // Let save the preset !
            for (length = 12; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                ;
            fullState->name[length] = '\0';
            synthState_->getTimbre(currentTimbre)->setPresetName(fullState->name);
            synthState_->savePreset(currentTimbre, fullState->preenFMBank, fullState->preenFMPresetNumber);
            break;
        case MENU_MIXER_SAVE_ENTER_NAME:
            // Let save the mixer !
            for (length = 12; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                ;
            fullState->name[length] = '\0';
            synthState_->getStorage()->getMixerBank()->saveMixer(fullState->preenFMMixer, fullState->preenFMMixerPresetNumber,
                                                                 fullState->name);
            break;
        case MENU_SEQUENCER_SAVE_ENTER_NAME:
            // Let save the sequence !
            for (length = 12; fullState->name[length - 1] == 0 || fullState->name[length - 1] == ' '; length--)
                ;
            fullState->name[length] = '\0';
            synthState_->getStorage()->getSequenceBank()->saveSequence(fullState->preenFMSequence, fullState->preenFMSeqSequencerNumber,
                                                                       fullState->name);
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
            synthState_->propagateAfterNewMixerLoad();
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
        backupTimbre(currentTimbre);
        fullState->preenFMBank = synthState_->getStorage()->getPatchBank()->getFile(fullState->preenFMBankNumber);
        synthState_->loadPreset(currentTimbre, fullState->preenFMBank, fullState->preenFMPresetNumber);
        break;
    case MENU_PRESET_LOAD_SELECT_DX7_BANK:
        backupTimbre(currentTimbre);
        fullState->preenFMBank = synthState_->getStorage()->getDX7SysexFile()->getFile(fullState->dx7BankNumber);
        synthState_->loadDx7Patch(currentTimbre, fullState->preenFMBank, fullState->dx7PresetNumber);
        break;
    case MENU_SD_RENAME_MIXER_SELECT_FILE:
    case MENU_MIXER_LOAD_SELECT:
    case MENU_MIXER_SAVE_SELECT:
        fullState->preenFMMixer = synthState_->getStorage()->getMixerBank()->getFile(fullState->preenFMMixerNumber);
        synthState_->propagateAfterNewMixerLoad();
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
        copyAndTransformString(fullState->name, synthState_->getTimbre(currentTimbre)->getPresetName());
        break;
    case MENU_MIXER_SAVE_ENTER_NAME:
        copyAndTransformString(fullState->name, synthState_->mixerState_.mixName_);
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
        backupTimbre(currentTimbre);
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

void ScreenMenu::updatePreviousChoice(uint8_t menuState) {
    struct FullState* fullState = &synthState_->fullState_;

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

const MenuItem* ScreenMenu::getMenuBack(int currentTimbre) {
    struct FullState* fullState = &synthState_->fullState_;

    const MenuItem* rMenuItem = 0;

    if (fullState->currentMenuItem->menuState == MENU_CONFIG_SETTINGS) {
        resetConfigPage();
    }

    // default menuSelect value
    fullState->menuSelect = 0; // MenuItemUtil::getParentMenuSelect(fullState->currentMenuItem->menuState);

    switch (fullState->currentMenuItem->menuState) {
    case MENU_PRESET_RANDOMIZER:
        // Put back original preset
        synthState_->propagateNoteOff();
        synthState_->propagateBeforeNewParamsLoad(currentTimbre);
        restoreTimbre(currentTimbre);
        synthState_->propagateAfterNewParamsLoad(currentTimbre);
        break;
    case MENU_PRESET_LOAD_SELECT:
    case MENU_PRESET_LOAD_SELECT_DX7_BANK:
        synthState_->propagateNoteOff();
        synthState_->propagateBeforeNewParamsLoad(currentTimbre);
        // Put back original preset
        restoreTimbre(currentTimbre);
        synthState_->propagateAfterNewParamsLoad(currentTimbre);
        break;
    default:
        break;
    }
    rMenuItem = MenuItemUtil::getParentMenuItem(fullState->currentMenuItem->menuState);
    return rMenuItem;
}

void ScreenMenu::moveExtensionAtTheEnd(char* fileName) {
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

void ScreenMenu::copyAndTransformString(char* dest, const char* source) {
    for (int k = 0; k < 12; k++) {
        dest[k] = ' ';
    }
    for (int k = 0; k < 12 && source[k] != 0; k++) {
        dest[k] = source[k];
    }
}

int ScreenMenu::copyStringAt(char* dest, int offset, const char* source) {
    int o = 0;
    while (source[o] != '\0') {
        dest[offset + o] = source[o];
        o++;
    }
    return o;
}

void ScreenMenu::newMenuState(FullState* fullState) {
    tft_->pauseRefresh();

    if (fullState->currentMenuItem->menuState == MENU_CONFIG_SETTINGS) {
        fullState->menuSelect = 0;
        configScrollOffset_ = 0;
        configPrevSelect_ = 0xFF;
        configPrevOffset_ = 0;
        configPrevValue_ = 0;
        configPrevEditMode_ = false;
        configPrevDirty_ = configBaselineValid_ ? configIsDirty() : false;
        configEditMode_ = false;
        configPendingClear_ = false;
        configPendingRows_ = 0;
        configPendingValue_ = false;
    }

    if (MenuItemUtil::getParentMenuItem(synthState_->fullState_.currentMenuItem->menuState)->menuState == MAIN_MENU) {
        menuStateRow_ = 1;
    }

    menuStateRow_++;

    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_RED);

    tft_->setCursor((menuStateRow_ - 1), menuStateRow_);
    if (synthState_->fullState_.currentMenuItem->menuState == MENU_CONFIG_SETTINGS) {
        tft_->print("Settings");
    } else {
        tft_->print(synthState_->fullState_.currentMenuItem->name);
    }
    switch (synthState_->fullState_.currentMenuItem->menuState) {
    case MENU_PRESET_LOAD_SELECT:
        tft_->setCharColor(COLOR_YELLOW);
        tft_->print(" in I");
        tft_->print(synthState_->getCurrentTimbre() + 1);
        tft_->setCharColor(COLOR_RED);
        break;
    case MENU_PRESET_SAVE_SELECT:
    case MENU_PRESET_RANDOMIZER:
        tft_->print(" ");
        tft_->setCharColor(COLOR_YELLOW);
        tft_->print(synthState_->getTimbre(synthState_->getCurrentTimbre())->getPresetName());
        tft_->setCharColor(COLOR_RED);
        break;

    case MENU_DEFAULT_MIXER_LOAD:
        tft_->setCharColor(COLOR_GRAY_LIGHT);
        tft_->setCursor(0, 12);
        tft_->print("Load default Mixer");
        break;
    case MENU_DEFAULT_MIXER_SAVE:
        tft_->setCharColor(COLOR_GRAY_LIGHT);
        tft_->setCursor(0, 12);
        tft_->print("Save default Mixer");
        break;
    case MENU_DEFAULT_MIXER_DELETE:
        tft_->setCharColor(COLOR_GRAY_LIGHT);
        tft_->setCursor(0, 12);
        tft_->print("Clear default Mixer");
        break;
    case MENU_DEFAULT_SEQUENCER_LOAD:
        tft_->setCharColor(COLOR_GRAY_LIGHT);
        tft_->setCursor(0, 12);
        tft_->print("Load default Sequencer");
        break;
    case MENU_DEFAULT_SEQUENCER_SAVE:
        tft_->setCharColor(COLOR_GRAY_LIGHT);
        tft_->setCursor(0, 12);
        tft_->print("Save default Sequencer");
        break;
    case MENU_DEFAULT_SEQUENCER_DELETE:
        tft_->setCharColor(COLOR_GRAY_LIGHT);
        tft_->setCursor(0, 12);
        tft_->print("Clear deflt Sequencer");
        break;

    default:
        break;
    }
    displayMenuState(fullState);
}

void ScreenMenu::displayMenuState(FullState* fullState) {

    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_GRAY);
    switch (synthState_->fullState_.currentMenuItem->menuState) {
    case MENU_PRESET_LOAD_SELECT:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - bank");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("d - preset");
        break;
    case MENU_PRESET_SAVE_SELECT:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - save in file");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("d - save in preset");
        break;
    case MENU_SEQUENCER_SAVE_SELECT:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - save in file");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("d - save in sequence");
        break;
    case MENU_SEQUENCER_LOAD_SELECT:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - load from file");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("d - sequence");
        break;
    case MENU_MIXER_LOAD_SELECT:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - mixer file");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("d - mixer");
        break;
    case MENU_MIXER_SAVE_SELECT:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - save in mixer file");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("d - save in mixer");
        break;
    case MENU_PRESET_LOAD_SELECT_DX7_BANK:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - dx7 sysex file");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("d - preset");
        break;
    case MENU_PRESET_SAVE_ENTER_NAME:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("    save in file");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - move    d - change char");
        break;
    case MENU_MIXER_SAVE_ENTER_NAME:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("    save in mixer file");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - move    d - change char");
        break;
    case MENU_SEQUENCER_SAVE_ENTER_NAME:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 6 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("    save in sequence file");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - move    d - change char");
        break;
    case MENU_PRESET_RANDOMIZER:
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(COLOR_GRAY);
        tft_->setCursor(1, 7);
        tft_->print("Operator  Envelope");
        tft_->setCursor(1, 9);
        tft_->print("Indices   Modulation");
        break;
    case MENU_SD_CREATE_PRESET_FILE:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 7 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("    Create preset bank");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - move    d - change char");
        break;
    case MENU_SD_CREATE_MIXER_FILE:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 7 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("    Create mixer bank");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - move    d - change char");
        break;
    case MENU_SD_CREATE_SEQUENCE_FILE:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 7 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("    Create sequence file");
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - move    d - change char");
        break;
    case MENU_SD_RENAME_MIXER_ENTER_NAME:
    case MENU_SD_RENAME_PRESET_ENTER_NAME:
    case MENU_SD_RENAME_SEQUENCE_ENTER_NAME:
        tft_->setCursorInPixel(1 * TFT_BIG_CHAR_WIDTH, 8 * TFT_BIG_CHAR_HEIGHT + 6);
        tft_->printSmallChars("a - move    d - change char");
        break;
    default:
        break;
    }
}

void ScreenMenu::newMenuSelect(FullState* fullState) {

    int length;

    if (fullState->currentMenuItem->menuState != MENU_CONFIG_SETTINGS) {
        displayPatchEngine(0, false);
    }

    switch (fullState->currentMenuItem->menuState) {
    case MENU_PRESET_LOAD_SELECT_DX7_BANK:
        displayBankSelect(fullState->dx7BankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY), fullState->preenFMBank->name);
        if (fullState->preenFMBank->fileType != FILE_EMPTY) {
            displayPatchSelect(fullState->dx7PresetNumber, synthState_->getTimbre(synthState_->getCurrentTimbre())->getPresetName());
        } else {
            displayPatchSelect(0, 0);
        }
        break;
    case MENU_PRESET_LOAD_SELECT: {
        displayBankSelect(fullState->preenFMBankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY), fullState->preenFMBank->name);
        uint32_t magic = 0;
        bool magicValid = storage_->getPatchBank()->getLastPatchMagic(magic);
        bool known = magicValid && presetMagicInfo(magic) != nullptr;
        if (fullState->preenFMBank->fileType != FILE_EMPTY) {
            displayPatchSelect(fullState->preenFMPresetNumber, known ? storage_->getPatchBank()->loadPatchName(
                                                                           fullState->preenFMBank, fullState->preenFMPresetNumber, magic)
                                                                     : "Not supported");
            displayPatchEngine(magic, magicValid);
        } else {
            displayPatchSelect(0, 0);
            displayPatchEngine(0, false);
        }
        break;
    }
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
    displayStateMessage:
        tft_->fillArea(0, 12 * TFT_BIG_CHAR_HEIGHT, 240, TFT_BIG_CHAR_HEIGHT, COLOR_BLACK);
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(8, 12);
        tft_->print(fullState->currentMenuItem->name);
        break;
    case MENU_PRESET_SAVE_SELECT: {
        displayBankSelect(fullState->preenFMBankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY), fullState->preenFMBank->name);
        if (fullState->preenFMBank->fileType != FILE_EMPTY) {
            uint32_t magic = 0;
            bool magicValid = storage_->getPatchBank()->loadPatchMagic(fullState->preenFMBank, fullState->preenFMPresetNumber, magic);
            bool known = magicValid && presetMagicInfo(magic) != nullptr;
            displayPatchSelect(fullState->preenFMPresetNumber, known ? storage_->getPatchBank()->loadPatchName(
                                                                           fullState->preenFMBank, fullState->preenFMPresetNumber, magic)
                                                                     : "Not supported");
            displayPatchEngine(magic, magicValid);
        } else {
            displayPatchSelect(0, 0);
            displayPatchEngine(0, false);
        }
        break;
    }
    case MENU_SEQUENCER_SAVE_SELECT:
        displayBankSelect(fullState->preenFMSeqBankNumber, (fullState->preenFMSequence->fileType == FILE_OK),
                          fullState->preenFMSequence->name);
        if (fullState->preenFMSequence->fileType == FILE_OK) {
            displayPatchSelect(fullState->preenFMSeqSequencerNumber, storage_->getSequenceBank()->loadSequenceName(
                                                                         fullState->preenFMSequence, fullState->preenFMSeqSequencerNumber));
        } else {
            displayPatchSelect(0, 0);
        }
        break;
    case MENU_SEQUENCER_LOAD_SELECT:
        displayBankSelect(fullState->preenFMSeqBankNumber, (fullState->preenFMSequence->fileType != FILE_EMPTY),
                          fullState->preenFMSequence->name);
        if (fullState->preenFMSequence->fileType != FILE_EMPTY) {
            displayPatchSelect(fullState->preenFMSeqSequencerNumber, storage_->getSequenceBank()->loadSequenceName(
                                                                         fullState->preenFMSequence, fullState->preenFMSeqSequencerNumber));
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
        tft_->setCharBackgroundColor(COLOR_BLACK);
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
        tft_->print(otherRandomizer[(int)fullState->randomizer.Oper]);
        tft_->setCursor(13, 8);
        tft_->print(enveloppeRandomizer[(int)fullState->randomizer.EnvT]);
        tft_->setCursor(3, 10);
        tft_->print(otherRandomizer[(int)fullState->randomizer.IM]);
        tft_->setCursor(13, 10);
        tft_->print(otherRandomizer[(int)fullState->randomizer.Modl]);
        break;
    case MENU_CONFIG_SETTINGS: {
        uint16_t sel = fullState->menuSelect;
        if (sel > MIDICONFIG_SIZE) {
            sel = MIDICONFIG_SIZE;
        }
        fullState->menuSelect = sel;

        if (!configBaselineValid_) {
            for (int i = 0; i < MIDICONFIG_SIZE; i++) {
                configSaved_[i] = fullState->midiConfigValue[i];
            }
            configBaselineValid_ = true;
        }

        const uint8_t maxOffset = (MIDICONFIG_SIZE + 1 > CONFIG_VISIBLE_ROWS) ? (uint8_t)(MIDICONFIG_SIZE + 1 - CONFIG_VISIBLE_ROWS) : 0;
        if (sel < configScrollOffset_) {
            configScrollOffset_ = (uint8_t)sel;
        } else if (sel >= configScrollOffset_ + CONFIG_VISIBLE_ROWS) {
            configScrollOffset_ = (uint8_t)(sel - CONFIG_VISIBLE_ROWS + 1);
        }
        if (configScrollOffset_ > maxOffset) {
            configScrollOffset_ = maxOffset;
        }

        const bool entry = (configPrevSelect_ == 0xFF);
        const bool offsetChanged = (configScrollOffset_ != configPrevOffset_);
        const bool selChanged = (sel != configPrevSelect_);
        const int value = configDisplayValue((uint8_t)sel);
        const bool valueChanged = (value != (int)configPrevValue_);
        const bool editChanged = (configEditMode_ != configPrevEditMode_);
        const bool dirtyChanged = (configIsDirty() != configPrevDirty_);

        if (entry || offsetChanged) {
            configPendingClear_ = true;
            configPendingRows_ = 0x7F;
            configPendingValue_ = true;
        } else {
            if (selChanged) {
                int oldRow = (int)configPrevSelect_ - (int)configScrollOffset_;
                int newRow = (int)sel - (int)configScrollOffset_;
                if (oldRow >= 0 && oldRow < CONFIG_VISIBLE_ROWS) {
                    configPendingRows_ |= (uint8_t)(1 << oldRow);
                }
                if (newRow >= 0 && newRow < CONFIG_VISIBLE_ROWS) {
                    configPendingRows_ |= (uint8_t)(1 << newRow);
                }
                configPendingValue_ = true;
            }
            if (valueChanged) {
                int row = (int)sel - (int)configScrollOffset_;
                if (row >= 0 && row < CONFIG_VISIBLE_ROWS) {
                    configPendingRows_ |= (uint8_t)(1 << row);
                }
                configPendingValue_ = true;
            }
            if (editChanged) {
                int row = (int)sel - (int)configScrollOffset_;
                if (row >= 0 && row < CONFIG_VISIBLE_ROWS) {
                    configPendingRows_ |= (uint8_t)(1 << row);
                }
                configPendingValue_ = true;
            }
        }

        configPrevSelect_ = (uint8_t)sel;
        configPrevOffset_ = configScrollOffset_;
        configPrevValue_ = (uint8_t)configDisplayValue((uint8_t)sel);
        configPrevEditMode_ = configEditMode_;
        configPrevDirty_ = configIsDirty();

        requestConfigRefresh();
        if (dirtyChanged || editChanged) {
            requestConfigButtons();
        }
        break;
    }

    case MENU_SD_RENAME_PRESET_SELECT_FILE:
        displayBankSelect(fullState->preenFMBankNumber, (fullState->preenFMBank->fileType != FILE_EMPTY), fullState->preenFMBank->name);
        break;
    case MENU_SD_RENAME_MIXER_SELECT_FILE:
        displayBankSelect(fullState->preenFMMixerNumber, (fullState->preenFMMixer->fileType != FILE_EMPTY), fullState->preenFMMixer->name);
        break;
    case MENU_SD_RENAME_SEQUENCE_SELECT_FILE:
        displayBankSelect(fullState->preenFMSeqBankNumber, (fullState->preenFMSequence->fileType != FILE_EMPTY),
                          fullState->preenFMSequence->name);
        break;
    default:
        break;
    }
}

void ScreenMenu::menuBack(const MenuItem* oldMenuItem, FullState* fullState) {

    int y = menuStateRow_ * TFT_BIG_CHAR_HEIGHT;
    tft_->fillArea(0, y, 240, 260 - y, COLOR_BLACK);

    menuStateRow_--;

    displayMenuState(fullState);
    newMenuSelect(fullState);
}

void ScreenMenu::displayBankSelect(int bankNumber, bool usable, const char* name) {

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

void ScreenMenu::displayPatchSelect(int presetNumber, const char* name) {
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

void ScreenMenu::displayPatchEngine(uint32_t magic, bool valid) {
    tft_->fillArea(0, 10 * TFT_BIG_CHAR_HEIGHT, 240, TFT_BIG_CHAR_HEIGHT, COLOR_BLACK);

    if (!valid) {
        return;
    }

    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_RED_DARK);
    tft_->setCursor(4, 10);
    const PresetMagicInfo* info = presetMagicInfo(magic);
    if (info != nullptr) {
        tft_->print(info->engineName);
    } else {
        char code[7];
        code[0] = '#';
        for (int i = 0; i < 4; i++) {
            const uint8_t c = (magic >> (24 - i * 8)) & 0xff;
            code[1 + i] = (c >= 0x20 && c <= 0x7e) ? (char)c : '?';
        }
        code[5] = '#';
        code[6] = 0;
        tft_->print(code);
    }
}

bool ScreenMenu::configIsEditable(uint8_t index) {
    return index < MIDICONFIG_SIZE && midiConfig[index].maxValue > 1;
}

bool ScreenMenu::configIsDirty() {
    // Save is disabled while a value is being edited: the value is only
    // committed to the configuration when OK is pressed.
    if (configEditMode_) {
        return false;
    }
    if (!configBaselineValid_) {
        return false;
    }
    for (int i = 0; i < MIDICONFIG_SIZE; i++) {
        if (synthState_->fullState_.midiConfigValue[i] != configSaved_[i]) {
            return true;
        }
    }
    return false;
}

int ScreenMenu::configDisplayValue(uint8_t index) {
    if (configEditMode_ && index == synthState_->fullState_.menuSelect) {
        return configEditValue_;
    }
    return synthState_->fullState_.midiConfigValue[index];
}

void ScreenMenu::applyConfigSideEffects(uint8_t index) {
    if (index == MIDICONFIG_TFT_BACKLIGHT) {
        uint8_t tft_bl = synthState_->fullState_.midiConfigValue[MIDICONFIG_TFT_BACKLIGHT];
        TIM1->CCR2 = tft_bl < 10 ? 10 : tft_bl;
    } else if (index == MIDICONFIG_REVERB_PARAMS) {
        synthState_->configValueChanged();
    }
}

void ScreenMenu::handleConfigButton(int currentTimbre, int button) {
    struct FullState* fullState = &synthState_->fullState_;
    uint8_t sel = (uint8_t)fullState->menuSelect;

    switch (button) {
    case BUTTON_SWITCH_1:
        if (!configEditMode_ && configIsEditable(sel)) {
            configEditMode_ = true;
            configEditOldValue_ = fullState->midiConfigValue[sel];
            configEditValue_ = configEditOldValue_;
            synthState_->propagateNewMenuSelect();
        }
        break;
    case BUTTON_SWITCH_2:
        if (configEditMode_) {
            fullState->midiConfigValue[sel] = configEditValue_;
            applyConfigSideEffects(sel);
            configEditMode_ = false;
            synthState_->propagateNewMenuSelect();
        }
        break;
    case BUTTON_SWITCH_3:
        if (configEditMode_) {
            fullState->midiConfigValue[sel] = configEditOldValue_;
            applyConfigSideEffects(sel);
            configEditMode_ = false;
            synthState_->propagateNewMenuSelect();
        }
        break;
    case BUTTON_SWITCH_5: {
        const MenuItem* oldMenuItem = fullState->currentMenuItem;
        fullState->currentMenuItem = getMenuBack(currentTimbre);
        updatePreviousChoice(fullState->currentMenuItem->menuState);
        synthState_->propagateMenuBack(oldMenuItem);
        break;
    }
    case BUTTON_SWITCH_6:
        if (!configEditMode_) {
            storage_->getConfigurationFile()->saveConfig(fullState->midiConfigValue);
            for (int i = 0; i < MIDICONFIG_SIZE; i++) {
                configSaved_[i] = fullState->midiConfigValue[i];
            }
            configBaselineValid_ = true;
            synthState_->propagateNewMenuSelect();
        }
        break;
    default:
        break;
    }
}

void ScreenMenu::resetConfigPage() {
    if (configEditMode_) {
        uint16_t sel = synthState_->fullState_.menuSelect;
        if (sel <= MIDICONFIG_SIZE) {
            synthState_->fullState_.midiConfigValue[sel] = configEditOldValue_;
            applyConfigSideEffects((uint8_t)sel);
        }
        configEditMode_ = false;
    }
    configScrollOffset_ = 0;
    configPrevSelect_ = 0xFF;
    configPrevOffset_ = 0;
    configPrevValue_ = 0;
    configPrevEditMode_ = false;
    configPrevDirty_ = false;
    configPendingClear_ = false;
    configPendingRows_ = 0;
    configPendingValue_ = false;
}

void ScreenMenu::requestConfigRefresh() {
    if (refreshStatusP_ != nullptr && *refreshStatusP_ == 0) {
        *refreshStatusP_ = 6;
    }
}

void ScreenMenu::requestConfigButtons() {
    if (refreshStatusP_ != nullptr && *refreshStatusP_ <= 6) {
        *refreshStatusP_ = 6;
    }
}

void ScreenMenu::drawConfigRowTitle(uint8_t row) {
    int y = CONFIG_LIST_Y + row * CONFIG_ROW_H;
    tft_->fillArea(0, y, 240, CONFIG_ROW_H, COLOR_BLACK);

    int item = configScrollOffset_ + row;
    if (item > MIDICONFIG_SIZE) {
        return;
    }

    bool selected = (item == synthState_->fullState_.menuSelect);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(selected ? COLOR_WHITE : COLOR_RED_DARK);
    tft_->setCursorInPixel(2, y);
    tft_->print(midiConfig[item].title);
}

void ScreenMenu::drawConfigRowValue(uint8_t row) {
    int y = CONFIG_LIST_Y + row * CONFIG_ROW_H;
    int item = configScrollOffset_ + row;
    if (item > MIDICONFIG_SIZE) {
        return;
    }

    int value = configDisplayValue((uint8_t)item);
    const char* valueName = midiConfig[item].valueName != nullptr ? midiConfig[item].valueName[value] : nullptr;

    int length;
    if (valueName != nullptr) {
        length = getLength(valueName);
    } else {
        length = 1;
        int v = value;
        while (v >= 10) {
            v /= 10;
            length++;
        }
    }

    int x = 240 - 2 - length * TFT_SMALL_CHAR_WIDTH;
    if (x < 0) {
        x = 0;
    }

    bool selected = (item == synthState_->fullState_.menuSelect);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor((selected && configEditMode_) ? COLOR_YELLOW : COLOR_RED_DARK);
    tft_->setCursorInPixel(x, y + 5);
    if (valueName != nullptr) {
        tft_->printSmallChars(valueName);
    } else {
        tft_->printSmallChar(value);
    }
}

void ScreenMenu::drawConfigValueLine() {
    tft_->fillArea(0, CONFIG_VALUE_AREA_Y, 240, CONFIG_VALUE_AREA_H, COLOR_BLACK);

    int item = synthState_->fullState_.menuSelect;
    if (item > MIDICONFIG_SIZE) {
        return;
    }

    tft_->setCharBackgroundColor(COLOR_BLACK);

    tft_->setCharColor(configEditMode_ ? COLOR_WHITE : COLOR_RED);
    int titleLength = getLength(midiConfig[item].title);
    int titleX = (240 - titleLength * TFT_SMALL_CHAR_WIDTH) / 2;
    if (titleX < 0) {
        titleX = 0;
    }
    tft_->setCursorInPixel(titleX, CONFIG_TITLE_Y);
    tft_->printSmallChars(midiConfig[item].title);

    int value = configDisplayValue((uint8_t)item);
    const char* valueName = midiConfig[item].valueName != nullptr ? midiConfig[item].valueName[value] : nullptr;
    tft_->setCharColor(configEditMode_ ? COLOR_YELLOW : COLOR_RED);

    if (valueName != nullptr) {
        int length = getLength(valueName);
        int x = (240 - length * TFT_BIG_CHAR_WIDTH) / 2;
        if (x < 0) {
            x = 0;
        }
        tft_->setCursorInPixel(x, CONFIG_VALUE_Y);
        tft_->print(valueName);
    } else {
        int length = 1;
        int v = value;
        while (v >= 10) {
            v /= 10;
            length++;
        }
        int x = (240 - length * TFT_BIG_CHAR_WIDTH) / 2;
        tft_->setCursorInPixel(x, CONFIG_VALUE_Y);
        tft_->print(value);
    }
}

void ScreenMenu::printScalaFrequency(float value) {
    int integer = (int)(value + .0005f);
    tft_->print(integer);
    tft_->print('.');
    value -= integer;
    tft_->print((int)(value * 10 + .0005f));
}

void ScreenMenu::setPreviousSynthMode(int synthModeBeforeMenu) {
    previousSynthMode_ = synthModeBeforeMenu;
}
