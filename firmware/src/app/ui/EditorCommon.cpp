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

static const struct EditorOneButtonState pfm3ButtonArpeggiator1State = {
    "Arpeggiator",
    {
        {
            ROW_ARPEGGIATOR1,
            ENCODER_ARPEGGIATOR_CLOCK },
        {
            ROW_ARPEGGIATOR1,
            ENCODER_ARPEGGIATOR_BPM },
        {
            ROW_ARPEGGIATOR1,
            ENCODER_ARPEGGIATOR_DIRECTION },
        {
            ROW_ARPEGGIATOR1,
            ENCODER_ARPEGGIATOR_OCTAVE },
        {
            ROW_ARPEGGIATOR2,
            ENCODER_ARPEGGIATOR_DIVISION },
        {
            ROW_ARPEGGIATOR2,
            ENCODER_ARPEGGIATOR_DURATION } } };

static const struct EditorOneButtonState pfm3ButtonArpeggiator2State = {
    "Arpeggiator",
    {
        {
            ROW_ARPEGGIATOR2,
            ENCODER_ARPEGGIATOR_PATTERN },
        {
            ROW_ARPEGGIATOR2,
            ENCODER_ARPEGGIATOR_LATCH },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_ARPEGGIATOR3,
            0 },
        {
            ROW_ARPEGGIATOR3,
            1 },
        {
            ROW_ARPEGGIATOR3,
            2 } } };

static const struct EditorOneButton pfm3ButtonArpeggiator = {
    "Arp",
    BUTTONID_EDIT_ARPEGIATOR,
    2,
    {
        &pfm3ButtonArpeggiator1State,
        &pfm3ButtonArpeggiator2State } };

static const struct EditorOneButtonState pfm3ButtonFx1State = {
    "Voice FX",
    {
        {
            ROW_EFFECT1,
            ENCODER_EFFECT_TYPE },
        {
            ROW_EFFECT1,
            ENCODER_EFFECT_PARAM1 },
        {
            ROW_EFFECT1,
            ENCODER_EFFECT_PARAM2 },
        {
            ROW_EFFECT1,
            ENCODER_EFFECT_PARAM3 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE } } };

static const struct EditorOneButtonState pfm3ButtonFx2State = {
    "Paraphonic FX",
    {
        {
            ROW_EFFECT2,
            ENCODER_EFFECT_TYPE },
        {
            ROW_EFFECT2,
            ENCODER_EFFECT_PARAM1 },
        {
            ROW_EFFECT2,
            ENCODER_EFFECT_PARAM2 },
        {
            ROW_EFFECT2,
            ENCODER_EFFECT_PARAM3 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE } } };

static const struct EditorOneButton pfm3ButtonFx1 = {
    "Fx1",
    BUTTONID_ONLY_ONE_STATE,
    1,
    {
        &pfm3ButtonFx1State } };

static const struct EditorOneButton pfm3ButtonFx2 = {
    "Fx2",
    BUTTONID_ONLY_ONE_STATE,
    1,
    {
        &pfm3ButtonFx2State } };

static const struct EditorEditMenu pfm3ArpFilterMenu = {
    "Arp/Fx",
    3,
    {
        &pfm3ButtonArpeggiator,
        &pfm3ButtonFx1,
        &pfm3ButtonFx2
    } };

EditorCommon::EditorCommon() {
}

void EditorCommon::init(ScreenEditor *parent, SynthState *synthState, FirmwareTftDisplay *tft) {
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

const struct EditorEditMenu* EditorCommon::getEngineMenu(int page) const {
    (void) page;
    return nullptr;
}

const struct EditorEditMenu* EditorCommon::getCommonMenu(int commonPage) const {
    if (commonPage == COMMON_PAGE_ARP_FX) {
        return &pfm3ArpFilterMenu;
    }
    return nullptr;
}
