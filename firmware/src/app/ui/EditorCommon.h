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

#ifndef EDITOR_COMMON
#define EDITOR_COMMON

#include "EditorParamPage.h"

/*
 * Editor for pages shared by all engines (Arp/Fx). The page is backed by the
 * PFM3 flat parameter model, so it reuses EditorParamPage mechanics.
 */
class EditorCommon : public EditorParamPage {
public:
    EditorCommon();

    void init(ScreenEditor* parent, SynthState* synthState, FirmwareTftDisplay* tft) override;
    SynthEngineType getEngineType() const override;
    const char* getEngineName() const override;
    const struct EditorEditMenu* getEngineMenu(int page) const override;
    const struct EditorEditMenu* getCommonMenu(int commonPage) const;
};

#endif
