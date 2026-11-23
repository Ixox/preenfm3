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

#include "TftDisplay.h"
#include "ili9341.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_rng.h"
#include <stdlib.h>
#include <string.h>

// 95 char + 16 step seqs + 1 note + 4 for sequencers
#define NUMBER_OF_CHARS (95 + 16 + 1 + 4)

#define likely(x)   __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define TFT_FOREGROUND_BIG_CHARS      (NUMBER_OF_CHARS * 11 * 18)
#define TFT_FOREGROUND_SMALL_CHARS    (96 * 7 * 10)
#define TFT_FOREGROUND_SEGMENT_DIGITS (12 * 9 * 5 * 4)
#define TFT_FOREGROUND_OSCILLO        (160 * 101)
#define TFT_FOREGROUND_ALGO           (80 * 100)
#define TFT_FOREGROUND_ENTRIES                                                                                                             \
    (TFT_FOREGROUND_BIG_CHARS + TFT_FOREGROUND_SMALL_CHARS + TFT_FOREGROUND_SEGMENT_DIGITS + TFT_FOREGROUND_OSCILLO + TFT_FOREGROUND_ALGO)
// Round up to 32 bytes (uint8_t entries) so the next buffer is 32-byte aligned.
#define TFT_FOREGROUND_SIZE (((TFT_FOREGROUND_ENTRIES + 31) / 32) * 32)

#define TFT_BACKGROUND_ENTRIES (NUMBER_OF_TFT_COLORS * 20 * 11 + 160 * 100 + 80 * 100)
// Round up to 16 uint16_t entries so the next buffer starts on a 32-byte boundary.
#define TFT_BACKGROUND_SIZE (((TFT_BACKGROUND_ENTRIES + 15) / 16) * 16)

RAM_D1_SECTION uint8_t tftForeground[TFT_FOREGROUND_SIZE];
RAM_D1_SECTION uint16_t tftBackground[TFT_BACKGROUND_SIZE];
RAM_D1_SECTION uint16_t tftMemory[240 * 320];

extern DMA2D_HandleTypeDef hdma2d;
extern RNG_HandleTypeDef hrng;

#define TFT_OSCILLO_Y 160
#define TFT_OSCILLO_X 0
#define TFT_ALGO_Y    160
#define TFT_ALGO_X    160

uint32_t tftPalette[NUMBER_OF_TFT_COLORS];
uint16_t tftPalette565[NUMBER_OF_TFT_COLORS];

uint32_t reducedColor(uint32_t x) {
    return
        // RED
        (((x & 0xe00000) >> 21) << 10) +
        (((x & 0x180000) >> 19) << 6)
        // GREEN
        + (((x & 0xe000) >> 13) << 3) +
        (((x & 0x1c00) >> 10) << 21)
        // BLUE
        + (((x & 0xc0) >> 6) << 19) + (((x & 0x38) >> 3) << 13);
}

uint16_t convertTo565(uint32_t x) {
    return ((x & 0xf80000) >> 8) + ((x & 0xfc00) >> 5) + ((x & 0xf8) >> 3);
}

TftDisplay::TftDisplay() {
    pushToTftInProgress_ = false;
    pushingY0_ = 0;
    pushingY1_ = 0;
    nextDirtyRow_ = 0;
    markAllDirty();

    currentAction_.actionType = 0;
    lastOscilloSaturateTic_ = 0;
    currentActionStep_ = 0;

    oscilloSamplePeriod_ = PREENFM_FREQUENCY / 440;
    oscilloSampleInc_ = oscilloSamplePeriod_ / OSCILLO_WIDTH;
    oscilloPhaseIndex_ = 0.0f;
    oscilloLowerFrequency_ = 440.0f;
    oscilloSmoothedValid_ = false;
    oscilloHistoryIndex_ = 0;

    for (int o = 0; o < OSCILLO_BUFFER_SIZE; o++) {
        oscilloFullPeriodSampl_[o] = 0.0f;
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
    tftPalette[COLOR_DARK_GRAY] = reducedColor(0x303030);
    tftPalette[COLOR_CYAN] = reducedColor(0x76dfef);
    tftPalette[COLOR_ORANGE] = reducedColor(0xff7f00);
    tftPalette[COLOR_LIGHT_GREEN] = reducedColor(0x70ff70);

    for (int c = 0; c < NUMBER_OF_TFT_COLORS; c++) {
        tftPalette565[c] = convertTo565(tftPalette[c]);
    }

    charColor_ = COLOR_GREEN;
    charBackgroundColor_ = COLOR_BLACK;
    tftRefreshing_ = true;
    fgAlgoDirty_ = true;
    flatOscilloAlreadyDisplayed_ = true;
    tftGetStatusMillis_ = 0;
    olscilloYScale_ = 1.0f;
    tftMustBeReset_ = false;
}

TftDisplay::~TftDisplay() {
    // TODO Auto-generated destructor stub
}

void TftDisplay::init(TftAlgo* tftAlgo) {

    this->tftAlgo_ = tftAlgo;
    // Characters
    int tftTmpIndex = 0;

    fgBigChars_ = &tftForeground[tftTmpIndex];

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

    fgSmallChars_ = &tftForeground[tftTmpIndex];

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

    fgSegmentDigit_ = &tftForeground[tftTmpIndex];

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

    fgOscillo_ = &tftForeground[tftTmpIndex];

    memset(fgOscillo_, 0, 160 * 101);
    tftTmpIndex += 160 * 101;

    fgAlgo_ = &tftForeground[tftTmpIndex];
    memset(fgAlgo_, 0, 80 * 100);
    tftTmpIndex += 80 * 100;

    // Check tftTmpIndex here for tftForeground size

    // ==============================
    // init Background
    int bgIndex = 0;

    for (int c = 0; c < NUMBER_OF_TFT_COLORS; c++) {
        bgColorChar_[c] = &tftBackground[bgIndex];
        for (int i = 0; i < 20 * 11; i++) {
            tftBackground[bgIndex + i] = tftPalette565[c];
        }
        bgIndex += 20 * 11;
    }

    // BG OScillo
    bgOscillo_ = &tftBackground[bgIndex];

    memset(bgOscillo_, 0, 160 * 100 * 2);
    uint16_t oscilloColor = 0x0842;
    for (int x = 0; x < 160; x++) {
        bgOscillo_[x] = oscilloColor;
        bgOscillo_[x + 99 * 160] = oscilloColor;
    }
    for (int y = 0; y < 100; y++) {
        bgOscillo_[y * 160] = oscilloColor;
        bgOscillo_[y * 160 + 159] = oscilloColor;
    }
    bgIndex += 160 * 100;

    bgAlgo_ = &tftBackground[bgIndex];

    memset(bgAlgo_, 0, 80 * 100 * 2);
    bgIndex += 80 * 100;
    // Check bgIndex here for tftBackground size (must be multiplied by 2)

    currentAction_.actionType = 0;
}

void TftDisplay::reset() {
    clearActions();
    ILI9341_Init();
    tftMustBeReset_ = false;
}

void TftDisplay::markRows(uint16_t y0, uint16_t y1) {
    if (y0 >= TFT_HEIGHT || y1 <= y0) {
        return;
    }
    if (y1 > TFT_HEIGHT) {
        y1 = TFT_HEIGHT;
    }

    // Keep the multi-word dirty-bitmap update atomic with the SPI error ISR.
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint16_t firstWord = y0 >> 5;
    uint16_t lastWord = (y1 - 1) >> 5;
    uint32_t firstMask = 0xFFFFFFFFUL << (y0 & 31);
    uint32_t lastMask = 0xFFFFFFFFUL >> (31 - ((y1 - 1) & 31));

    if (firstWord == lastWord) {
        dirtyRows_[firstWord] |= firstMask & lastMask;
    } else {
        dirtyRows_[firstWord] |= firstMask;
        for (uint16_t w = firstWord + 1; w < lastWord; w++) {
            dirtyRows_[w] = 0xFFFFFFFFUL;
        }
        dirtyRows_[lastWord] |= lastMask;
    }

    __set_PRIMASK(primask);
}

void TftDisplay::clearRows(uint16_t y0, uint16_t y1) {
    if (y0 >= TFT_HEIGHT || y1 <= y0) {
        return;
    }
    if (y1 > TFT_HEIGHT) {
        y1 = TFT_HEIGHT;
    }

    uint16_t firstWord = y0 >> 5;
    uint16_t lastWord = (y1 - 1) >> 5;
    uint32_t firstMask = 0xFFFFFFFFUL << (y0 & 31);
    uint32_t lastMask = 0xFFFFFFFFUL >> (31 - ((y1 - 1) & 31));

    if (firstWord == lastWord) {
        dirtyRows_[firstWord] &= ~(firstMask & lastMask);
    } else {
        dirtyRows_[firstWord] &= ~firstMask;
        for (uint16_t w = firstWord + 1; w < lastWord; w++) {
            dirtyRows_[w] = 0;
        }
        dirtyRows_[lastWord] &= ~lastMask;
    }
}

void TftDisplay::markAllDirty() {
    memset(dirtyRows_, 0xFF, sizeof(dirtyRows_));
}

bool TftDisplay::anyDirty() const {
    for (uint16_t w = 0; w < TFT_DIRTY_WORDS; w++) {
        if (dirtyRows_[w] != 0) {
            return true;
        }
    }
    return false;
}

bool TftDisplay::findNextDirtyRun(uint16_t startRow, uint16_t& y0, uint16_t& y1) const {
    if (startRow >= TFT_HEIGHT) {
        startRow = 0;
    }

    for (uint16_t pass = 0; pass < 2; pass++) {
        uint16_t begin = (pass == 0) ? startRow : 0;
        uint16_t end = (pass == 0) ? TFT_HEIGHT : startRow;

        for (uint16_t row = begin; row < end; row++) {
            if (dirtyRows_[row >> 5] & (1UL << (row & 31))) {
                y0 = row;

                uint16_t maxEnd = row + TFT_MAX_BAND_HEIGHT;
                if (maxEnd > TFT_HEIGHT) {
                    maxEnd = TFT_HEIGHT;
                }

                y1 = row + 1;
                while (y1 < maxEnd && (dirtyRows_[y1 >> 5] & (1UL << (y1 & 31)))) {
                    y1++;
                }
                return true;
            }
        }
    }

    return false;
}

void TftDisplay::setDirtyArea(uint16_t y, uint16_t height) {
    uint32_t end = (uint32_t)y + height;
    if (y >= TFT_HEIGHT || end <= y) {
        return;
    }
    if (end > TFT_HEIGHT) {
        end = TFT_HEIGHT;
    }
    markRows(y, (uint16_t)end);
}

void TftDisplay::ticTimer(bool checkDisplayPower) {

    uint32_t currentMillis = HAL_GetTick();

    if (checkDisplayPower && (currentMillis - tftGetStatusMillis_) > 500) {
        status_[1] = 1;
        if (ILI9341_ReadPowerMode(status_) != HAL_OK) {
            status_[1] = 1;
        }
        // Try to detect when TFT is BLANK
        if (status_[1] != 222 && status_[1] != 156) {
            if (status_[3] == status_[1]) {
                tftMustBeReset_ = true;
            }
        }
        // status_2 allows to make sure we got 2 wrong number in a row
        status_[3] = status_[1];
        tftGetStatusMillis_ = currentMillis;
        return;
    }

    if (currentAction_.actionType == TFT_WAITCYCLE) {
        currentAction_.param2--;
        if (currentAction_.param2 == 0) {
            currentAction_.actionType = 0;
        }
    }
}

void TftDisplay::processActions(uint8_t maxSteps) {
    for (uint8_t n = 0; n < maxSteps; n++) {
        if (!PFM_IS_DMA2D_READY()) {
            break;
        }
        if (currentAction_.actionType == 0) {
            if (tftActions_.getCount() == 0) {
                break;
            }
            uint32_t primask = __get_PRIMASK();
            __disable_irq();
            currentAction_ = tftActions_.remove();
            currentActionStep_ = 0;
            __set_PRIMASK(primask);
        }
        if (currentAction_.actionType == TFT_WAITCYCLE) {
            break;
        }
        processActionStep();
    }

    if (!pushToTftInProgress_ && anyDirty()) {
        pushToTft();
    }
}

void TftDisplay::processActionStep() {

    uint32_t offset;

    switch (currentAction_.actionType) {
    case TFT_NOP:
        return;
    case TFT_DISPLAY_ONE_CHAR:
        // Display Char
        if (PFM_IS_DMA2D_READY()) {

            WRITE_REG(hdma2d.Instance->FGCOLR, tftPalette[currentAction_.param4]);
            MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_M2M_BLEND);

            offset = currentAction_.param2 * 240 + currentAction_.param1;
            MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 240 - 11);
            MODIFY_REG(hdma2d.Instance->BGOR, DMA2D_OOR_LO, 0);
            MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (18 | (11 << DMA2D_POSITION_NLR_PL)));
            WRITE_REG(hdma2d.Instance->FGMAR, (uint32_t)(fgBigChars_ + currentAction_.param3 * 18 * 11));
            WRITE_REG(hdma2d.Instance->BGMAR, (uint32_t)(bgColorChar_[currentAction_.param5]));
            WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)(tftMemory + offset));

            setDirtyArea(currentAction_.param2, 18);

            currentAction_.actionType = 0;

            PFM_START_DMA2D();
        }
        break;
    case TFT_DISPLAY_ONE_SMALL_CHAR:
        // Display Char
        if (PFM_IS_DMA2D_READY()) {

            WRITE_REG(hdma2d.Instance->FGCOLR, tftPalette[currentAction_.param4]);
            MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_M2M_BLEND);
            offset = currentAction_.param2 * 240 + currentAction_.param1;
            MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 240 - 7);
            MODIFY_REG(hdma2d.Instance->BGOR, DMA2D_OOR_LO, 0);
            MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (10 | (7 << DMA2D_POSITION_NLR_PL)));
            WRITE_REG(hdma2d.Instance->FGMAR, (uint32_t)(fgSmallChars_ + currentAction_.param3 * 10 * 7));
            WRITE_REG(hdma2d.Instance->BGMAR, (uint32_t)(bgColorChar_[currentAction_.param5]));
            WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)(tftMemory + offset));

            setDirtyArea(currentAction_.param2, 10);

            currentAction_.actionType = 0;
            PFM_START_DMA2D();
        }
        break;
    case TFT_DRAW_FILL_TFT:
        // CLEAR tftMemory
        if (PFM_IS_DMA2D_READY()) {

            MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
            WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[currentAction_.param5]);

            MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 0);
            MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (TFT_HEIGHT | (TFT_WIDTH << DMA2D_POSITION_NLR_PL)));
            WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)tftMemory);
            PFM_START_DMA2D();

            markAllDirty();
            currentAction_.actionType = 0;
        }
        break;
    case TFT_DRAW_OSCILLO:
        // 2 steps
        // 0 : copy oscillo+background => tftMemory
        // 1 : copy back background=>oscillo
        switch (currentAction_.param3) {
        case 0:
            // Display Oscilloscope
            if (PFM_IS_DMA2D_READY()) {
                uint32_t currentTic = HAL_GetTick();

                // Keep red color for 5 refreshes after saturation detected
                // param param1 == 1 means last oscillo refresh saturated
                if (unlikely(currentAction_.param1 == 1)) {
                    lastOscilloSaturateTic_ = currentTic;
                }
                if (unlikely(currentAction_.param1 == 1 || (currentTic < (lastOscilloSaturateTic_ + 10)))) {
                    WRITE_REG(hdma2d.Instance->FGCOLR, tftPalette[COLOR_RED]);
                } else {
                    WRITE_REG(hdma2d.Instance->FGCOLR, tftPalette[COLOR_CYAN]);
                }
                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_M2M_BLEND);

                offset = TFT_OSCILLO_Y * 240 + TFT_OSCILLO_X;
                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 240 - 160);
                MODIFY_REG(hdma2d.Instance->BGOR, DMA2D_OOR_LO, 0);
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (100 | (160 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->FGMAR, (uint32_t)(fgOscillo_));
                WRITE_REG(hdma2d.Instance->BGMAR, (uint32_t)(bgOscillo_));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)(tftMemory + offset));
                PFM_START_DMA2D();
                currentAction_.param3 = 1;
            }
            break;
        case 1:
            // Clear Oscilloscope
            if (PFM_IS_DMA2D_READY()) {
                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
                WRITE_REG(hdma2d.Instance->OCOLR, 0);

                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 0);
                // Output is rgb565 (16 bits) and we fill A8 (8 bits) so we can divide the height by 2
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (50 | (160 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)(fgOscillo_));
                PFM_START_DMA2D();
                currentAction_.actionType = 0;
                setDirtyArea(TFT_OSCILLO_Y, 100);
            }
            break;
        }
        break;
    case TFT_DRAW_ALGO:
        switch (currentAction_.param3) {
        case 0:
            // Clear ALGO ON BackGround
            if (PFM_IS_DMA2D_READY()) {
                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
                WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[COLOR_BLACK]);

                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 0);
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (100 | (80 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)(bgAlgo_));
                PFM_START_DMA2D();

                currentAction_.param3 = 1;
            }
            break;
        case 1:
            this->tftAlgo_->setBufferAdress(bgAlgo_);
            this->tftAlgo_->drawAlgoFromTable(currentAction_.algoTable);

            if (fgAlgoDirty_) {
                currentAction_.param3 = 2;
            } else {
                currentAction_.param3 = 3;
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
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (100 | (40 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)(fgAlgo_));
                PFM_START_DMA2D();

                fgAlgoDirty_ = false;
                currentAction_.param3 = 3;
            }

            break;
        case 3:
            // Copy ALGO from BG to TFT
            if (PFM_IS_DMA2D_READY()) {
                WRITE_REG(hdma2d.Instance->FGCOLR, tftPalette[COLOR_YELLOW]);
                MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_M2M_BLEND);

                MODIFY_REG(hdma2d.Instance->BGOR, DMA2D_OOR_LO, 0);
                MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 240 - 80);
                MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (100 | (80 << DMA2D_POSITION_NLR_PL)));
                WRITE_REG(hdma2d.Instance->FGMAR, (uint32_t)fgAlgo_);
                WRITE_REG(hdma2d.Instance->BGMAR, (uint32_t)(bgAlgo_));
                WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)(tftMemory + TFT_ALGO_Y * 240 + TFT_ALGO_X));
                PFM_START_DMA2D();

                setDirtyArea(TFT_OSCILLO_Y, 100);
                currentAction_.actionType = 0;
            }
            break;
        }

        break;
    case TFT_ERASE_HIGHLIGHT_ALGO_OPERATOR:
        this->tftAlgo_->setFGBufferAdress(fgAlgo_);
        this->tftAlgo_->highlightOperator(false, currentAction_.param1);

        // Switch to draw_algo step 2
        // To copy ALGO to TFT
        fgAlgoDirty_ = true;
        currentAction_.actionType = TFT_DRAW_ALGO;
        currentAction_.param3 = 3;
        break;
    case TFT_HIGHLIGHT_ALGO_OPERATOR:
        this->tftAlgo_->setFGBufferAdress(fgAlgo_);
        this->tftAlgo_->highlightOperator(true, currentAction_.param1);

        // Switch to draw_algo step 2
        // To copy ALGO to TFT
        fgAlgoDirty_ = true;
        currentAction_.actionType = TFT_DRAW_ALGO;
        currentAction_.param3 = 3;
        break;
    case TFT_CLEAR_ALGO_FG:
        // Clear ALGO ON Foreground
        if (PFM_IS_DMA2D_READY()) {
            MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
            WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[COLOR_BLACK]);

            MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 0);
            // 40 and not 80 because fgAlgo is a 8 bits buffer
            MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (100 | (40 << DMA2D_POSITION_NLR_PL)));
            WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)(fgAlgo_));
            PFM_START_DMA2D();

            fgAlgoDirty_ = false;
            currentAction_.actionType = TFT_DRAW_ALGO;
            currentAction_.param3 = 3;
        }
        break;
    case TFT_HIGHTLIGHT_ALGO_IM:
        this->tftAlgo_->setFGBufferAdress(fgAlgo_);
        this->tftAlgo_->highlightIM(true, currentAction_.param1, currentAction_.param4, currentAction_.param5);

        // Switch to draw_algo step 2
        // To copy ALGO to TFT
        fgAlgoDirty_ = true;
        currentAction_.actionType = TFT_DRAW_ALGO;
        currentAction_.param3 = 3;
        break;
    case TFT_ERASE_HIGHTLIGHT_ALGO_IM:
        this->tftAlgo_->setFGBufferAdress(fgAlgo_);
        this->tftAlgo_->highlightIM(false, currentAction_.param1, currentAction_.param4, currentAction_.param5);

        // Switch to draw_algo step 2
        // To copy ALGO to TFT
        fgAlgoDirty_ = true;
        currentAction_.actionType = TFT_DRAW_ALGO;
        currentAction_.param3 = 3;
        break;
    case TFT_HIGHLIGHT_ALGO_FEEDBACK:
        this->tftAlgo_->setFGBufferAdress(fgAlgo_);
        this->tftAlgo_->highlightFeedback(true, currentAction_.param4, currentAction_.param5);

        // Switch to draw_algo step 2
        // To copy ALGO to TFT
        fgAlgoDirty_ = true;
        currentAction_.actionType = TFT_DRAW_ALGO;
        currentAction_.param3 = 3;
        break;
    case TFT_ERASE_HIGHLIGHT_ALGO_FEEDBACK:
        this->tftAlgo_->setFGBufferAdress(fgAlgo_);
        this->tftAlgo_->highlightFeedback(false, currentAction_.param4, currentAction_.param5);

        // Switch to draw_algo step 2
        // To copy ALGO to TFT
        fgAlgoDirty_ = true;
        currentAction_.actionType = TFT_DRAW_ALGO;
        currentAction_.param3 = 3;
        break;
    case TFT_DRAW_FILL_AREA:
        // Fill area of tftMemory defined by params
        if (PFM_IS_DMA2D_READY()) {

            MODIFY_REG(hdma2d.Instance->CR, DMA2D_CR_MODE, DMA2D_R2M);
            WRITE_REG(hdma2d.Instance->OCOLR, tftPalette565[currentAction_.param5]);

            offset = currentAction_.param1 + currentAction_.param2 * 240;

            MODIFY_REG(hdma2d.Instance->OOR, DMA2D_OOR_LO, 240 - currentAction_.param3);
            MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL),
                       (currentAction_.param4 | (currentAction_.param3 << DMA2D_POSITION_NLR_PL)));
            WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)(tftMemory + offset));

            PFM_START_DMA2D();

            setDirtyArea(currentAction_.param2, currentAction_.param4);
            currentAction_.actionType = 0;
        }
        break;
    case TFT_RESTART_REFRESH:
        tftRefreshing_ = true;
        // start from the top
        nextDirtyRow_ = 0;
        currentAction_.actionType = 0;
        break;
    case TFT_PAUSE_REFRESH:
        tftRefreshing_ = false;
        currentAction_.actionType = 0;
        break;
    }
    additionalActions();
}

/*
 * Push tftMemory to physical TFT.
 * Dirty rows are tracked as a 320-bit bitmap; one contiguous run is pushed
 * per call, capped at TFT_MAX_BAND_HEIGHT rows.
 */
bool TftDisplay::pushToTft() {

    if (!tftRefreshing_ || pushToTftInProgress_ || !anyDirty()) {
        return false;
    }

    // Do not start an SPI transfer while DMA2D is still composing tftMemory.
    if (!PFM_IS_DMA2D_READY()) {
        return false;
    }

    uint16_t y0, y1;
    if (!findNextDirtyRun(nextDirtyRow_, y0, y1)) {
        return false;
    }

    ILI9341_Select();

    if (ILI9341_SetAddressWindow(y0, y1 - 1) != HAL_OK) {
        ILI9341_Unselect();
        return false;
    }

    PFM_SET_PIN(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin);

    // Make sure the composed pixels are visible before SPI DMA reads them.
    __DSB();

    uint32_t offset = (uint32_t)y0 * TFT_WIDTH * 2;
    uint32_t size = (uint32_t)(y1 - y0) * TFT_WIDTH * 2;

    pushingY0_ = y0;
    pushingY1_ = y1;
    pushToTftInProgress_ = true;
    clearRows(y0, y1);

    if (HAL_OK != HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, (uint8_t*)tftMemory + offset, (uint16_t)size)) {
        markRows(y0, y1);
        pushToTftInProgress_ = false;
        ILI9341_Unselect();
        return false;
    }

    nextDirtyRow_ = (y1 >= TFT_HEIGHT) ? 0 : y1;
    return true;
}

void TftDisplay::pushToTftFinished() {
    ILI9341_Unselect();
    pushToTftInProgress_ = false;
}

void TftDisplay::pushToTftError() {
    // Restore the in-flight range so it is retried on the next push.
    ILI9341_Unselect();
    markRows(pushingY0_, pushingY1_);
    pushToTftInProgress_ = false;
}

void TftDisplay::setCursor(uint8_t x, uint16_t y) {
    cursorX_ = x * TFT_BIG_CHAR_WIDTH;
    cursorY_ = y * TFT_BIG_CHAR_HEIGHT;
}

void TftDisplay::setCursorSmallChar(uint8_t x, uint16_t y) {
    cursorX_ = x * TFT_SMALL_CHAR_WIDTH;
    cursorY_ = y * TFT_SMALL_CHAR_HEIGHT;
}

void TftDisplay::setCursorInPixel(uint8_t x, uint16_t y) {
    cursorX_ = x;
    cursorY_ = y;
}

void TftDisplay::clearMixerValues() {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_FILL_AREA;
    newAction.param1 = 160;
    newAction.param2 = 75;
    newAction.param3 = 80;
    newAction.param4 = 190;
    newAction.param5 = COLOR_BLACK;
    tftActions_.insert(newAction);
    // Clear info line
    newAction.param1 = 0;
    newAction.param2 = 50;
    newAction.param3 = 240;
    newAction.param4 = 20;
    tftActions_.insert(newAction);
}

void TftDisplay::clearMixerLabels() {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_FILL_AREA;
    newAction.param1 = 0;
    newAction.param2 = 75;
    newAction.param3 = 160;
    newAction.param4 = 190;
    newAction.param5 = COLOR_BLACK;
    tftActions_.insert(newAction);
}

void TftDisplay::clear() {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_FILL_TFT;
    newAction.param5 = COLOR_BLACK;
    tftActions_.insert(newAction);
    bHasJustBeenCleared_ = true;
}

void TftDisplay::print(float f) {
    if (f < 0.0) {
        print('-');
        f = -f;
    } else {
        print(' ');
    }
    if (f < 10.0f) {
        int integer = (int)f;
        print((char)((char)integer + '0'));
        print('.');
        f -= integer;
        int valueTimes100 = (int)(f * 100 + .0005);
        if (valueTimes100 < 10) {
            print("0");
            print(valueTimes100);
        } else {
            print(valueTimes100);
        }
    } else {
        int integer = (int)f;
        print(integer);
        print('.');
        f -= integer;
        print((int)(f * 10));
    }
}

void TftDisplay::printFloatWithOneDecimal(float f) {
    if (f < 10.0f) {
        print('0');
    }
    int integer = (int)f;
    print(integer);
    print('.');
    f -= integer;
    int valueTimes10 = (int)(f * 10 + .0005);
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

void TftDisplay::printSmallChars(const char* str, int length) {
    int i = 0;
    char c = str[i++];
    while (c != '\0' and i <= length) {
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
    while (this->cursorX_ <= 240 - TFT_BIG_CHAR_WIDTH) {
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
        print((char)('0' + buf[i - 1]));
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
        printSmallChar((char)('0' + buf[i - 1]));
    }
}

void TftDisplay::print(char c, TFT_COLOR color, TFT_COLOR bgColor) {
    TFTAction newAction;
    newAction.actionType = TFT_DISPLAY_ONE_CHAR;
    newAction.param1 = cursorX_;
    newAction.param2 = cursorY_;
    if ((c >= 32) && (c <= 32 + 94)) {
        newAction.param3 = c - 32;
    } else {
        newAction.param3 = 35;
    }
    newAction.param4 = color;
    newAction.param5 = bgColor;
    tftActions_.insert(newAction);
    cursorX_ += TFT_BIG_CHAR_WIDTH;
}

void TftDisplay::print(char c) {
    TFTAction newAction;
    newAction.actionType = TFT_DISPLAY_ONE_CHAR;
    newAction.param1 = cursorX_;
    newAction.param2 = cursorY_;
    if ((c >= 32) && (c <= 32 + NUMBER_OF_CHARS)) {
        newAction.param3 = c - 32;
    } else {
        newAction.param3 = 3;
    }
    newAction.param4 = charColor_;
    newAction.param5 = charBackgroundColor_;
    tftActions_.insert(newAction);
    cursorX_ += TFT_BIG_CHAR_WIDTH;
}

void TftDisplay::printSmallChar(char c) {
    TFTAction newAction;
    newAction.actionType = TFT_DISPLAY_ONE_SMALL_CHAR;
    newAction.param1 = cursorX_;
    newAction.param2 = cursorY_;
    if ((c >= 32) && (c < 32 + 97)) {
        newAction.param3 = c - 32;
    } else {
        newAction.param3 = 3;
    }
    newAction.param4 = charColor_;
    newAction.param5 = charBackgroundColor_;
    tftActions_.insert(newAction);
    cursorX_ += TFT_SMALL_CHAR_WIDTH;
}

void TftDisplay::oscilloRrefresh() {
    float oscilloSampleReadPtr = oscilloSamplePeriod_ - oscilloPhaseIndexToUse_;
    bool saturate = false;
    int32_t maxYValue = 0;
    float multiplier = olscilloYScale_ * 50.0f;
    uint32_t total = 0;
    for (int x = 0; x < 160; x++) {
        // Use _usat on value before multiplying ?????
        int8_t oValue = (int8_t)(oscilloFullPeriodSampl_[(int)oscilloSampleReadPtr] * multiplier);
        total += abs(oValue);
        if (unlikely(oValue > 50)) {
            saturate = true;
            oscilloYValue_[x] = 50;
        } else if (unlikely(oValue < -50)) {
            saturate = true;
            oscilloYValue_[x] = -50;
        } else {
            oscilloYValue_[x] = oValue;
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

        oscilloSampleReadPtr += oscilloSampleInc_;
    }

    bool resetTrace = (!oscilloSmoothedValid_ || total == 0);

    // Low-pass the displayed trace per column (alpha = 0.5). Reset on the
    // first frame or when the trace goes flat, so it never fades from zeros.
    if (resetTrace) {
        for (int x = 0; x < OSCILLO_WIDTH; x++) {
            oscilloSmoothedY_[x] = oscilloYValue_[x];
        }
        oscilloSmoothedValid_ = (total > 0);
    } else {
        for (int x = 0; x < OSCILLO_WIDTH; x++) {
            oscilloSmoothedY_[x] = (int8_t)(oscilloSmoothedY_[x] + (oscilloYValue_[x] - oscilloSmoothedY_[x]) / 2);
        }
    }

    // Keep the last frames for the phosphor trail.
    if (resetTrace) {
        for (int h = 0; h < OSCILLO_TRAIL_FRAMES; h++) {
            memcpy(oscilloHistory_[h], oscilloSmoothedY_, OSCILLO_WIDTH);
        }
        oscilloHistoryIndex_ = 0;
    } else {
        memcpy(oscilloHistory_[oscilloHistoryIndex_], oscilloSmoothedY_, OSCILLO_WIDTH);
        oscilloHistoryIndex_ = (oscilloHistoryIndex_ + 1) % OSCILLO_TRAIL_FRAMES;
    }
    uint8_t latestSlot = (oscilloHistoryIndex_ + OSCILLO_TRAIL_FRAMES - 1) % OSCILLO_TRAIL_FRAMES;

    // flatOscillo must be displayed only once
    if (total == 0) {
        if (!flatOscilloAlreadyDisplayed_) {
            total = 1;
            flatOscilloAlreadyDisplayed_ = true;
        }
    } else {
        flatOscilloAlreadyDisplayed_ = false;
    }

    if (total > 0) {
        int indexMiddle = 50 * 160;

        // Draw the older frames first (oldest to newest) as dim points.
#ifdef OSCILLO_TRAIL_FRAMES_NO
        static const uint8_t trailAlpha[5] = {0xff, 0x50, 0x40, 0x30, 0x20};
        static_assert(OSCILLO_TRAIL_FRAMES <= 5, "trailAlpha table too small");
        for (int age = OSCILLO_TRAIL_FRAMES - 1; age >= 1; age--) {
            uint8_t slot = (latestSlot + OSCILLO_TRAIL_FRAMES - age) % OSCILLO_TRAIL_FRAMES;
            uint8_t alpha = trailAlpha[age];
            for (int x = 0; x < OSCILLO_WIDTH; x++) {
                fgOscillo_[indexMiddle + x + oscilloHistory_[slot][x] * 160] = alpha;
            }
        }
#endif

        for (int x = 0; x < 159; x++) {
            if (oscilloSmoothedY_[x] < (oscilloSmoothedY_[x + 1] - 1)) {
                for (int oy = oscilloSmoothedY_[x]; oy < oscilloSmoothedY_[x + 1]; oy++) {
                    fgOscillo_[indexMiddle + x + oy * 160] = 0xff;
                }
            } else if (oscilloSmoothedY_[x] > (oscilloSmoothedY_[x + 1] + 1)) {
                for (int oy = oscilloSmoothedY_[x]; oy > oscilloSmoothedY_[x + 1]; oy--) {
                    fgOscillo_[indexMiddle + x + oy * 160] = 0xff;
                }
            } else {
                fgOscillo_[indexMiddle + x + oscilloSmoothedY_[x] * 160] = 0xff;
            }
        }
        // Ask for tft push and refresh
        TFTAction newAction;
        newAction.actionType = TFT_DRAW_OSCILLO;
        // We don't want oscillo in red when saturation since we have olscilloYScale
        // newAction.param1 = saturate ? 1 : 0;
        newAction.param1 = 0;
        newAction.param3 = 0;
        tftActions_.insert(newAction);

        // Dynamically adjust olscilloYScale
        if (saturate) {
            olscilloYScale_ -= 1.0f;
        } else {
            if (maxYValue < 25 && olscilloYScale_ < 8.0f) {
                olscilloYScale_ += 1.0f;
            }
        }
    }
    // We can start filling again the oscillo buffer
    oscilloDataWritePtr_ = 0;
}

void TftDisplay::fillArea(uint8_t x, uint16_t y, uint8_t width, uint16_t height, uint8_t color) {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_FILL_AREA;
    newAction.param1 = x;
    newAction.param2 = y;
    newAction.param3 = width;
    newAction.param4 = height;
    newAction.param5 = color;
    tftActions_.insert(newAction);
}

void TftDisplay::oscilloNewLowerFrequency(float lf) {
    oscilloSamplePeriod_ = PREENFM_FREQUENCY / lf;
    // The buffer holds two periods; the trace uses one of them.
    if (oscilloSamplePeriod_ > OSCILLO_BUFFER_SIZE / 2) {
        oscilloSamplePeriod_ = OSCILLO_BUFFER_SIZE / 2;
    }
    oscilloSampleInc_ = (float)oscilloSamplePeriod_ / OSCILLO_WIDTH;

    // Re-anchor the display low-pass on a note change (> 5%).
    if (oscilloLowerFrequency_ > 0.0f && (lf > oscilloLowerFrequency_ * 1.05f || lf < oscilloLowerFrequency_ * 0.95f)) {
        oscilloSmoothedValid_ = false;
    }
    oscilloLowerFrequency_ = lf;

    // oscilloPhaseIndexToUse = oscilloSamplePeriod - (oscilloPhaseIndex % ((int)oscilloSamplePeriod + 1));
    int divInt = oscilloPhaseIndex_ / oscilloSamplePeriod_;

    oscilloPhaseIndexToUse_ = oscilloPhaseIndex_ - divInt * oscilloSamplePeriod_;
}

void TftDisplay::oscilloRecord32Samples(float* samples) {
    oscilloPhaseIndex_ += 32.0f;
    // Record two periods so the displayed window is always contiguous.
    if (oscilloDataWritePtr_ < oscilloSamplePeriod_ * 2.0f) {
        for (int r = 0; r < 32; r++) {
            oscilloFullPeriodSampl_[oscilloDataWritePtr_++] = samples[r * 2];
        }
    }
}

void TftDisplay::drawAlgoTable(const uint8_t* algoTable) {
    TFTAction newAction;
    newAction.actionType = TFT_DRAW_ALGO;
    newAction.param3 = 0;
    newAction.algoTable = algoTable;
    tftActions_.insert(newAction);
}

void TftDisplay::clearAlgoFG() {
    TFTAction newAction;
    newAction.actionType = TFT_CLEAR_ALGO_FG;
    tftActions_.insert(newAction);
}

void TftDisplay::highlightOperator(int op) {
    TFTAction newAction;
    newAction.actionType = TFT_HIGHLIGHT_ALGO_OPERATOR;
    newAction.param1 = op;
    newAction.param3 = 0;
    tftActions_.insert(newAction);
}

void TftDisplay::eraseHighlightOperator(int op) {
    TFTAction newAction;
    newAction.actionType = TFT_ERASE_HIGHLIGHT_ALGO_OPERATOR;
    newAction.param1 = op;
    newAction.param3 = 0;
    tftActions_.insert(newAction);
}

void TftDisplay::highlightIM(uint8_t imNum, uint8_t opSource, uint8_t opDest) {
    TFTAction newAction;
    newAction.actionType = TFT_HIGHTLIGHT_ALGO_IM;
    newAction.param1 = imNum;
    newAction.param3 = 0;
    newAction.param4 = opSource;
    newAction.param5 = opDest;
    tftActions_.insert(newAction);
}

void TftDisplay::eraseHighlightIM(uint8_t imNum, uint8_t opSource, uint8_t opDest) {
    TFTAction newAction;
    newAction.actionType = TFT_ERASE_HIGHTLIGHT_ALGO_IM;
    newAction.param1 = imNum;
    newAction.param3 = 0;
    newAction.param4 = opSource;
    newAction.param5 = opDest;
    tftActions_.insert(newAction);
}

void TftDisplay::highlightFeedback(uint8_t imNum, uint8_t opSource, uint8_t opDest) {
    TFTAction newAction;
    newAction.actionType = TFT_HIGHLIGHT_ALGO_FEEDBACK;
    newAction.param1 = imNum;
    newAction.param3 = 0;
    newAction.param4 = opSource;
    newAction.param5 = opDest;
    tftActions_.insert(newAction);
}

void TftDisplay::eraseHighlightFeedback(uint8_t imNum, uint8_t opSource, uint8_t opDest) {
    TFTAction newAction;
    newAction.actionType = TFT_ERASE_HIGHLIGHT_ALGO_FEEDBACK;
    newAction.param1 = imNum;
    newAction.param3 = 0;
    newAction.param4 = opSource;
    newAction.param5 = opDest;
    tftActions_.insert(newAction);
}

void TftDisplay::restartRefreshTft() {
    TFTAction newAction;
    newAction.actionType = TFT_RESTART_REFRESH;
    tftActions_.insert(newAction);
}

void TftDisplay::pauseRefresh() {
    TFTAction newAction;
    newAction.actionType = TFT_PAUSE_REFRESH;
    tftActions_.insert(newAction);
}

void TftDisplay::waitCycle(uint16_t waitCycle) {
    TFTAction newAction;
    newAction.actionType = TFT_WAITCYCLE;
    newAction.param2 = waitCycle;
    tftActions_.insert(newAction);
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
            fillArea(left, y + 19, BUTTON_WIDTH / 2, 2, activeState == 1 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(x, y + 19, BUTTON_WIDTH / 2, 2, activeState == 2 ? COLOR_WHITE : COLOR_GRAY);
            break;
        case 3:
            fillArea(left, y + 19, BUTTON_WIDTH / 3, 2, activeState == 1 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH / 3, y + 19, BUTTON_WIDTH / 3, 2, activeState == 2 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH / 3 * 2, y + 19, BUTTON_WIDTH / 3, 2, activeState == 3 ? COLOR_WHITE : COLOR_GRAY);
            break;
        case 4:
            fillArea(left, y + 19, BUTTON_WIDTH / 4, 2, activeState == 1 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH / 4, y + 19, 18, 2, activeState == 2 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(x, y + 19, BUTTON_WIDTH / 4, 2, activeState == 3 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH / 4 * 3, y + 19, BUTTON_WIDTH / 4, 2, activeState == 4 ? COLOR_WHITE : COLOR_GRAY);
            break;
        case 5:
            fillArea(left, y + 19, BUTTON_WIDTH / 5, 2, activeState == 1 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH / 5, y + 19, 18, 2, activeState == 2 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH / 5 * 2, y + 19, BUTTON_WIDTH / 5, 2, activeState == 3 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH / 5 * 3, y + 19, BUTTON_WIDTH / 5, 2, activeState == 4 ? COLOR_WHITE : COLOR_GRAY);
            fillArea(left + BUTTON_WIDTH / 5 * 4, y + 19, BUTTON_WIDTH / 5, 2, activeState == 5 ? COLOR_WHITE : COLOR_GRAY);
            break;
        default: {
            // Generic indicator for pages with more than 5 states (9W9 TOM/HAT).
            const uint8_t segment = BUTTON_WIDTH / numberOfStates;
            for (uint8_t k = 0; k < numberOfStates; k++) {
                const uint8_t w = (k == numberOfStates - 1) ? (BUTTON_WIDTH - segment * (numberOfStates - 1)) : segment;
                fillArea(left + k * segment, y + 19, w, 2, activeState == k + 1 ? COLOR_WHITE : COLOR_GRAY);
            }
            break;
        }
        }
    } else {
        if (numberOfStates > 0) {
            fillArea(left, y + 20, BUTTON_WIDTH, 1, COLOR_GRAY);
        }
    }

    int len = -1;
    while (label[++len] != 0)
        ;

    x = x - len * TFT_BIG_CHAR_WIDTH / 2;
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
    fillArea(x - BUTTON_WIDTH / 2, y, BUTTON_WIDTH, 21, background);

    int len = -1;
    while (label[++len] != 0)
        ;

    x = x - len * TFT_BIG_CHAR_WIDTH / 2;
    y = y + 1;

    setCharColor(color);
    setCharBackgroundColor(background);
    setCursorInPixel(x, y);
    print(label);
    setCharBackgroundColor(COLOR_BLACK);
}

void TftDisplay::drawButtonBiState(uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, uint8_t currentSelection, uint8_t nulmberOfSelection,
                                   uint8_t buttonState, int8_t baseNumber) {
    y += buttonNumber > 2 ? dyLine2 : 0;

    uint8_t x1 = (buttonNumber % 3) * 80 + 4;

    fillArea(x1, y, BUTTON_WIDTH, 21, COLOR_DARK_BLUE);
    setCharBackgroundColor(COLOR_DARK_BLUE);

    for (int b = 0; b < 2; b++) {
        uint8_t drawnSubButton = baseNumber + b;

        if (drawnSubButton > nulmberOfSelection) {
            continue;
        }

        int x2 = x1 + b * BUTTON_WIDTH / 2;
        if (currentSelection == drawnSubButton) {
            // Current operator
            setCharColor(COLOR_WHITE);
            fillArea(x2, y + 19, BUTTON_WIDTH / 2, 2, COLOR_WHITE);
        } else {
            setCharColor(COLOR_GRAY);
            if (b == buttonState) {
                fillArea(x2, y + 20, BUTTON_WIDTH / 2, 1, COLOR_GRAY);
            }
        }

        if (unlikely(drawnSubButton >= 9)) {
            x2 -= 5;
        }
        setCursorInPixel(x2 + 12, y + 1);
        print(drawnSubButton + 1);
    }
}

void TftDisplay::drawSimpleBorderButton(const char* label, uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, TFT_COLOR color,
                                        TFT_COLOR background) {

    uint8_t x = (buttonNumber % 3) * 80 + 40; // 40 / 120 / 200
    y += buttonNumber > 2 ? dyLine2 : 0;
    fillArea(x - BUTTON_WIDTH / 2, y, BUTTON_WIDTH, 21, background);
    fillArea(x - BUTTON_WIDTH / 2 + 1, y + 1, BUTTON_WIDTH - 2, 21 - 2, COLOR_BLACK);
    setCharBackgroundColor(COLOR_BLACK);

    int len = -1;
    while (label[++len] != 0)
        ;

    x = x - len * TFT_BIG_CHAR_WIDTH / 2;
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
        int integer = (int)(value + .0005f);
        print(integer);
        print('.');
        value -= integer;
        int valueTimes100 = (int)(value * 100.0f + .0005f);
        if (valueTimes100 < 10) {
            print("0");
            print(valueTimes100);
        } else {
            print(valueTimes100);
        }
    } else {
        int integer = (int)(value + .0005f);
        print(integer);
        print('.');
        value -= integer;
        print((int)(value * 10 + .0005f));
    }
}

void TftDisplay::drawLevelMetter(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t pixelPerDb, float volume, bool isComp,
                                 float gr) {
    float maxX = width + (volume + gr) * pixelPerDb;
    if (maxX < 0) {
        maxX = 0;
    }

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
        fillArea(x + maxX, y, width - maxX, height, COLOR_BLACK);
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
        fillArea(x + maxX, y, width - maxX, height, COLOR_BLACK);
    }
}
