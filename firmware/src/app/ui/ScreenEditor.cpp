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


#include <string.h>
#include <FirmwareTftDisplay.h>
#include "ScreenEditor.h"
#include "EditorDx7.h"
#include "EditorPfm3.h"
#include "EditorCommon.h"
#include "byos/EditorByos.h"
#include "ScreenTypes.h"
#include "SynthState.h"
#include "Timbre.h"
#include "preenfm3.h"

static EditorDx7 dx7EditorInstance;
static EditorPfm3 pfm3EditorInstance;
static EditorCommon commonEditorInstance;
static EditorByos byosEditorInstance;

ScreenEditor::ScreenEditor() {
    firstEditor_ = nullptr;
    cachedEngineType_ = SYNTH_ENGINE_COUNT;
    cachedEditor_ = nullptr;
    currentTimbre_ = 0;
    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        presetModifed_[k] = false;
    }
}

void ScreenEditor::init(SynthState *synthState, FirmwareTftDisplay *tft) {
    synthState_ = synthState;
    tft_ = tft;
    firstEditor_ = nullptr;
    cachedEngineType_ = SYNTH_ENGINE_COUNT;
    cachedEditor_ = nullptr;
    dx7EditorInstance.init(this, synthState, tft);
    pfm3EditorInstance.init(this, synthState, tft);
    commonEditorInstance.init(this, synthState, tft);
    byosEditorInstance.init(this, synthState, tft);
}

void ScreenEditor::insertEditor(EditorEngine *editor) {
    editor->nextEditor = firstEditor_;
    firstEditor_ = editor;
    cachedEngineType_ = SYNTH_ENGINE_COUNT;
    cachedEditor_ = nullptr;
}

EditorEngine *ScreenEditor::activeEditor() const {
    EditorEngine *engine = editorForEngine();
    if (engine == nullptr) {
        return nullptr;
    }
    int page = synthState_->fullState.mainPage;
    if (engine->getEngineMenu(page) != nullptr) {
        return engine;
    }
    if (engine->getCommonPage(page) != COMMON_PAGE_NONE) {
        return &commonEditorInstance;
    }
    return nullptr;
}

SynthEngineType ScreenEditor::currentEngineType() const {
    return (SynthEngineType) synthState_->mixerState.instrumentState_[currentTimbre_].engine;
}

EditorEngine *ScreenEditor::editorForEngine() const {
    SynthEngineType type = currentEngineType();
    if (cachedEditor_ != nullptr && cachedEngineType_ == type) {
        return cachedEditor_;
    }
    for (EditorEngine *editor = firstEditor_; editor != nullptr; editor = editor->nextEditor) {
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

const struct EditorEditMenu* ScreenEditor::getMainMenu(int page) const {
    EditorEngine *engine = editorForEngine();
    if (engine == nullptr) {
        return nullptr;
    }
    const struct EditorEditMenu *menu = engine->getEngineMenu(page);
    if (menu != nullptr) {
        return menu;
    }
    int commonPage = engine->getCommonPage(page);
    if (commonPage != COMMON_PAGE_NONE) {
        return commonEditorInstance.getCommonMenu(commonPage);
    }
    return nullptr;
}

const struct EditorEditMenu* ScreenEditor::currentEditMenu() const {
    return getMainMenu(synthState_->fullState.mainPage);
}

const char* ScreenEditor::mainMenuName(int button) const {
    const struct EditorEditMenu *menu = getMainMenu(button);
    return menu != nullptr ? menu->mainMenuName : "";
}

void ScreenEditor::markPresetModified(int timbre) {
    presetModifed_[timbre] = true;
    if (timbre == currentTimbre_) {
        displayPreset();
    }
}

void ScreenEditor::drawAlgorithm() {
    EditorEngine *editor = editorForEngine();
    if (editor != nullptr) {
        editor->drawAlgorithm();
    }
}

void ScreenEditor::newTimbre(int timbre) {
    currentTimbre_ = timbre;
    EditorEngine *editor = editorForEngine();
    if (editor != nullptr) {
        editor->newTimbre();
    }
}

void ScreenEditor::newPfm3ParamValue(int &refreshStatus, int timbre, int currentRow, int encoder,
    ParameterDisplay *param, float oldValue, float newValue) {
    EditorEngine *editor = activeEditor();
    if (editor != nullptr) {
        editor->newPfm3ParamValue(refreshStatus, timbre, currentRow, encoder, param, oldValue, newValue);
    }
}

void ScreenEditor::displayParamValue(int encoder, TFT_COLOR color) {
    EditorEngine *editor = activeEditor();
    if (editor != nullptr) {
        editor->displayParamValue(encoder, color);
    }
}

void ScreenEditor::refreshEditorByStep(int &refreshStatus, int &endRefreshStatus) {
    EditorEngine *engineEditor = editorForEngine();
    if (engineEditor == nullptr) {
        return;
    }

    // The selected page may not exist for the current engine (e.g. after an
    // engine switch): fall back to the main menu instead of drawing nothing.
    if (synthState_->fullState.mainPage >= 0 && activeEditor() == nullptr) {
        synthState_->fullState.mainPage = -1;
        synthState_->fullState.editPage = 0;
        refreshStatus = 20;
        endRefreshStatus = 0;
        return;
    }

    if (synthState_->fullState.mainPage == -1) {
        switch (refreshStatus) {
        case 20:
            tft_->pauseRefresh();
            tft_->clear();
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
            tft_->drawButton(mainMenuName(button), 270, 29, button, 0, 1);
            break;
        }
        case 13: {
            const char *title = engineEditor->getEngineName();
            int length = strlen(title);
            tft_->setCursorInPixel(120 - (length * TFT_BIG_CHAR_WIDTH) / 2, 60);
            tft_->print(title, COLOR_CYAN, COLOR_BLACK);
            break;
        }
        case 11:
            break;
        case 10:
            engineEditor->drawAlgorithm();
            break;
        }
    } else {
        EditorEngine *editor = activeEditor();
        const struct EditorEditMenu *editMenu = currentEditMenu();
        if (editor == nullptr || editMenu == nullptr) {
            return;
        }
        if (synthState_->fullState.editPage >= editMenu->numberOfButtons) {
            synthState_->fullState.editPage = 0;
        }
        const struct EditorOneButton *page = editMenu->pages[synthState_->fullState.editPage];

        switch (refreshStatus) {
        case 21:
            tft_->pauseRefresh();
            tft_->clear();
            displayPreset();
            tft_->oscilloForceNextDisplay();
            engineEditor->drawAlgorithm();
            break;
        case 20:
            tft_->pauseRefresh();
            tft_->fillArea(0, 40, 240, 120, COLOR_BLACK);
            tft_->fillArea(0, 270, 240, 30, COLOR_BLACK);
            tft_->setCursor(0, 2);
            tft_->setCharBackgroundColor(COLOR_BLACK);
            tft_->setCharColor(COLOR_CYAN);
            editor->drawPageHeader();
            tft_->print(page->states[synthState_->fullState.buttonState[page->buttonId]]->stateLabel);
            editor->preparePage();
            break;
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
            const struct RowEncoder rowEncoder =
                page->states[synthState_->fullState.buttonState[page->buttonId]]->rowEncoder[button];
            editor->drawParameterName(button, rowEncoder, editMenu, page);
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

void ScreenEditor::tempoClick() {
    EditorEngine *editor = activeEditor();
    if (editor != nullptr) {
        editor->tempoClick();
    }
    displayLevelMeter();
}

void ScreenEditor::displayLevelMeter() {
    static float lastVolume, lastGainReduction;

    if (synthState_->mixerState.levelMeterWhere_ == 2) {
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

            bool isCompressed = synthState_->mixerState.instrumentState_[currentTimbre_].compressorType > 0;
            tft_->drawLevelMetter(5, 150, 230, 3, 5, volume, isCompressed, gr);
        }
    }
}

void ScreenEditor::displayPreset() {

    tft_->fillArea(0, 0, 240, 21, COLOR_DARK_BLUE);

    tft_->setCharBackgroundColor(COLOR_DARK_BLUE);
    tft_->setCharColor(COLOR_YELLOW);
    tft_->setCursorInPixel(2, 2);
    if (synthState_->getIsPlayingNote()) {
        tft_->print('~');
    } else {
        tft_->print('I');
    }
    tft_->print((char) ('0' + currentTimbre_ + 1));
    tft_->print(' ');
    tft_->setCharColor(COLOR_WHITE);
    tft_->print(synthState_->pfm3Preset->presetName);
    tft_->setCharColor(COLOR_YELLOW);
    if (presetModifed_[currentTimbre_]) {
        tft_->print('*');
    }
    tft_->setCharBackgroundColor(COLOR_BLACK);
}

void ScreenEditor::resetAllPresetModified() {
    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        presetModifed_[k] = false;
    }
}

void ScreenEditor::setPresetModified(int timbre, bool state) {
    presetModifed_[timbre] = state;
}

void ScreenEditor::updateArpPattern(int currentRow, int encoder, int oldValue, int newValue) {

    const int index = (int) synthState_->pfm3Preset->engineArp2.pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT;
    const arp_pattern_t user_pattern = synthState_->pfm3Preset->engineArpUserPatterns.patterns[index];

    if (synthState_->pfm3Preset->engineArp2.pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT) {
        return;
    }

    if (encoder == 1) {
        // new value to display
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(synthState_->patternSelect, 6);
        tft_->print((char) (127 + ((newValue >> (synthState_->patternSelect)) & 0x1) * 15));
    } else if (encoder == 0) {
        if ((oldValue % 4) == 0) {
            tft_->setCharColor(COLOR_WHITE);
        } else {
            tft_->setCharColor(COLOR_GRAY);
        }
        tft_->setCursor(oldValue, 6);
        tft_->print((char) (127 + ((user_pattern >> (oldValue)) & 0x1) * 15));
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(newValue, 6);
        tft_->print((char) (127 + ((user_pattern >> (newValue)) & 0x1) * 15));
    }
}

void ScreenEditor::updateStepSequencer(int currentRow, int encoder, int oldValue, int newValue) {
    int whichStepSeq = currentRow - ROW_LFOSEQ1;

    StepSequencerSteps *seqSteps = &((StepSequencerSteps*) (&synthState_->pfm3Preset->lfoSteps1))[whichStepSeq];

    tft_->setCharBackgroundColor(COLOR_BLACK);

    if (encoder == 3) {
        // new value to display
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(synthState_->stepSelect[whichStepSeq], 6);
        tft_->print((char) (127 + newValue));
    } else if (encoder == 2) {
        if ((oldValue % 4) == 0) {
            tft_->setCharColor(COLOR_WHITE);
        } else {
            tft_->setCharColor(COLOR_GRAY);
        }
        tft_->setCursor(oldValue, 6);
        tft_->print((char) (127 + seqSteps->steps[oldValue]));
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(newValue, 6);
        tft_->print((char) (127 + seqSteps->steps[newValue]));
    }
}

void ScreenEditor::encoderTurned(int encoder6, int ticks) {
    struct FullState *fullState = &(synthState_->fullState);

    if (unlikely(fullState->mainPage == -1)) {
        return;
    }

    EditorEngine *editor = activeEditor();
    if (editor != nullptr) {
        editor->encoderTurned(encoder6, ticks);
    }
}

void ScreenEditor::newEngineParamValue(int timbre, int paramId, int context, float oldValue, float newValue) {
    EditorEngine *editor = activeEditor();
    if (editor != nullptr) {
        editor->newEngineParamValue(timbre, paramId, context, oldValue, newValue);
    }
}

void ScreenEditor::encoderTurnedWhileButtonPressed(int encoder6, int ticks, int button) {

    EditorEngine *editor = activeEditor();
    if (editor != nullptr) {
        editor->encoderTurnedWhileButtonPressed(encoder6, ticks, button);
    }
}

void ScreenEditor::buttonPressed(int button) {
    if (synthState_->fullState.mainPage == -1) {
        buttonPressedMenuPage(button);
    } else {
        EditorEngine *editor = activeEditor();
        if (editor != nullptr) {
            editor->buttonPressed(button);
        } else {
            // No editor for this page (e.g. engine switched while on a page
            // that the new engine does not provide): go back to the menu.
            synthState_->fullState.mainPage = -1;
            synthState_->fullState.editPage = 0;
        }
    }

    synthState_->propagateNewPfm3Page();
}

void ScreenEditor::buttonPressedMenuPage(int button) {
    if (button >= BUTTON_PFM3_1 && button <= BUTTON_PFM3_6
            && getMainMenu(button) != nullptr) {
        synthState_->fullState.mainPage = button;
        int editPage = editPageSelected_[synthState_->fullState.mainPage];
        if (editPage >= currentEditMenu()->numberOfButtons) {
            editPage = 0;
        }
        synthState_->fullState.editPage = editPage;
     }
}

void ScreenEditor::displayPopup(TFT_COLOR color, char* text, uint8_t length) {
    tft_->fillArea(60, 66, 120, 36, color);
    tft_->fillArea(62, 68, 116, 32, COLOR_BLACK);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(color);
    tft_->setCursorInPixel(119 - ((length * 11) / 2), 75);
    tft_->print(text);
    tft_->waitCycle(500);
    synthState_->propagateNewPfm3Page();
}

void ScreenEditor::buttonLongPressed(int instrument, int button) {
    EditorEngine *editor = activeEditor();
    if (editor != nullptr) {
        editor->buttonLongPressed(instrument, button);
    }
}

void ScreenEditor::refreshOscilloBG() {
    EditorEngine *editor = activeEditor();
    if (editor == nullptr) {
        // Main-page menu (mainPage == -1) or invalid page: still clear/redraw
        // the oscilloscope background for the current engine.
        editor = editorForEngine();
    }
    if (editor != nullptr) {
        editor->refreshOscilloBG();
    }
}
