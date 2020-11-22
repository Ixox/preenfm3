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
#include "stm32h7xx_hal.h"
#include "TftDisplay.h"
#include "ili9341.h"

// 95 char + 16 step seqs + 1 note + 4 for sequencers
#define NUMBER_OF_CHARS (95 + 16 + 1 + 4)

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)


RAM_D1_SECTION uint8_t tftForeground[60000];
RAM_D1_SECTION uint16_t tftBackground[42860 * 2];
RAM_D1_SECTION uint16_t tftMemory[240 * 320];

extern DMA2D_HandleTypeDef hdma2d;


#define PFM_IS_DMA2D_READY() (hdma2d.Instance->CR & DMA2D_CR_START) == 0
#define PFM_START_DMA2D() hdma2d.Instance->CR |= DMA2D_CR_START


#define BG_OFFSET_BLACK 0
#define BG_OFFSET_DARK_BLUE 20*12
#define BG_OFFSET_LIGHT_BLUE 20*12 * 2

#define ABS(X)  ((X) > 0 ? (X) : -(X))


enum {
    TFT_NOP = 0,
    TFT_DISPLAY_ONE_CHAR,
    TFT_DISPLAY_ONE_SMALL_CHAR,
    TFT_DRAW_FILL_TFT,
    TFT_DRAW_OSCILLO,
    TFT_DRAW_ALGO,
    TFT_HIGHLIGHT_ALGO_OPERATOR,
    TFT_HIGHTLIGHT_ALGO_IM,
    TFT_ERASE_HIGHTLIGHT_ALGO_IM,
    TFT_CLEAR_ALGO_FG,
    TFT_DRAW_FILL_AREA,
    TFT_RESTART_REFRESH,
    TFT_PAUSE_REFRESH,
    TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM
};

#define TFT_OSCILLO_Y 160
#define TFT_OSCILLO_X 0
#define TFT_ALGO_Y 160
#define TFT_ALGO_X 160

uint32_t tftPalette[NUMBER_OF_TFT_COLORS];
uint16_t tftPalette565[NUMBER_OF_TFT_COLORS];

uint32_t reducedColor(uint32_t x) {
    return
    // RED
    (((x & 0xe00000) >> 21) << 10) + (((x & 0x180000) >> 19) << 6)
    // GREEN
            + (((x & 0xe000) >> 13) << 3) + (((x & 0x1c00) >> 10) << 21)
            // BLUE
            + (((x & 0xc0) >> 6) << 19) + (((x & 0x38) >> 3) << 13);
}

uint16_t convertTo565(uint32_t x) {
    return ((x & 0xf80000) >> 8) + ((x & 0xfc00) >> 5) + ((x & 0xf8) >> 3);
}

TftDisplay::TftDisplay() {
    pushToTftInProgress = false;

    tftDirtyBits = TFT_PART_ALL_BITS;

    areaY[TFT_PART_HEADER] = 0;
    areaHeight[TFT_PART_HEADER] = 40;
    areaY[TFT_PART_VALUES] = 40;
    areaHeight[TFT_PART_VALUES] = 120;
    areaY[TFT_PART_OSCILLO] = 160;
    areaHeight[TFT_PART_OSCILLO] = 110;
    areaY[TFT_PART_BUTTONS] = 270;
    areaHeight[TFT_PART_BUTTONS] = 50;

    currentAction.actionType = 0;
    lastOscilloSaturateTic = 0;
    currentActionStep = 0;

    oscilloSamplePeriod = PREENFM_FREQUENCY / 440;
    oscilloSampleInc = oscilloSamplePeriod / OSCILLO_WIDTH;
    oscilloPhaseIndex = 0.0f;

    for (int o = 0; o < OSCILLO_BUFFER_SIZE; o++) {
        oscilloFullPeriodSampl[o] = 0.0f;
    }

    memset(tftMemory, 0, 240 * 320 * 2);

    tftPalette[COLOR_BLACK] = reducedColor(0x0);
    tftPalette[COLOR_WHITE] = reducedColor(0xffffff);
    tftPalette[COLOR_BLUE] = reducedColor(0x6060ff);
    tftPalette[COLOR_DARK_BLUE] = reducedColor(0x202060);
    tftPalette[COLOR_YELLOW] = reducedColor(0xffff00);
    tftPalette[COLOR_DARK_YELLOW] = reducedColor(0x504010);
    tftPalette[COLOR_RED] = reducedColor(0xff4010);
    tftPalette[COLOR_DARK_RED] = reducedColor(0x401000);
    tftPalette[COLOR_GREEN] = reducedColor(0x40ff40);
    tftPalette[COLOR_DARK_GREEN] = reducedColor(0x004000);
    tftPalette[COLOR_LIGHT_GRAY] = reducedColor(0xb0b0b0);
    tftPalette[COLOR_MEDIUM_GRAY] = reducedColor(0x909090);
    tftPalette[COLOR_GRAY] = reducedColor(0x606060);
    tftPalette[COLOR_DARK_GRAY] = reducedColor(0x202020);
    tftPalette[COLOR_CYAN] = reducedColor(0x76dfef);
    tftPalette[COLOR_ORANGE] = reducedColor(0xff7f00);
    tftPalette[COLOR_LIGHT_GREEN] = reducedColor(0x70ff70);


    for (int c = 0; c < NUMBER_OF_TFT_COLORS; c++) {
        tftPalette565[c] = convertTo565(tftPalette[c]);
    }

    charColor = COLOR_GREEN;
    charBackgroundColor = COLOR_BLACK;
    tftRefreshing = true;
    fgAlgoDirty = true;
    oscilloIsClean = true;
    flatOscilloAlreadyDisplayed = true;
    tftPushMillis = 0;
    part = 0;
    olscilloYScale = 1.0f;
}

TftDisplay::~TftDisplay() {
    // TODO Auto-generated destructor stub
}

void TftDisplay::init(TftAlgo* tftAlgo) {

    this->tftAlgo = tftAlgo;
    // Characters
    int tftTmpIndex = 0;

    fgBigChars = &tftForeground[tftTmpIndex];


    for (int ch = 0; ch < NUMBER_OF_CHARS; ch++) {
        int index = ch * Font_11x18.height;
        for (int i = 0; i < Font_11x18.height; i++) {
            uint32_t b = Font_11x18.data[index + i];
            for (int j = 0; j < Font_11x18.width; j++) {
                if ((b << j) & 0x8000) {
                    tftForeground[tftTmpIndex++] = 0xff;
                } else {
                    tftForeground[tftTmpIndex++] = 0x0;
                }
            }
        }
    }

    tftTmpIndex /= 4;
    tftTmpIndex++;
    tftTmpIndex *= 4;

    fgSmallChars = &tftForeground[tftTmpIndex];

    for (int ch = 0; ch < 96; ch++) {
        int index = ch * Font_7x10.height;
        for (int i = 0; i < Font_7x10.height; i++) {
            uint32_t b = Font_7x10.data[index + i];
            for (int j = 0; j < Font_7x10.width; j++) {
                if ((b << (j + 1)) & 0x8000) {
                    tftForeground[tftTmpIndex++] = 0xff;
                } else {
                    tftForeground[tftTmpIndex++] = 0x0;
                }
            }
        }
    }

    tftTmpIndex /= 4;
    tftTmpIndex++;
    tftTmpIndex *= 4;

    fgSegmentDigit = &tftForeground[tftTmpIndex];

    for (int c = 0; c < 12; c++) {
        for (int i = 0; i < 9; i++) {
            uint8_t byte = segmentFont[c * 9 + i];
            for (int j = 0; j < 5; j++) {
                if (((byte >> (7 - j)) & 0x1) == 0x1) {
                    tftForeground[tftTmpIndex + j * 2] = 0xff;
                    tftForeground[tftTmpIndex + j * 2 + 1] = 0xff;
                    tftForeground[tftTmpIndex + 5 * 2 + j * 2] = 0xff;
                    tftForeground[tftTmpIndex + 5 * 2 + j * 2 + 1] = 0xff;

                } else {
                    tftForeground[tftTmpIndex + j * 2] = 0x0;
                    tftForeground[tftTmpIndex + j * 2 + 1] = 0x0;
                    tftForeground[tftTmpIndex + 5 * 2 + j * 2] = 0x0;
                    tftForeground[tftTmpIndex + 5 * 2 + j * 2 + 1] = 0x0;
                }
            }
            tftTmpIndex += 5 * 4;
        }
    }

    tftTmpIndex /= 4;
    tftTmpIndex++;
    tftTmpIndex *= 4;

    fgOscillo = &tftForeground[tftTmpIndex];

    memset(fgOscillo, 0, 160 * 101);
    tftTmpIndex += 160 * 101;

    fgAlgo = &tftForeground[tftTmpIndex];
    memset(fgAlgo, 0, 80 * 100);
    tftTmpIndex += 80 * 100;

    // Check tftTmpIndex here for tftForeground size


    // ==============================
    // init Background
    int bgIndex = 0;

    for (int c = 0; c < NUMBER_OF_TFT_COLORS; c++) {
        bgColorChar[c] = &tftBackground[bgIndex];
        for (int i = 0; i < 20 * 11; i++) {
            tftBackground[bgIndex + i] = tftPalette565[c];
        }
        bgIndex += 20 * 11;
    }

    // BG OScillo
    bgOscillo = &tftBackground[bgIndex];

    memset(bgOscillo, 0, 160 * 100 * 2);
    uint16_t oscilloColor = 0x0842;
    for (int x = 0; x < 160; x++) {
        bgOscillo[x] = oscilloColor;
        bgOscillo[x + 99 * 160] = oscilloColor;
    }
    for (int y = 0; y < 100; y++) {
        bgOscillo[y * 160] = oscilloColor;
        bgOscillo[y * 160 + 159] = oscilloColor;
    }
    bgIndex += 160 * 100;


    bgAlgo = &tftBackground[bgIndex];

    memset(bgAlgo, 0, 80 * 100 * 2);
    bgIndex += 80 * 100;
    // Check bgIndex here for tftBackground size (must be multiplied by 2)

    currentAction.actionType = 0;

}

void TftDisplay::initWaveFormExt(int index, float* waveform, int size) {
    waveForm[index].waveForms = waveform;
    waveForm[index].size = size;
}


void TftDisplay::setDirtyArea(uint16_t y, uint16_t height) {

    uint16_t yBottom = y + height;

    if (unlikely(y < 40)) {
        tftDirtyBits |= 0b0001;
    }

    if (unlikely(y < 160 && yBottom >= 40)) {
        tftDirtyBits |= 0b0010;
    }

    if (unlikely(y < 270 && yBottom >= 160)) {
        tftDirtyBits |= 0b0100;
    }

    if (unlikely(yBottom >= 270)) {
        tftDirtyBits |= 0b1000;
    }
}

void TftDisplay::tic() {
    uint32_t offset;


    uint32_t currentMillis = HAL_GetTick();
    if (unlikely((currentMillis - tftPushMillis) > 15)) {
        if (pushToTft()) {
           // a part have been pushed
           tftPushMillis = currentMillis;
           return;
        }
    }


    // If doing nothing try to take next action
    if ((currentAction.actionType == 0) && (tftActions.getCount() > 0)) {
        currentAction = tftActions.remove();
        currentActionStep = 0;
    }

    switch (currentAction.actionType) {
    case TFT_NOP:
        return;
    case TFT_DISPLAY_ONE_CHAR:
        // Display Char
        if (PFM_IS_DMA2D_READY()) {

            WRITE_REG(hdma2d.Instance->FGCOLR, tftPalette[currentAction.param4]);
            MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_M2M_BLEND);

            offset = currentAction.param2 * 240 + currentAction.param1;
            MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 240 - 11);
            MODIFY_REG(hdma2d.Instance->BGOR, DMA2D_OOR_LO, 0);
            MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL), (18 | (11 << DMA2D_POSITION_NLR_PL)));
            WRITE_REG(hdma2d.Instance->FGMAR, (uint32_t )(fgBigChars + currentAction.param3 * 18 * 11));
            WRITE_REG(hdma2d.Instance->BGMAR, (uint32_t )(bgColorChar[currentAction.param5]));
            WRITE_REG(hdma2d.Instance->OMAR, (uint32_t )(tftMemory + offset));

            setDirtyArea(currentAction.param2, 18);

            currentAction.actionType = 0;

            PFM_START_DMA2D();
        }
        break;
    case TFT_DISPLAY_ONE_SMALL_CHAR:
        // Display Char
        if (PFM_IS_DMA2D_READY()) {

            WRITE_REG(hdma2d.Instance->FGCOLR, tftPalette[currentAction.param4]);
            MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_M2M_BLEND);
            offset = currentAction.param2 * 240 + currentAction.param1;
            MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 240 - 7);
            MODIFY_REG(hdma2d.Instance->BGOR, DMA2D_OOR_LO, 0);
            MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL), (10 | (7 << DMA2D_POSITION_NLR_PL)));
            WRITE_REG(hdma2d.Instance->FGMAR, (uint32_t )(fgSmallChars + currentAction.param3 * 10 * 7));
            WRITE_REG(hdma2d.Instance->BGMAR, (uint32_t )(bgColorChar[currentAction.param5]));
            WRITE_REG(hdma2d.Instance->OMAR, (uint32_t )(tftMemory + offset));

            setDirtyArea(currentAction.param2, 10);

            currentAction.actionType = 0;
            PFM_START_DMA2D();
        }
        break;
    case TFT_DRAW_FILL_TFT:
        // CLEAR tftMemory
        // 4 steps : tft is divided in 4 parts
        if (PFM_IS_DMA2D_READY()) {

            MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
            WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[currentAction.param5]);

            MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 0);

            MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL),
                    (areaHeight[currentAction.param3] | (240 << DMA2D_POSITION_NLR_PL)));
            WRITE_REG(hdma2d.Instance->OMAR, (uint32_t )(tftMemory + areaY[currentAction.param3] * 240));
            PFM_START_DMA2D();

            if (currentAction.param3 == 0) {
                tftDirtyBits = TFT_PART_ALL_BITS;
                currentAction.actionType = 0;
            } else {
                currentAction.param3--;
            }
        }
        break;
    case TFT_DRAW_OSCILLO:
        // 2 steps
        // 0 : copy oscillo+background => tftMemory
        // 1 : copy back background=>oscillo
        switch (currentAction.param3) {
        case 0:
            // Display Oscilloscope
            if (PFM_IS_DMA2D_READY()) {
                uint32_t currentTic = HAL_GetTick();
                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_M2M_BLEND);

                // Keep red color for 5 refreshes after saturation detected
                // param param1 == 1 means last oscillo refresh saturated
                if (unlikely(currentAction.param1 == 1)) {
                    lastOscilloSaturateTic = currentTic;
                }
                if (unlikely(currentAction.param1 == 1 || (currentTic < (lastOscilloSaturateTic + 10)))) {
                    WRITE_REG(hdma2d.Instance->FGCOLR, tftPalette[COLOR_RED]);
                } else {
                    WRITE_REG(hdma2d.Instance->FGCOLR, tftPalette[COLOR_GREEN]);
                }

                offset = TFT_OSCILLO_Y * 240 + TFT_OSCILLO_X;
                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 240 - 160);
                MODIFY_REG(hdma2d.Instance->BGOR, DMA2D_OOR_LO, 0);
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL), (100 | (160 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->FGMAR, (uint32_t )(fgOscillo));
                WRITE_REG(hdma2d.Instance->BGMAR, (uint32_t )(bgOscillo));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t )(tftMemory + offset));
                PFM_START_DMA2D();
                currentAction.param3 = 1;
            }
            break;
        case 1:
            // Clear Oscilloscope
            if (PFM_IS_DMA2D_READY()) {
                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
                WRITE_REG(hdma2d.Instance->OCOLR, 0);

                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 0);
                // Output is rgb565 (16 bits) and we fill A8 (8 bits) so we can divide the height by 2
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL), (50 | (160 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t )(fgOscillo));
                PFM_START_DMA2D();
                currentAction.actionType = 0;
                tftDirtyBits |= TFT_PART_OSCILLO_BITS;
            }
            break;
        }
        break;
    case TFT_DRAW_ALGO:
        switch (currentAction.param3) {
        case 0:
            // Clear ALGO ON BackGround
            if (PFM_IS_DMA2D_READY()) {
                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
                WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[COLOR_BLACK]);

                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 0);
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL), (100 | (80 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t )(bgAlgo));
                PFM_START_DMA2D();

                currentAction.param3 = 1;
            }
            break;
        case 1:
            this->tftAlgo->setBufferAdress(bgAlgo);
            this->tftAlgo->drawAlgo(currentAction.param1);

            if (fgAlgoDirty) {
                currentAction.param3 = 2;
            } else {
                currentAction.param3 = 3;
            }
            break;
        case 2:
            // And clean FG
            // Clear ALGO ON Foreground
            if (PFM_IS_DMA2D_READY()) {
                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
                WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[COLOR_BLACK]);

                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 0);
                // 40 and not 80 because fgAlgo is a 8 bits buffer
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL), (100 | (40 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t )(fgAlgo));
                PFM_START_DMA2D();

                fgAlgoDirty = false;
                currentAction.param3 = 3;
            }

            break;
        case 3:
            // Copy ALGO from BG to TFT
            if (PFM_IS_DMA2D_READY()) {
                WRITE_REG(hdma2d.Instance->FGCOLR, tftPalette[COLOR_YELLOW]);
                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_M2M_BLEND);

                MODIFY_REG(hdma2d.Instance->BGOR, DMA2D_OOR_LO, 0);
                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 240 - 80);
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL), (100 | (80 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->FGMAR, (uint32_t )fgAlgo);
                WRITE_REG(hdma2d.Instance->BGMAR, (uint32_t )(bgAlgo));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)(tftMemory + TFT_ALGO_Y * 240 + TFT_ALGO_X));
                PFM_START_DMA2D();

                tftDirtyBits |= TFT_PART_OSCILLO_BITS;
                currentAction.actionType = 0;
            }
            break;
        }

        break;
    case TFT_HIGHLIGHT_ALGO_OPERATOR:
        switch (currentAction.param3) {
        case 0:
            // Clear ALGO ON Foreground
            if (PFM_IS_DMA2D_READY()) {
                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
                WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[COLOR_BLACK]);

                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 0);
                // 40 and not 80 because fgAlgo is a 8 bits buffer
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL), (100 | (40 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t )(fgAlgo));
                PFM_START_DMA2D();

                currentAction.param3 = 1;
            }
            break;
        case 1:
            this->tftAlgo->setFGBufferAdress(fgAlgo);
            this->tftAlgo->drawAlgoOperator(currentAction.param1, currentAction.param2);

            // Switch to draw_algo step 2
            // To copy ALGO to TFT
            fgAlgoDirty = true;
            currentAction.actionType = TFT_DRAW_ALGO;
            currentAction.param3 = 3;
            break;
        }
        break;
    case TFT_CLEAR_ALGO_FG:
        // Clear ALGO ON Foreground
        if (PFM_IS_DMA2D_READY()) {
            MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
            WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[COLOR_BLACK]);

            MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 0);
            // 40 and not 80 because fgAlgo is a 8 bits buffer
            MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL), (100 | (40 << DMA2D_POSITION_NLR_PL)));
            WRITE_REG(hdma2d.Instance->OMAR, (uint32_t )(fgAlgo));
            PFM_START_DMA2D();

            fgAlgoDirty = false;
            currentAction.actionType = TFT_DRAW_ALGO;
            currentAction.param3 = 3;
        }
        break;
    case TFT_HIGHTLIGHT_ALGO_IM:
        this->tftAlgo->setFGBufferAdress(fgAlgo);
        this->tftAlgo->highlightIM(true, currentAction.param1, currentAction.param4, currentAction.param5);

        // Switch to draw_algo step 2
        // To copy ALGO to TFT
        fgAlgoDirty = true;
        currentAction.actionType = TFT_DRAW_ALGO;
        currentAction.param3 = 3;
        break;
    case TFT_ERASE_HIGHTLIGHT_ALGO_IM:
        this->tftAlgo->setFGBufferAdress(fgAlgo);
        this->tftAlgo->highlightIM(false, currentAction.param1, currentAction.param4, currentAction.param5);

        // Switch to draw_algo step 2
        // To copy ALGO to TFT
        fgAlgoDirty = true;
        currentAction.actionType = TFT_DRAW_ALGO;
        currentAction.param3 = 3;
        break;
    case TFT_DRAW_FILL_AREA:
        // Fill area of tftMemory defined by params
        if (PFM_IS_DMA2D_READY()) {

            MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
            WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[currentAction.param5]);

            offset = currentAction.param1 + currentAction.param2 * 240;

            MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 240 - currentAction.param3);
            MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL),
                    (currentAction.param4 | (currentAction.param3 << DMA2D_POSITION_NLR_PL)));
            WRITE_REG(hdma2d.Instance->OMAR, (uint32_t )(tftMemory + offset));

            PFM_START_DMA2D();

            setDirtyArea(currentAction.param2, currentAction.param4);
            currentAction.actionType = 0;
        }
        break;
    case TFT_RESTART_REFRESH:
        tftRefreshing = true;
        // start from the top
        part = 0;
        currentAction.actionType = 0;
        break;
    case TFT_PAUSE_REFRESH:
        tftRefreshing = false;
        currentAction.actionType = 0;
        break;
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
            if (currentAction.param1 < TFT_NUMBER_OF_WAVEFORM_EXT) {
                oscilloBgDrawOperatorShape(waveForm[currentAction.param1].waveForms, waveForm[currentAction.param1].size);
            } else if (currentAction.param1 == TFT_DRAW_ENVELOPPE) {
                oscilloBgDrawEnvelope();
            } else if (currentAction.param1 == TFT_DRAW_LFO) {
                oscilloBgDrawLfo();
            }
            currentAction.actionType = 0;
            oscilloForceNextDisplay();
            break;
        }
    }
}

/*
 * Push tftMemory to physical TFT
 * Screen is divided into UPDATE_NUMBER_OF_PARTS parts, and only at a time is pushed.
 */
bool TftDisplay::pushToTft() {
    if (tftDirtyBits == 0 || !tftRefreshing) {
        return false;
    }

    // 4 max - can break before
    int p;
    for (p = 0; p < 4 ; p++) {
        if ((tftDirtyBits & (1UL << part)) > 0) {
            // Update TFT part
            ILI9341_Select();


            uint16_t height = areaHeight[part];
            uint16_t y = areaY[part];

            if (ILI9341_SetAddressWindow(0, y, 239, y + height - 1) == HAL_OK) {
                PFM_SET_PIN(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin);

                if (HAL_OK == HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, (uint8_t *) tftMemory + (y * 240 * 2), height * 240 * 2)) {
                    tftDirtyBits &= ~(1UL << part);
                    pushToTftInProgress = true;
                }
            }
            part = (part + 1) % TFT_NUMBER_OF_PARTS;
            break;
        }
        part = (part + 1) % TFT_NUMBER_OF_PARTS;
    }


    return true;
}

void TftDisplay::pushToTftFinished() {
    pushToTftInProgress = false;
    ILI9341_Unselect();
}



void TftDisplay::setCursor(uint8_t x, uint16_t y) {
    cursorX = x * TFT_BIG_CHAR_WIDTH;
    cursorY = y * TFT_BIG_CHAR_HEIGHT;
}

void TftDisplay::setCursorSmallChar(uint8_t x, uint16_t y) {
    cursorX = x * TFT_SMALL_CHAR_WIDTH;
    cursorY = y * TFT_SMALL_CHAR_HEIGHT;
}

void TftDisplay::setCursorInPixel(uint8_t x, uint16_t y) {
    cursorX = x;
    cursorY = y;
}

void TftDisplay::clearMixerValues() {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_FILL_AREA;
    newAction.param1 = 160;
    newAction.param2 = 75;
    newAction.param3 = 80;
    newAction.param4 = 190;
    newAction.param5 = COLOR_BLACK;
    tftActions.insert(newAction);
    // Clear info line
    newAction.param1 = 0;
    newAction.param2 = 50;
    newAction.param3 = 240;
    newAction.param4 = 20;
    tftActions.insert(newAction);
}

void TftDisplay::clearMixerLabels() {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_FILL_AREA;
    newAction.param1 = 0;
    newAction.param2 = 75;
    newAction.param3 = 160;
    newAction.param4 = 190;
    newAction.param5 = COLOR_BLACK;
    tftActions.insert(newAction);
}


void TftDisplay::clear() {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_FILL_TFT;
    newAction.param3 = TFT_NUMBER_OF_PARTS - 1;
    newAction.param5 = COLOR_BLACK;
    tftActions.insert(newAction);
    bHasJustBeenCleared = true;
}

void TftDisplay::print(float f) {
    if (f < 0.0) {
        print('-');
        f = -f;
    } else {
        print(' ');
    }
    if (f < 10.0f) {
        int integer = (int) f;
        print((char) ((char) integer + '0'));
        print('.');
        f -= integer;
        int valueTimes100 = (int) (f * 100 + .0005);
        if (valueTimes100 < 10) {
            print("0");
            print(valueTimes100);
        } else {
            print(valueTimes100);
        }
    } else {
        int integer = (int) f;
        print(integer);
        print('.');
        f -= integer;
        print((int) (f * 10));
    }
}

void TftDisplay::printFloatWithOneDecimal(float f) {
    if (f < 10.0f) {
        print('0');
    }
    int integer = (int) f;
    print(integer);
    print('.');
    f -= integer;
    int valueTimes10 = (int) (f * 10 + .0005);
    print(valueTimes10);
}

void TftDisplay::print(const char* str) {
    int i = 0;
    char c = str[i++];
    while (c != '\0') {
        print(c);
        c = str[i++];
    }
}

bool TftDisplay::print(const char* str, int length) {
    int i = 0;
    char c = str[i++];
    while (c != '\0' && i <= length) {
        print(c);
        c = str[i++];
    }
    return c == '\0';
}


void TftDisplay::printSmallChars(const char* str) {
    int i = 0;
    char c = str[i++];
    while (c != '\0') {
        printSmallChar(c);
        c = str[i++];
    }
}

void TftDisplay::print(const char* str, TFT_COLOR color, TFT_COLOR bgColor) {

    int i = 0;
    char c = str[i++];
    while (c != '\0') {
        print(c, color, bgColor);
        c = str[i++];
    }
}

void TftDisplay::printSpaceTillEndOfLine() {
    while (this->cursorX <= 240 - TFT_BIG_CHAR_WIDTH) {
        print(' ');
    }
}

void TftDisplay::print(int n) {
    unsigned char buf[12];
    unsigned long i = 0;

    if (unlikely(n == 0)) {
        print('0');
        return;
    }

    if (unlikely(n < 0)) {
        print('-');
        n = -n;
    }

    while (n > 0) {
        buf[i++] = n % 10;
        n /= 10;
    }

    for (; i > 0; i--) {
        print((char) ('0' + buf[i - 1]));
    }
}

void TftDisplay::printSmallChar(int n) {
    unsigned char buf[12];
    unsigned long i = 0;

    if (unlikely(n == 0)) {
        printSmallChar('0');
        return;
    }

    if (unlikely(n < 0)) {
        printSmallChar('-');
        n = -n;
    }

    while (n > 0) {
        buf[i++] = n % 10;
        n /= 10;
    }

    for (; i > 0; i--) {
        printSmallChar((char) ('0' + buf[i - 1]));
    }
}

void TftDisplay::print(char c, TFT_COLOR color, TFT_COLOR bgColor) {
    TFTAction newAction;
    newAction.actionType = TFT_DISPLAY_ONE_CHAR;
    newAction.param1 = cursorX;
    newAction.param2 = cursorY;
    if ((c >= 32) && (c <= 32 + 94)) {
        newAction.param3 = c - 32;
    } else {
        newAction.param3 = 35;
    }
    newAction.param4 = color;
    newAction.param5 = bgColor;
    tftActions.insert(newAction);
    cursorX += TFT_BIG_CHAR_WIDTH;
}

void TftDisplay::print(char c) {
    TFTAction newAction;
    newAction.actionType = TFT_DISPLAY_ONE_CHAR;
    newAction.param1 = cursorX;
    newAction.param2 = cursorY;
    if ((c >= 32) && (c <= 32 + NUMBER_OF_CHARS)) {
        newAction.param3 = c - 32;
    } else {
        newAction.param3 = 3;
    }
    newAction.param4 = charColor;
    newAction.param5 = charBackgroundColor;
    tftActions.insert(newAction);
    cursorX += TFT_BIG_CHAR_WIDTH;
}

void TftDisplay::printSmallChar(char c) {
    TFTAction newAction;
    newAction.actionType = TFT_DISPLAY_ONE_SMALL_CHAR;
    newAction.param1 = cursorX;
    newAction.param2 = cursorY;
    if ((c >= 32) && (c < 32 + 97)) {
        newAction.param3 = c - 32;
    } else {
        newAction.param3 = 3;
    }
    newAction.param4 = charColor;
    newAction.param5 = charBackgroundColor;
    tftActions.insert(newAction);
    cursorX += TFT_SMALL_CHAR_WIDTH;
}

void TftDisplay::oscilloRrefresh() {
    float oscilloSampleReadPtr = oscilloSamplePeriod - oscilloPhaseIndexToUse;
    bool saturate = false;
    int32_t maxYValue = 0;
    float multiplier = olscilloYScale * 50.0f;
    uint32_t total = 0;
    for (int x = 0; x < 160; x++) {
        // Use _usat on value before multiplying ?????
        int8_t oValue = (int8_t) (oscilloFullPeriodSampl[(int) oscilloSampleReadPtr] * multiplier);
        total += abs(oValue);
        if (unlikely(oValue > 50)) {
            saturate = true;
            oscilloYValue[x] = 50;
        } else if (unlikely(oValue < -50)) {
            saturate = true;
            oscilloYValue[x] = -50;
        } else {
            oscilloYValue[x] = oValue;
        }

        if (oValue > 0) {
            if (unlikely(maxYValue < oValue)) {
                maxYValue = oValue;
            }
        } else if (oValue < 0) {
            if (unlikely(maxYValue < -oValue)) {
                maxYValue = -oValue;
            }
        }

        oscilloSampleReadPtr += oscilloSampleInc;
        if (oscilloSampleReadPtr > oscilloSamplePeriod) {
            oscilloSampleReadPtr -= oscilloSamplePeriod;
        }
    }

    // flatOscillo must be displayed only once
    if (total == 0) {
        if (!flatOscilloAlreadyDisplayed) {
            total = 1;
            flatOscilloAlreadyDisplayed = true;
        }
    } else {
        flatOscilloAlreadyDisplayed = false;
    }

    if (total > 0) {
        int indexMiddle = 50 * 160;

        for (int x = 0; x < 159; x++) {
            if (oscilloYValue[x] < (oscilloYValue[x + 1] - 1)) {
                for (int oy = oscilloYValue[x]; oy < oscilloYValue[x + 1]; oy++) {
                    fgOscillo[indexMiddle + x + oy * 160] = 0xff;
                }
            } else if (oscilloYValue[x] > (oscilloYValue[x + 1] + 1)) {
                for (int oy = oscilloYValue[x]; oy > oscilloYValue[x + 1]; oy--) {
                    fgOscillo[indexMiddle + x + oy * 160] = 0xff;
                }
            } else {
                fgOscillo[indexMiddle + x + oscilloYValue[x] * 160] = 0xff;
            }
        }
        // Ask for tft push and refresh
        TFTAction newAction;
        newAction.actionType = TFT_DRAW_OSCILLO;
        // We don't want oscillo in red when saturation since we have olscilloYScale
        //newAction.param1 = saturate ? 1 : 0;
        newAction.param1 = 0;
        newAction.param3 = 0;
        tftActions.insert(newAction);

        // Dynamically adjust olscilloYScale
        if (saturate) {
            olscilloYScale -= .2f;
        } else {
            if (maxYValue < 30 && olscilloYScale < .8f) {
                olscilloYScale += .2f;
            }
        }
    }
    // We can start filling again the oscillo buffer
	oscilloDataWritePtr = 0;
}

void TftDisplay::fillArea(uint8_t x, uint16_t y, uint8_t width, uint16_t height, uint8_t color) {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_FILL_AREA;
    newAction.param1 = x;
    newAction.param2 = y;
    newAction.param3 = width;
    newAction.param4 = height;
    newAction.param5 = color;
    tftActions.insert(newAction);

}

void TftDisplay::oscilloNewLowerFrequency(float lf) {
    oscilloSamplePeriod = PREENFM_FREQUENCY / lf;
    if (oscilloSamplePeriod > OSCILLO_BUFFER_SIZE) {
        oscilloSamplePeriod = OSCILLO_BUFFER_SIZE;
    }
    oscilloSampleInc = (float) oscilloSamplePeriod / OSCILLO_WIDTH;

    // oscilloPhaseIndexToUse = oscilloSamplePeriod - (oscilloPhaseIndex % ((int)oscilloSamplePeriod + 1));
    int divInt = oscilloPhaseIndex / oscilloSamplePeriod;

    oscilloPhaseIndexToUse = oscilloPhaseIndex - divInt * oscilloSamplePeriod;
}

void TftDisplay::oscilloRecord32Samples(float *samples) {
    oscilloPhaseIndex += 32.0f;
    if (oscilloDataWritePtr < oscilloSamplePeriod) {
        for (int r = 0; r < 32; r++) {
            oscilloFullPeriodSampl[oscilloDataWritePtr++] = samples[r * 2];
        }
    }
}

void TftDisplay::drawAlgo(int algo) {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_ALGO;
    newAction.param1 = algo;
    newAction.param3 = 0;
    tftActions.insert(newAction);
}

void TftDisplay::highlightOperator(int algo, int op) {
    TFTAction newAction;
    newAction.actionType = TFT_HIGHLIGHT_ALGO_OPERATOR;
    newAction.param1 = algo;
    newAction.param2 = op;
    newAction.param3 = 0;
    tftActions.insert(newAction);
}

void TftDisplay::highlightIM(uint8_t imNum, uint8_t opSource, uint8_t opDest) {
    TFTAction newAction;
    newAction.actionType = TFT_HIGHTLIGHT_ALGO_IM;
    newAction.param1 = imNum;
    newAction.param3 = 0;
    newAction.param4 = opSource;
    newAction.param5 = opDest;
    tftActions.insert(newAction);
}

void TftDisplay::eraseHighlightIM(uint8_t imNum, uint8_t opSource, uint8_t opDest) {
    TFTAction newAction;
    newAction.actionType = TFT_ERASE_HIGHTLIGHT_ALGO_IM;
    newAction.param1 = imNum;
    newAction.param3 = 0;
    newAction.param4 = opSource;
    newAction.param5 = opDest;
    tftActions.insert(newAction);
}

void TftDisplay::restartRefreshTft() {
    TFTAction newAction;
    newAction.actionType = TFT_RESTART_REFRESH;
    tftActions.insert(newAction);
}

void TftDisplay::pauseRefresh() {
    TFTAction newAction;
    newAction.actionType = TFT_PAUSE_REFRESH;
    tftActions.insert(newAction);
}

#define BUTTON_WIDTH 72

void TftDisplay::drawButton(const char* label, uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, uint8_t numberOfStates,
        uint8_t activeState, TFT_COLOR background) {

    uint8_t x = (buttonNumber % 3) * 80 + 40; // 40 / 120 / 200
    y += buttonNumber > 2 ? dyLine2 : 0;
    int left = x - BUTTON_WIDTH / 2;
    fillArea(left, y, BUTTON_WIDTH, 21, background);

    if (activeState > 0) {

        switch (numberOfStates) {
        case 1:
            fillArea(left, y + 20, BUTTON_WIDTH, 1, COLOR_WHITE);
            break;
        case 2:
            fillArea(left, y + 19, BUTTON_WIDTH/2, 2, activeState == 1 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(x, y + 19, BUTTON_WIDTH/2, 2, activeState == 2 ? COLOR_WHITE : COLOR_GRAY);
            break;
        case 3:
            fillArea(left, y + 19, BUTTON_WIDTH/3, 2, activeState == 1 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH/3, y + 19, BUTTON_WIDTH/3, 2, activeState == 2 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH/3*2, y + 19, BUTTON_WIDTH/3, 2, activeState == 3 ? COLOR_WHITE : COLOR_GRAY);
            break;
        case 4:
            fillArea(left, y + 19, BUTTON_WIDTH/4, 2, activeState == 1 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH/4, y + BUTTON_WIDTH/4, 18, 2, activeState == 2 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(x, y + 19, BUTTON_WIDTH/4, 2, activeState == 3 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH/4*3, y + 19, BUTTON_WIDTH/4, 2, activeState == 4 ? COLOR_WHITE : COLOR_GRAY);
            break;
        }
    } else {
        if (numberOfStates > 0) {
            fillArea(left, y + 20, BUTTON_WIDTH, 1, COLOR_GRAY);
        }
    }

    int len = -1;
    while (label[++len] != 0)
        ;

    x = x  - len * TFT_BIG_CHAR_WIDTH / 2;
    y = y + 1;

    if (numberOfStates > 0) {
        setCharColor(activeState > 0 ? COLOR_WHITE : COLOR_GRAY);
    } else {
        // No state.. simple button
        setCharColor(COLOR_WHITE);
    }
    setCharBackgroundColor(background);
    setCursorInPixel(x, y);
    print(label);
    setCharBackgroundColor(COLOR_BLACK);

}


void TftDisplay::drawSimpleButton(const char* label, uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, TFT_COLOR color,
        TFT_COLOR background) {

    uint8_t x = (buttonNumber % 3) * 80 + 40; // 40 / 120 / 200
    y += buttonNumber > 2 ? dyLine2 : 0;
    fillArea(x - BUTTON_WIDTH/2, y, BUTTON_WIDTH, 21, background);

    int len = -1;
    while (label[++len] != 0)
        ;

    x = x  - len * TFT_BIG_CHAR_WIDTH / 2;
    y = y + 1;

    setCharColor(color);
    setCharBackgroundColor(background);
    setCursorInPixel(x, y);
    print(label);
    setCharBackgroundColor(COLOR_BLACK);
}

void TftDisplay::drawSimpleBorderButton(const char* label, uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, TFT_COLOR color,
        TFT_COLOR background) {

    uint8_t x = (buttonNumber % 3) * 80 + 40; // 40 / 120 / 200
    y += buttonNumber > 2 ? dyLine2 : 0;
    fillArea(x - BUTTON_WIDTH/2, y, BUTTON_WIDTH, 21, background);
    fillArea(x - BUTTON_WIDTH/2 + 1, y + 1, BUTTON_WIDTH - 2, 21 - 2, COLOR_BLACK);
    setCharBackgroundColor(COLOR_BLACK);

    int len = -1;
    while (label[++len] != 0)
        ;

    x = x  - len * TFT_BIG_CHAR_WIDTH / 2;
    y = y + 1;

    setCharColor(color);
    setCursorInPixel(x, y);
    print(label);
}

void TftDisplay::printValueWithSpace(int value) {
    print(value);

    if (value > 99) {
        print(' ');
    } else if (value > 9) {
        print("  ");
    } else if (value > -1) {
        print("   ");
    } else if (value > -10) {
        print("  ");
    } else if (value > -100) {
        print(' ');
    }
}

void TftDisplay::printFloatWithSpace(float value) {
    if (value < 0.0) {
        print('-');
        value = -value;
    } else {
        print(' ');
    }
    if (value < 10.0f) {
        int integer = (int) (value + .0005f);
        print(integer);
        print('.');
        value -= integer;
        int valueTimes100 = (int) (value * 100.0f + .0005f);
        if (valueTimes100 < 10) {
            print("0");
            print(valueTimes100);
        } else {
            print(valueTimes100);
        }
    } else {
        int integer = (int) (value + .0005f);
        print(integer);
        print('.');
        value -= integer;
        print((int) (value * 10 + .0005f));
    }
}


void TftDisplay::oscilloBgDrawOperatorShape(float* waveForm, int size) {

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

void TftDisplay::oscilloBgActionOperatorShape(int wfNumber) {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
    newAction.param1 = wfNumber;
    newAction.param3 = 0;
    tftActions.insert(newAction);

    oscilloIsClean = false;
}


void TftDisplay::oscilloBgActionClear() {
    if (!oscilloIsClean) {
        TFTAction newAction;
        newAction.param1 = 255;
        newAction.param3 = 0;
        newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
        tftActions.insert(newAction);
        oscilloIsClean = true;
    }
}


void TftDisplay::oscilloBgActionLfo() {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
    newAction.param1 = TFT_DRAW_LFO;
    newAction.param3 = 0;
    tftActions.insert(newAction);

    oscilloIsClean = false;
}

void TftDisplay::oscilloBgSetEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL) {
    oscilParams1[0] = a;
    oscilParams1[1] = d + oscilParams1[0];
    oscilParams1[2] = s + oscilParams1[1];
    oscilParams1[3] = r + oscilParams1[2];

    oscilParams2[0] = aL;
    oscilParams2[1] = dL;
    oscilParams2[2] = sL;
    oscilParams2[3] = rL;

    oscilParams1[4] = COLOR_YELLOW;

}

void TftDisplay::oscilloBgSetLfoEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL) {
    oscilloBgSetEnvelope(a, d, s, r, aL, dL, sL, rL);
    oscilParams1[4] = COLOR_BLUE;
}


void TftDisplay::oscilloBgSetLfo(float shape, float freq, float kSyn, float bias, float phase) {
    if (freq >= 100.0f) {
        freq = 1.0f;
    }
    oscilParams1[0] = shape;
    oscilParams1[1] = freq;
    oscilParams1[2] = kSyn;
    oscilParams1[3] = bias;
    oscilParams1[4] = phase;

}


void TftDisplay::oscilloBgDrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
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


void TftDisplay::oscilloBgActionEnvelope() {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM;
    newAction.param1 = TFT_DRAW_ENVELOPPE;
    newAction.param3 = 0;
    tftActions.insert(newAction);

    oscilloIsClean = false;
}

void TftDisplay::oscilloBgDrawEnvelope() {
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

    oscilloBgDrawLine(0, 0, (int)(oscilParams1[0] * scale), (int)(oscilParams2[0] * 97.0f), color);
    oscilloBgDrawLine((int)(oscilParams1[0] * scale), (int)(oscilParams2[0] * 97.0f), (int)(oscilParams1[1] * scale), (int)(oscilParams2[1] * 97.0f), color);
    oscilloBgDrawLine((int)(oscilParams1[1] * scale), (int)(oscilParams2[1] * 97.0f), (int)(oscilParams1[2] * scale), (int)(oscilParams2[2] * 97.0f), color);
    oscilloBgDrawLine((int)(oscilParams1[2] * scale), (int)(oscilParams2[2] * 97.0f), (int)(oscilParams1[3] * scale), (int)(oscilParams2[3] * 97.0f), color);

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


void TftDisplay::oscilloBgDrawLfo() {
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
    case 4: {
        // Rand
        uint32_t random32bit = 0b10101101110001101110010010010011;
        float incIndex = 1.0f / 160.0f * oscilParams1[1];
        float index = oscilParams1[4];
        for (int x = 0; x < 160; x++) {
            index += incIndex;

            if (index > 1.0f)  {
                index -= 1.0f;
                random32bit = 214013 * random32bit + 2531011;
            }
            // Take 8 bits (0 to 255), divide by 127 and minus 1 -> [-1.0; 1.0];
            float sample =  ((float) (random32bit &0xff)) * 0.00784313725f - 1.0f;
            oscilloYValue[x] = (int)(sample * 48.0f);
        }
        break;
    }
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

void TftDisplay::drawLevelMetter(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t pixelPerDb, float volume, bool isComp, float gr) {
    float maxX = width + (volume + gr) * pixelPerDb;
    if (maxX < 0) {
        maxX = 0;
    }
    // Erase
    fillArea(x, y, width, height, COLOR_BLACK);

    uint8_t visuColo = COLOR_LIGHT_GREEN;
    uint8_t visuColorAfterThresh = COLOR_WHITE;

    if (isComp) {
        // Saturation ?
        if (maxX > width) {
            maxX = width;
            visuColo = COLOR_RED;
            visuColorAfterThresh = COLOR_RED;
        }

        // With compressor
        int afterThresh = 0;
        int threashold = 12;
        int threshInPixel = width - threashold * pixelPerDb;
        if (maxX > threshInPixel) {
            afterThresh = (maxX - threshInPixel);
            maxX = threshInPixel;
        }
        fillArea(x, y, maxX, height, visuColo);
        if (afterThresh > 0) {
            fillArea(x + threshInPixel, y, afterThresh, height, visuColorAfterThresh);
            fillArea(x + threshInPixel, y, 1, height, COLOR_ORANGE);
        }

        if (gr < -.5) {
            fillArea(x, y, -gr * pixelPerDb, height, COLOR_ORANGE);
        }
    } else {
        // Without compressor
        if (maxX > width) {
            maxX = width;
            visuColo = COLOR_RED;
        }
        fillArea(x, y, maxX, height, visuColo);
    }

}
