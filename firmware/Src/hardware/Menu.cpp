/*
 * Copyright 2020 Xavier Hosxe
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

#include "Menu.h"
#include "PreenFMFileType.h"
#include "version.h"

#ifndef OVERCLOCK
#define OVERCLOCK_STRING
#else
#define OVERCLOCK_STRING "o"
#endif


const char* yesNo [] = { "No", "Yes" };
const char* midiReceives[] = { "None", "CC", "NRPN", "CC & NRPN" };
const char* midiSends [] = { "None", "CC", "NRPN" };
const char* bootOptions [] = { "Default", "Bank", "Mixer", "DX7" };
const char* encoderType [] = { "12", "24", "12i", "24i" };
const char* usbMidiText[] = { "Off", "In", "In/Out" };
const char* version[] = { PFM3_FIRMWARE_VERSION };
const char* oledSaver[] = { "Off", "2m", "5m", "10m", "1h" };



/*
 * Mixer Preset Seq SD Config

SD -> Create Rename
Create -> Mixer Preset Seq
Rename -> Mixre Preset Seq

Mixer -> Load Save Dfl
Dfl -> Load Save Delete

Preset -> Load Save DX7  Rand

Seq -> Load Save Dfl
Dfl -> Load Save Delete

 */

const struct MenuItem allMenus[]  = {
        {
                MAIN_MENU,
                "",
				MENUTYPE_WITHSUBMENU,
                5,
                {MENU_MIXER, MENU_PRESET, MENU_SEQUENCER, MENU_SD, MENU_CONFIG_SETTINGS  }
        },
        // === MIXER
        {
                MENU_MIXER,
                "Mixer",
                MENUTYPE_WITHSUBMENU,
                3,
                { MENU_MIXER_LOAD_SELECT, MENU_MIXER_SAVE_SELECT, MENU_DEFAULT_MIXER }
        },
        {
                MENU_MIXER_SAVE_SELECT,
                "Save",
                MENUTYPE_FILESELECT_SAVE,
                NUMBEROFPREENFMMIXERS,
                {MENU_MIXER_SAVE_ENTER_NAME}
        },
        {
                MENU_MIXER_SAVE_ENTER_NAME,
                "!Mixer",
                MENUTYPE_ENTERNAME,
                12,
                {MENU_DONE}
        },
        {
                MENU_MIXER_LOAD_SELECT,
                "Load",
                MENUTYPE_FILESELECT_LOAD,
                NUMBEROFPREENFMMIXERS,
                {MENU_DONE}
        },
        // == PRESET
        {
                MENU_PRESET,
                "Preset",
                MENUTYPE_WITHSUBMENU,
                5,
                { MENU_PRESET_LOAD_SELECT, MENU_PRESET_SAVE_SELECT, MENU_PRESET_LOAD_SELECT_DX7_BANK, MENU_PRESET_RANDOMIZER, MENU_PRESET_NEW }
        },
        {
                MENU_PRESET_SAVE_SELECT,
                "Save",
                MENUTYPE_FILESELECT_SAVE,
                NUMBEROFPREENFMBANKS,
                {MENU_PRESET_SAVE_ENTER_NAME}
        },
        {
                MENU_PRESET_SAVE_ENTER_NAME,
                "!Preset",
                MENUTYPE_ENTERNAME,
                12,
                {MENU_DONE}
        },
        {
                MENU_PRESET_LOAD_SELECT,
                "Load",
				MENUTYPE_FILESELECT_LOAD,
                NUMBEROFPREENFMBANKS,
                {MENU_DONE}
        },
        {
                MENU_PRESET_LOAD_SELECT_DX7_BANK,
                "DX7",
                MENUTYPE_FILESELECT_LOAD,
                NUMBEROFDX7BANKS,
                {MENU_DONE}
        },

        {
                MENU_PRESET_RANDOMIZER,
                "Rand",
                MENUTYPE_RANDOMIZER,
                4,
                {MENU_DONE}
        },
        {
                MENU_PRESET_NEW,
                "New",
                MENUTYPE_CONFIRM,
                1,
                {MENU_DONE}
        },
        // == SEQUENCER
        {
                MENU_SEQUENCER,
                "Seq",
                MENUTYPE_WITHSUBMENU,
                3,
                { MENU_SEQUENCER_LOAD_SELECT, MENU_SEQUENCER_SAVE_SELECT, MENU_DEFAULT_SEQUENCER}
        },
        {
                MENU_SEQUENCER_SAVE_SELECT,
                "Save",
                MENUTYPE_FILESELECT_SAVE,
                NUMBEROFPREENFMSEQUENCES,
                {MENU_SEQUENCER_SAVE_ENTER_NAME}
        },
        {
                MENU_SEQUENCER_SAVE_ENTER_NAME,
                "!Seq",
                MENUTYPE_ENTERNAME,
                12,
                {MENU_DONE}
        },
        {
                MENU_SEQUENCER_LOAD_SELECT,
                "Load",
                MENUTYPE_FILESELECT_LOAD,
                NUMBEROFPREENFMSEQUENCES,
                {MENU_DONE}
        },
        // == DEFAULT MIXER
        {
                MENU_DEFAULT_MIXER,
                "Defl",
				MENUTYPE_WITHSUBMENU,
                3,
                {MENU_DEFAULT_MIXER_LOAD, MENU_DEFAULT_MIXER_SAVE, MENU_DEFAULT_MIXER_DELETE}
        },
        {
                MENU_DEFAULT_MIXER_SAVE,
                "Save",
                MENUTYPE_CONFIRM,
                1,
                {MENU_DONE}
        },
        {
                MENU_DEFAULT_MIXER_LOAD,
                "Load",
                MENUTYPE_CONFIRM,
                1,
                {MENU_DONE}
        },
        {
                MENU_DEFAULT_MIXER_DELETE,
                "Clear",
                MENUTYPE_CONFIRM,
                1,
                {MENU_DONE}
        },
        // == DEFAULT SEQUENCER
        {
                MENU_DEFAULT_SEQUENCER,
                "Defl",
				MENUTYPE_WITHSUBMENU,
                3,
                {MENU_DEFAULT_SEQUENCER_LOAD, MENU_DEFAULT_SEQUENCER_SAVE, MENU_DEFAULT_SEQUENCER_DELETE}
        },
        {
                MENU_DEFAULT_SEQUENCER_SAVE,
                "Save",
                MENUTYPE_CONFIRM,
                1,
                {MENU_DONE}
        },
        {
                MENU_DEFAULT_SEQUENCER_LOAD,
                "Load",
                MENUTYPE_CONFIRM,
                1,
                {MENU_DONE}
        },
        {
                MENU_DEFAULT_SEQUENCER_DELETE,
                "Clear",
                MENUTYPE_CONFIRM,
                1,
                {MENU_DONE}
        },
        // ==  SD
        {
                MENU_SD,
                "SD",
				MENUTYPE_WITHSUBMENU,
                2,
                {MENU_SD_CREATE, MENU_SD_RENAME}
        },
        // == SD CREATE
        {
                MENU_SD_CREATE,
                "Create",
                MENUTYPE_WITHSUBMENU,
                3,
                { MENU_SD_CREATE_MIXER_FILE, MENU_SD_CREATE_PRESET_FILE, MENU_SD_CREATE_SEQUENCE_FILE }
        },
        {
                MENU_SD_CREATE_MIXER_FILE,
                "Mixer",
                MENUTYPE_ENTERNAME,
                8,
                {MENU_DONE}
        },
        {
                MENU_SD_CREATE_PRESET_FILE,
                "Preset",
                MENUTYPE_ENTERNAME,
                8,
                {MENU_DONE}
        },
        {
                MENU_SD_CREATE_SEQUENCE_FILE,
                "Seq",
                MENUTYPE_ENTERNAME,
                8,
                {MENU_DONE}
        },
        // == SD RENAME
        {
                MENU_SD_RENAME,
                "Rename",
                MENUTYPE_WITHSUBMENU,
                3,
                {MENU_SD_RENAME_MIXER_SELECT_FILE, MENU_SD_RENAME_PRESET_SELECT_FILE, MENU_SD_RENAME_SEQUENCE_SELECT_FILE}
        },
        {
                MENU_SD_RENAME_MIXER_SELECT_FILE,
                "Mixer",
                MENUTYPE_FILESELECT_SAVE,
                NUMBEROFPREENFMBANKS,
                {MENU_SD_RENAME_MIXER_ENTER_NAME}
        },
        {
                MENU_SD_RENAME_PRESET_SELECT_FILE,
                "Preset",
                MENUTYPE_FILESELECT_SAVE,
                NUMBEROFPREENFMMIXERS,
                {MENU_SD_RENAME_PRESET_ENTER_NAME}
        },
        {
                MENU_SD_RENAME_SEQUENCE_SELECT_FILE,
                "Seq",
                MENUTYPE_FILESELECT_SAVE,
                NUMBEROFPREENFMSEQUENCES,
                {MENU_SD_RENAME_SEQUENCE_ENTER_NAME}
        },
        {
                MENU_SD_RENAME_MIXER_ENTER_NAME,
                "New name",
                MENUTYPE_ENTERNAME,
                8,
                {MENU_DONE}
        },
        {
                MENU_SD_RENAME_PRESET_ENTER_NAME,
                "New name",
                MENUTYPE_ENTERNAME,
                8,
                {MENU_DONE}
        },
        {
                MENU_SD_RENAME_SEQUENCE_ENTER_NAME,
                "New name",
                MENUTYPE_ENTERNAME,
                8,
                {MENU_DONE}
        },
        // == SETTINGS

        {
                MENU_CONFIG_SETTINGS,
                "Config",
				MENUTYPE_SETTINGS,
                MIDICONFIG_SIZE + 1,
                {MENU_DONE}
        },
        // === DONE
        {
                MENU_DONE,
                "Done",
                MENUTYPE_TEMPORARY,
                0,
                {MENU_DONE}
        },
        // === CANCELED
        {
                MENU_CANCEL,
                "Canceled",
                MENUTYPE_TEMPORARY,
                0,
                {MENU_CANCEL}
        },
        // === ERROR
        {
                MENU_ERROR,
                "## Error ##",
                MENUTYPE_TEMPORARY,
                0,
                {MENU_CANCEL}
        },
        // == In progress
        {
                MENU_IN_PROGRESS,
                "Wait...",
                MENUTYPE_TEMPORARY,
                0,
                {MENU_IN_PROGRESS}
        },
};

const struct MidiConfig midiConfig[]  = {
        {
                "Usb Midi",
                "usbmidi",
                3,
				usbMidiText
        },
        {
                "Receives",
                "midireceives",
                4,
                midiReceives
        },
        {
                "Sends",
                "midisend",
                3,
                midiSends
        },
        {
                "Program Change",
                "programchange",
                2,
                yesNo
        },
        {
                "Boot To",
                "boot",
                4,
                bootOptions
        },
        {
                "Encoder Driver",
                "encoders",
                4,
                encoderType
        },
        {
                "Test Note",
                "testnote",
                127,
                0
        },
        {
                "Test Note Velocity",
                "testvelocity",
                127,
                0
        },
        {
                "Arp in Preset",
                "arpinpreset",
                2,
                yesNo
        },
        {
                "Boot Sound",
                "bootsound",
                2,
                yesNo
        },
        {
                "Cpu Usage",
                "cpuusage",
                2,
                yesNo
        },
        {
                "TFT backlight",
                "tftbacklight",
                101,
                0
        },
        // Don't count in MIDICONFIG_SIZE
        {
                "Firmware Version",
                "",
                1,
                version
        }

};
