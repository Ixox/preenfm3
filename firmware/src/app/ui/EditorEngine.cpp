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
#include "ScreenEditor.h"
#include "SynthState.h"

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
    uint8_t state = synthState_->fullState_.buttonState[page->buttonId];
    if (state >= (uint8_t)page->stateCount) {
        state = 0;
    }
    return page->states[state]->slots[encoder];
}

int EditorEngine::currentTimbre() const {
    return synthState_->getCurrentTimbre();
}

void EditorEngine::drawOperatorSelector(int button, int maxOperator) {
    const FullState& fs = synthState_->fullState_;
    tft_->drawButtonBiState(270, 29, button, operatorNumber_, maxOperator, fs.buttonState[BUTTONID_OPERATOR_1_2 + button - 3],
                            (button - 3) * 2);
}

void EditorEngine::drawMatrixSelector(int button, uint8_t pageButtonId, int maxRow) {
    const FullState& fs = synthState_->fullState_;
    tft_->drawButtonBiState(270, 29, button, editPage_ * 2 + fs.buttonState[pageButtonId], maxRow,
                            fs.buttonState[BUTTONID_MATRIX_1_2 + button], button * 2);
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
    if (button < editMenu->buttonCount) {
        const struct EditorPageButton* buttonPage = editMenu->buttons[button];
        tft_->drawButton(buttonPage->buttonLabel, 270, 29, button, buttonPage->stateCount,
                         editPage_ == button ? synthState_->fullState_.buttonState[page->buttonId] + 1 : 0);
    } else {
        tft_->drawButton("", 270, 29, button, 0, 0);
    }
}
