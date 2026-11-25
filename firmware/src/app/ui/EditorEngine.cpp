/*
 * Copyright 2026 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "EditorEngine.h"

#include <string.h>

#include "ScreenEditor.h"
#include "SynthState.h"

void EditorEngine::displayEditorTitle() {
    const char* title = getEngineName();
    int length = (int)strlen(title);
    tft_->setCursorInPixel(120 - (length * TFT_BIG_CHAR_WIDTH) / 2, 60);
    tft_->print(title, COLOR_CYAN_LIGHT, getTheme().backgroundColor);
}

uint8_t EditorEngine::getButtonState(uint8_t buttonId) const {
    if (buttonId >= BUTTONID_ENGINE_PAGE_1 && buttonId <= BUTTONID_ENGINE_PAGE_8) {
        return enginePageState_[buttonId - BUTTONID_ENGINE_PAGE_1];
    }
    return synthState_->fullState_.buttonState[buttonId];
}

uint8_t EditorEngine::getButtonState(const struct EditorPageButton* page) const {
    uint8_t state = getButtonState(page->buttonId);
    if (state >= (uint8_t)page->stateCount) {
        state = 0;
    }
    return state;
}

uint8_t& EditorEngine::buttonStateRef(uint8_t buttonId) {
    if (buttonId >= BUTTONID_ENGINE_PAGE_1 && buttonId <= BUTTONID_ENGINE_PAGE_8) {
        return enginePageState_[buttonId - BUTTONID_ENGINE_PAGE_1];
    }
    return synthState_->fullState_.buttonState[buttonId];
}

const struct EditorMenu* EditorEngine::currentEditMenu() const {
    return parent_->getMainMenu(synthState_->fullState_.mainPage);
}

const struct EditorPageButton* EditorEngine::currentPage() const {
    const struct EditorMenu* editMenu = currentEditMenu();
    if (editMenu == nullptr) {
        return nullptr;
    }
    int editPage = editPage_;
    if (editPage >= editMenu->buttonCount) {
        editPage = 0;
    }
    return editMenu->buttons[editPage];
}

EditorSlot EditorEngine::slotForEncoder(int encoder) const {
    const struct EditorPageButton* page = currentPage();
    if (page == nullptr) {
        EditorSlot none = {EditorSlotKind::Empty, 0, 0};
        return none;
    }
    return page->states[getButtonState(page)]->slots[encoder];
}

int EditorEngine::currentTimbre() const {
    return synthState_->getCurrentTimbre();
}

void EditorEngine::drawOperatorSelector(int button, int maxOperator) {
    const FullState& fs = synthState_->fullState_;
    const EditorTheme& theme = getTheme();
    tft_->drawButtonBiState(270, 29, button, operatorNumber_, maxOperator, fs.buttonState[BUTTONID_OPERATOR_1_2 + button - 3],
                            (button - 3) * 2, theme.buttonBackground, theme.buttonText, theme.buttonTextInactive,
                            theme.buttonIndicatorActive, theme.buttonIndicatorInactive, theme.buttonSelectorFilled);
}

void EditorEngine::drawMatrixSelector(int button, uint8_t pageButtonId, int maxRow) {
    const FullState& fs = synthState_->fullState_;
    const EditorTheme& theme = getTheme();
    tft_->drawButtonBiState(270, 29, button, editPage_ * 2 + fs.buttonState[pageButtonId], maxRow,
                            fs.buttonState[BUTTONID_MATRIX_1_2 + button], button * 2, theme.buttonBackground, theme.buttonText,
                            theme.buttonTextInactive, theme.buttonIndicatorActive, theme.buttonIndicatorInactive,
                            theme.buttonSelectorFilled);
}

void EditorEngine::selectOperator(int button, int maxOperator) {
    FullState& fs = synthState_->fullState_;
    uint8_t buttonSelected = button - BUTTON_SWITCH_4;
    uint8_t currentButton = operatorNumber_ >> 1;
    uint8_t buttonId = BUTTONID_OPERATOR_1_2 + buttonSelected;
    uint8_t newButtonState = fs.buttonState[buttonId];
    if (buttonSelected == currentButton) {
        newButtonState = (newButtonState + 1) & 0x1;
    }
    uint8_t newOperatorSelect = buttonSelected * 2 + newButtonState;
    if (newOperatorSelect <= maxOperator) {
        operatorNumber_ = newOperatorSelect;
        fs.buttonState[buttonId] = newButtonState;
        updatePage();
    }
}

void EditorEngine::drawEditorButtonDefault(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page) {
    const EditorTheme& theme = getTheme();
    if (button < editMenu->buttonCount) {
        const struct EditorPageButton* buttonPage = editMenu->buttons[button];
        tft_->drawButton(buttonPage->buttonLabel, 270, 29, button, buttonPage->stateCount,
                         editPage_ == button ? getButtonState(page->buttonId) + 1 : 0, theme.buttonBackground, theme.buttonText,
                         theme.buttonTextInactive, theme.buttonIndicatorActive, theme.buttonIndicatorInactive);
    } else {
        tft_->drawButton("", 270, 29, button, 0, 0, theme.buttonBackground, theme.buttonText, theme.buttonTextInactive,
                         theme.buttonIndicatorActive, theme.buttonIndicatorInactive);
    }
}
