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

const struct EditorEditMenu* EditorEngine::currentEditMenu() const {
    return parent_->getMainMenu(synthState_->fullState.mainPage);
}

const struct EditorOneButton* EditorEngine::currentPage() const {
    const struct EditorEditMenu *editMenu = currentEditMenu();
    if (editMenu == nullptr) {
        return nullptr;
    }
    int editPage = synthState_->fullState.editPage;
    if (editPage >= editMenu->numberOfButtons) {
        editPage = 0;
    }
    return editMenu->pages[editPage];
}

struct RowEncoder EditorEngine::rowEncoderFor(int encoder) const {
    const struct EditorOneButton *page = currentPage();
    if (page == nullptr) {
        struct RowEncoder none = { ROW_NONE, ENCODER_NONE };
        return none;
    }
    return page->states[synthState_->fullState.buttonState[page->buttonId]]->rowEncoder[encoder];
}

int EditorEngine::currentTimbre() const {
    return synthState_->getCurrentTimbre();
}
