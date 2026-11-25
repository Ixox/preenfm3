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

#include "ScreenEditor.h"
#include "EditorCommon.h"
#include "ScreenTypes.h"
#include "SynthEngineFactory.h"
#include "SynthState.h"
#include "Timbre.h"
#include "engine/byos/EditorByos.h"
#include "engine/drum909/EditorDrum909.h"
#include "engine/msfa/EditorDx7.h"
#include "engine/pfm3/EditorPfm3.h"
#include "engine/pfm3/Preenfm3Engine.h"
#include "preenfm3.h"
#include <FirmwareTftDisplay.h>
#include <string.h>

static EditorDx7 dx7EditorInstance;
static EditorPfm3 pfm3EditorInstance;
static EditorCommon commonEditorInstance;
static EditorByos byosEditorInstance;
static EditorDrum909 drum909EditorInstance;

ScreenEditor::ScreenEditor() {
    firstEditor_ = nullptr;
    cachedEngineType_ = SYNTH_ENGINE_COUNT;
    cachedEditor_ = nullptr;
    currentTimbre_ = 0;
    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        presetModifed_[k] = false;
    }
}

void ScreenEditor::init(SynthState* synthState, FirmwareTftDisplay* tft) {
    synthState_ = synthState;
    tft_ = tft;
    firstEditor_ = nullptr;
    cachedEngineType_ = SYNTH_ENGINE_COUNT;
    cachedEditor_ = nullptr;
    dx7EditorInstance.init(this, synthState, tft);
    pfm3EditorInstance.init(this, synthState, tft);
    commonEditorInstance.init(this, synthState, tft);
    byosEditorInstance.init(this, synthState, tft);
    drum909EditorInstance.init(this, synthState, tft);
}

void ScreenEditor::insertEditor(EditorEngine* editor) {
    editor->nextEditor_ = firstEditor_;
    firstEditor_ = editor;
    cachedEngineType_ = SYNTH_ENGINE_COUNT;
    cachedEditor_ = nullptr;
}

EditorEngine* ScreenEditor::activeEditor() const {
    EditorEngine* engine = editorForEngine();
    if (engine == nullptr) {
        return nullptr;
    }
    int page = synthState_->fullState_.mainPage;
    if (engine->getEngineMenu(page) != nullptr) {
        return engine;
    }
    if (engine->getCommonPage(page) != COMMON_PAGE_NONE) {
        return &commonEditorInstance;
    }
    return nullptr;
}

SynthEngineType ScreenEditor::currentEngineType() const {
    return (SynthEngineType)synthState_->mixerState_.instrumentState_[currentTimbre_].engine;
}

EditorEngine* ScreenEditor::editorForEngine() const {
    SynthEngineType type = currentEngineType();
    if (cachedEditor_ != nullptr && cachedEngineType_ == type) {
        return cachedEditor_;
    }
    for (EditorEngine* editor = firstEditor_; editor != nullptr; editor = editor->nextEditor_) {
        if (editor->getEngineType() == type) {
            cachedEngineType_ = type;
            cachedEditor_ = editor;
            return editor;
        }
    }
    cachedEngineType_ = type;
    cachedEditor_ = nullptr;
    return nullptr;
}

const struct EditorMenu* ScreenEditor::getMainMenu(int page) const {
    EditorEngine* engine = editorForEngine();
    if (engine == nullptr) {
        return nullptr;
    }
    const struct EditorMenu* menu = engine->getEngineMenu(page);
    if (menu != nullptr) {
        return menu;
    }
    int commonPage = engine->getCommonPage(page);
    if (commonPage != COMMON_PAGE_NONE) {
        return commonEditorInstance.getCommonMenu(commonPage);
    }
    return nullptr;
}

const struct EditorMenu* ScreenEditor::currentEditMenu() const {
    return getMainMenu(synthState_->fullState_.mainPage);
}

const char* ScreenEditor::mainMenuName(int button) const {
    const struct EditorMenu* menu = getMainMenu(button);
    return menu != nullptr ? menu->mainMenuName : "";
}

void ScreenEditor::markPresetModified(int timbre) {
    presetModifed_[timbre] = true;
    if (timbre == currentTimbre_) {
        displayPreset();
    }
}

void ScreenEditor::drawAlgorithm() {
    EditorEngine* editor = editorForEngine();
    if (editor != nullptr) {
        editor->drawAlgorithm();
    }
}

void ScreenEditor::newTimbre(int timbre) {
    currentTimbre_ = timbre;
    EditorEngine* editor = editorForEngine();
    if (editor != nullptr) {
        editor->newTimbre();
    }
}

void ScreenEditor::displayParamValue(int encoder, TFT_COLOR color) {
    EditorEngine* editor = activeEditor();
    if (editor != nullptr) {
        editor->displayParamValue(encoder, color);
    }
}

void ScreenEditor::refreshEditorByStep(int& refreshStatus, int& endRefreshStatus) {
    EditorEngine* engineEditor = editorForEngine();
    if (engineEditor == nullptr) {
        return;
    }

    // The selected page may not exist for the current engine (e.g. after an
    // engine switch): fall back to the main menu instead of drawing nothing.
    if (synthState_->fullState_.mainPage >= 0 && activeEditor() == nullptr) {
        synthState_->fullState_.mainPage = -1;
        refreshStatus = 20;
        endRefreshStatus = 0;
        return;
    }

    if (synthState_->fullState_.mainPage == -1) {
        switch (refreshStatus) {
        case 20:
            tft_->pauseRefresh();
            tft_->clear(engineEditor->getTheme().backgroundColor);
            displayPreset();
            tft_->oscilloForceNextDisplay();
            break;
        case 19:
        case 18:
        case 17:
        case 16:
        case 15:
        case 14: {
            int button = 19 - refreshStatus;
            const EditorTheme& theme = engineEditor->getTheme();
            tft_->drawButton(mainMenuName(button), 270, 29, button, 0, 1, theme.buttonBackground, theme.buttonText,
                             theme.buttonTextInactive, theme.buttonIndicatorActive, theme.buttonIndicatorInactive);
            break;
        }
        case 13:
            engineEditor->displayEditorTitle();
            break;
        case 11:
            break;
        case 10:
            engineEditor->drawAlgorithm();
            break;
        }
    } else {
        EditorEngine* editor = activeEditor();
        const struct EditorMenu* editMenu = currentEditMenu();
        if (editor == nullptr || editMenu == nullptr) {
            return;
        }
        if (editor->editPage() >= editMenu->buttonCount) {
            editor->setEditPage(0);
        }
        const struct EditorPageButton* page = editMenu->buttons[editor->editPage()];

        switch (refreshStatus) {
        case 21:
            tft_->pauseRefresh();
            tft_->clear(engineEditor->getTheme().backgroundColor);
            displayPreset();
            tft_->oscilloForceNextDisplay();
            engineEditor->drawAlgorithm();
            break;
        case 20: {
            const EditorTheme& theme = editor->getTheme();
            const EditorTheme& engineTheme = engineEditor->getTheme();
            const TFT_COLOR background = engineTheme.backgroundColor;
            const TFT_COLOR labelBackground = engineTheme.pageLabelBackground;
            const int pageLabelY = LINE_ROW_NAME * TFT_BIG_CHAR_HEIGHT - 8;
            const TFT_COLOR labelColor = labelBackground != COLOR_NONE ? labelBackground : background;
            tft_->pauseRefresh();
            tft_->fillArea(0, 40, 240, 120, background);
            tft_->fillArea(0, 270, 240, 30, background);
            tft_->fillArea(0, pageLabelY - 1, 240, TFT_BIG_CHAR_HEIGHT, labelColor);
            tft_->setCursorInPixel(0, pageLabelY);
            tft_->setCharBackgroundColor(labelColor);
            tft_->setCharColor(theme.pageLabelColor);
            editor->printPageHeader();
            tft_->print(page->states[editor->getButtonState(page)]->stateLabel);
            editor->preparePage();
            break;
        }
        case 18:
        case 17:
        case 16:
        case 15:
        case 14:
        case 13: {
            int button = 18 - refreshStatus;
            editor->drawEditorButton(button, editMenu, page);
            break;
        }
        case 12:
        case 11:
        case 10:
        case 9:
        case 8:
        case 7: {
            int button = 12 - refreshStatus;
            const EditorSlot slot = editor->slotForEncoder(button);
            editor->printParameterName(button, slot, editMenu, page);
            break;
        }
        case 6:
        case 5:
        case 4:
        case 3:
        case 2:
        case 1:
            editor->displayParamValue(6 - refreshStatus, COLOR_WHITE);
            break;
        }
    }

    if (refreshStatus == endRefreshStatus) {
        endRefreshStatus = 0;
        refreshStatus = 0;
    }
}

void ScreenEditor::slowTick() {
    EditorEngine* editor = activeEditor();
    EditorEngine* engineEditor = editorForEngine();
    if (editor != nullptr) {
        editor->slowTick();
    }
    // Also tick the engine editor when a shared page (e.g. Arp/Fx) is active,
    // or on the engine main page (mainPage == -1) where no page editor exists.
    if (engineEditor != nullptr && engineEditor != editor) {
        engineEditor->slowTick();
    }
    displayLevelMeter();
}

void ScreenEditor::displayLevelMeter() {
    static float lastVolume, lastGainReduction;

    if (synthState_->mixerState_.levelMeterWhere_ == 2) {
        float volume = getCompInstrumentVolume(currentTimbre_);
        float gr = getCompInstrumentGainReduction(currentTimbre_);
        // gr tend to slower reach 0
        // Let'as accelerate when we don't want to draw the metter anymore
        if (gr > -.1f) {
            gr = 0;
        }
        if (volume != lastVolume || gr != lastGainReduction) {
            lastVolume = volume;
            lastGainReduction = gr;

            bool isCompressed = synthState_->mixerState_.instrumentState_[currentTimbre_].compressorType > 0;
            tft_->drawLevelMetter(5, 150, 230, 3, 5, volume, isCompressed, gr);
        }
    }
}

struct Pfm3Preset* ScreenEditor::pfm3Preset() const {
    return SynthEngineFactory::getPfm3(currentTimbre_)->getPfm3Preset();
}

void ScreenEditor::displayPreset() {
    const EditorTheme& theme = editorForEngine()->getTheme();
    TFT_COLOR background = theme.headerBackground;
    TFT_COLOR instrumentColor = theme.headerInstrumentColor;
    TFT_COLOR presetNameColor = theme.headerPresetName;

    tft_->fillArea(0, 0, 240, 21, background);

    tft_->setCharBackgroundColor(background);
    tft_->setCharColor(instrumentColor);
    tft_->setCursorInPixel(2, 2);
    if (synthState_->getIsPlayingNote()) {
        tft_->print('~');
    } else {
        tft_->print('I');
    }
    tft_->print((char)('0' + currentTimbre_ + 1));
    tft_->print(' ');
    tft_->setCharColor(presetNameColor);
    tft_->print(pfm3Preset()->presetName);
    tft_->setCharColor(instrumentColor);
    if (presetModifed_[currentTimbre_]) {
        tft_->print('*');
    }
    if (theme.headerBorderBottom != COLOR_NONE) {
        tft_->fillArea(0, 21, 240, 1, theme.headerBorderBottom);
    }
    tft_->setCharBackgroundColor(theme.backgroundColor);
}

void ScreenEditor::resetAllPresetModified() {
    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        presetModifed_[k] = false;
    }
}

void ScreenEditor::setPresetModified(int timbre, bool state) {
    presetModifed_[timbre] = state;
}

void ScreenEditor::encoderTurned(int encoder, int ticks) {
    struct FullState* fullState = &(synthState_->fullState_);

    if (unlikely(fullState->mainPage == -1)) {
        return;
    }

    EditorEngine* editor = activeEditor();
    if (editor != nullptr) {
        editor->encoderTurned(encoder, ticks);
    }
}

void ScreenEditor::newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue) {
    EditorEngine* editor = activeEditor();
    if (editor != nullptr) {
        editor->newEngineParamValue(refreshStatus, timbre, paramId, context, oldValue, newValue);
    }
}

void ScreenEditor::encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) {

    EditorEngine* editor = activeEditor();
    if (editor != nullptr) {
        editor->encoderTurnedWhileButtonPressed(encoder, ticks, button);
    }
}

void ScreenEditor::buttonPressed(int button) {
    if (synthState_->fullState_.mainPage == -1) {
        buttonPressedMenuPage(button);
    } else {
        EditorEngine* editor = activeEditor();
        if (editor != nullptr) {
            editor->buttonPressed(button);
        } else {
            // No editor for this page (e.g. engine switched while on a page
            // that the new engine does not provide): go back to the menu.
            synthState_->fullState_.mainPage = -1;
        }
    }

    synthState_->propagateNewScreenPage();
}

void ScreenEditor::buttonPressedMenuPage(int button) {
    if (button >= BUTTON_SWITCH_1 && button <= BUTTON_SWITCH_6 && getMainMenu(button) != nullptr) {
        synthState_->fullState_.mainPage = button;
        EditorEngine* editor = activeEditor();
        int editPage = editor != nullptr ? editor->editPageSelected(button) : 0;
        const struct EditorMenu* editMenu = currentEditMenu();
        if (editMenu != nullptr && editPage >= editMenu->buttonCount) {
            editPage = 0;
        }
        if (editor != nullptr) {
            editor->setEditPage(editPage);
        }
    }
}

void ScreenEditor::displayPopup(TFT_COLOR color, const char* text, uint8_t length) {
    tft_->fillArea(60, 66, 120, 36, color);
    tft_->fillArea(62, 68, 116, 32, COLOR_BLACK);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(color);
    tft_->setCursorInPixel(119 - ((length * 11) / 2), 75);
    tft_->print(text);
    tft_->waitCycle(500);
    synthState_->propagateNewScreenPage();
}

void ScreenEditor::buttonLongPressed(int instrument, int button) {
    EditorEngine* editor = activeEditor();
    if (editor != nullptr) {
        editor->buttonLongPressed(instrument, button);
    }
}

void ScreenEditor::refreshOscilloBG() {
    EditorEngine* editor = activeEditor();
    if (editor == nullptr) {
        // Main-page menu (mainPage == -1) or invalid page: still clear/redraw
        // the oscilloscope background for the current engine.
        editor = editorForEngine();
    }
    if (editor != nullptr) {
        editor->refreshOscilloBG();
    }
}
