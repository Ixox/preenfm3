/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <dot> hosxe (at) g m a i l <dot> com)
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

#ifndef HARDWARE_TFTALGO_H_
#define HARDWARE_TFTALGO_H_

#define PREENFM_FREQUENCY 47916.0f
#define ABS(X)            ((X) > 0 ? (X) : -(X))

// Algorithm operator positions are stored in fixed point (position * 2) so
// that half-cell positions can be represented in the uint8_t tables.
#define POS(x) ((uint8_t)((x) * 2))

#include "stm32h7xx_hal.h"
// #include "Common.h"

enum {
    OPERATOR = 1,
    IM,
    IMSYNC,
    MIX,
    OPERATORSYNC,
    FEEDBACK,
    END,
};

struct ModulationIndex {
    unsigned int source : 4;
    unsigned int destination : 4;
};

class TftAlgo {
public:
    TftAlgo();
    virtual ~TftAlgo();

    void setBufferAdress(uint16_t* bufferAdress);
    void setFGBufferAdress(uint8_t* bufferAdress);
    void drawAlgoFromTable(const uint8_t* algoInfo);
    void highlightIM(bool draw, uint8_t imNum, uint8_t opSource, uint8_t opDest);
    void highlightFeedback(bool draw, uint8_t opSource, uint8_t opDest);
    void highlightOperator(bool draw, uint8_t opNum);
    static void drawSegmentGlyph(uint16_t* buffer, uint16_t width, uint16_t height, int16_t x, int16_t y, uint8_t glyph, uint16_t color);

private:
    void drawLine(uint8_t mode, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
    void setColor(uint16_t col);
    void drawNumber(int x, int y, int opNum);
    void drawOperator(uint8_t opNum);
    void drawIM(uint8_t mode, uint8_t imNum, uint8_t opSource, uint8_t opDest);
    void drawFeedback(uint8_t mode, uint8_t opSource, uint8_t opDest);
    void drawCarrierArrow(uint8_t opNum);
    void computeLayout();
    void plot(uint8_t mode, int16_t x, int16_t y);
    void fillRect(int16_t x, int16_t y, int16_t width, int16_t height);
    void hLine(int16_t x0, int16_t x1, int16_t y);
    void vLine(int16_t y0, int16_t y1, int16_t x);

    float operatorPosition_[6];
    int16_t operatorX_[6];
    int16_t operatorY_[6];
    uint8_t operatorMix_[6];
    uint8_t operatorFeedback_[6];
    uint8_t imLinkSource_[8];
    uint8_t imLinkDest_[8];
    uint8_t imLinkCount_;
    uint16_t* buffer_;
    uint8_t* fgBuffer_;
    uint16_t color_;
};

#endif /* HARDWARE_TFTALGO_H_ */