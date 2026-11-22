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

#include "EditorEngine.h"
#include "SynthParamListener.h"

class SynthState;
class TftDisplay;
class ScreenEditor;

class ScreenEditor {
public:
    ScreenEditor();

    void init(SynthState* synthState, FirmwareTftDisplay* tft);

    // Register an engine editor; called from each editor's init().
    void insertEditor(EditorEngine* editor);
    // Editor handling the current timbre and main page, or nullptr.
    EditorEngine* activeEditor() const;
    // Editor implementing the current timbre's engine, or nullptr.
    EditorEngine* editorForEngine() const;
    // Engine selected for the current timbre.
    SynthEngineType currentEngineType() const;

    // Menu for a main page, resolved by the current engine's editor.
    const struct EditorMenu* getMainMenu(int page) const;
    const struct EditorMenu* currentEditMenu() const;
    const char* mainMenuName(int button) const;
    void markPresetModified(int timbre);
    void drawAlgorithm();

    void refreshEditorByStep(int& refreshStatus, int& endRefreshStatus);
    void displayParamValue(int encoder, TFT_COLOR color);
    void newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue);
    void slowTick();

    bool isPresetModifed(int timbre) {
        return presetModifed_[timbre];
    }
    void setPresetModified(int timbre, bool state);
    void resetAllPresetModified();
    void displayPreset();
    void displayPopup(TFT_COLOR color, char* text, uint8_t length);

    void encoderTurned(int encoder, int ticks);
    void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button);
    void buttonPressed(int button);
    void buttonLongPressed(int instrument, int button);
    void newTimbre(int timbre);

    void refreshOscilloBG();

private:
    void buttonPressedMenuPage(int button);
    void displayLevelMeter();
    struct Pfm3Preset* pfm3Preset() const;
    FirmwareTftDisplay* tft_;
    SynthState* synthState_;
    EditorEngine* firstEditor_;
    // editorForEngine() cache, keyed by engine type.
    mutable SynthEngineType cachedEngineType_;
    mutable EditorEngine* cachedEditor_;

    bool presetModifed_[NUMBER_OF_TIMBRES];
    int currentTimbre_;

};

#endif
