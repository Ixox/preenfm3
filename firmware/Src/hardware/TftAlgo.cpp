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



#include "stm32h7xx_hal.h"
#include "TftAlgo.h"
#include "Common.h"

#define RGB565_WHITE 0xffff
#define RGB565_RED 0x00f8
#define RGB565_BLUE 0x1f00
#define RGB565_GREEN 0xe007
#define RGB565_ORANGE 0x00fc
#define RGB565_YELLOW 0xe0ff
#define RGB565_CYAN 0xff07
#define RGB565_PURPLE 0xBFE0

const uint8_t digitBits[] = {
    // 0
    0b11111000,
    0b10001000,
    0b10001000,
    0b10001000,
    0b11111000,
    // 1
    0b00100000,
    0b00100000,
    0b00100000,
    0b00100000,
    0b00100000,
    // 2
    0b11111000,
    0b00001000,
    0b11111000,
    0b10000000,
    0b11111000,
    // 3
    0b11111000,
    0b00001000,
    0b00111000,
    0b00001000,
    0b11111000,
    // 4
    0b10001000,
    0b10001000,
    0b11111000,
    0b00001000,
    0b00001000,
    // 5
    0b11111000,
    0b10000000,
    0b11111000,
    0b00001000,
    0b11111000,
    // 6
    0b11111000,
    0b10000000,
    0b11111000,
    0b10001000,
    0b11111000,
    // 7
    0b11111000,
    0b00001000,
    0b00001000,
    0b00001000,
    0b00001000,
    // 8
    0b11111000,
    0b10001000,
    0b11111000,
    0b10001000,
    0b11111000,
    // 9
    0b11111000,
    0b10001000,
    0b11111000,
    0b00001000,
    0b11111000
};


const uint8_t algo1[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 3, 2,
    IM, 6, 3, 3,
    END,
};

const uint8_t algo2[] = {
    MIX, 1,
    MIX, 2,
    OPERATOR, 1, 10,
    OPERATOR, 2, 12,
    OPERATOR, 3, 5,
    IM, 1, 3, 1,
    IM, 2, 3, 2,
    IM, 6, 3, 3,
    END,
};


const uint8_t algo3[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 2,
    OPERATOR, 4, 6,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 4, 1,
    IM, 4, 3, 4,
    IM, 6, 3, 3,
    END,
};

const uint8_t algo4[] = {
    MIX, 1,
    MIX, 2,
    OPERATOR, 1, 10,
    OPERATOR, 2, 12,
    OPERATOR, 3, 4,
    OPERATOR, 4, 3,
    IM, 1, 3, 1,
    IM, 2, 4, 2,
    IM, 3, 3, 2,
    IM, 4, 4, 3,
    IM, 6, 4, 4,
    END,
};

const uint8_t algo5[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 8,
    OPERATOR, 3, 4,
    OPERATOR, 4, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 2,
    IM, 3, 4, 3,
    IM, 4, 4, 2,
    IM, 4, 4, 2,
    IM, 6, 4, 4,
    END,
};

const uint8_t algo6[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 11,
    OPERATOR, 3, 12,
    OPERATOR, 4, 5,
    IM, 1, 4, 1,
    IM, 2, 4, 2,
    IM, 3, 4, 3,
    IM, 6, 4, 4,
    END,
};

const uint8_t algo7[] = {
    MIX, 1,
    MIX, 3,
    MIX, 5,
    OPERATOR, 1, 10,
    OPERATOR, 3, 11,
    OPERATOR, 5, 12,
    OPERATOR, 2, 4,
    OPERATOR, 4, 2,
    OPERATOR, 6, 6,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 6, 5,
    IM, 4, 4, 6,
    IM, 6, 4, 4,
    END,
};

const uint8_t algo8[] = {
    MIX, 1,
    MIX, 5,
    OPERATOR, 1, 10,
    OPERATOR, 2, 1,
    OPERATOR, 3, 2,
    OPERATOR, 4, 3,
    OPERATOR, 5, 12,
    OPERATOR, 6, 9,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 4, 1,
    IM, 4, 6, 5,
    IM, 6, 4, 4,
    END,
};

const uint8_t algo9[] = {
    MIX, 1,
    MIX, 4,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 5,
    OPERATOR, 4, 12,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 5, 4,
    IM, 4, 6, 5,
    IM, 6, 6, 6,
    END,
};

const uint8_t algo10[] = {
    MIX, 1,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 7,
    OPERATOR, 3, 12,
    OPERATOR, 4, 9,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 5, 4,
    IM, 4, 6, 5,
    IM, 6, 2, 2,
    END,
};

const uint8_t algo11[] = {
    MIX, 1,
    MIX, 4,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 1,
    OPERATOR, 4, 12,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 2,
    IM, 3, 5, 4,
    IM, 4, 6, 5,
    IM, 6, 6, 6,
    END,
};

const uint8_t algo12[] = {
    MIX, 1,
    MIX, 3,
    MIX, 5,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 12,
    OPERATOR, 6, 6,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 6, 5,
    IM, 6, 6, 6,
    END,
};

const uint8_t algo13[] = {
    MIX, 1,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 5, 3,
    IM, 4, 6, 5,
    IM, 6, 4, 4,
    END,
};

const uint8_t algo14[] = {
    MIX, 1,
    MIX, 4,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 1,
    OPERATOR, 4, 11,
    OPERATOR, 5, 5,
    OPERATOR, 6, 6,
    IM, 1, 2, 1,
    IM, 2, 3, 2,
    IM, 3, 5, 4,
    IM, 4, 6, 4,
    IM, 6, 6, 6,
    END,
};

const uint8_t algo15[] = {
    MIX, 1,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 7,
    OPERATOR, 3, 12,
    OPERATOR, 4, 1,
    OPERATOR, 5, 2,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 5, 3,
    IM, 4, 6, 3,
    IM, 6, 6, 6,
    END,
};

const uint8_t algo16[] = {
    MIX, 1,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 12,
    OPERATOR, 4, 9,
    OPERATOR, 5, 2,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 5, 4,
    IM, 4, 6, 4,
    IM, 6, 2, 2,
    END,
};

const uint8_t algo17[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 5,
    OPERATOR, 4, 2,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 4, 3,
    IM, 4, 5, 1,
    IM, 5, 6, 5,
    IM, 6, 2, 2,
    END,
};

const uint8_t algo18[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 7,
    OPERATOR, 3, 8,
    OPERATOR, 4, 9,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 4, 1,
    IM, 4, 5, 4,
    IM, 5, 6, 5,
    IM, 6, 3, 3,
    END,
};

const uint8_t algo19[] = {
    MIX, 1,
    MIX, 4,
    MIX, 5,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 1,
    OPERATOR, 4, 11,
    OPERATOR, 5, 12,
    OPERATOR, 6, 5,
    IM, 1, 2, 1,
    IM, 2, 3, 2,
    IM, 3, 6, 4,
    IM, 4, 6, 5,
    IM, 6, 6, 6,
    END,
};

const uint8_t algo20[] = {
    MIX, 1,
    MIX, 2,
    MIX, 4,
    OPERATOR, 1, 10,
    OPERATOR, 2, 11,
    OPERATOR, 3, 4,
    OPERATOR, 4, 12,
    OPERATOR, 5, 5,
    OPERATOR, 6, 6,
    IM, 1, 3, 1,
    IM, 2, 3, 2,
    IM, 3, 5, 4,
    IM, 4, 6, 4,
    IM, 6, 3, 3,
    END,
};

const uint8_t algo21[] = {
    MIX, 1,
    MIX, 2,
    MIX, 4,
    MIX, 5,
    OPERATOR, 1, 4,
    OPERATOR, 2, 6,
    OPERATOR, 3, 2,
    OPERATOR, 4, 10,
    OPERATOR, 5, 12,
    OPERATOR, 6, 8,
    IM, 1, 3, 1,
    IM, 2, 3, 2,
    IM, 3, 6, 4,
    IM, 4, 6, 5,
    IM, 6, 3, 3,
    END,
};

const uint8_t algo22[] = {
    MIX, 1,
    MIX, 3,
    MIX, 4,
    MIX, 5,
    OPERATOR, 1, 4,
    OPERATOR, 2, 1,
    OPERATOR, 3, 10,
    OPERATOR, 4, 11,
    OPERATOR, 5, 12,
    OPERATOR, 6, 6,
    IM, 1, 2, 1,
    IM, 2, 6, 3,
    IM, 3, 6, 4,
    IM, 4, 6, 5,
    IM, 6, 6, 6,
    END,
};

const uint8_t algo23[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    MIX, 4,
    MIX, 5,
    OPERATOR, 1, 1,
    OPERATOR, 2, 3,
    OPERATOR, 3, 10,
    OPERATOR, 4, 11,
    OPERATOR, 5, 12,
    OPERATOR, 6, 5,
    IM, 1, 6, 3,
    IM, 2, 6, 4,
    IM, 3, 6, 5,
    IM, 6, 6, 6,
    END,
};

const uint8_t algo24[] = {
    MIX, 1,
    MIX, 3,
    MIX, 6,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 2,
    OPERATOR, 6, 12,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 5, 4,
    IM, 6, 5, 5,
    END,
};

const uint8_t algo25[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    MIX, 5,
    OPERATOR, 1, 1,
    OPERATOR, 2, 10,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 12,
    OPERATOR, 6, 6,
    IM, 1, 4, 3,
    IM, 2, 6, 5,
    IM, 6, 6, 6,
    END,
};

const uint8_t algo26[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    MIX, 6,
    OPERATOR, 1, 1,
    OPERATOR, 2, 10,
    OPERATOR, 3, 11,
    OPERATOR, 4, 8,
    OPERATOR, 5, 5,
    OPERATOR, 6, 12,
    IM, 1, 4, 3,
    IM, 2, 5, 4,
    IM, 6, 5, 5,
    END,
};

const uint8_t algo27[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    MIX, 4,
    MIX, 5,
    MIX, 6,
    OPERATOR, 1, 4,
    OPERATOR, 2, 5,
    OPERATOR, 3, 6,
    OPERATOR, 4, 10,
    OPERATOR, 5, 11,
    OPERATOR, 6, 12,
    IM, 6, 6, 6,
    END,
};

const uint8_t algo28[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    MIX, 4,
    MIX, 5,
    OPERATOR, 1, 4,
    OPERATOR, 2, 5,
    OPERATOR, 3, 6,
    OPERATOR, 4, 10,
    OPERATOR, 5, 11,
    OPERATOR, 6, 8,
    IM, 1, 6, 5,
    IM, 6, 6, 6,
    END,
};

const uint8_t algo29[] = {
    MIX, 1,
    MIX, 2,
    OPERATOR, 1, 10,
    OPERATOR, 2, 12,
    OPERATORSYNC, 3, 4,
    OPERATOR, 4, 3,
    IMSYNC, 1, 3, 1,
    IM, 2, 4, 2,
    IM, 3, 4, 3,
    IM, 6, 4, 4,
    END,
};

const uint8_t algo30[] = {
    MIX, 1,
    MIX, 2,
    OPERATOR, 1, 10,
    OPERATOR, 2, 12,
    OPERATORSYNC, 3, 4,
    OPERATOR, 4, 3,
    IMSYNC, 1, 3, 1,
    IMSYNC, 2, 3, 2,
    IM, 3, 4, 1,
    IM, 4, 4, 2,
    IM, 6, 4, 4,
    END,
};

const uint8_t algo31[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 11,
    OPERATOR, 3, 12,
    OPERATORSYNC, 4, 5,
    IMSYNC, 1, 4, 1,
    IMSYNC, 2, 4, 2,
    IMSYNC, 3, 4, 3,
    END,
};

const uint8_t algo32[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 2,
    OPERATORSYNC, 4, 6,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IMSYNC, 3, 4, 1,
    IM, 4, 3, 4,
    IM, 6, 3, 3,
    END,
};

#define NUMBER_OF_ALGOS 32
const uint8_t* allAlgos[NUMBER_OF_ALGOS] = { algo1, algo2, algo3 , algo4, algo5, algo6, algo7, algo8, algo9, algo10, algo11,
        algo12, algo13, algo14, algo15, algo16, algo17, algo18, algo19, algo20, algo21, algo22, algo23, algo24, algo25,
        algo26, algo27, algo28, algo29, algo30, algo31, algo32 };

struct ModulationIndex modulationIndex[ALGO_END][6];
bool carrierOperator[ALGO_END][6];



#define GETX1(o) (((o-1) % 3)*26 + 6)
#define GETY1(o) (((o-1)/3)*25 + 2)

#define SETPIXEL(x, y) buffer_[(x)+(y)*80]=color_

#define SETFGPIXEL(x, y) fgBuffer_[(x)+(y)*80]=0xff
#define DELFGPIXEL(x, y) fgBuffer_[(x)+(y)*80]=0x00

TftAlgo::TftAlgo() {

    for (int a = 0; a < NUMBER_OF_ALGOS; a++) {
        const uint8_t *algoInfo = allAlgos[a];
        int idx = 0;
        for (int o = 0; o < 6; o++) {
            carrierOperator[a][o] = false;
            modulationIndex[a][o].source = 0;
            modulationIndex[a][o].destination = 0;
        }
        while (algoInfo[idx] != END) {
            switch (algoInfo[idx++]) {
                case OPERATORSYNC:
                case OPERATOR:
                    idx += 2;
                    break;
                case IM:
                case IMSYNC:
                    modulationIndex[a][algoInfo[idx] - 1].source = algoInfo[idx + 1];
                    modulationIndex[a][algoInfo[idx] - 1].destination = algoInfo[idx + 2];
                    idx += 3;
                    break;
                case MIX:
                    carrierOperator[a][algoInfo[idx] - 1] = true;
                    idx++;
                    break;
            }
        }
    }
}

TftAlgo::~TftAlgo() {
}

void TftAlgo::setBufferAdress(uint16_t *bufferAdress) {
    buffer_ = bufferAdress;
}

void TftAlgo::setFGBufferAdress(uint8_t *bufferAdress) {
    fgBuffer_ = bufferAdress;
}

void TftAlgo::setColor(uint16_t col) {
    color_ = col;
}

/*
 * @param mode : 1=BG, 2=FG draw, 3=FG erase
 *
 */
void TftAlgo::drawLine(uint8_t mode, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0, yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0, curpixel = 0;

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
        num = deltax * 0.5f;
        numadd = deltay;
        numpixels = deltax;
    } else {
        xinc2 = 0;
        yinc1 = 0;
        den = deltay;
        num = deltay * 0.5f;
        numadd = deltax;
        numpixels = deltay;
    }

    for (curpixel = 0; curpixel <= numpixels; curpixel++) {
        switch (mode) {
            case 1:
                SETPIXEL(x, y);
                break;
            case 2:
                SETFGPIXEL(x, y);
                break;
            case 3:
                DELFGPIXEL(x, y);
                break;
        }
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

void TftAlgo::drawNumber(int x, int y, int opNum) {

    if (operatorMix_[opNum - 1] == MIX) {
        // Carrier
        setColor(RGB565_YELLOW);
    } else {
        // modulator
        setColor(RGB565_ORANGE);
    }
    for (int j = 0; j < 5; j++) {
        uint8_t line = digitBits[opNum * 5 + j] >> 3;
        for (int i = 0; i < 5; i++) {
            if (((line >> (4 - i)) & 1) == 1) {
                SETPIXEL(x + i * 2, y + j * 2);
                SETPIXEL(x + i * 2 + 1, y + j * 2);
                SETPIXEL(x + i * 2 + 1, y + j * 2 + 1);
                SETPIXEL(x + i * 2, y + j * 2 + 1);
            }
        }
    }
}

void TftAlgo::highlightOperator(bool draw, uint8_t opNum) {

    uint8_t opPosition = operatorPosition_[opNum];

    if (unlikely(opPosition == 0)) {
        return;
    }

    int32_t color32 = draw ? 0xffffffff : 0x0;
    int16_t color16 = draw ? 0xffff : 0x0;

    int x1 = GETX1(opPosition) - 2;
    int y1 = GETY1(opPosition) - 2;

    if (x1 < 0) {
        x1 = 0;
    }
    if (y1 < 0) {
        y1 = 0;
    }

    for (int x = x1; x < x1 + 20; x += 4) {
        *((int32_t*) (&fgBuffer_[(x) + (y1) * 80])) = color32;
        *((int32_t*) (&fgBuffer_[(x) + (y1 + 1) * 80])) = color32;
        *((int32_t*) (&fgBuffer_[(x) + (y1 + 19) * 80])) = color32;
        *((int32_t*) (&fgBuffer_[(x) + (y1 + 20) * 80])) = color32;
    }
    for (int y = y1; y < y1 + 20; y++) {
        *((int16_t*) (&fgBuffer_[(x1) + (y) * 80])) = color16;
        *((int16_t*) (&fgBuffer_[(x1 + 19) + (y) * 80])) = color16;
    }
}

void TftAlgo::drawOperator(uint8_t opNum, uint8_t opPosition) {

    if (operatorMix_[opNum - 1] == MIX) {
        // Carrier
        setColor(RGB565_CYAN);
    } else if (operatorMix_[opNum - 1] == OPERATORSYNC) {
        // modulator sync
        setColor(RGB565_PURPLE);
    } else {
        // modulator
        setColor(RGB565_BLUE);
    }

    int x1 = GETX1(opPosition);
    int y1 = GETY1(opPosition);

    for (int x = x1; x < x1 + 16; x++) {
        SETPIXEL(x, y1);
        SETPIXEL(x, y1 + 16);
    }
    for (int y = y1; y < y1 + 16; y++) {
        SETPIXEL(x1, y);
        SETPIXEL(x1 + 16, y);
    }
//    setColor(0x00ff);
//    drawNumber(x1 + 3, y1 + 3, opNum);
}

void TftAlgo::drawIM(uint8_t mode, uint8_t imNum, uint8_t opSource, uint8_t opDest) {

    if (opSource != opDest) {
        int xSource = GETX1(operatorPosition_[opSource-1]) + 8;
        int ySource = GETY1(operatorPosition_[opSource-1]) + 17;

        int xDest = GETX1(operatorPosition_[opDest-1]) + 8;
        int yDest = GETY1(operatorPosition_[opDest-1]) - 1;

        drawLine(mode, xSource, ySource, xDest, yDest);
    } else {
        // Feedback
        int x = GETX1(operatorPosition_[opSource-1]) + 8;
        int y = GETY1(operatorPosition_[opSource-1]) + 8;

        drawLine(mode, x + 8, y, x + 10, y);
        drawLine(mode, x + 10, y, x + 10, y - 10);
        drawLine(mode, x + 10, y - 10, x, y - 10);
        drawLine(mode, x, y - 10, x, y - 8);
    }
}

// Draw the IM on
void TftAlgo::highlightIM(bool draw, uint8_t imNum, uint8_t opSource, uint8_t opDest) {
    drawIM(draw ? 2 : 3, imNum, opSource, opDest);
}

void TftAlgo::drawMix(uint8_t imNum) {
    setColor(0x0ff0);
}


void TftAlgo::drawAlgo(int algo) {
    int idx = 0;
    algo = algo % NUMBER_OF_ALGOS;
    const uint8_t *algoInfo = allAlgos[algo];
    for (int i = 0; i < 6; i++) {
        operatorPosition_[i] = 0;
        imSource_[i] = 0;
        imDest_[i] = 0;
        operatorMix_[i] = OPERATOR; // default
    }

    while (algoInfo[idx] != END) {
        switch (algoInfo[idx++]) {
            case OPERATORSYNC:
                operatorMix_[algoInfo[idx] - 1] = OPERATORSYNC;
            case OPERATOR:
                operatorPosition_[algoInfo[idx] - 1] = algoInfo[idx + 1];
                drawOperator(algoInfo[idx], algoInfo[idx + 1]);
                idx += 2;
                break;
            case IM:
                imSource_[algoInfo[idx] - 1] = algoInfo[idx + 1];
                imDest_[algoInfo[idx] - 1] = algoInfo[idx + 2];
                setColor(RGB565_RED);
                drawIM(1, algoInfo[idx], algoInfo[idx + 1], algoInfo[idx + 2]);
                idx += 3;
                break;
            case IMSYNC:
                imSource_[algoInfo[idx] - 1] = algoInfo[idx + 1];
                imDest_[algoInfo[idx] - 1] = algoInfo[idx + 2];
                setColor(RGB565_PURPLE);
                drawIM(1, algoInfo[idx], algoInfo[idx + 1], algoInfo[idx + 2]);
                idx += 3;
                break;
            case MIX:
                operatorMix_[algoInfo[idx] - 1] = MIX;
                drawMix(algoInfo[idx++]);
                break;
        }
    }

    // Draw Number After to draw on IM
    int x1, y1;
    idx = 0;
    while (algoInfo[idx] != END) {
        switch (algoInfo[idx++]) {
            case OPERATORSYNC:
            case OPERATOR:
                x1 = GETX1(algoInfo[idx + 1]);
                y1 = GETY1(algoInfo[idx + 1]);
                drawNumber(x1 + 3, y1 + 3, algoInfo[idx]);
                idx += 2;
                break;
            case IM:
            case IMSYNC:
                idx += 3;
                break;
            case MIX:
                idx++;
                break;
        }
    }

}

const uint8_t* TftAlgo::getDigitBits(uint8_t digit) {
    return &digitBits[(digit % 10) * 5];
}
