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

#include "EditorCommon.h"
#include "SynthState.h"

// clang-format off
static const EditorPageState arp1GenericState = {
    "Arpeggiator",
    {
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Arp1, 0, 0), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Arp1, 0, 1), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Arp1, 0, 2), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Arp1, 0, 3), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Arp2, 0, 1), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Arp2, 0, 2), 1),
    } };

static const EditorPageState arp2GenericState = {
    "Arpeggiator",
    {
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Arp2, 0, 0), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Arp2, 0, 3), 1),
        BUTTON_EMPTY(),
        { EditorSlotKind::Custom, 0, 0 },
        { EditorSlotKind::Custom, 1, 0 },
        { EditorSlotKind::Custom, 2, 0 },
    } };

static const EditorPageState fx1GenericState = {
    "Voice FX",
    {
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Effect1, 0, 0), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Effect1, 0, 1), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Effect1, 0, 2), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Effect1, 0, 3), 1),
        BUTTON_EMPTY(),
        BUTTON_EMPTY(),
    } };

static const EditorPageState fx2GenericState = {
    "Paraphonic FX",
    {
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Effect2, 0, 0), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Effect2, 0, 1), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Effect2, 0, 2), 1),
        BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Effect2, 0, 3), 1),
        BUTTON_EMPTY(),
        BUTTON_EMPTY(),
    } };
// clang-format on

EditorCommon::EditorCommon() {
}

static const struct EditorPageButton pfm3ButtonArpeggiator = {"Arp", BUTTONID_EDIT_ARPEGIATOR, 2, {&arp1GenericState, &arp2GenericState}};

static const struct EditorPageButton pfm3ButtonFx1 = {"Fx1", BUTTONID_ONLY_ONE_STATE, 1, {&fx1GenericState}};

static const struct EditorPageButton pfm3ButtonFx2 = {"Fx2", BUTTONID_ONLY_ONE_STATE, 1, {&fx2GenericState}};

static const struct EditorMenu pfm3ArpFilterMenu = {"Arp/Fx", 3, {&pfm3ButtonArpeggiator, &pfm3ButtonFx1, &pfm3ButtonFx2}};

void EditorCommon::init(ScreenEditor* parent, SynthState* synthState, FirmwareTftDisplay* tft) {
    parent_ = parent;
    synthState_ = synthState;
    tft_ = tft;
}

SynthEngineType EditorCommon::getEngineType() const {
    return SYNTH_ENGINE_COUNT;
}

const char* EditorCommon::getEngineName() const {
    return "";
}

const struct EditorMenu* EditorCommon::getEngineMenu(int page) const {
    (void)page;
    return nullptr;
}

const struct EditorMenu* EditorCommon::getCommonMenu(int commonPage) const {
    if (commonPage == COMMON_PAGE_ARP_FX) {
        return &pfm3ArpFilterMenu;
    }
    return nullptr;
}
