/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
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

#ifndef ENCODERS_H_
#define ENCODERS_H_

#include "stm32h7xx_hal.h"

#include "RingBuffer.h"
#include "EncodersListener.h"
#include "preenfm3_pins.h"

#define NUMBER_OF_ENCODERS 6
#define NUMBER_OF_BUTTONS_MIN 12
#define NUMBER_OF_BUTTONS_MAX 18

enum LastEncoderMove {
    LAST_MOVE_NONE = 0, LAST_MOVE_INC, LAST_MOVE_DEC
};

struct EncoderStatus {
    char value;
    bool b1;
};

class Encoders {
public:
    Encoders();
    ~Encoders();
    void checkStatus(uint8_t encoderType, uint8_t encoderPush);
    void checkSimpleStatus();
    int getRegisterBits(uint8_t encoderPush);

    void insertListener(EncodersListener *listener) {
        if (firstListener != 0) {
            listener->nextListener = firstListener;
        }
        firstListener = listener;
    }

    void encoderTurned(int encoder, int ticks) {
        if (firstButtonDown_ == -1) {
            for (EncodersListener *listener = firstListener; listener != 0; listener = listener->nextListener) {
                listener->encoderTurned(encoder, ticks);
            }
        } else {
            for (EncodersListener *listener = firstListener; listener != 0; listener = listener->nextListener) {
                listener->encoderTurnedWhileButtonPressed(encoder, ticks, firstButtonDown_);
            }
            buttonUsedFromSomethingElse_[firstButtonDown_] = true;
        }
    }

    void encoderTurnedWileButtonDown(int encoder, int ticks) {
        for (EncodersListener *listener = firstListener; listener != 0; listener = listener->nextListener) {
            listener->encoderTurned(encoder, ticks);
        }
    }

    void buttonPressed(int button) {
        for (EncodersListener *listener = firstListener; listener != 0; listener = listener->nextListener) {
            listener->buttonPressed(button);
        }
    }

    void buttonLongPressed(int button) {
        for (EncodersListener *listener = firstListener; listener != 0; listener = listener->nextListener) {
            listener->buttonLongPressed(button);
        }
    }

    void twoButtonsPressed(int button1, int button2) {
        for (EncodersListener *listener = firstListener; listener != 0; listener = listener->nextListener) {
            listener->twoButtonsPressed(button1, button2);
        }
    }

private:
    int action_[2][16];
    int encoderBit1_[NUMBER_OF_ENCODERS];
    int encoderBit2_[NUMBER_OF_ENCODERS];
    int encoderState_[NUMBER_OF_ENCODERS];
    int timerAction_[NUMBER_OF_ENCODERS];

    LastEncoderMove lastMove_[NUMBER_OF_ENCODERS];
    int tickSpeed_[NUMBER_OF_ENCODERS];

    int buttonBit_[NUMBER_OF_BUTTONS_MAX];
    int buttonTimer_[NUMBER_OF_BUTTONS_MAX];
    bool buttonUsedFromSomethingElse_[NUMBER_OF_BUTTONS_MAX];
    bool buttonPreviousState_[NUMBER_OF_BUTTONS_MAX];
    int firstButtonDown_;

    int encoderTimer_;

    EncodersListener *firstListener;
};

#endif /* ENCODERS_H_ */
