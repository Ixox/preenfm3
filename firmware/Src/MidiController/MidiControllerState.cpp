/*
 * Copyright 2021 Xavier Hosxe
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

#include "stm32h7xx_hal.h"
#include "MidiControllerState.h"
#include "RingBuffer.h"

extern RingBuffer<uint8_t, 64> usartBufferOut;
extern UART_HandleTypeDef huart1;


MidiControllerState::MidiControllerState() {
    int cpt = 1;
    for (int pageNumber = 0; pageNumber < MIDI_NUMBER_OF_PAGES; pageNumber++) {
        for (int i = 0; i < 6; i++) {
            midiPage_[pageNumber].encoder_[i].controller = 15 + cpt;
            midiPage_[pageNumber].encoder_[i].value = (pageNumber & 0x1) == 0 ? 0 : 64;
            midiPage_[pageNumber].encoder_[i].midiChannel = 16;
            midiPage_[pageNumber].encoder_[i].maxValue = 127;

            midiPage_[pageNumber].button_[i].controller = 59 + cpt;
            midiPage_[pageNumber].button_[i].midiChannel = 16;
            midiPage_[pageNumber].button_[i].value = 0;
            midiPage_[pageNumber].button_[i].valueOff = 0;
            midiPage_[pageNumber].button_[i].valueOn = 127;
            midiPage_[pageNumber].button_[i].buttonType = (pageNumber & 0x1) == 0 ? MIDI_BUTTON_TYPE_PUSH : MIDI_BUTTON_TYPE_TOGGLE;

            strcpy(midiPage_[pageNumber].encoder_[i].name, "Enc00", 5);
            strcpy(midiPage_[pageNumber].button_[i].name, "But00", 5);

            int cptDdiz = cpt / 10;
            int cptUnit = cpt % 10;
            midiPage_[pageNumber].button_[i].name[3]  = '0' + cptDdiz;
            midiPage_[pageNumber].encoder_[i].name[3] = '0' + cptDdiz;
            midiPage_[pageNumber].button_[i].name[4]  = '0' + cptUnit;
            midiPage_[pageNumber].encoder_[i].name[4] = '0' + cptUnit;

            cpt++;
        }
    }
}

MidiControllerState::~MidiControllerState() {
}


void MidiControllerState::encoderDelta(uint8_t pageNumber, uint8_t globalMidiChannel, uint32_t encoderNumber, int delta) {
    MidiEncoder* encoder =  &midiPage_[pageNumber].encoder_[encoderNumber];
    int newValue = encoder->value + delta;
    newValue = newValue < encoder->minValue ? encoder->minValue : newValue;
    newValue = newValue > encoder->maxValue ? encoder->maxValue : newValue;

    if (newValue != encoder->value) {
        encoder->value = newValue;
        uint8_t midiChannel = encoder->midiChannel == 16 ? globalMidiChannel : encoder->midiChannel;
        usartBufferOut.insert((uint8_t)(0xb0 + midiChannel));
        usartBufferOut.insert((uint8_t)encoder->controller);
        usartBufferOut.insert((uint8_t)encoder->value);
        sendMidiDin5Out();
    }
}



void MidiControllerState::buttonDown(uint8_t pageNumber, uint8_t globalMidiChannel, uint32_t buttonNumber) {
    MidiButton* button =  &midiPage_[pageNumber].button_[buttonNumber];
    if (button->buttonType == MIDI_BUTTON_TYPE_PUSH) {
        button->value = 1;
    } else if (button->buttonType == MIDI_BUTTON_TYPE_TOGGLE) {
        button->value = (button->value == 0? 1 : 0);
    }
    uint8_t midiChannel = button->midiChannel == 16 ? globalMidiChannel : button->midiChannel;

    usartBufferOut.insert((uint8_t)(0xb0 + midiChannel));
    usartBufferOut.insert((uint8_t)button->controller);
    usartBufferOut.insert((uint8_t)button->getValue());
    sendMidiDin5Out();
}

bool MidiControllerState::buttonUp(uint8_t pageNumber, uint8_t globalMidiChannel, uint32_t buttonNumber) {
    MidiButton* button =  &midiPage_[pageNumber].button_[buttonNumber];
    if (button->buttonType == MIDI_BUTTON_TYPE_PUSH) {
        button->value = 0;

        uint8_t midiChannel = button->midiChannel == 16 ? globalMidiChannel : button->midiChannel;
        usartBufferOut.insert((uint8_t)(0xb0 + midiChannel));
        usartBufferOut.insert((uint8_t)button->controller);
        usartBufferOut.insert((uint8_t)button->getValue());
        sendMidiDin5Out();
        return true;
    }
    return false;
}


void MidiControllerState::strcpy(char* dest, const char *src, int len) {
    for (int l = 0; l < len; l++) {
        dest[l] = src[l];
    }
}

void MidiControllerState::sendMidiDin5Out() {
    // Enable interupt to send Midi buffer :
    SET_BIT(huart1.Instance->CR1, USART_CR1_TXEIE_TXFNFIE);
}

