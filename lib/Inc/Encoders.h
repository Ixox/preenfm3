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

enum EncoderActionType {
	ENCODER_TURNED = 0,
	ENCODER_TURNED_WHILE_BUTTON_PRESSED,
	ENCODER_BUTTON_CLICKED,
	ENCODER_TWO_BUTTON_PRESSED,
	ENCODER_LONG_BUTTON_PRESSED,
    ENCODER_BUTTON_DOWN,
    ENCODER_BUTTON_UP,
};




struct EncoderAction {
	uint8_t actionType;
	uint8_t encoder;
	int8_t ticks;
	uint8_t button1;
	uint8_t button2;
};

struct EncoderStatus {
    char value;
    bool b1;
};

class Encoders {
public:
    Encoders();
    ~Encoders();
    /**
     * checkStatus Stacks the following actions :
     *  ENCODER_TURNED
     *  ENCODER_TURNED_WHILE_BUTTON_PRESSED
     *  ENCODER_BUTTON_CLICKED
     *  ENCODER_TWO_BUTTON_PRESSED
     *  ENCODER_LONG_BUTTON_PRESSED
     */
    void checkStatus(uint8_t encoderType, uint8_t encoderPush);
    /**
     * checkStatusUpDown Stacks the following actions :
     *  ENCODER_TURNED
     *  ENCODER_BUTTON_DOWN
     *  ENCODER_BUTTON_UP
     */
    void checkStatusUpDown(uint8_t encoderType, uint8_t encoderPush);
    void checkSimpleStatus();
    uint32_t getRegisterBits(uint8_t encoderPush);
    void processActions();

    void insertListener(EncodersListener *listener) {
        if (firstListener != 0) {
            listener->nextListener = firstListener;
        }
        firstListener = listener;
    }

    void clearState();

    void encoderTurned(int encoder, int ticks) {
		for (EncodersListener *listener = firstListener; listener != 0; listener = listener->nextListener) {
			listener->encoderTurned(encoder, ticks);
		}
    }

    void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) {
		for (EncodersListener *listener = firstListener; listener != 0; listener = listener->nextListener) {
			listener->encoderTurnedWhileButtonPressed(encoder, ticks, button);
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

    void buttonUp(int button) {
        for (EncodersListener *listener = firstListener; listener != 0; listener = listener->nextListener) {
            listener->buttonUp(button);
        }
    }

    void buttonDown(int button) {
        for (EncodersListener *listener = firstListener; listener != 0; listener = listener->nextListener) {
            listener->buttonDown(button);
        }
    }


private:
    uint8_t action_[2][16];
    uint32_t encoderBit1_[NUMBER_OF_ENCODERS];
    uint32_t encoderBit2_[NUMBER_OF_ENCODERS];
    uint32_t encoderState_[NUMBER_OF_ENCODERS];
    uint32_t timerAction_[NUMBER_OF_ENCODERS];

    LastEncoderMove lastMove_[NUMBER_OF_ENCODERS];
    int8_t tickSpeed_[NUMBER_OF_ENCODERS];

    uint32_t buttonBit_[NUMBER_OF_BUTTONS_MAX];
    uint32_t buttonTimer_[NUMBER_OF_BUTTONS_MAX];
    bool buttonUsedFromSomethingElse_[NUMBER_OF_BUTTONS_MAX];
    bool buttonPreviousState_[NUMBER_OF_BUTTONS_MAX];
    int8_t firstButtonDown_;

    uint32_t encoderTimer_;
    // execute action can be async
    RingBuffer<EncoderAction, 16> actions_;

    EncodersListener *firstListener;
};

#endif /* ENCODERS_H_ */
