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

#ifndef SCREEN_EDITOR
#define SCREEN_EDITOR

#include "FirmwareTftDisplay.h"
#include "SynthParamListener.h"
#include "ScreenTypes.h"

class SynthState; class TftDisplay; class ScreenMsfaEditor; class ScreenEditor; class ScreenPfm3Editor;

struct EditorOneButtonState {
    const char *stateLabel;
    const RowEncoder rowEncoder[6];
};

struct EditorOneButton {
    const char *buttonLabel;
    const uint8_t buttonId;
    const int numberOfStates;
    const EditorOneButtonState *states[];

};

struct EditorEditMenu {
    const char *mainMenuName;
    const int numberOfButtons;
    const EditorOneButton *pages[];
};

struct EditorMainMenu {
    const EditorEditMenu *editMenu[6];
};

/*
 * Common interface for engine-specific editor pages.
 * ScreenEditor owns one implementation per engine and dispatches to the
 * active one. Calls are per page/note/block, never per sample.
 */
class EngineEditor {
public:
    virtual ~EngineEditor() {}

    virtual void init(ScreenEditor *parent, SynthState *synthState, FirmwareTftDisplay *tft) = 0;
    virtual const struct EditorEditMenu* getEditMenu(int page) const = 0;
    virtual void refreshEditorByStep(int &refreshStatus, int &endRefreshStatus) = 0;
    virtual void displayParamValue(int encoder, TFT_COLOR color) = 0;
    virtual void newParamValue(int &refreshStatus, int timbre, int currentRow, int encoder,
        ParameterDisplay *param, float oldValue, float newValue) = 0;
    virtual void tempoClick() = 0;
    virtual void encoderTurned(int encoder, int ticks) = 0;
    virtual void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) = 0;
    virtual void buttonPressed(int button) = 0;
    virtual void buttonLongPressed(int instrument, int button) = 0;
    virtual void refreshOscillatorOperatorShape() {}
    virtual void refreshLfoOscillator() {}
    virtual void refreshLfoEnv() {}
    virtual void checkOperatorNumber() {}
};

class ScreenEditor {
public:
    ScreenEditor();

    void init(SynthState *synthState, FirmwareTftDisplay *tft);

    bool isMsfaTimbre() const;
    const struct EditorEditMenu* currentEditMenu() const;
    const char* mainMenuName(int button) const;
    void markPresetModified(int timbre);

    void refreshEditorByStep(int &refreshStatus, int &endRefreshStatus);
    void displayParamValue(int encoder, TFT_COLOR color);
    void newParamValue(int &refreshStatus, int timbre, int currentRow, int encoder, ParameterDisplay *param, float oldValue, float newValue);
    void tempoClick();

    bool isPresetModifed(int timbre) {
        return presetModifed_[timbre];
    }
    void setPresetModified(int timbre, bool state);
    void resetAllPresetModified();
    void setEditPageSelected(int mainPage, int editPage) {
        editPageSelected_[mainPage] = editPage;
    }
    void updateStepSequencer(int currentRow, int encoder, int oldValue, int newValue);
    void updateArpPattern(int currentRow, int encoder, int oldValue, int newValue);
    void displayPreset();
    void displayPopup(TFT_COLOR color, char* text, uint8_t length);

    void encoderTurned(int encoder, int ticks);
    void encoderTurnedWhileButtonPressed(int encoder6, int ticks, int button);
    void buttonPressed(int button);
    void buttonLongPressed(int instrument, int button);
    void newTimbre(int timbre);

    void refreshOscillatorOperatorShape();
    void refreshOscillatorOperatorEnvelope();
    void refreshLfoOscillator();
    void refreshLfoEnv();

private:
    void buttonPressedMenuPage(int button);
    void displayLevelMeter();
    FirmwareTftDisplay *tft_;
    SynthState *synthState_;
    ScreenMsfaEditor *msfaEditor_;
    ScreenPfm3Editor *pfm3Editor_;

    bool presetModifed_[NUMBER_OF_TIMBRES];
    int currentTimbre_;

    // Remember curent editPage in mainPage
    uint8_t editPageSelected_[6];
};

#endif
