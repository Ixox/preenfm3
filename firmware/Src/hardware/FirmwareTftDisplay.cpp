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
#include "Env.h"

extern DMA2D_HandleTypeDef hdma2d;
extern RNG_HandleTypeDef hrng;

extern uint16_t tftPalette565[NUMBER_OF_TFT_COLORS];

const float randomDisplay[32] = { 0.679268211, -0.559346101, 0.93210829, 0.263967825,
        0.868206376, -0.955814486, 0.767822461, -0.260222359, -0.617473204,
        -0.844648436, 0.380078337, 0.481601148, -0.807739871, 0.995462356,
        -0.854993442, 0.704896648, 0.258727388, 0.947767668, -0.12564,
        0.513101208, -0.75714895, -0.11723284, 0.026869686, -0.738605819,
        -0.262181885, 0.856453, 0.209812963, -0.976879332, -0.019152814,
        -0.213303568, 0.55, 0.379348783 };

FirmwareTftDisplay::FirmwareTftDisplay() : TftDisplay() {
   oscilloIsClean = true;
   envInQueue = 0;
   lfoInQueue = 0;
   operatorInQueue = 0;
}

FirmwareTftDisplay::~FirmwareTftDisplay() {
    // TODO Auto-generated destructor stub
}


void FirmwareTftDisplay::clearActions() {
    tftActions.clear();
    envInQueue = 0;
    lfoInQueue = 0;
    operatorInQueue = 0;
}


void FirmwareTftDisplay::initWaveFormExt(int index, float* waveform, int size) {
    waveForm[index].waveForms = waveform;
    waveForm[index].size = size;
}


void FirmwareTftDisplay::additionalActions() {

    // Just in case
    if (tftActions.getCount() == 0) {
        envInQueue = 0;
        lfoInQueue = 0;
        operatorInQueue = 0;
    }

    switch (currentAction.actionType) {
    case TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM:
        switch (currentAction.param3) {
        case 0:
            // Copy bgOscillo2 to bgOscillo (that clears current waveform in oscilo)
            if (PFM_IS_DMA2D_READY()) {

                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
                WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[COLOR_BLACK]);

                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 2);
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL),
                        (98 | (158 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t )(bgOscillo + 161));

                PFM_START_DMA2D();
                currentAction.param3 = 1;
            }
            break;
        case 1:
            if (currentAction.param1 <= TFT_NUMBER_OF_WAVEFORM_EXT) {
                oscilloBgDrawOperatorShape(waveForm[currentAction.param1].waveForms, waveForm[currentAction.param1].size);
                operatorInQueue--;
            } else if (currentAction.param1 == TFT_DRAW_ENVELOPPE) {
                oscilloBgDrawEnvelope();
                envInQueue--;
            } else if (currentAction.param1 == TFT_DRAW_LFO) {
                oscilloBgDrawLfo();
                lfoInQueue--;
            }
            currentAction.actionType = 0;
            oscilloForceNextDisplay();
            break;
        }
    }
}



void FirmwareTftDisplay::oscilloBgDrawOperatorShape(float* waveForm, int size) {

    int indexMiddle = 50 * 160;
    uint16_t oscilloColor = tftPalette565[COLOR_YELLOW];

    for (int x = 0; x < 160; x++) {
        float index = x * ((float)size) / 160.0f;
        oscilloYValue[x] = (int) (waveForm[(int)index] * 48.0f);
    }

    for (int x = 1; x < 159; x++) {
        if (oscilloYValue[x] < (oscilloYValue[x + 1] - 1)) {
            for (int oy = oscilloYValue[x]; oy < oscilloYValue[x + 1]; oy++) {
                bgOscillo[indexMiddle + x + oy * 160] = oscilloColor;
            }
        } else if (oscilloYValue[x] > (oscilloYValue[x + 1] + 1)) {
            for (int oy = oscilloYValue[x]; oy > oscilloYValue[x + 1]; oy--) {
                bgOscillo[indexMiddle + x + oy * 160] = oscilloColor;
            }
        } else {
            bgOscillo[indexMiddle + x + oscilloYValue[x] * 160] = oscilloColor;
        }
    }
}



void FirmwareTftDisplay::oscilloBgActionClear() {
    if (!oscilloIsClean) {
        TFTAction newAction;
        newAction.param1 = 255;
        newAction.param3 = 0;
        newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
        tftActions.insert(newAction);
        oscilloIsClean = true;
    }
}


void FirmwareTftDisplay::oscilloBgActionOperatorShape(int wfNumber) {
    if (operatorInQueue >= 2) {
        // No need to ask for new drawing
        // params have been updated and next drawing will use them
        return;
    }
    operatorInQueue++;
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
    newAction.param1 = wfNumber;
    newAction.param3 = 0;
    tftActions.insert(newAction);

    oscilloIsClean = false;
}

void FirmwareTftDisplay::oscilloBgActionLfo() {
    if (lfoInQueue >= 2) {
        // No need to ask for new drawing
        // params have been updated and next drawing will use them
        return;
    }
    lfoInQueue++;

    TFTAction newAction;
    newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
    newAction.param1 = TFT_DRAW_LFO;
    newAction.param3 = 0;
    tftActions.insert(newAction);

    oscilloIsClean = false;
}


void FirmwareTftDisplay::oscilloBgActionEnvelope() {
    if (envInQueue >= 2) {
        // No need to ask for new drawing
        // params have been updated and next drawing will use them
        return;
    }
    envInQueue++;
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
    newAction.param1 = TFT_DRAW_ENVELOPPE;
    newAction.param3 = 0;
    tftActions.insert(newAction);

    oscilloIsClean = false;
}



void FirmwareTftDisplay::oscilloBgSetEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL, int8_t aCurve, int8_t dCurve, int8_t sCurve, int8_t rCurve) {
    oscilParams1[0] = a;
    oscilParams1[1] = d + oscilParams1[0];
    oscilParams1[2] = s + oscilParams1[1];
    oscilParams1[3] = r + oscilParams1[2];

    oscilParams2[0] = aL;
    oscilParams2[1] = dL;
    oscilParams2[2] = sL;
    oscilParams2[3] = rL;

    envCurve[0] = aCurve;
    envCurve[1] = dCurve;
    envCurve[2] = sCurve;
    envCurve[3] = rCurve;

    oscilParams1[4] = COLOR_YELLOW;

}

void FirmwareTftDisplay::oscilloBgSetLfoEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL) {
    oscilParams1[0] = a;
    oscilParams1[1] = d + oscilParams1[0];
    oscilParams1[2] = s + oscilParams1[1];
    oscilParams1[3] = r + oscilParams1[2];

    oscilParams2[0] = aL;
    oscilParams2[1] = dL;
    oscilParams2[2] = sL;
    oscilParams2[3] = rL;

    envCurve[0] = 1;
    envCurve[1] = 1;
    envCurve[2] = 1;
    envCurve[3] = 1;

    oscilParams1[4] = COLOR_BLUE;
}


void FirmwareTftDisplay::oscilloBgSetLfo(float shape, float freq, float kSyn, float bias, float phase) {
    if (freq >= 100.0f) {
        freq = 1.0f;
    }
    oscilParams1[0] = shape;
    oscilParams1[1] = freq;
    oscilParams1[2] = kSyn;
    oscilParams1[3] = bias;
    oscilParams1[4] = phase;

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
                float yInterpolated = yValue1 * (1-indexRest) + yValue2 * indexRest;
                py = yInterpolated * height + y1;
            }
            // Draw vertical line between old y value and new one
            if (unlikely(x == x1)) {
                bgOscillo[indexLow + x - py * 160] = color;
            } else {
                if (oldPy <= py) {
                    for (int y = oldPy; y <= py; y += 1) {
                        bgOscillo[indexLow + x - y * 160] = color;
                    }
                } else {
                    for (int y = py; y <= oldPy; y += 1) {
                        bgOscillo[indexLow + x - y * 160] = color;
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
        bgOscillo[indexLow + x - y * 160] = color;

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
        bgOscillo[indexLow + x1 - y * 160] = color;
    }
}

void FirmwareTftDisplay::oscilloBgDrawEnvelope() {
    float div;

    if (oscilParams1[3] < 1.0f) {
        div = 1.0f;
    } else if (oscilParams1[3] < 2.0f) {
        div = 2.0f;
    } else {
        div = ((int)(oscilParams1[3] * .2f)) * 5.0 + 5.0f ;
    }

    float scale = 158.0f / div;
    uint16_t color = tftPalette565[(int)oscilParams1[4]];
    uint16_t darkGrey = tftPalette565[COLOR_DARK_GRAY];

    // Draw the enveloppe
    oscilloBgDrawVerticalLine((int)(oscilParams1[0] * scale), 0, (int)(oscilParams2[0] * 97.0f), darkGrey);
    oscilloBgDrawVerticalLine((int)(oscilParams1[1] * scale), 0, (int)(oscilParams2[1] * 97.0f), darkGrey);
    oscilloBgDrawVerticalLine((int)(oscilParams1[2] * scale), 0, (int)(oscilParams2[2] * 97.0f), darkGrey);


    oscilloBgDrawEnvStep(0, 0, (int)(oscilParams1[0] * scale), (int)(oscilParams2[0] * 97.0f), color, envCurve[0]);
    oscilloBgDrawEnvStep((int)(oscilParams1[0] * scale), (int)(oscilParams2[0] * 97.0f), (int)(oscilParams1[1] * scale), (int)(oscilParams2[1] * 97.0f), color, envCurve[1]);
    oscilloBgDrawEnvStep((int)(oscilParams1[1] * scale), (int)(oscilParams2[1] * 97.0f), (int)(oscilParams1[2] * scale), (int)(oscilParams2[2] * 97.0f), color, envCurve[2]);
    oscilloBgDrawEnvStep((int)(oscilParams1[2] * scale), (int)(oscilParams2[2] * 97.0f), (int)(oscilParams1[3] * scale), (int)(oscilParams2[3] * 97.0f), color, envCurve[3]);

    uint8_t digits[] = { (uint8_t)(((int)div) / 10),  (uint8_t)(((int)div) % 10) };
    for (int d = 0; d < 2; d++) {
        if ((d == 1) || (digits[d] != 0)) {
            int x = 136 + d * 11;
            int y = 2;
            const uint8_t* digitBits = tftAlgo->getDigitBits(digits[d]);

            for (int j = 0; j < 5; j++) {
                const uint8_t line = digitBits[j] >> 3;
                for (int i = 0; i < 5; i++) {
                    if (((line >> (4 - i)) & 1) == 1) {
                        bgOscillo[(x + i * 2)     + (y + j * 2) * 160] = color;
                        bgOscillo[(x + i * 2 + 1) + (y + j * 2) * 160] = color;
                        bgOscillo[(x + i * 2)     + (y + j * 2 + 1) * 160] = color;
                        bgOscillo[(x + i * 2 + 1) + (y + j * 2 + 1) * 160] = color;
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
    float incIndex = 1.0f / 160.0f * oscilParams1[1];
    float sample = 0.0f;
    float nextSample = 0.0f;
    float sampleLp = 0.0f;
    int8_t sampleInt = 0;

    // Adjust to enter index>1 condition when x==0
    float index = oscilParams1[4] + 1.0f - incIndex;
    int randIndex = 0;
    for (int x = 0; x < 160; x++) {
        index += incIndex;

        if (unlikely(index >= 1.0f))  {
            index -= 1.0f;
            sample =  randomDisplay[(randIndex++) % 32] * 48.0f;
            switch (randtype) {
            case 0:
                sampleInt = (int) sample;
                break;
            case 1:
                sampleLp = sample * .4f + sampleLp * 0.6f;
                sampleInt = (int) sampleLp;
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
            oscilloYValue[x] = sampleInt;
        } else {
            oscilloYValue[x] =  (int) ((nextSample - sample) * index + sample);
        }
    }
}


void FirmwareTftDisplay::oscilloBgDrawLfo() {
    // Sclae 160 pixel = 1 second of LFO
    int indexMiddle = 50 * 160;
    uint16_t oscilloColor = tftPalette565[COLOR_BLUE];

    // Shape
    switch ((int)oscilParams1[0]) {
    case 0: {
        // Sin
        float *samples = waveForm[0].waveForms;
        int size = waveForm[0].size;
        for (int x = 0; x < 160; x++) {
            float index = ((float)x) * ((float)size) / 160.0f * oscilParams1[1] + size * oscilParams1[4];
            int iIndex = index;
            iIndex %= size;
            oscilloYValue[x] = (int) (samples[iIndex] * 48.0f);
        }
        break;
    }
    case 1: {
        // Saw
        int size = 200;
        float incSample = 0.005f;
        for (int x = 0; x < 160; x++) {
            float index = ((float)x) * ((float)size) / 160.0f * oscilParams1[1] + size * oscilParams1[4];
            int iIndex = index;
            iIndex %= size;
            oscilloYValue[x] = -47 + (int) ((float)iIndex) * incSample * 94.0f;
        }
        break;
    }
    case 2: {
        // Triange
        int size = 200;
        // incSample twice 1/200 for triangle
        float incSample = 0.01f;
        for (int x = 0; x < 160; x++) {
            float index = ((float)x) * ((float)size) / 160.0f * oscilParams1[1] + size * oscilParams1[4];
            int iIndex = index;
            iIndex %= size;
            if (iIndex < 100) {
                oscilloYValue[x] = (int) ((float)iIndex) * incSample * 47.0f;
            } else {
                oscilloYValue[x] = 95 - (int) ((float)iIndex) * incSample * 47.0f;
            }
            oscilloYValue[x] = - 47 + oscilloYValue[x] * 2;
        }
        break;
    }
    case 3: {
        // Square
        int size = 50;
        for (int x = 0; x < 160; x++) {
            float index = ((float)x) * ((float)size) / 160.0f * oscilParams1[1] + size * oscilParams1[4];
            int iIndex = index;
            iIndex %= size;
            oscilloYValue[x] = iIndex < 25 ? 47 : -47;
        }
        break;
    }
    case 4:
    case 5:
    case 6:
    case 7:
        // Rand
        oscilloFillWithRand((int)oscilParams1[0] - 4);
        break;
    }

    if (oscilParams1[2] > 0.0f) {
        // Ksyn
        // 1/160 = 0.00627
        float kSyncInc = 1.0f / (160.0f * oscilParams1[2]);
        float kSync = 0;
        for (int x = 0; x < 160; x++) {
            if (kSync < 1) {
                oscilloYValue[x] = ((float)oscilloYValue[x] * kSync);
            } else {
                break;
            }
            kSync += kSyncInc;
        }
    }

    if (oscilParams1[3] != 0.0f) {
        for (int x = 0; x < 160; x++) {
            oscilloYValue[x] += (oscilParams1[3] * 48.0f);
        }
    }


    for (int x = 1; x < 159; x++) {
        if (oscilloYValue[x] < (oscilloYValue[x + 1] - 1)) {
            for (int oy = oscilloYValue[x]; oy < oscilloYValue[x + 1]; oy++) {
                if (likely(oy > -48 && oy < 48)) {
                    bgOscillo[indexMiddle + x - oy * 160] = oscilloColor;
                }
            }
        } else if (oscilloYValue[x] > (oscilloYValue[x + 1] + 1)) {
            for (int oy = oscilloYValue[x]; oy > oscilloYValue[x + 1]; oy--) {
                if (likely(oy > -48 && oy < 48)) {
                    bgOscillo[indexMiddle + x - oy * 160] = oscilloColor;
                }
            }
        } else {
            if (likely(oscilloYValue[x] > -48 && oscilloYValue[x] < 48)) {
                bgOscillo[indexMiddle + x - oscilloYValue[x] * 160] = oscilloColor;
            }
        }
    }

}
