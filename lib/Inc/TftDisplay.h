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
	TFT_PART_OSCILLO,
    TFT_PART_OSCILLO2,
	TFT_PART_BUTTONS
};

#define TFT_PART_HEADER_BITS        0b000001
#define TFT_PART_VALUES_BITS        0b000110
#define TFT_PART_OSCILLO_BITS       0b011000
#define TFT_PART_BUTTONS_BITS       0b100000
#define TFT_PART_ALL_BITS           0b111111

#define TFT_NUMBER_OF_PARTS 6


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

#define TFT_NUMBER_OF_WAVEFORM_EXT 14
#define TFT_DRAW_ENVELOPPE TFT_NUMBER_OF_WAVEFORM_EXT+1
#define TFT_DRAW_LFO TFT_DRAW_ENVELOPPE+1

struct WaveFormExt {
    float* waveForms;
    int size;
};

enum TFTActionType {
    clear, charAt
};

struct TFTAction {
    uint8_t actionType :4;
    uint8_t param1;
    uint16_t param2;
    uint8_t param3;
    uint16_t param4;
    uint8_t param5;
};

#define OSCILLO_WIDTH 160
#define OSCILLO_BUFFER_SIZE 1024

class TftDisplay {
public:
    TftDisplay();
    virtual ~TftDisplay();
    void init(TftAlgo* tftAlgo);
    void initWaveFormExt(int index, float* waveform, int size);

    void tic();
    bool pushToTft();
    void pushToTftFinished();

    void setCursor(uint8_t x, uint16_t y);
    void setCursorSmallChar(uint8_t x, uint16_t y);
    void setCursorInPixel(uint8_t x, uint16_t y);
    void pauseRefresh();
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

    void clear();
    void clearMixerValues();
    void clearMixerLabels();
    void fillArea(uint8_t x, uint16_t y, uint8_t width, uint16_t height, uint8_t color);
    void clearActions() {
        tftActions.clear();
    }

    // Oscillo
    void oscilloRrefresh();
    void oscilloNewLowerFrequency(float lf);
    void oscilloRecord32Samples(float *samples);

    void drawAlgo(int algo);
    void highlightOperator(int algo, int op);
    void highlightIM(uint8_t imNum, uint8_t opSource, uint8_t opDest);
    void eraseHighlightIM(uint8_t imNum, uint8_t opSource, uint8_t opDest);

    void setCharColor(TFT_COLOR newColor) {
        charColor = (uint8_t) newColor;
    }
    void setCharBackgroundColor(TFT_COLOR newColor) {
        charBackgroundColor = (uint8_t) newColor;
    }
    int getNumberOfPendingActions() {
        return tftActions.getCount();
    }

    void printValueWithSpace(int value);
    void printFloatWithSpace(float value);

    void oscilloBgActionOperatorShape(int wfNumber);
    void oscilloBgActionEnvelope();
    void oscilloBgActionLfo();
    void oscilloBgActionClear();

    void oscilloBgSetEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL);
    void oscilloBgSetLfoEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL);
    void oscilloBgSetLfo(float shape, float freq, float kSyn, float bias, float phase);
    void oscilloForceNextDisplay() { flatOscilloAlreadyDisplayed = false; }

    bool hasJustBeenCleared() { return bHasJustBeenCleared; }
    void resetHasJustBeenCleared() { bHasJustBeenCleared = false; }

    void drawLevelMetter(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t pixelPerDb, float volume, bool isComp, float gr);

private:
    void oscilloBgDrawOperatorShape(float* waveForm, int size);
    void oscilloBgDrawEnvelope();
    void oscilloBgDrawLfo();
    void oscilloBgDrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

    RingBuffer<TFTAction, TFTACTION_BUFFER_SIZE> tftActions;
    TFTAction currentAction;
    uint8_t currentActionStep;
    uint32_t lastOscilloSaturateTic;
    uint32_t tftPushMillis;
    // adress of the segment digit;
    uint8_t *fgSegmentDigit;
    uint8_t *fgBigChars;
    uint8_t *fgSmallChars;
    uint8_t *fgOscillo;
    uint8_t *fgAlgo;
    uint8_t cursorX;
    uint16_t cursorY;
    bool fgAlgoDirty;

    int8_t oscilloYValue[OSCILLO_WIDTH];
    float oscilloSamplePeriod;
    float oscilloLowerFrequency;
    float oscilloSampleInc;
//	float oscilloSampleReadPtr;
    int oscilloDataWritePtr;
    float oscilloPhaseIndex;
    uint32_t oscilloPhaseIndexToUse;
    bool oscilloIsClean;
    float olscilloYScale;

    float oscilloFullPeriodSampl[OSCILLO_BUFFER_SIZE];
    TftAlgo *tftAlgo;
    uint8_t charBackgroundColor, charColor;
    uint16_t *bgColorChar[NUMBER_OF_TFT_COLORS];
    uint16_t *bgOscillo, *bgAlgo;

    // TFT refresh
    bool tftRefreshing;
    uint16_t areaHeight[TFT_NUMBER_OF_PARTS];
    uint16_t areaY[TFT_NUMBER_OF_PARTS];

    WaveFormExt waveForm[TFT_NUMBER_OF_WAVEFORM_EXT];
    float oscilParams1[6];
    float oscilParams2[6];

    bool flatOscilloAlreadyDisplayed;
    bool bHasJustBeenCleared;

    bool pushToTftInProgress;
    uint32_t tftDirtyBits;
    uint8_t part;
};

#endif /* HARDWARE_TFTDISPLAY_H_ */
