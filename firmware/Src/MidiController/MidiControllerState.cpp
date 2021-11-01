/*
 * MidiControllerState.cpp
 *
 *  Created on: 31 oct. 2021
 *      Author: Xavier
 */

#include "stm32h7xx_hal.h"
#include "MidiControllerState.h"
#include "RingBuffer.h"

extern RingBuffer<uint8_t, 64> usartBufferOut;
extern UART_HandleTypeDef huart1;


MidiControllerState::MidiControllerState() {
    for (int i = 0; i < 6; i++) {
        encoder_[i].controller = 21 + i;
        encoder_[i].value = 64;
        encoder_[i].midiChannel = 16;
        encoder_[i].maxValue = 127;

        button_[i].controller = 41 + i;
        button_[i].midiChannel = 16;
        button_[i].valueLow = 0;
        button_[i].valueHigh = 127;
        button_[i].buttonType = (i % 2) == 0 ? MIDI_BUTTON_TYPE_PUSH : MIDI_BUTTON_TYPE_TOGGLE;

        strcpy(encoder_[i].name, "Enc0  ", 5);
        encoder_[i].name[3] = '0' + i;

        strcpy(button_[i].name, "But0  ", 5);
        button_[i].name[3] = '0' + i;
    }
}

MidiControllerState::~MidiControllerState() {
}


void MidiControllerState::encoderDelta(uint8_t globalMidiChannel, uint32_t encoder, int delta) {
    int newValue = encoder_[encoder].value + delta;
    newValue = newValue < encoder_[encoder].minValue ? encoder_[encoder].minValue : newValue;
    newValue = newValue > encoder_[encoder].maxValue ? encoder_[encoder].maxValue : newValue;

    if (newValue != encoder_[encoder].value) {
        encoder_[encoder].value = newValue;
        uint8_t midiChannel = encoder_[encoder].midiChannel == 16 ? globalMidiChannel : encoder_[encoder].midiChannel;
        usartBufferOut.insert((uint8_t)(0xb0 + midiChannel));
        usartBufferOut.insert((uint8_t)encoder_[encoder].controller);
        usartBufferOut.insert((uint8_t)encoder_[encoder].value);
        sendMidiDin5Out();
    }
}



void MidiControllerState::buttonDown(uint8_t globalMidiChannel, uint32_t button) {
    if (button_[button].buttonType == MIDI_BUTTON_TYPE_PUSH) {
        button_[button].value = button_[button].valueHigh;
    } else if (button_[button].buttonType == MIDI_BUTTON_TYPE_TOGGLE) {
        button_[button].value = (button_[button].value == button_[button].valueHigh ? button_[button].valueLow : button_[button].valueHigh);
    }
    uint8_t midiChannel = button_[button].midiChannel == 16 ? globalMidiChannel : button_[button].midiChannel;

    usartBufferOut.insert((uint8_t)(0xb0 + midiChannel));
    usartBufferOut.insert((uint8_t)button_[button].controller);
    usartBufferOut.insert((uint8_t)button_[button].value);
    sendMidiDin5Out();
}

bool MidiControllerState::buttonUp(uint8_t globalMidiChannel, uint32_t button) {
    if (button_[button].buttonType == MIDI_BUTTON_TYPE_PUSH) {
        button_[button].value = button_[button].valueLow;

        uint8_t midiChannel = button_[button].midiChannel == 16 ? globalMidiChannel : button_[button].midiChannel;
        usartBufferOut.insert((uint8_t)(0xb0 + midiChannel));
        usartBufferOut.insert((uint8_t)button_[button].controller);
        usartBufferOut.insert((uint8_t)button_[button].value);
        sendMidiDin5Out();
        return true;
    }
    return false;
}


void MidiControllerState::strcpy(char* dest, char *src, int len) {
    for (int l = 0; l < len; l++) {
        dest[l] = src[l];
    }
}

void MidiControllerState::sendMidiDin5Out() {
    // Enable interupt to send Midi buffer :
    SET_BIT(huart1.Instance->CR1, USART_CR1_TXEIE_TXFNFIE);
}

