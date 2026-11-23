/*
 * Copyright 2022 Xavier Hosxe
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

#include "FirmwareTftDisplay.h"
#include "Common.h"
#include "engine/pfm3/Env.h"

extern DMA2D_HandleTypeDef hdma2d;
extern RNG_HandleTypeDef hrng;

extern uint16_t tftPalette565[NUMBER_OF_TFT_COLORS];

const float randomDisplay[32] = {0.679268211,  -0.559346101, 0.93210829,   0.263967825,  0.868206376, -0.955814486, 0.767822461,
                                 -0.260222359, -0.617473204, -0.844648436, 0.380078337,  0.481601148, -0.807739871, 0.995462356,
                                 -0.854993442, 0.704896648,  0.258727388,  0.947767668,  -0.12564,    0.513101208,  -0.75714895,
                                 -0.11723284,  0.026869686,  -0.738605819, -0.262181885, 0.856453,    0.209812963,  -0.976879332,
                                 -0.019152814, -0.213303568, 0.55,         0.379348783};

FirmwareTftDisplay::FirmwareTftDisplay() : TftDisplay() {
    oscilloIsClean_ = true;
    envInQueue_ = 0;
    lfoInQueue_ = 0;
    operatorInQueue_ = 0;
    arpFullInQueue_ = 0;
    arpPartialInQueue_ = 0;
    arpStepSelected_ = 0;
    arpStepLength_ = 16;
    arpStepModeMask_ = 0;
    for (int k = 0; k < 16; k++) {
        arpStepLevels_[k] = 0;
        arpStepColors_[k] = COLOR_WHITE;
        arpStepModeColors_[k] = COLOR_WHITE;
    }
}

FirmwareTftDisplay::~FirmwareTftDisplay() {
    // TODO Auto-generated destructor stub
}

void FirmwareTftDisplay::clearActions() {
    tftActions_.clear();
    envInQueue_ = 0;
    lfoInQueue_ = 0;
    operatorInQueue_ = 0;
    arpFullInQueue_ = 0;
    arpPartialInQueue_ = 0;
    arpPending_ = ArpBgUpdate();
}

void FirmwareTftDisplay::initWaveFormExt(int index, const float* waveform, int size) {
    waveForm_[index].waveForms = waveform;
    waveForm_[index].size = size;
}

void FirmwareTftDisplay::additionalActions() {

    // Just in case
    if (tftActions_.getCount() == 0) {
        envInQueue_ = 0;
        lfoInQueue_ = 0;
        operatorInQueue_ = 0;
        arpFullInQueue_ = 0;
        arpPartialInQueue_ = 0;
    }

    switch (currentAction_.actionType) {
    case TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM:
        switch (currentAction_.param3) {
        case 0:
            // Copy bgOscillo2 to bgOscillo (that clears current waveform in oscilo)
            if (PFM_IS_DMA2D_READY()) {

                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
                WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[COLOR_BLACK]);

                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 2);
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (98 | (158 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)(bgOscillo_ + 161));

                PFM_START_DMA2D();
                currentAction_.param3 = 1;
            }
            break;
        case 1:
            if (currentAction_.param1 < TFT_NUMBER_OF_WAVEFORM_EXT) {
                oscilloBgDrawOperatorShape(waveForm_[currentAction_.param1].waveForms, waveForm_[currentAction_.param1].size);
                operatorInQueue_--;
            } else if (currentAction_.param1 == TFT_DRAW_ENVELOPPE) {
                oscilloBgDrawEnvelope();
                envInQueue_--;
            } else if (currentAction_.param1 == TFT_DRAW_LFO) {
                oscilloBgDrawLfo();
                lfoInQueue_--;
            } else if (currentAction_.param1 == TFT_DRAW_ARP_STEPS) {
                if (currentAction_.param2 == 1) {
                    drawArpStepsFull();
                    arpPending_ = ArpBgUpdate();
                    arpFullInQueue_--;
                } else {
                    if (!arpPending_.full) {
                        drawArpStepsPartial();
                    }
                    arpPartialInQueue_--;
                }
            }
            currentAction_.actionType = 0;
            oscilloForceNextDisplay();
            break;
        }
    }
}

void FirmwareTftDisplay::oscilloBgDrawOperatorShape(const float* waveForm, int size) {

    int indexMiddle = 50 * 160;
    uint16_t oscilloColor = tftPalette565[COLOR_YELLOW];

    for (int x = 0; x < 160; x++) {
        float index = x * ((float)size) / 160.0f;
        oscilloYValue_[x] = (int)(waveForm[(int)index] * 48.0f);
    }

    for (int x = 1; x < 159; x++) {
        if (oscilloYValue_[x] < (oscilloYValue_[x + 1] - 1)) {
            for (int oy = oscilloYValue_[x]; oy < oscilloYValue_[x + 1]; oy++) {
                bgOscillo_[indexMiddle + x + oy * 160] = oscilloColor;
            }
        } else if (oscilloYValue_[x] > (oscilloYValue_[x + 1] + 1)) {
            for (int oy = oscilloYValue_[x]; oy > oscilloYValue_[x + 1]; oy--) {
                bgOscillo_[indexMiddle + x + oy * 160] = oscilloColor;
            }
        } else {
            bgOscillo_[indexMiddle + x + oscilloYValue_[x] * 160] = oscilloColor;
        }
    }
}

void FirmwareTftDisplay::oscilloBgActionClear() {
    if (!oscilloIsClean_) {
        TFTAction newAction;
        newAction.param1 = 255;
        newAction.param3 = 0;
        newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
        tftActions_.insert(newAction);
        oscilloIsClean_ = true;
    }
}

void FirmwareTftDisplay::oscilloBgActionOperatorShape(int wfNumber) {
    if (operatorInQueue_ >= 2) {
        // No need to ask for new drawing
        // params have been updated and next drawing will use them
        return;
    }
    operatorInQueue_++;
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
    newAction.param1 = wfNumber;
    newAction.param3 = 0;
    tftActions_.insert(newAction);

    oscilloIsClean_ = false;
}

void FirmwareTftDisplay::oscilloBgActionLfo() {
    if (lfoInQueue_ >= 2) {
        // No need to ask for new drawing
        // params have been updated and next drawing will use them
        return;
    }
    lfoInQueue_++;

    TFTAction newAction;
    newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
    newAction.param1 = TFT_DRAW_LFO;
    newAction.param3 = 0;
    tftActions_.insert(newAction);

    oscilloIsClean_ = false;
}

void FirmwareTftDisplay::oscilloBgActionEnvelope() {
    if (envInQueue_ >= 2) {
        // No need to ask for new drawing
        // params have been updated and next drawing will use them
        return;
    }
    envInQueue_++;
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
    newAction.param1 = TFT_DRAW_ENVELOPPE;
    newAction.param3 = 0;
    tftActions_.insert(newAction);

    oscilloIsClean_ = false;
}

void FirmwareTftDisplay::oscilloBgSetArpSteps(const uint8_t levels[16], const uint8_t colors[16], const uint8_t modeColors[16],
                                              uint16_t modeMask, int selected, int length) {
    for (int k = 0; k < 16; k++) {
        arpStepLevels_[k] = levels[k] > 15 ? 15 : levels[k];
        arpStepColors_[k] = colors[k];
        arpStepModeColors_[k] = modeColors[k];
    }
    arpStepModeMask_ = modeMask;
    arpStepSelected_ = (uint8_t)((selected < 0) ? 0 : (selected > 15 ? 15 : selected));
    arpStepLength_ = (uint8_t)((length < 1) ? 1 : (length > 16 ? 16 : length));
}

static const int kArpIndexLow = 98 * 160 + 1;
static const int kArpBandBottom = 97 - (ARP_STEP_BAR_MAX_HEIGHT - 1);
static const int kArpBandTop = 97;
static const int kArpModeBottom = kArpBandBottom + (ARP_STEP_BAR_MAX_HEIGHT - ARP_STEP_BAR_MODE_SIZE);
static const int kArpLineBottom = kArpBandBottom - 5;

static int arpCellWidth(int k) {
    int w = ARP_STEP_BAR_WIDTH;
    const int x0 = k * ARP_STEP_PITCH;
    if (x0 + w > 158) {
        w = 158 - x0;
    }
    return w;
}

void FirmwareTftDisplay::oscilloBgArp(const ArpBgUpdate& update) {
    if (update.full) {
        arpPending_.full = true;
        arpPending_.stepMask = 0;
        arpPending_.modeMask = 0;
        arpPending_.cursorMask = 0;
        arpPending_.lengthMask = 0;
        queueArpSteps(true);
        return;
    }
    if (arpPending_.full) {
        return;
    }
    arpPending_.stepMask |= update.stepMask;
    arpPending_.modeMask |= update.modeMask;
    arpPending_.cursorMask |= update.cursorMask;
    arpPending_.lengthMask |= update.lengthMask;
    queueArpSteps(false);
}

void FirmwareTftDisplay::queueArpSteps(bool full) {
    if (full) {
        if (arpFullInQueue_ == 0) {
            arpFullInQueue_++;
            TFTAction newAction;
            newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
            newAction.param1 = TFT_DRAW_ARP_STEPS;
            newAction.param2 = 1;
            newAction.param3 = 0;
            tftActions_.insert(newAction);
            oscilloIsClean_ = false;
        }
        return;
    }
    if (arpFullInQueue_ == 0 && arpPartialInQueue_ == 0) {
        arpPartialInQueue_++;
        TFTAction newAction;
        newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
        newAction.param1 = TFT_DRAW_ARP_STEPS;
        newAction.param2 = 0;
        newAction.param3 = 1;
        tftActions_.insert(newAction);
        oscilloIsClean_ = false;
    }
}

void FirmwareTftDisplay::drawArpStepCell(int k) {
    const int x0 = k * ARP_STEP_PITCH;
    const int w = arpCellWidth(k);
    const int stepH = (arpStepLevels_[k] * (ARP_STEP_BAR_MAX_HEIGHT - ARP_STEP_BAR_MODE_SIZE)) / 15;
    const uint16_t color = tftPalette565[arpStepColors_[k]];
    const uint16_t black = tftPalette565[COLOR_BLACK];
    for (int y = kArpBandBottom; y < kArpModeBottom; y++) {
        uint16_t* row = &bgOscillo_[kArpIndexLow + x0 - y * 160];
        const uint16_t c = (y < kArpBandBottom + stepH) ? color : black;
        for (int x = 0; x < w; x++) {
            row[x] = c;
        }
    }
}

void FirmwareTftDisplay::drawArpModeCell(int k) {
    const int x0 = k * ARP_STEP_PITCH;
    const int w = arpCellWidth(k);
    const bool hasMode = ((arpStepModeMask_ >> k) & 0x1u) != 0;
    const uint16_t color = hasMode ? tftPalette565[arpStepModeColors_[k]] : tftPalette565[COLOR_BLACK];
    for (int y = kArpModeBottom; y <= kArpBandTop; y++) {
        uint16_t* row = &bgOscillo_[kArpIndexLow + x0 - y * 160];
        for (int x = 0; x < w; x++) {
            row[x] = color;
        }
    }
}

void FirmwareTftDisplay::drawArpEndLine(int length, bool draw) {
    if (length < 1 || length > 16) {
        return;
    }
    int x = length * ARP_STEP_PITCH - 1;
    if (x < 0) {
        x = 0;
    } else if (x > 157) {
        x = 157;
    }
    if (draw) {
        const uint16_t color = tftPalette565[COLOR_RED];
        for (int y = kArpLineBottom; y <= kArpBandTop; y++) {
            bgOscillo_[kArpIndexLow + x - y * 160] = color;
        }
    } else if (length == 16) {
        // The length-16 end line sits inside the last step: restore the cell
        // and the area below the baseline down to the cursor bottom.
        drawArpStepCell(15);
        drawArpModeCell(15);
        const uint16_t black = tftPalette565[COLOR_BLACK];
        for (int y = kArpLineBottom; y < kArpBandBottom - 1; y++) {
            bgOscillo_[kArpIndexLow + x - y * 160] = black;
        }
    } else {
        const uint16_t separator = tftPalette565[COLOR_DARK_GRAY];
        for (int y = kArpBandBottom - 1; y <= kArpBandTop; y++) {
            bgOscillo_[kArpIndexLow + x - y * 160] = separator;
        }
        const uint16_t black = tftPalette565[COLOR_BLACK];
        for (int y = kArpLineBottom; y < kArpBandBottom - 1; y++) {
            bgOscillo_[kArpIndexLow + x - y * 160] = black;
        }
    }
}

void FirmwareTftDisplay::clearArpArrow(int k) {
    const int center = k * ARP_STEP_PITCH + ARP_STEP_BAR_WIDTH / 2;
    static const uint8_t arrowWidths[4] = {1, 3, 5, 7};
    const uint16_t black = tftPalette565[COLOR_BLACK];
    for (int row = 0; row < 4; row++) {
        const int half = arrowWidths[row] / 2;
        const int y = kArpBandBottom - 2 - row;
        for (int x = center - half; x <= center + half; x++) {
            if (x >= 0 && x <= 157) {
                bgOscillo_[kArpIndexLow + x - y * 160] = black;
            }
        }
    }
}

void FirmwareTftDisplay::drawArpArrow(int k) {
    const int center = k * ARP_STEP_PITCH + ARP_STEP_BAR_WIDTH / 2;
    static const uint8_t arrowWidths[4] = {1, 3, 5, 7};
    const uint16_t color = tftPalette565[COLOR_YELLOW];
    for (int row = 0; row < 4; row++) {
        const int half = arrowWidths[row] / 2;
        const int y = kArpBandBottom - 2 - row;
        for (int x = center - half; x <= center + half; x++) {
            if (x >= 0 && x <= 157) {
                bgOscillo_[kArpIndexLow + x - y * 160] = color;
            }
        }
    }
}

void FirmwareTftDisplay::drawArpBaselineSeparators() {
    const uint16_t color = tftPalette565[COLOR_DARK_GRAY];
    for (int x = 0; x < 158; x++) {
        bgOscillo_[kArpIndexLow + x - (kArpBandBottom - 1) * 160] = color;
    }
    for (int k = 1; k < 16; k++) {
        const int x = k * ARP_STEP_PITCH - 1;
        for (int y = kArpBandBottom - 1; y <= kArpBandTop; y++) {
            bgOscillo_[kArpIndexLow + x - y * 160] = color;
        }
    }
}

void FirmwareTftDisplay::drawArpStepsFull() {
    drawArpBaselineSeparators();
    for (int k = 0; k < 16; k++) {
        drawArpStepCell(k);
        drawArpModeCell(k);
    }
    drawArpEndLine(arpStepLength_, true);
    drawArpArrow(arpStepSelected_);
}

void FirmwareTftDisplay::drawArpStepsPartial() {
    const ArpBgUpdate d = arpPending_;
    for (int k = 0; k < 16; k++) {
        if ((d.stepMask >> k) & 0x1u) {
            drawArpStepCell(k);
        }
        if ((d.modeMask >> k) & 0x1u) {
            drawArpModeCell(k);
        }
    }
    for (int l = 0; l < 16; l++) {
        if ((d.lengthMask >> l) & 0x1u) {
            drawArpEndLine(l + 1, false);
        }
    }
    for (int k = 0; k < 16; k++) {
        if ((d.cursorMask >> k) & 0x1u) {
            clearArpArrow(k);
        }
    }
    // Draw the current end line before the arrow so the arrow stays on top.
    drawArpEndLine(arpStepLength_, true);
    drawArpArrow(arpStepSelected_);
    arpPending_ = ArpBgUpdate();
}

void FirmwareTftDisplay::oscilloBgSetEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL, int8_t aCurve,
                                              int8_t dCurve, int8_t sCurve, int8_t rCurve) {
    oscilParams1_[0] = a;
    oscilParams1_[1] = d + oscilParams1_[0];
    oscilParams1_[2] = s + oscilParams1_[1];
    oscilParams1_[3] = r + oscilParams1_[2];

    oscilParams2_[0] = aL;
    oscilParams2_[1] = dL;
    oscilParams2_[2] = sL;
    oscilParams2_[3] = rL;

    envCurve_[0] = aCurve;
    envCurve_[1] = dCurve;
    envCurve_[2] = sCurve;
    envCurve_[3] = rCurve;

    oscilParams1_[4] = COLOR_YELLOW;
}

void FirmwareTftDisplay::oscilloBgSetLfoEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL) {
    oscilParams1_[0] = a;
    oscilParams1_[1] = d + oscilParams1_[0];
    oscilParams1_[2] = s + oscilParams1_[1];
    oscilParams1_[3] = r + oscilParams1_[2];

    oscilParams2_[0] = aL;
    oscilParams2_[1] = dL;
    oscilParams2_[2] = sL;
    oscilParams2_[3] = rL;

    envCurve_[0] = 1;
    envCurve_[1] = 1;
    envCurve_[2] = 1;
    envCurve_[3] = 1;

    oscilParams1_[4] = COLOR_BLUE;
}

void FirmwareTftDisplay::oscilloBgSetLfo(float shape, float freq, float kSyn, float bias, float phase) {
    if (freq >= 100.0f) {
        freq = 1.0f;
    }
    oscilParams1_[0] = shape;
    oscilParams1_[1] = freq;
    oscilParams1_[2] = kSyn;
    oscilParams1_[3] = bias;
    oscilParams1_[4] = phase;
}

void FirmwareTftDisplay::oscilloBgDrawEnvStep(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color, int8_t curve) {
    if (curve == CURVE_TYPE_LIN || x1 == x2) {
        oscilloBgDrawLine(x1, y1, x2, y2, color);
    } else {
        int indexLow = 98 * 160 + 1;
        float width = x2 - x1;
        float invWidth = 1.0f / width;
        float height = y2 - y1;
        int oldPy = 0;
        int py;
        for (int x = x1; x <= x2; x++) {
            if (unlikely(x == x2)) {
                py = allLfoTables[curve].table[63] * height + y1;
            } else {
                int indexInt = x - x1;
                // Let's take only 62 value because we use index+1 bellow
                float index = 62.0f * (float)indexInt * invWidth;
                // Linear interpolation
                int index2 = (int)index;
                float indexRest = index - index2;
                float yValue1 = allLfoTables[curve].table[(int)index];
                float yValue2 = allLfoTables[curve].table[(int)index + 1];
                float yInterpolated = yValue1 * (1 - indexRest) + yValue2 * indexRest;
                py = yInterpolated * height + y1;
            }
            // Draw vertical line between old y value and new one
            if (unlikely(x == x1)) {
                bgOscillo_[indexLow + x - py * 160] = color;
            } else {
                if (oldPy <= py) {
                    for (int y = oldPy; y <= py; y += 1) {
                        bgOscillo_[indexLow + x - y * 160] = color;
                    }
                } else {
                    for (int y = py; y <= oldPy; y += 1) {
                        bgOscillo_[indexLow + x - y * 160] = color;
                    }
                }
            }
            oldPy = py;
        }
    }
}

void FirmwareTftDisplay::oscilloBgDrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0, yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0,
            curpixel = 0;
    int indexLow = 98 * 160 + 1;

    deltax = ABS(x2 - x1);
    deltay = ABS(y2 - y1);
    x = x1;
    y = y1;

    if (x2 >= x1) {
        xinc1 = 1;
        xinc2 = 1;
    } else {
        xinc1 = -1;
        xinc2 = -1;
    }

    if (y2 >= y1) {
        yinc1 = 1;
        yinc2 = 1;
    } else {
        yinc1 = -1;
        yinc2 = -1;
    }

    if (deltax >= deltay) {
        xinc1 = 0;
        yinc2 = 0;
        den = deltax;
        num = deltax / 2;
        numadd = deltay;
        numpixels = deltax;
    } else {
        xinc2 = 0;
        yinc1 = 0;
        den = deltay;
        num = deltay / 2;
        numadd = deltax;
        numpixels = deltay;
    }

    for (curpixel = 0; curpixel <= numpixels; curpixel++) {
        //  SETPIXEL(x, y);
        bgOscillo_[indexLow + x - y * 160] = color;

        num += numadd;
        if (num >= den) {
            num -= den;
            x += xinc1;
            y += yinc1;
        }
        x += xinc2;
        y += yinc2;
    }
}

void FirmwareTftDisplay::oscilloBgDrawVerticalLine(int16_t x1, int16_t y1, int16_t y2, uint16_t color) {
    int indexLow = 98 * 160 + 1;
    for (int y = y1; y <= y2; y += 1) {
        bgOscillo_[indexLow + x1 - y * 160] = color;
    }
}

void FirmwareTftDisplay::oscilloBgDrawEnvelope() {
    float div;
    bool displayScale = true;

    if (oscilParams1_[3] < 0.2f) {
        // Very short envelope: keep it visible at about 1/5 of the width.
        // The scale label is hidden in this case.
        div = oscilParams1_[3] * 5.0f;
        if (div < 0.0001f) {
            div = 0.0001f;
        }
        displayScale = false;
    } else if (oscilParams1_[3] < 1.0f) {
        div = 1.0f;
    } else if (oscilParams1_[3] < 2.0f) {
        div = 2.0f;
    } else {
        div = ((int)(oscilParams1_[3] * .2f)) * 5.0 + 5.0f;
    }

    float scale = 158.0f / div;
    uint16_t color = tftPalette565[(int)oscilParams1_[4]];
    uint16_t darkGrey = tftPalette565[COLOR_DARK_GRAY];

    // Draw the enveloppe
    oscilloBgDrawVerticalLine((int)(oscilParams1_[0] * scale), 0, (int)(oscilParams2_[0] * 97.0f), darkGrey);
    oscilloBgDrawVerticalLine((int)(oscilParams1_[1] * scale), 0, (int)(oscilParams2_[1] * 97.0f), darkGrey);
    oscilloBgDrawVerticalLine((int)(oscilParams1_[2] * scale), 0, (int)(oscilParams2_[2] * 97.0f), darkGrey);

    oscilloBgDrawEnvStep(0, 0, (int)(oscilParams1_[0] * scale), (int)(oscilParams2_[0] * 97.0f), color, envCurve_[0]);
    oscilloBgDrawEnvStep((int)(oscilParams1_[0] * scale), (int)(oscilParams2_[0] * 97.0f), (int)(oscilParams1_[1] * scale),
                         (int)(oscilParams2_[1] * 97.0f), color, envCurve_[1]);
    oscilloBgDrawEnvStep((int)(oscilParams1_[1] * scale), (int)(oscilParams2_[1] * 97.0f), (int)(oscilParams1_[2] * scale),
                         (int)(oscilParams2_[2] * 97.0f), color, envCurve_[2]);
    oscilloBgDrawEnvStep((int)(oscilParams1_[2] * scale), (int)(oscilParams2_[2] * 97.0f), (int)(oscilParams1_[3] * scale),
                         (int)(oscilParams2_[3] * 97.0f), color, envCurve_[3]);

    if (displayScale) {
        uint8_t digits[] = {(uint8_t)(((int)div) / 10), (uint8_t)(((int)div) % 10)};
        for (int d = 0; d < 2; d++) {
            if ((d == 1) || (digits[d] != 0)) {
                int x = 136 + d * 11;
                int y = 2;
                const uint8_t* digitBits = tftAlgo_->getDigitBits(digits[d]);

                for (int j = 0; j < 5; j++) {
                    const uint8_t line = digitBits[j] >> 3;
                    for (int i = 0; i < 5; i++) {
                        if (((line >> (4 - i)) & 1) == 1) {
                            bgOscillo_[(x + i * 2) + (y + j * 2) * 160] = color;
                            bgOscillo_[(x + i * 2 + 1) + (y + j * 2) * 160] = color;
                            bgOscillo_[(x + i * 2) + (y + j * 2 + 1) * 160] = color;
                            bgOscillo_[(x + i * 2 + 1) + (y + j * 2 + 1) * 160] = color;
                        }
                    }
                }
            }
        }
    }
}

/*
 * randType == 0 : random
 * randType == 1 : Brownian
 * randType == 2 : Wandering
 * randType == 3 : Flow
 */
void FirmwareTftDisplay::oscilloFillWithRand(int randtype) {
    // Rand
    float incIndex = 1.0f / 160.0f * oscilParams1_[1];
    float sample = 0.0f;
    float nextSample = 0.0f;
    float sampleLp = 0.0f;
    int8_t sampleInt = 0;

    // Adjust to enter index>1 condition when x==0
    float index = oscilParams1_[4] + 1.0f - incIndex;
    int randIndex = 0;
    for (int x = 0; x < 160; x++) {
        index += incIndex;

        if (unlikely(index >= 1.0f)) {
            index -= 1.0f;
            sample = randomDisplay[(randIndex++) % 32] * 48.0f;
            switch (randtype) {
            case 0:
                sampleInt = (int)sample;
                break;
            case 1:
                sampleLp = sample * .4f + sampleLp * 0.6f;
                sampleInt = (int)sampleLp;
                break;
            case 2:
                sampleLp = sample;
                sample = nextSample;
                nextSample = sampleLp;
                break;
            case 3:
                sampleLp = sample * .4f + sampleLp * 0.6f;
                sample = nextSample;
                nextSample = sampleLp;
                break;
            }
        }
        if (randtype <= 1) {
            oscilloYValue_[x] = sampleInt;
        } else {
            oscilloYValue_[x] = (int)((nextSample - sample) * index + sample);
        }
    }
}

void FirmwareTftDisplay::oscilloBgDrawLfo() {
    // Sclae 160 pixel = 1 second of LFO
    int indexMiddle = 50 * 160;
    uint16_t oscilloColor = tftPalette565[COLOR_BLUE];

    // Shape
    switch ((int)oscilParams1_[0]) {
    case 0: {
        // Sin
        const float* samples = waveForm_[0].waveForms;
        int size = waveForm_[0].size;
        for (int x = 0; x < 160; x++) {
            float index = ((float)x) * ((float)size) / 160.0f * oscilParams1_[1] + size * oscilParams1_[4];
            int iIndex = index;
            iIndex %= size;
            oscilloYValue_[x] = (int)(samples[iIndex] * 48.0f);
        }
        break;
    }
    case 1: {
        // Saw
        int size = 200;
        float incSample = 0.005f;
        for (int x = 0; x < 160; x++) {
            float index = ((float)x) * ((float)size) / 160.0f * oscilParams1_[1] + size * oscilParams1_[4];
            int iIndex = index;
            iIndex %= size;
            oscilloYValue_[x] = -47 + (int)((float)iIndex) * incSample * 94.0f;
        }
        break;
    }
    case 2: {
        // Triange
        int size = 200;
        // incSample twice 1/200 for triangle
        float incSample = 0.01f;
        for (int x = 0; x < 160; x++) {
            float index = ((float)x) * ((float)size) / 160.0f * oscilParams1_[1] + size * oscilParams1_[4];
            int iIndex = index;
            iIndex %= size;
            if (iIndex < 100) {
                oscilloYValue_[x] = (int)((float)iIndex) * incSample * 47.0f;
            } else {
                oscilloYValue_[x] = 95 - (int)((float)iIndex) * incSample * 47.0f;
            }
            oscilloYValue_[x] = -47 + oscilloYValue_[x] * 2;
        }
        break;
    }
    case 3: {
        // Square
        int size = 50;
        for (int x = 0; x < 160; x++) {
            float index = ((float)x) * ((float)size) / 160.0f * oscilParams1_[1] + size * oscilParams1_[4];
            int iIndex = index;
            iIndex %= size;
            oscilloYValue_[x] = iIndex < 25 ? 47 : -47;
        }
        break;
    }
    case 4:
    case 5:
    case 6:
    case 7:
        // Rand
        oscilloFillWithRand((int)oscilParams1_[0] - 4);
        break;
    }

    if (oscilParams1_[2] > 0.0f) {
        // Ksyn
        // 1/160 = 0.00627
        float kSyncInc = 1.0f / (160.0f * oscilParams1_[2]);
        float kSync = 0;
        for (int x = 0; x < 160; x++) {
            if (kSync < 1) {
                oscilloYValue_[x] = ((float)oscilloYValue_[x] * kSync);
            } else {
                break;
            }
            kSync += kSyncInc;
        }
    }

    if (oscilParams1_[3] != 0.0f) {
        for (int x = 0; x < 160; x++) {
            oscilloYValue_[x] += (oscilParams1_[3] * 48.0f);
        }
    }

    for (int x = 1; x < 159; x++) {
        if (oscilloYValue_[x] < (oscilloYValue_[x + 1] - 1)) {
            for (int oy = oscilloYValue_[x]; oy < oscilloYValue_[x + 1]; oy++) {
                if (likely(oy > -48 && oy < 48)) {
                    bgOscillo_[indexMiddle + x - oy * 160] = oscilloColor;
                }
            }
        } else if (oscilloYValue_[x] > (oscilloYValue_[x + 1] + 1)) {
            for (int oy = oscilloYValue_[x]; oy > oscilloYValue_[x + 1]; oy--) {
                if (likely(oy > -48 && oy < 48)) {
                    bgOscillo_[indexMiddle + x - oy * 160] = oscilloColor;
                }
            }
        } else {
            if (likely(oscilloYValue_[x] > -48 && oscilloYValue_[x] < 48)) {
                bgOscillo_[indexMiddle + x - oscilloYValue_[x] * 160] = oscilloColor;
            }
        }
    }
}
