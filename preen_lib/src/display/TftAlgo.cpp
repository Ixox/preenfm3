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

#include "TftAlgo.h"
#include "Common.h"
#include "fonts.h"
#include "stm32h7xx_hal.h"

#define RGB565_WHITE     0xffff
#define RGB565_RED       0x00f8
#define RGB565_YELLOW    0xe0ff
#define RGB565_PURPLE    0x14a0
#define RGB565_CARRIER   0xd824
#define RGB565_MODULATOR 0x5023

#define ALGO_W      80
#define ALGO_H      100
#define ALGO_CELL   18
#define ALGO_MARGIN 4

static const uint8_t segmentColumnX[5] = {0, 2, 3, 4, 6};
static const uint8_t segmentRowY[9] = {0, 2, 3, 4, 6, 8, 9, 10, 12};

// Feedback loop sampled from the approved drawing, relative to the operator
// top-left corner. Only self-feedback operators use this template.
static const int8_t loopY_[18] = {-7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
static const char* const loopRow_[18] = {
    ".............RRRR.....", "..........RRRRRRRRRR..", ".........RRRR....RRRR.", ".........RR........RRR", ".......RRRRR........RR",
    "........RRR.........RR", ".........R..........RR", "....................RR", "....................RR", "....................RR",
    "....................RR", "....................RR", "....................RR", "...................RRR", "...................RRR",
    "...................RR.", "..................RRR.", "..................RR..",
};

static uint16_t stored565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t color = (uint16_t)(((uint16_t)(r & 0xf8) << 8) | ((uint16_t)(g & 0xfc) << 3) | (b >> 3));
    return (uint16_t)((color >> 8) | (color << 8));
}

static uint16_t shade565(uint8_t r, uint8_t g, uint8_t b, float factor) {
    return stored565((uint8_t)(r * factor + 0.5f), (uint8_t)(g * factor + 0.5f), (uint8_t)(b * factor + 0.5f));
}

// Round to nearest, ties to even (same as Python's round() used by the
// preview tool).
static int roundHalfEven(float value) {
    int integer = (int)value;
    float fraction = value - (float)integer;
    if (fraction > 0.5f) {
        return integer + 1;
    }
    if (fraction < 0.5f) {
        return integer;
    }
    return (integer & 1) ? integer + 1 : integer;
}

static int spreadHorizontal(int index, int count) {
    if (count <= 1) {
        return (ALGO_W - ALGO_CELL) / 2;
    }
    float available = (float)(ALGO_W - 2 * ALGO_MARGIN);
    float pitch = (available - ALGO_CELL) / (float)(count - 1);
    if (pitch > ALGO_CELL * 2.0f) {
        pitch = ALGO_CELL * 2.0f;
    }
    float span = (count - 1) * pitch + ALGO_CELL;
    float start = (ALGO_W - span) * 0.5f;
    return roundHalfEven(start + index * pitch);
}

static int spreadVertical(int index, int count, int topReserve, int bottomReserve) {
    float available = (float)(ALGO_H - topReserve - bottomReserve);
    if (count <= 1) {
        return roundHalfEven(topReserve + (available - ALGO_CELL) * 0.5f);
    }
    float pitch = (available - ALGO_CELL) / (float)(count - 1);
    if (pitch > ALGO_CELL * 2.0f) {
        pitch = ALGO_CELL * 2.0f;
    }
    float span = (count - 1) * pitch + ALGO_CELL;
    float start = topReserve + (available - span) * 0.5f;
    return roundHalfEven(start + index * pitch);
}

TftAlgo::TftAlgo() : imLinkCount_(0), buffer_(0), fgBuffer_(0), color_(0) {
    for (int i = 0; i < 6; i++) {
        operatorPosition_[i] = 0.0f;
        operatorX_[i] = 0;
        operatorY_[i] = 0;
        operatorMix_[i] = OPERATOR;
        operatorFeedback_[i] = 0;
    }
}

TftAlgo::~TftAlgo() {
}

void TftAlgo::setBufferAdress(uint16_t* bufferAdress) {
    buffer_ = bufferAdress;
}

void TftAlgo::setFGBufferAdress(uint8_t* bufferAdress) {
    fgBuffer_ = bufferAdress;
}

void TftAlgo::setColor(uint16_t col) {
    color_ = col;
}

void TftAlgo::plot(uint8_t mode, int16_t x, int16_t y) {
    if ((uint16_t)x >= ALGO_W || (uint16_t)y >= ALGO_H) {
        return;
    }
    int offset = (int)x + (int)y * ALGO_W;
    switch (mode) {
    case 1:
        buffer_[offset] = color_;
        break;
    case 2:
        fgBuffer_[offset] = 0xff;
        break;
    case 3:
        fgBuffer_[offset] = 0x00;
        break;
    }
}

void TftAlgo::hLine(int16_t x0, int16_t x1, int16_t y) {
    if (x0 > x1) {
        int16_t tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    for (int16_t x = x0; x <= x1; x++) {
        plot(1, x, y);
    }
}

void TftAlgo::vLine(int16_t y0, int16_t y1, int16_t x) {
    if (y0 > y1) {
        int16_t tmp = y0;
        y0 = y1;
        y1 = tmp;
    }
    for (int16_t y = y0; y <= y1; y++) {
        plot(1, x, y);
    }
}

void TftAlgo::fillRect(int16_t x, int16_t y, int16_t width, int16_t height) {
    for (int16_t yy = y; yy < y + height; yy++) {
        hLine(x, x + width - 1, yy);
    }
}

/*
 * @param mode : 1=BG, 2=FG draw, 3=FG erase
 */
void TftAlgo::drawLine(uint8_t mode, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    int dx = ABS(x1 - x0);
    int dy = ABS(y1 - y0);
    int stepX = (x0 < x1) ? 1 : -1;
    int stepY = (y0 < y1) ? 1 : -1;
    int offsetX = 0;
    int offsetY = 0;

    if (dx >= dy) {
        offsetY = 1;
    } else {
        offsetX = 1;
    }

    int err = dx - dy;
    while (true) {
        plot(mode, x0, y0);
        plot(mode, x0 + offsetX, y0 + offsetY);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int err2 = 2 * err;
        if (err2 > -dy) {
            err -= dy;
            x0 += stepX;
        }
        if (err2 < dx) {
            err += dx;
            y0 += stepY;
        }
    }
}

void TftAlgo::drawSegmentGlyph(uint16_t* buffer, uint16_t width, uint16_t height, int16_t x, int16_t y, uint8_t glyph, uint16_t color) {
    const uint8_t* digit = &segmentFont[glyph * 9];
    for (int row = 0; row < 9; row++) {
        uint8_t line = digit[row];
        for (int col = 0; col < 5; col++) {
            if (((line >> (7 - col)) & 1) != 0) {
                int px = x + segmentColumnX[col];
                int py = y + segmentRowY[row];
                for (int dy = 0; dy < 2; dy++) {
                    int yy = py + dy;
                    if (yy < 0 || yy >= (int)height) {
                        continue;
                    }
                    for (int dx = 0; dx < 2; dx++) {
                        int xx = px + dx;
                        if (xx >= 0 && xx < (int)width) {
                            buffer[xx + yy * width] = color;
                        }
                    }
                }
            }
        }
    }
}

void TftAlgo::drawNumber(int x, int y, int opNum) {
    drawSegmentGlyph(buffer_, ALGO_W, ALGO_H, x, y, opNum, RGB565_YELLOW);
}

void TftAlgo::drawOperator(uint8_t opNum) {
    int x = operatorX_[opNum - 1];
    int y = operatorY_[opNum - 1];

    uint8_t r = 33;
    uint8_t g = 105;
    uint8_t b = 128;
    if (operatorMix_[opNum - 1] == MIX) {
        r = 37;
        g = 155;
        b = 194;
    } else if (operatorMix_[opNum - 1] == OPERATORSYNC) {
        r = 160;
        g = 0;
        b = 160;
    }

    uint16_t color = stored565(r, g, b);
    uint16_t inner = shade565(r, g, b, 0.96f);
    uint16_t dark = shade565(r, g, b, 0.086f);

    // Outer 2px ring, corners chamfered.
    setColor(color);
    hLine(x + 2, x + ALGO_CELL - 3, y);
    hLine(x + 2, x + ALGO_CELL - 3, y + ALGO_CELL - 1);
    vLine(y + 2, y + ALGO_CELL - 3, x);
    vLine(y + 2, y + ALGO_CELL - 3, x + ALGO_CELL - 1);

    // Inner 1px ring, square.
    setColor(inner);
    hLine(x + 1, x + ALGO_CELL - 2, y + 1);
    hLine(x + 1, x + ALGO_CELL - 2, y + ALGO_CELL - 2);
    vLine(y + 1, y + ALGO_CELL - 2, x + 1);
    vLine(y + 1, y + ALGO_CELL - 2, x + ALGO_CELL - 2);

    // One extra inner pixel in each corner of the black interior.
    plot(1, x + 2, y + 2);
    plot(1, x + ALGO_CELL - 3, y + 2);
    plot(1, x + 2, y + ALGO_CELL - 3);
    plot(1, x + ALGO_CELL - 3, y + ALGO_CELL - 3);

    if (operatorFeedback_[opNum - 1]) {
        setColor(dark);
        plot(1, x, y);
        plot(1, x + 1, y);
        plot(1, x, y + 1);
    }
}

void TftAlgo::highlightOperator(bool draw, uint8_t opNum) {
    if (opNum >= 6 || operatorPosition_[opNum] <= 0.0f) {
        return;
    }

    uint8_t mode = draw ? 2 : 3;
    int x = operatorX_[opNum] - 2;
    int y = operatorY_[opNum] - 2;
    for (int i = 0; i < ALGO_CELL + 4; i++) {
        plot(mode, x + i, y);
        plot(mode, x + i, y + ALGO_CELL + 3);
        plot(mode, x, y + i);
        plot(mode, x + ALGO_CELL + 3, y + i);
    }
}

void TftAlgo::drawIM(uint8_t mode, uint8_t imNum, uint8_t opSource, uint8_t opDest) {
    (void)imNum;

    int16_t xSource = operatorX_[opSource - 1] + ALGO_CELL / 2;
    int16_t ySource = operatorY_[opSource - 1] + ALGO_CELL;
    int16_t xDest = operatorX_[opDest - 1] + ALGO_CELL / 2;
    int16_t yDest = operatorY_[opDest - 1] - 1;

    drawLine(mode, xSource, ySource, xDest, yDest);
}

// Draw the IM on
void TftAlgo::highlightIM(bool draw, uint8_t imNum, uint8_t opSource, uint8_t opDest) {
    drawIM(draw ? 2 : 3, imNum, opSource, opDest);
}

void TftAlgo::drawFeedback(uint8_t mode, uint8_t opSource, uint8_t opDest) {
    int x = operatorX_[opSource - 1];
    int y = operatorY_[opSource - 1];

    if (opSource == opDest) {
        for (int row = 0; row < 18; row++) {
            const char* line = loopRow_[row];
            for (int offset = 0; line[offset] != '\0'; offset++) {
                if (line[offset] == 'R') {
                    plot(mode, x + offset, y + loopY_[row]);
                }
            }
        }
        return;
    }

    int half = ALGO_CELL / 2;
    int destX = operatorX_[opDest - 1];
    int destY = operatorY_[opDest - 1];
    int startX = x + ALGO_CELL;
    int startY = y + half;
    int topY = destY - 4;
    int corridor = x + ALGO_CELL + 2;

    drawLine(mode, startX, startY, corridor, startY);
    drawLine(mode, corridor, startY, corridor, topY + 2);
    drawLine(mode, corridor, topY + 2, corridor - 2, topY);
    drawLine(mode, corridor - 2, topY, destX + half, topY);
    drawLine(mode, destX + half, topY, destX + half, destY);
}

void TftAlgo::highlightFeedback(bool draw, uint8_t opSource, uint8_t opDest) {
    drawFeedback(draw ? 2 : 3, opSource, opDest);
}

void TftAlgo::drawCarrierArrow(uint8_t opNum) {
    int x = operatorX_[opNum - 1];
    int y = operatorY_[opNum - 1];

    setColor(RGB565_CARRIER);
    for (int row = 0; row < 4; row++) {
        int count = 8 - row * 2;
        int left = x + (ALGO_CELL - 8) / 2 + row;
        hLine(left, left + count - 1, y + ALGO_CELL + row);
    }
}

void TftAlgo::computeLayout() {
    bool usedColumn[3] = {false, false, false};
    bool usedRow[4] = {false, false, false, false};
    int columnOf[6];
    int rowOf[6];
    float fractionOf[6];

    for (int op = 0; op < 6; op++) {
        columnOf[op] = -1;
        rowOf[op] = -1;
        fractionOf[op] = 0.0f;
        float position = operatorPosition_[op];
        if (position <= 0.0f) {
            continue;
        }
        float grid = position - 1.0f;
        int integer = (int)grid;
        int column = integer % 3;
        int row = integer / 3;
        float fraction = grid - (float)integer;
        columnOf[op] = column;
        rowOf[op] = row;
        fractionOf[op] = fraction;
        usedColumn[column] = true;
        if (fraction > 0.0f && column + 1 < 3) {
            usedColumn[column + 1] = true;
        }
        if (row >= 0 && row < 4) {
            usedRow[row] = true;
        }
    }

    int usedColumns[3];
    int usedColumnCount = 0;
    for (int c = 0; c < 3; c++) {
        if (usedColumn[c]) {
            usedColumns[usedColumnCount++] = c;
        }
    }
    if (usedColumnCount == 0) {
        usedColumns[usedColumnCount++] = 0;
    }

    int usedRows[4];
    int usedRowCount = 0;
    for (int r = 0; r < 4; r++) {
        if (usedRow[r]) {
            usedRows[usedRowCount++] = r;
        }
    }
    if (usedRowCount == 0) {
        usedRows[usedRowCount++] = 0;
    }

    int topReserve = ALGO_MARGIN;
    int bottomReserve = ALGO_MARGIN;
    for (int op = 0; op < 6; op++) {
        if (operatorPosition_[op] <= 0.0f) {
            continue;
        }
        if (operatorFeedback_[op] && rowOf[op] == usedRows[0]) {
            topReserve = 7;
        }
        if (operatorMix_[op] == MIX && rowOf[op] == usedRows[usedRowCount - 1]) {
            bottomReserve = 5;
        }
    }

    int columnX[3];
    for (int i = 0; i < usedColumnCount; i++) {
        columnX[usedColumns[i]] = spreadHorizontal(i, usedColumnCount);
    }

    int rowY[4];
    for (int i = 0; i < usedRowCount; i++) {
        rowY[usedRows[i]] = spreadVertical(i, usedRowCount, topReserve, bottomReserve);
    }

    int baseX[6];
    for (int op = 0; op < 6; op++) {
        if (operatorPosition_[op] <= 0.0f) {
            continue;
        }
        int column = columnOf[op];
        int x = columnX[column];
        if (fractionOf[op] > 0.0f && column + 1 < 3 && usedColumn[column + 1]) {
            x = roundHalfEven((float)columnX[column] + fractionOf[op] * (columnX[column + 1] - columnX[column]));
        }
        baseX[op] = x;
    }

    bool hasOverride[6] = {false, false, false, false, false, false};
    int overrideX[6] = {0, 0, 0, 0, 0, 0};

    for (int source = 1; source <= 6; source++) {
        if (operatorPosition_[source - 1] <= 0.0f) {
            continue;
        }
        int destinations[6];
        int destinationCount = 0;
        for (int link = 0; link < imLinkCount_; link++) {
            if (imLinkSource_[link] == source) {
                destinations[destinationCount++] = imLinkDest_[link];
            }
        }
        if (destinationCount < 2) {
            continue;
        }

        int row = rowOf[source - 1];
        bool sameRow = true;
        int sum = 0;
        for (int i = 0; i < destinationCount; i++) {
            int dest = destinations[i];
            if (operatorPosition_[dest - 1] <= 0.0f || rowOf[dest - 1] != row) {
                sameRow = false;
                break;
            }
            sum += columnX[columnOf[dest - 1]];
        }
        if (!sameRow) {
            continue;
        }

        int center = roundHalfEven((float)sum / (float)destinationCount);
        bool clash = false;
        for (int other = 1; other <= 6; other++) {
            if (other == source || operatorPosition_[other - 1] <= 0.0f || rowOf[other - 1] != row) {
                continue;
            }
            int delta = center - baseX[other - 1];
            if (delta < 0) {
                delta = -delta;
            }
            if (delta < ALGO_CELL + 2) {
                clash = true;
                break;
            }
        }
        if (!clash) {
            hasOverride[source - 1] = true;
            overrideX[source - 1] = center;
        }
    }

    for (int op = 0; op < 6; op++) {
        if (operatorPosition_[op] <= 0.0f) {
            continue;
        }
        operatorX_[op] = hasOverride[op] ? overrideX[op] : baseX[op];
        operatorY_[op] = rowY[rowOf[op]];
    }
}

void TftAlgo::drawAlgoFromTable(const uint8_t* algoInfo) {
    for (int i = 0; i < 6; i++) {
        operatorPosition_[i] = 0.0f;
        operatorX_[i] = 0;
        operatorY_[i] = 0;
        operatorMix_[i] = OPERATOR;
        operatorFeedback_[i] = 0;
    }
    imLinkCount_ = 0;

    int idx = 0;
    while (algoInfo[idx] != END) {
        switch (algoInfo[idx++]) {
        case OPERATORSYNC:
            operatorMix_[algoInfo[idx] - 1] = OPERATORSYNC;
        case OPERATOR: {
            uint8_t op = algoInfo[idx];
            operatorPosition_[op - 1] = algoInfo[idx + 1] * 0.5f;
            idx += 2;
            break;
        }
        case IM:
        case IMSYNC:
            if (imLinkCount_ < 8) {
                imLinkSource_[imLinkCount_] = algoInfo[idx + 1];
                imLinkDest_[imLinkCount_] = algoInfo[idx + 2];
                imLinkCount_++;
            }
            idx += 3;
            break;
        case FEEDBACK:
            operatorFeedback_[algoInfo[idx + 1] - 1] = 1;
            idx += 3;
            break;
        case MIX:
            operatorMix_[algoInfo[idx] - 1] = MIX;
            idx++;
            break;
        }
    }

    computeLayout();

    // Links are drawn first so the operator boxes hide them.
    idx = 0;
    while (algoInfo[idx] != END) {
        switch (algoInfo[idx++]) {
        case IM:
            setColor(RGB565_RED);
            drawIM(1, algoInfo[idx], algoInfo[idx + 1], algoInfo[idx + 2]);
            idx += 3;
            break;
        case IMSYNC:
            setColor(RGB565_PURPLE);
            drawIM(1, algoInfo[idx], algoInfo[idx + 1], algoInfo[idx + 2]);
            idx += 3;
            break;
        case FEEDBACK:
            setColor(RGB565_RED);
            drawFeedback(1, algoInfo[idx + 1], algoInfo[idx + 2]);
            idx += 3;
            break;
        case OPERATORSYNC:
        case OPERATOR:
            idx += 2;
            break;
        case MIX:
            idx++;
            break;
        }
    }

    // Clear the inside of each box, then draw all frames.
    idx = 0;
    while (algoInfo[idx] != END) {
        switch (algoInfo[idx++]) {
        case OPERATORSYNC:
        case OPERATOR: {
            uint8_t op = algoInfo[idx];
            setColor(0x0000);
            fillRect(operatorX_[op - 1], operatorY_[op - 1], ALGO_CELL, ALGO_CELL);
            idx += 2;
            break;
        }
        case IM:
        case IMSYNC:
        case FEEDBACK:
            idx += 3;
            break;
        case MIX:
            idx++;
            break;
        }
    }

    idx = 0;
    while (algoInfo[idx] != END) {
        switch (algoInfo[idx++]) {
        case OPERATORSYNC:
        case OPERATOR:
            drawOperator(algoInfo[idx]);
            idx += 2;
            break;
        case IM:
        case IMSYNC:
        case FEEDBACK:
            idx += 3;
            break;
        case MIX:
            idx++;
            break;
        }
    }

    idx = 0;
    while (algoInfo[idx] != END) {
        switch (algoInfo[idx++]) {
        case OPERATORSYNC:
        case OPERATOR:
            drawNumber(operatorX_[algoInfo[idx] - 1] + 5, operatorY_[algoInfo[idx] - 1] + 2, algoInfo[idx]);
            idx += 2;
            break;
        case IM:
        case IMSYNC:
        case FEEDBACK:
            idx += 3;
            break;
        case MIX:
            idx++;
            break;
        }
    }

    idx = 0;
    while (algoInfo[idx] != END) {
        switch (algoInfo[idx++]) {
        case MIX:
            drawCarrierArrow(algoInfo[idx]);
            idx++;
            break;
        case OPERATORSYNC:
        case OPERATOR:
            idx += 2;
            break;
        case IM:
        case IMSYNC:
        case FEEDBACK:
            idx += 3;
            break;
        }
    }
}
