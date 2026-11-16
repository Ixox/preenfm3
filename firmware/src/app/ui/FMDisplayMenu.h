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

#ifndef FMDISPLAY_MENU
#define FMDISPLAY_MENU

#include "Common.h"
#include "Menu.h"

class SynthState;
class TftDisplay;

class FMDisplayMenu {
public:
    void init(SynthState *synthState, TftDisplay *tft, Storage *storage);
    void refreshMenuByStep(int currentTimbre, int refreshStatus);
    void encoderTurned(int currentTimbre, int encoder, int ticks);
    void buttonPressed(int currentTimbre, int button);
    void updatePreviousChoice(uint8_t menuState);
    void newMenuState(FullState *fullState);
    void displayMenuState(FullState *fullState);
    void newMenuSelect(FullState *fullState);
    void menuBack(const MenuItem *oldMenuItem, FullState *fullState);
    void setPreviousSynthMode(int synthModeBeforeMenu);
private:
    bool changePresetSelect(uint32_t *valueToChange, int ticks, int max);
    bool changePresetSelect(uint8_t *valueToChange, int ticks, int max);
    bool changePresetSelect(uint16_t *valueToChange, int ticks, int max);
    bool changePresetSelect(char *valueToChange, int ticks, int max);
    bool changeCharSelect(char *valueToChange, int ticks);
    const MenuItem* getMenuBack(int currentTimbre);
    void copyAndTransformString(char *dest, const char *source);
    int copyStringAt(char *dest, int offset, const char *source);
    void moveExtensionAtTheEnd(char *fileName);
    void copySynthParams(char *source, char *dest);
    void displayBankSelect(int bankNumber, bool usable, const char *name);
    void displayPatchSelect(int presetNumber, const char *name);
    void printScalaFrequency(float value);

    TftDisplay *tft_;
    SynthState *synthState_;
    Storage *storage_;

    uint8_t previousConfigSetting_;
    uint8_t menuStateRow_;

    int previousSynthMode_;
};

#endif
