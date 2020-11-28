/*
 * Copyright 2020 Xavier Hosxe
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

#ifndef MENU_H_
#define MENU_H_

#define INTERNAL_LAST_BANK 71

#include "Storage.h"

enum {
    MIDICONFIG_USB = 0,
    MIDICONFIG_RECEIVES,
    MIDICONFIG_SENDS,
    MIDICONFIG_PROGRAM_CHANGE,
    MIDICONFIG_BOOT_START,
    MIDICONFIG_ENCODER,
    MIDICONFIG_TEST_NOTE,
    MIDICONFIG_TEST_VELOCITY,
    MIDICONFIG_ARPEGGIATOR_IN_PRESET,
    MIDICONFIG_BOOT_SOUND,
    MIDICONFIG_CPU_USAGE,
    MIDICONFIG_SIZE
};

enum SynthEditMode {
    SYNTH_MODE_MIXER = 1, SYNTH_MODE_EDIT_PFM3, SYNTH_MODE_MENU, SYNTH_MODE_SEQUENCER, SYNTH_MODE_REINIT_TFT
};

enum MidiConfigUSB {
    USBMIDI_OFF = 0, USBMIDI_IN, USBMIDI_IN_AND_OUT
};

enum MenuState {
    MAIN_MENU = 0,

    MENU_MIXER,
    MENU_MIXER_SAVE_SELECT,
    MENU_MIXER_SAVE_ENTER_NAME,
    MENU_MIXER_LOAD_SELECT,

    MENU_PRESET,
    MENU_PRESET_SAVE_SELECT,
    MENU_PRESET_SAVE_ENTER_NAME,
    MENU_PRESET_LOAD_SELECT,
    MENU_PRESET_LOAD_SELECT_DX7_BANK,

    MENU_PRESET_RANDOMIZER,
    MENU_PRESET_NEW,

    MENU_SEQUENCER,
    MENU_SEQUENCER_SAVE_SELECT,
    MENU_SEQUENCER_SAVE_ENTER_NAME,
    MENU_SEQUENCER_LOAD_SELECT,

    MENU_SD,

    MENU_SD_CREATE,
    MENU_SD_CREATE_MIXER_FILE,
    MENU_SD_CREATE_PRESET_FILE,
    MENU_SD_CREATE_SEQUENCE_FILE,

    MENU_SD_RENAME,
    MENU_SD_RENAME_MIXER_SELECT_FILE,
    MENU_SD_RENAME_MIXER_ENTER_NAME,
    MENU_SD_RENAME_PRESET_SELECT_FILE,
    MENU_SD_RENAME_PRESET_ENTER_NAME,
    MENU_SD_RENAME_SEQUENCE_SELECT_FILE,
    MENU_SD_RENAME_SEQUENCE_ENTER_NAME,

    MENU_CONFIG_SETTINGS,

    MENU_DONE,
    MENU_CANCEL,
    MENU_IN_PROGRESS,
    MENU_ERROR,

    MENU_DEFAULT_MIXER,
    MENU_DEFAULT_MIXER_LOAD,
    MENU_DEFAULT_MIXER_SAVE,
    MENU_DEFAULT_MIXER_DELETE,

    MENU_DEFAULT_SEQUENCER,
    MENU_DEFAULT_SEQUENCER_LOAD,
    MENU_DEFAULT_SEQUENCER_SAVE,
    MENU_DEFAULT_SEQUENCER_DELETE,

    LAST_MENU
};

enum {
    MENUTYPE_WITHSUBMENU = 0,
    MENUTYPE_FILESELECT_LOAD,
    MENUTYPE_FILESELECT_SAVE,
    MENUTYPE_RANDOMIZER,
    MENUTYPE_ENTERNAME,
    MENUTYPE_SETTINGS,
    MENUTYPE_TEMPORARY,
    MENUTYPE_CONFIRM
};

struct MenuItem {
    MenuState menuState;
    const char *name;
    uint8_t menuType;
    short maxValue;
    MenuState subMenu[6];
};

struct Randomizer {
    // lcd->print("OpFr EnvT IM   Modl");
    int8_t Oper;
    int8_t EnvT;
    int8_t IM;
    int8_t Modl;
};

struct PreviousMenuChoice {
    uint8_t main;
    uint8_t mixer;
    uint8_t defltMix;
    uint8_t defltSeq;
    uint8_t preset;
    uint8_t sequencer;
    uint8_t dx7;
};

struct FullState {
    SynthEditMode synthMode;
    SynthEditMode synthModeBeforeMenu;
    uint16_t menuSelect;
    uint8_t previousMenuSelect;
    const MenuItem *currentMenuItem;
    char name[13];

    uint8_t previousChoice;
    struct PreviousMenuChoice previousMenuChoice;
    uint8_t midiConfigValue[MIDICONFIG_SIZE + 1];

    uint8_t preenFMBankNumber;
    uint8_t preenFMPresetNumber;
    const struct PFM3File *preenFMBank;

    uint8_t preenFMMixerNumber;
    uint8_t preenFMMixerPresetNumber;
    const struct PFM3File *preenFMMixer;

    uint8_t preenFMSeqBankNumber;
    uint8_t preenFMSeqSequencerNumber;
    const struct PFM3File *preenFMSequence;

    uint16_t dx7BankNumber;
    uint8_t dx7PresetNumber;
    const struct PFM3File *dx7Bank;
    struct Randomizer randomizer;
    uint8_t mixerCurrentEdit;
    uint8_t menuCurrentEdit;

    // pfm3 edit page state
    // -1 means no main page selectec then index in mainMenu.
    int8_t mainPage;
    uint8_t editPage;
    uint8_t buttonState[NUMBER_OF_BUTTONIDS];
    uint8_t operatorNumber;
};

struct MidiConfig {
    const char *title;
    const char *nameInFile;
    uint8_t maxValue;
    const char **valueName;
};

extern const struct MenuItem allMenus[];
extern const struct MidiConfig midiConfig[];

class MenuItemUtil {
public:
    static const MenuItem* getMenuItem(MenuState ms) {
        const MenuItem *item = &allMenus[0];
        int cpt = 0;
        while (item->menuState != LAST_MENU) {
            if (item->menuState == ms) {
                return item;
            }
            cpt++;
            item = &allMenus[cpt];
        }
        return 0;
    }

    static const MenuItem* getParentMenuItem(MenuState ms) {
        // MENU_DONE exception -> return itself to block back button
        if (ms == MENU_DONE || ms == MENU_CANCEL || ms == MENU_ERROR) {
            return getMenuItem(ms);
        }
        const MenuItem *item = &allMenus[0];
        int cpt = 0;
        while (item->menuState != LAST_MENU) {
            for (int k = 0; k < 6; k++) {
                if (item->subMenu[k] == ms) {
                    return item;
                }
            }
            cpt++;
            item = &allMenus[cpt];
        }
        return 0;
    }
};

#endif /* MENU_H_ */
