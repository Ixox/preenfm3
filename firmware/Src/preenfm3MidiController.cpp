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
#include "SynthState.h"
#include "TftDisplay.h"
#include "FMDisplayMidiController.h"
#include "MidiControllerState.h"
#include "MidiControllerFile.h"
#include "TftDisplay.h"
#include "Storage.h"
#include "ili9341.h"
#include "Menu.h"
#include "Encoders.h"
#include "preenfm3lib.h"

#ifdef __cplusplus
extern "C" {
#endif


extern Storage sdCard;
extern UART_HandleTypeDef huart1;
extern RingBuffer<uint8_t, 64> usartBufferOut;
extern RingBuffer<uint8_t, 64> usartBufferIn;
extern TIM_HandleTypeDef htim1;
extern TftDisplay tft;
extern SynthState synthState;
extern Encoders encoders;


extern volatile bool readyForTFT;
extern uint32_t tftCpt;
extern uint32_t ledCpt;

void preenfm3MidiControllerDI();

FMDisplayMidiController display;
MidiControllerState midiControllerState;
MidiControllerFile midiControllerFile;

void preenfm3MidiControllerInit() {
    preenfm3MidiControllerDI();

    uint32_t erreurSD = preenfm3LibInitSD();

    tft.init(NULL);
    ILI9341_Init();
    display.init(&tft, &midiControllerState, &midiControllerFile);

    display.setResetRefreshStatus();

    // Let's start main tft refresh loop
    readyForTFT = true;

    // Prepare Screen before turning on the backlight. It's nicer.
    if (erreurSD == 0) {
        while (display.needRefresh()) {
            if (tft.getNumberOfPendingActions() < 100) {
                display.refreshAllScreenByStep();
            }
        }
    } else {
        tft.clear();
        tft.setCharBackgroundColor(COLOR_BLACK);
        tft.setCharColor(COLOR_RED);
        tft.setCursor(5, 7);
        tft.print("SD CARD ERROR");
        tft.setCursor(8, 8);
        tft.print("#");
        tft.print((int)erreurSD);
        tft.print("#");
    }
    // For tft.tic
    HAL_Delay(400);

    // the TFT should be ready, we can turn on the led backlight
    // the TFT should be ready, we can turn on the led backlight
    uint8_t tft_bl =  synthState.fullState.midiConfigValue[MIDICONFIG_TFT_BACKLIGHT];

    TIM1->CCR2 = tft_bl < 10 ? 10 : tft_bl;
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);

    if (erreurSD > 0) {
        HAL_Delay(2000);
        // Refresh all
    }
    HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_RESET);
}


void preenfm3MidiControllerLoop() {
    encoders.processActions();

    if (display.needRefresh() && tft.getNumberOfPendingActions() < 100) {
        while (display.needRefresh() && tft.getNumberOfPendingActions() < 100) {
            display.refreshAllScreenByStep();
        }
    }

}


void preenfm3MidiControllerTic() {
    if (unlikely(!readyForTFT)) {
        return;
    }

    tftCpt++;

    // check encoder/buttons status : 500 times per seconds
    if ((tftCpt & 0x1) == 0) {
        encoders.checkStatusUpDown(synthState.fullState.midiConfigValue[MIDICONFIG_ENCODER], synthState.fullState.midiConfigValue[MIDICONFIG_ENCODER_PUSH]);
    }

    // TFT DMA2D opertations
    tft.tic(synthState.fullState.midiConfigValue[MIDICONFIG_TFT_AUTO_REINIT] == 1);

    // Pfm3 Misc tics
    switch (tftCpt & 0xff) {
    case 1:
        ledCpt++;
        if (unlikely(ledCpt == 1)) {
            PFM_SET_PIN(GPIOE, LED_TEST_Pin);
        } else if (unlikely(ledCpt == 2)) {
            PFM_CLEAR_PIN(GPIOE, LED_TEST_Pin);
        } else if (unlikely(ledCpt == 10)) {
            ledCpt = 0;
        }
        break;
    default:
        break;
    }
}

// Dependency infection
void preenfm3MidiControllerDI() {
    // ---------------------------------------
    // Dependencies Injection

    // to SynthStateAware Class
    // MidiDecoder, Synth (Env,Osc, Lfo, Matrix, Voice ), tft, PresetUtil...

    sdCard.init(NULL, NULL, NULL, NULL, NULL, NULL);

    // Load preferences
    sdCard.getConfigurationFile()->loadConfig(synthState.fullState.midiConfigValue);
    sdCard.getMixerBank()->loadDefaultMixer();
    sdCard.getSequenceBank()->loadDefaultSequence();
    sdCard.getUserWaveform()->loadUserWaveforms();

    encoders.insertListener(&display);

}

#ifdef __cplusplus
}
#endif

