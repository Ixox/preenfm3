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

#include "Encoders.h"

static inline void delay_micros(uint32_t ns) {

    /* fudge for function call overhead  */
    //us--;
    asm volatile("   mov r0, %[ns]          \n\t"
        "1: subs r0, #1            \n\t"
        "   bhi 1b                 \n\t"
        :
        : [ns] "r" (ns)
        : "r0");
}
;

Encoders::Encoders() {
    // PreenFM Control board 1.2 (pushable encoder)
    char encoderPins[] = { 17, 18, 15, 16, 9, 10, 20, 19, 14, 13, 12, 11 };
    char buttonPins[] = { 23, 21, 4, 24, 3, 2, 22, 5, 6, 7, 8, 1, 32, 31, 30, 27, 28, 29 };

    /*
     0: 0000 = 00
     1: 0001 = 00
     2: 0010 = 00
     3: 0011 = 00
     4: 0100 = 02
     5: 0101 = 00
     6: 0110 = 00
     7: 0111 = 01 // 1
     8: 1000 = 00
     9: 1001 = 00
     A: 1010 = 00
     B: 1011 = 02 // 2
     C: 1100 = 00
     D: 1101 = 00
     E: 1110 = 00
     F: 1111 = 00
     */
    int actionToCopy[2][16] = { { 0, 0, 0, 0, 2, 0, 0, 1, 1, 0, 0, 2, 0, 0, 0, 0 }, /* N12 */
                                { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0 } /* N24 */
    };


    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 16; j++) {
            action_[i][j] = actionToCopy[i][j];
        }
    }

    firstListener = 0;

    for (int k = 0; k < NUMBER_OF_ENCODERS; k++) {
        encoderBit1_[k] = 1 << (encoderPins[k * 2] - 1);
        encoderBit2_[k] = 1 << (encoderPins[k * 2 + 1] - 1);
        lastMove_[k] = LAST_MOVE_NONE;
        tickSpeed_[k] = 1;
    }

    for (int k = 0; k < NUMBER_OF_BUTTONS_MAX; k++) {
        buttonBit_[k] = 1 << (buttonPins[k] - 1);
        buttonPreviousState_[k] = false;
        // > 30
        buttonTimer_[k] = 31;
        buttonUsedFromSomethingElse_[k] = false;
    }

    encoderTimer_ = 0;
    firstButtonDown_ = -1;

}

Encoders::~Encoders() {
}

int Encoders::getRegisterBits(uint8_t encoderPush) {
    // Copy the values in the HC165 registers
    HAL_GPIO_WritePin(HC165_LOAD_GPIO_Port, HC165_LOAD_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(HC165_LOAD_GPIO_Port, HC165_LOAD_Pin, GPIO_PIN_SET);

    // Analyse the new value
    int registerBits = 0;
    int numberOfPins = encoderPush == 0 ? 24 : 32;
    for (int i = 0; i < numberOfPins; i++) {
        HAL_GPIO_WritePin(HC165_CLK_GPIO_Port, HC165_CLK_Pin, GPIO_PIN_RESET);
        registerBits |= (HAL_GPIO_ReadPin(HC165_DATA_GPIO_Port, HC165_DATA_Pin) << i);
        HAL_GPIO_WritePin(HC165_CLK_GPIO_Port, HC165_CLK_Pin, GPIO_PIN_SET);
    }
    // User selected pushed ancoder and he doesn't have pushed encoders
    if (numberOfPins == 32 && (registerBits & 0xFF000000) == 0) {
        registerBits |= 0xFF000000;
    }
    return registerBits;
}

void Encoders::checkSimpleStatus() {
    int registerBits = getRegisterBits(0);

    for (int k = 0; k < NUMBER_OF_BUTTONS_MIN; k++) {
        // button is pressed ?
        bool b1 = ((registerBits & buttonBit_[k]) == 0);
        // button is pressed
        if (b1) {
            if (buttonTimer_[k] > 30) {
                buttonPressed(k);
            }
            buttonTimer_[k] = 0;
        }
        buttonTimer_[k]++;
    }
}

void Encoders::checkStatus(uint8_t encoderType, uint8_t encoderPush) {
    int registerBits = getRegisterBits(encoderPush);

    // target the right action row.
    // encoderType : First bit for type, second bit for inversed
    int *actionEnc = action_[encoderType & 0x1];
    int inversedEnc = (encoderType & 0x2) == 0 ? 1 : -1;

    for (int k = 0; k < NUMBER_OF_ENCODERS; k++) {
        bool b1 = ((registerBits & encoderBit1_[k]) == 0);
        bool b2 = ((registerBits & encoderBit2_[k]) == 0);

        encoderState_[k] <<= 2;
        encoderState_[k] &= 0xf;
        if (b1) {
            encoderState_[k] |= 1;
        }
        if (b2) {
            encoderState_[k] |= 2;
        }

        if (actionEnc[encoderState_[k]] == 1 && lastMove_[k] != LAST_MOVE_DEC) {
            encoderTurned(k, tickSpeed_[k] * inversedEnc);
            tickSpeed_[k] += 3;
            lastMove_[k] = LAST_MOVE_INC;
            timerAction_[k] = 60;
        } else if (actionEnc[encoderState_[k]] == 2 && lastMove_[k] != LAST_MOVE_INC) {
            encoderTurned(k, -tickSpeed_[k] * inversedEnc);
            tickSpeed_[k] += 3;
            lastMove_[k] = LAST_MOVE_DEC;
            timerAction_[k] = 60;
        } else {
            if (timerAction_[k] > 1) {
                timerAction_[k]--;
            } else if (timerAction_[k] == 1) {
                timerAction_[k]--;
                lastMove_[k] = LAST_MOVE_NONE;
            }
            if (tickSpeed_[k] > 1 && ((encoderTimer_ & 0x3) == 0)) {
                tickSpeed_[k] = tickSpeed_[k] - 1;
            }
        }
        if (tickSpeed_[k] > 10) {
            tickSpeed_[k] = 10;
        }
    }

    int numberOfButton = encoderPush == 0 ? NUMBER_OF_BUTTONS_MIN : NUMBER_OF_BUTTONS_MAX;
    for (int k = 0; k < numberOfButton; k++) {
        bool b1 = ((registerBits & buttonBit_[k]) == 0);

        // button is pressed
        if (b1) {
            buttonTimer_[k]++;
            // just pressed ?
            if (!buttonPreviousState_[k]) {
                if (firstButtonDown_ == -1) {
                    firstButtonDown_ = k;
                    buttonUsedFromSomethingElse_[k] = false;
                } else {
                    twoButtonsPressed(firstButtonDown_, k);
                    buttonUsedFromSomethingElse_[firstButtonDown_] = true;
                    buttonUsedFromSomethingElse_[k] = true;
                }
            } else {
                if (buttonTimer_[k] > 350 && !buttonUsedFromSomethingElse_[k]) {
                    buttonLongPressed(k);
                    buttonUsedFromSomethingElse_[k] = true;
                }
            }
        } else {
            // Just unpressed ?
            if (buttonPreviousState_[k]) {
                if (firstButtonDown_ == k) {
                    firstButtonDown_ = -1;
                }
                if (buttonPreviousState_[k] && !buttonUsedFromSomethingElse_[k]) {
                    // Just released
                    if (buttonTimer_[k] > 15) {
                        buttonPressed(k);
                    }
                }
                buttonTimer_[k] = 0;
            }
        }

        buttonPreviousState_[k] = b1;
    }
    encoderTimer_++;
}
