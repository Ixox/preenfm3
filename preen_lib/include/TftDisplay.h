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

#ifndef HARDWARE_TFTDISPLAY_H_
#define HARDWARE_TFTDISPLAY_H_

#include "RingBuffer.h"
#include "TftAlgo.h"
#include "stm32h7xx_hal.h"

#define TFTACTION_BUFFER_SIZE    128
#define TFT_MAX_ACTIONS_PER_LOOP 8

enum {
    TFT_NOP = 0,
    TFT_DISPLAY_ONE_CHAR,
    TFT_DISPLAY_ONE_SMALL_CHAR,
    TFT_DRAW_FILL_TFT,
    TFT_DRAW_OSCILLO,
    TFT_DRAW_ALGO,
    TFT_HIGHLIGHT_ALGO_OPERATOR,
    TFT_ERASE_HIGHLIGHT_ALGO_OPERATOR,
    TFT_HIGHTLIGHT_ALGO_IM,
    TFT_ERASE_HIGHTLIGHT_ALGO_IM,
    TFT_CLEAR_ALGO_FG,
    TFT_DRAW_FILL_AREA,
    TFT_RESTART_REFRESH,
    TFT_PAUSE_REFRESH,
    TFT_WAITCYCLE,
    TFT_STANDARD_LAST_ACTION = TFT_WAITCYCLE,
    TFT_HIGHLIGHT_ALGO_FEEDBACK,
    TFT_ERASE_HIGHLIGHT_ALGO_FEEDBACK,
    TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM,
    TFT_DRAW_LOGO
};

#define TFT_WIDTH           240
#define TFT_HEIGHT          320
#define TFT_DIRTY_WORDS     ((TFT_HEIGHT + 31) / 32)
#define TFT_MAX_BAND_HEIGHT 128

#define RAM_D1_SECTION        __attribute__((section(".ram_d1")))
#define DMA2D_POSITION_NLR_PL (uint32_t)POSITION_VAL(DMA2D_NLR_PL) /*!< Required left shift to set pixels per lines value */

enum TFT_COLOR {
    COLOR_NONE = -1,
    // Neutral
    COLOR_BLACK = 0,
    COLOR_WHITE,
    COLOR_CREAM,
    COLOR_GRAY_DARK,
    COLOR_GRAY,
    COLOR_GRAY_MEDIUM,
    COLOR_GRAY_LIGHT,

    // Red
    COLOR_RED_DARK,
    COLOR_RED,
    COLOR_RED_LIGHT,
    COLOR_BURNT_PEACH,

    // Orange
    COLOR_ORANGE_DARK,
    COLOR_ORANGE,
    COLOR_ORANGE_LIGHT,
    COLOR_APRICOT_CREAM,

    // Yellow
    COLOR_YELLOW_DARK,
    COLOR_YELLOW,
    COLOR_YELLOW_LIGHT,

    // Green
    COLOR_GREEN_VERY_DARK,
    COLOR_GREEN_DARK,
    COLOR_GREEN,
    COLOR_GREEN_LIGHT,
    COLOR_MUTED_TEAL,

    // Cyan
    COLOR_CYAN_DARK,
    COLOR_CYAN,
    COLOR_CYAN_LIGHT,

    // Blue
    COLOR_BLUE_VERY_DARK,
    COLOR_BLUE_DARK,
    COLOR_BLUE,
    COLOR_BLUE_LIGHT,

    // Purple
    COLOR_PURPLE_DARK,
    COLOR_PURPLE,
    COLOR_PURPLE_LIGHT,

    // Magenta
    COLOR_MAGENTA_DARK,
    COLOR_MAGENTA,
    COLOR_MAGENTA_LIGHT,

    NUMBER_OF_TFT_COLORS
};

#define TFT_BIG_CHAR_WIDTH  11
#define TFT_BIG_CHAR_HEIGHT 20

#define TFT_SMALL_CHAR_WIDTH  7
#define TFT_SMALL_CHAR_HEIGHT 10

enum TFTActionType { clear, charAt };

// Flash-resident L4 logo: 2 pixels per byte, MSB first, width must be even.
// The CLUT holds 16 ARGB8888 entries (the DMA2D has no RGB565 CLUT mode).
struct TftLogo {
    const uint8_t* data;
    const uint32_t* clut;
    uint8_t width;
    uint8_t height;
};

struct TFTAction {
    uint8_t actionType = 0;
    uint8_t param1 = 0;
    uint16_t param2 = 0;
    uint8_t param3 = 0;
    uint16_t param4 = 0;
    uint8_t param5 = 0;
    union {
        const uint8_t* algoTable = nullptr;
        const TftLogo* logo;
    };
};

#define OSCILLO_WIDTH 160
// Two periods, so the displayed trace never has to wrap inside the buffer.
#define OSCILLO_BUFFER_SIZE 2048
// Stored frames for the phosphor trail (1 = no trail; max 5 with trailAlpha).
#define OSCILLO_TRAIL_FRAMES 3
// Fill baseline in Y units. 0 = centre line, 49 = lowest visible row.
#define OSCILLO_FILL_BASELINE_Y 49

#define PFM_IS_DMA2D_READY() (hdma2d.Instance->CR & DMA2D_CR_START) == 0
#define PFM_START_DMA2D()                                                                                                                  \
    do {                                                                                                                                   \
        __DSB();                                                                                                                           \
        hdma2d.Instance->CR |= DMA2D_CR_START;                                                                                             \
    } while (0)

class TftDisplay {
public:
    TftDisplay();
    virtual ~TftDisplay();
    void init(TftAlgo* tftAlgo);

    void ticTimer(bool checkDisplayPower);
    void processActions(uint8_t maxSteps);
    virtual void additionalActions() {};

    bool pushToTft();
    void pushToTftFinished();
    void pushToTftError();

    void setCursor(uint8_t x, uint16_t y);
    void setCursorSmallChar(uint8_t x, uint16_t y);
    void setCursorInPixel(uint8_t x, uint16_t y);
    void pauseRefresh();
    void waitCycle(uint16_t waitCycle);
    void restartRefreshTft();

    void print(const char* str);
    bool print(const char* str, int length);
    void print(const char* str, TFT_COLOR color, TFT_COLOR bgColor);
    void printSpaceTillEndOfLine();
    void print(int number);
    void print(char c);
    void printSmallChars(const char* str);
    void printSmallChars(const char* str, int length);
    void printSmallChar(char c);
    void printSmallChar(int number);
    void print(char c, TFT_COLOR color, TFT_COLOR bgColor);
    void printFloatWithOneDecimal(float f);
    void print(float f);
    void setDirtyArea(uint16_t y, uint16_t height);

    void drawButton(const char* label, uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, uint8_t numberOfStates, uint8_t activeState,
                    TFT_COLOR background = COLOR_BLUE_DARK, TFT_COLOR textColor = COLOR_WHITE, TFT_COLOR textColorInactive = COLOR_GRAY,
                    TFT_COLOR indicatorActive = COLOR_WHITE, TFT_COLOR indicatorInactive = COLOR_GRAY);
    void drawSimpleButton(const char* label, uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, TFT_COLOR color, TFT_COLOR background);
    void drawSimpleBorderButton(const char* label, uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, TFT_COLOR color,
                                TFT_COLOR background);
    void drawButtonBiState(uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, uint8_t currentSelection, uint8_t nulmberOfSelection,
                           uint8_t buttonState, int8_t baseNumber, TFT_COLOR background = COLOR_BLUE_DARK,
                           TFT_COLOR textColor = COLOR_WHITE, TFT_COLOR textColorInactive = COLOR_GRAY,
                           TFT_COLOR indicatorActive = COLOR_WHITE, TFT_COLOR indicatorInactive = COLOR_GRAY, bool filled = true);

    void clear(TFT_COLOR color = COLOR_BLACK);
    void clearMixerValues();
    void clearMixerLabels();
    void fillArea(uint8_t x, uint16_t y, uint8_t width, uint16_t height, uint8_t color);
    virtual void clearActions() {
        tftActions_.clear();
    }

    // Oscillo
    void oscilloRrefresh();
    void oscilloNewLowerFrequency(float lf);
    void oscilloRecord32Samples(float* samples);
    void setOscilloPosition(uint8_t x, uint8_t y);
    void setAlgoPosition(uint8_t x, uint8_t y);

    void drawAlgoTable(const uint8_t* algoTable);
    void drawLogo(const TftLogo* logo);
    void drawLogoAt(const TftLogo* logo, uint8_t x, uint16_t y);
    void highlightOperator(int op);
    void eraseHighlightOperator(int op);
    void highlightIM(uint8_t imNum, uint8_t opSource, uint8_t opDest);
    void eraseHighlightIM(uint8_t imNum, uint8_t opSource, uint8_t opDest);
    void highlightFeedback(uint8_t imNum, uint8_t opSource, uint8_t opDest);
    void eraseHighlightFeedback(uint8_t imNum, uint8_t opSource, uint8_t opDest);
    void clearAlgoFG();
    void setCursoraddXY(uint8_t x, uint16_t y) {
        cursorX_ += x;
        cursorY_ += y;
    }
    void setCharColor(TFT_COLOR newColor) {
        charColor_ = (uint8_t)newColor;
    }
    void setCharBackgroundColor(TFT_COLOR newColor) {
        charBackgroundColor_ = (uint8_t)newColor;
    }
    int getNumberOfPendingActions() {
        return tftActions_.getCount();
    }

    void printValueWithSpace(int value);
    void printFloatWithSpace(float value);

    void oscilloForceNextDisplay() {
        flatOscilloAlreadyDisplayed_ = false;
        oscilloSmoothedValid_ = false;
    }

    bool hasJustBeenCleared() {
        return bHasJustBeenCleared_;
    }
    void resetHasJustBeenCleared() {
        bHasJustBeenCleared_ = false;
    }

    void drawLevelMetter(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t pixelPerDb, float volume, bool isComp, float gr,
                         float thresholdDb);

    bool mustBeReset() {
        return tftMustBeReset_;
    }
    void reset();

protected:
    int8_t oscilloYValue_[OSCILLO_WIDTH];
    int8_t oscilloSmoothedY_[OSCILLO_WIDTH];
    int8_t oscilloHistory_[OSCILLO_TRAIL_FRAMES][OSCILLO_WIDTH];
    uint8_t oscilloHistoryIndex_;
    bool oscilloSmoothedValid_;
    uint16_t* bgOscillo_;
    RingBuffer<TFTAction, TFTACTION_BUFFER_SIZE> tftActions_;
    TFTAction currentAction_;
    uint8_t currentActionStep_;
    TftAlgo* tftAlgo_;

private:
    void oscilloBgDrawOperatorShape(float* waveForm, int size);
    void oscilloBgDrawEnvelope();
    void oscilloBgDrawLfo();
    void oscilloFillWithRand(int randtype);
    void oscilloBgDrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

    void processActionStep();
    void markRows(uint16_t y0, uint16_t y1);
    void clearRows(uint16_t y0, uint16_t y1);
    void markAllDirty();
    bool anyDirty() const;
    bool findNextDirtyRun(uint16_t startRow, uint16_t& y0, uint16_t& y1) const;

    uint32_t lastOscilloSaturateTic_;
    uint32_t tftGetStatusMillis_;
    uint8_t* fgBigChars_;
    uint8_t* fgSmallChars_;
    uint8_t* fgOscillo_;
    uint8_t* fgAlgo_;
    uint8_t oscilloX_ = 0;
    uint8_t oscilloY_ = 160;
    uint8_t algoX_ = 160;
    uint8_t algoY_ = 160;
    uint8_t cursorX_;
    uint16_t cursorY_;
    bool fgAlgoDirty_;

    float oscilloSamplePeriod_;
    float oscilloLowerFrequency_;
    float oscilloSampleInc_;
    //	float oscilloSampleReadPtr;
    int oscilloDataWritePtr_;
    float oscilloPhaseIndex_;
    uint32_t oscilloPhaseIndexToUse_;
    float olscilloYScale_;

    float oscilloFullPeriodSampl_[OSCILLO_BUFFER_SIZE];
    uint8_t charBackgroundColor_, charColor_;
    uint8_t* bgCharAlpha_;
    uint16_t* bgAlgo_;

    // TFT refresh
    bool tftRefreshing_;
    uint32_t dirtyRows_[TFT_DIRTY_WORDS];
    uint16_t nextDirtyRow_;

    bool flatOscilloAlreadyDisplayed_;
    bool bHasJustBeenCleared_;

    volatile bool pushToTftInProgress_;
    uint16_t pushingY0_;
    uint16_t pushingY1_;

    // 3 bytes to deal with power status
    uint8_t status_[4];
    // Set to true if there is a TFT problem
    bool tftMustBeReset_;
    uint8_t waitCycle_;
};

#endif /* HARDWARE_TFTDISPLAY_H_ */
