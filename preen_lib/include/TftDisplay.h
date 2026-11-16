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

#include "stm32h7xx_hal.h"
#include "RingBuffer.h"
#include "TftAlgo.h"

#define TFTACTION_BUFFER_SIZE 128

enum {
	TFT_PART_HEADER = 0,
	TFT_PART_VALUES,
    TFT_PART_VALUES2,
    TFT_PART_VALUES3,
	TFT_PART_OSCILLO,
    TFT_PART_OSCILLO2,
    TFT_PART_OSCILLO3,
	TFT_PART_BUTTONS
};

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
    TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM
};


#define TFT_PART_HEADER_BITS        0b00000001
#define TFT_PART_VALUES_BITS        0b00001110
#define TFT_PART_OSCILLO_BITS       0b01110000
#define TFT_PART_BUTTONS_BITS       0b10000000
#define TFT_PART_ALL_BITS           0b11111111

#define TFT_NUMBER_OF_PARTS 8


#define RAM_D1_SECTION __attribute__((section(".ram_d1")))
#define DMA2D_POSITION_NLR_PL         (uint32_t)POSITION_VAL(DMA2D_NLR_PL)        /*!< Required left shift to set pixels per lines value */

enum TFT_COLOR {
    COLOR_BLACK = 0,
    COLOR_WHITE,
    COLOR_BLUE,
    COLOR_DARK_BLUE,
    COLOR_CYAN,
    COLOR_YELLOW,
    COLOR_DARK_YELLOW,
    COLOR_ORANGE,
    COLOR_RED,
    COLOR_DARK_RED,
    COLOR_GREEN,
    COLOR_DARK_GREEN,
    COLOR_LIGHT_GREEN,
    COLOR_GRAY,
    COLOR_DARK_GRAY,
    COLOR_LIGHT_GRAY,
    COLOR_MEDIUM_GRAY,
    NUMBER_OF_TFT_COLORS
};

#define TFT_BIG_CHAR_WIDTH 11
#define TFT_BIG_CHAR_HEIGHT 20

#define TFT_SMALL_CHAR_WIDTH 7
#define TFT_SMALL_CHAR_HEIGHT 10


enum TFTActionType {
    clear, charAt
};

struct TFTAction {
    uint8_t actionType = 0;
    uint8_t param1 = 0;
    uint16_t param2 = 0;
    uint8_t param3 = 0;
    uint16_t param4 = 0;
    uint8_t param5 = 0;
};

#define OSCILLO_WIDTH 160
#define OSCILLO_BUFFER_SIZE 1024

#define PFM_IS_DMA2D_READY() (hdma2d.Instance->CR & DMA2D_CR_START) == 0
#define PFM_START_DMA2D() do { __DSB(); hdma2d.Instance->CR |= DMA2D_CR_START; } while (0)



class TftDisplay {
public:
    TftDisplay();
    virtual ~TftDisplay();
    void init(TftAlgo* tftAlgo);

    void tic(bool checkDisplayPower);
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
            TFT_COLOR background = COLOR_DARK_BLUE);
    void drawSimpleButton(const char* label, uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, TFT_COLOR color, TFT_COLOR background);
    void drawSimpleBorderButton(const char* label, uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, TFT_COLOR color, TFT_COLOR background);
    void drawButtonBiState(uint16_t y, uint8_t dyLine2, uint8_t buttonNumber, uint8_t currentSelection, uint8_t nulmberOfSelection, uint8_t buttonState, int8_t baseNumber);


    void clear();
    void clearMixerValues();
    void clearMixerLabels();
    void fillArea(uint8_t x, uint16_t y, uint8_t width, uint16_t height, uint8_t color);
    virtual void clearActions() {
        tftActions_.clear();
    }

    // Oscillo
    void oscilloRrefresh();
    void oscilloNewLowerFrequency(float lf);
    void oscilloRecord32Samples(float *samples);

    void drawAlgo(int algo);
    void drawDx7Algo(int algo);
    bool getDx7Feedback(int algo, uint8_t &opSource, uint8_t &opDest);
    void highlightOperator(int op);
    void eraseHighlightOperator(int op);
    void highlightIM(uint8_t imNum, uint8_t opSource, uint8_t opDest);
    void eraseHighlightIM(uint8_t imNum, uint8_t opSource, uint8_t opDest);
    void highlightFeedback(uint8_t imNum, uint8_t opSource, uint8_t opDest);
    void eraseHighlightFeedback(uint8_t imNum, uint8_t opSource, uint8_t opDest);
    void clearAlgoFG();
    void setCursoraddXY(uint8_t x, uint16_t y) {
        cursorX_ +=x;
        cursorY_ +=y;
    }
    void setCharColor(TFT_COLOR newColor) {
        charColor_ = (uint8_t) newColor;
    }
    void setCharBackgroundColor(TFT_COLOR newColor) {
        charBackgroundColor_ = (uint8_t) newColor;
    }
    int getNumberOfPendingActions() {
        return tftActions_.getCount();
    }

    void printValueWithSpace(int value);
    void printFloatWithSpace(float value);

    void oscilloForceNextDisplay() { flatOscilloAlreadyDisplayed_ = false; }

    bool hasJustBeenCleared() { return bHasJustBeenCleared_; }
    void resetHasJustBeenCleared() { bHasJustBeenCleared_ = false; }

    void drawLevelMetter(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t pixelPerDb, float volume, bool isComp, float gr);

    bool mustBeReset() { return tftMustBeReset_; }
    void reset();

protected:
    int8_t oscilloYValue_[OSCILLO_WIDTH];
    uint16_t *bgOscillo_;
    RingBuffer<TFTAction, TFTACTION_BUFFER_SIZE> tftActions_;
    TFTAction currentAction_;
    uint8_t currentActionStep_;
    TftAlgo *tftAlgo_;

private:
    void oscilloBgDrawOperatorShape(float* waveForm, int size);
    void oscilloBgDrawEnvelope();
    void oscilloBgDrawLfo();
    void oscilloFillWithRand(int randtype);
    void oscilloBgDrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

    uint32_t lastOscilloSaturateTic_;
    uint32_t tftPushMillis_;
    uint32_t tftGetStatusMillis_;
    // adress of the segment digit;
    uint8_t *fgSegmentDigit_;
    uint8_t *fgBigChars_;
    uint8_t *fgSmallChars_;
    uint8_t *fgOscillo_;
    uint8_t *fgAlgo_;
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
    uint16_t *bgColorChar_[NUMBER_OF_TFT_COLORS];
    uint16_t *bgAlgo_;

    // TFT refresh
    bool tftRefreshing_;
    uint16_t areaHeight_[TFT_NUMBER_OF_PARTS];
    uint16_t areaY_[TFT_NUMBER_OF_PARTS];

    bool flatOscilloAlreadyDisplayed_;
    bool bHasJustBeenCleared_;

    bool pushToTftInProgress_;
    uint8_t pushingPart_;
    uint32_t tftDirtyBits_;
    uint8_t part_;

    // 3 bytes to deal with power status
    uint8_t status_[4];
    // Set to true if there is a TFT problem
    bool tftMustBeReset_;
    uint8_t waitCycle_;
};

#endif /* HARDWARE_TFTDISPLAY_H_ */
