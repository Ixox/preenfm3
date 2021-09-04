/*
 * Copyright 2019 Xavier Hosxe
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
#include "fatfs.h"
#include "Synth.h"
#include "SynthState.h"
#include "MidiDecoder.h"
#include "FMDisplay3.h"
#include "FMDisplayMixer.h"
#include "FMDisplayEditor.h"
#include "FMDisplayMenu.h"
#include "FMDisplaySequencer.h"
#include "TftDisplay.h"
#include "ili9341.h"
#include "Storage.h"
#include "Encoders.h"
#include "Hexter.h"
#include "Sequencer.h"

extern UART_HandleTypeDef huart1;
extern SAI_HandleTypeDef hsai_BlockA1;
extern SAI_HandleTypeDef hsai_BlockB1;
extern SAI_HandleTypeDef hsai_BlockA2;
extern RingBuffer<uint8_t, 64> usartBufferOut;
extern RingBuffer<uint8_t, 64> usartBufferIn;
extern TIM_HandleTypeDef htim1;

#define RAM_D1_SECTION __attribute__((section(".ram_d1")))
#define RAM_D2_SECTION __attribute__((section(".ram_d2")))
#define RAM_D2B_SECTION __attribute__((section(".ram_d2b")))
#define RAM_D3_SECTION __attribute__((section(".ram_d3")))

SynthState synthState;
FMDisplay3 fmDisplay3;
FMDisplayEditor displayEditor;
FMDisplayMenu displayMenu;
FMDisplayMixer displayMixer;
FMDisplaySequencer displaySequencer;
MidiDecoder midiDecoder;
Encoders encoders;
TftDisplay tft;
TftAlgo tftAlgo;
Synth synth;
Storage sdCard;
Hexter hexter;
Sequencer sequencer;
uint8_t saturatedOutput = 0;
uint8_t saturatedOutputMem = 0;
bool saturatedOutputDisplayed = 0;
int ili9341NumberOfErrorsOnScreen = 0;
int ili9341NumberOfErrors = 0;

RingBuffer<uint8_t, 64> usbMidi;

RAM_D2_SECTION int32_t waveform1[64 * 2];
RAM_D2_SECTION int32_t waveform2[64 * 2];
RAM_D2_SECTION int32_t waveform3[64 * 2];

#ifdef __cplusplus
extern "C" {
#endif
#include "preenfm3lib.h"

volatile bool readyForTFT = false;
uint32_t tftCpt = 0;
uint32_t oscilloMillis = 1;
uint32_t cpuUsageMillis = 1;
uint32_t saturatedOutputMillis = 0;


uint32_t ledCpt = 0;;
float *timbreSamples;
int previousCpuUsage = 101;
uint8_t previousNumberOfPlayingVoices = 255;

// Defined bellow
void dependencyInjection();

void preenfm3Init() {

    uint32_t erreurSD = preenfm3LibInitSD();

    tft.init(&tftAlgo);
    ILI9341_Init();

    dependencyInjection();

    fmDisplay3.setRefreshStatus(20);

    for (int s = 0; s < 128; s++) {
        waveform1[s] = 0;
        waveform2[s] = 0;
        waveform3[s] = 0;
    }

    HAL_SAI_Transmit_DMA(&hsai_BlockA2, (uint8_t *) waveform3, 64 * 2);
    HAL_SAI_Transmit_DMA(&hsai_BlockB1, (uint8_t *) waveform2, 64 * 2);
    HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *) waveform1, 64 * 2);

    timbreSamples = synth.getTimbre(synthState.getCurrentTimbre())->getSampleBlock();

    // Let's start main tft refresh loop
    readyForTFT = true;

    // Prepare Screen before turning on the backlight. It's nicer.
    if (erreurSD == 0) {
        while (fmDisplay3.needRefresh()) {
            if (tft.getNumberOfPendingActions() < 100) {
                fmDisplay3.refreshAllScreenByStep();
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
        fmDisplay3.newSynthMode(&synthState.fullState);
    }
    HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_RESET);
}


void preenfm3Loop() {
    uint32_t currentMillis = HAL_GetTick();

    /*
     * We process here the midi action the cannot be done in the high priori audio loop
     */
    midiDecoder.processAsyncActions();

    // Always process actions if any from the encoders and button
    encoders.processActions();

    bool tftHasJustBeenCleared = tft.hasJustBeenCleared();

    if (unlikely(saturatedOutput > 0)) {
        saturatedOutputMillis = currentMillis + 500;
        saturatedOutputMem = saturatedOutput;
        saturatedOutput = 0;
    }
    if (unlikely(saturatedOutputMillis > currentMillis)) {
        if (unlikely(!saturatedOutputDisplayed)) {
            tft.fillArea(0, 20, 240, 20, COLOR_RED);

            tft.setCharBackgroundColor(COLOR_RED);
            tft.setCharColor(COLOR_BLACK);

            tft.setCursor(6, 1);
            tft.print("CLIPPING ");
            for (int o = 0; o < 3; o++) {
                if ((saturatedOutputMem & (1 << o)) > 0) {
                    tft.print(o + 1);
                }
            }
            saturatedOutputDisplayed = true;
        }
    } else {
        if (unlikely(saturatedOutputDisplayed)) {
            tft.fillArea(0, 20, 240, 20, COLOR_BLACK);
            saturatedOutputDisplayed = false;
        }
    }

    if (unlikely(tftHasJustBeenCleared))  {
        tft.resetHasJustBeenCleared();
        // force cpu usage refresh
        cpuUsageMillis = 0;
        previousCpuUsage = 101;
    }

    if (fmDisplay3.needRefresh() && tft.getNumberOfPendingActions() < 100) {
        while (fmDisplay3.needRefresh() && tft.getNumberOfPendingActions() < 100) {
            fmDisplay3.refreshAllScreenByStep();
        }
    }

    if (synthState.fullState.synthMode == SYNTH_MODE_EDIT_PFM3) {
        // Oscillo refreqh : 8 Hz = 125 ms
        if (unlikely((currentMillis - oscilloMillis) >= 125)) {
            oscilloMillis = currentMillis;
            tft.oscilloRrefresh();
            float lf = synth.getLowerNoteFrequency(synthState.getCurrentTimbre());
			tft.oscilloNewLowerFrequency(lf);
            timbreSamples = synth.getTimbre(synthState.getCurrentTimbre())->getSampleBlock();
        }
    } else {
        tft.oscilloForceNextDisplay();
        // Leave some time before redrawing the oscillo when whe are back to EDIT mode
        // so that it's not erased just after beeing displayed once
        oscilloMillis = currentMillis;
    }


    if (synthState.fullState.midiConfigValue[MIDICONFIG_TFT_AUTO_REINIT] == 1) {
        // Detect TFT errors
        if (unlikely(tft.mustBeReset())) {
            tft.reset();
            fmDisplay3.setRefreshStatus(21);
            ili9341NumberOfErrors++;
        }

        if (ili9341NumberOfErrors != ili9341NumberOfErrorsOnScreen || tftHasJustBeenCleared) {
            ili9341NumberOfErrorsOnScreen = ili9341NumberOfErrors;
            tft.setCharColor(COLOR_GRAY);
            tft.setCursorInPixel(5, 25);
            tft.printSmallChar(ili9341NumberOfErrors);
        }
    }

    if (synthState.fullState.midiConfigValue[MIDICONFIG_CPU_USAGE]) {
        if (unlikely((currentMillis - cpuUsageMillis) > 1000)) {
            cpuUsageMillis = currentMillis;
            int cpuUsage = (int)(synth.getCpuUsage() * 10.0f);
            uint8_t numberOfPlayingVoices = synth.getNumberOfPlayingVoices();
            if (cpuUsage != previousCpuUsage) {
                previousCpuUsage = cpuUsage;
                tft.setCharBackgroundColor(COLOR_BLACK);
                tft.setCharColor(COLOR_GRAY);
                tft.setCursorInPixel(90,25);
                int  cpuUsage10 = cpuUsage / 10;
                if (cpuUsage10 < 10) {
                    tft.printSmallChar('0');
                }
                tft.printSmallChar(cpuUsage10);
                tft.printSmallChar('.');
                cpuUsage -= cpuUsage10 * 10.0f;
                tft.printSmallChar((int)cpuUsage);
                tft.printSmallChar('%');
            }

            if (numberOfPlayingVoices != previousNumberOfPlayingVoices || tftHasJustBeenCleared) {
                tft.setCharBackgroundColor(COLOR_BLACK);
                tft.setCharColor(COLOR_GRAY);
                previousNumberOfPlayingVoices = numberOfPlayingVoices;
                tft.setCursorInPixel(150, 25);
                if (numberOfPlayingVoices < 10) {
                    tft.printSmallChar(' ');
                }
                tft.printSmallChar((int)numberOfPlayingVoices);
            }
        }
    }
}

void preenfm3Tic() {

    if (unlikely(!readyForTFT)) {
        return;
    }

	tftCpt++;

	// check encoder/buttons status : 500 times per seconds
    if ((tftCpt & 0x1) == 0) {
        encoders.checkStatus(synthState.fullState.midiConfigValue[MIDICONFIG_ENCODER], synthState.fullState.midiConfigValue[MIDICONFIG_ENCODER_PUSH]);
    }

    // In case of sequencer running on internal BPM
    sequencer.ticMillis();

    // TFT DMA2D opertations
    tft.tic(synthState.fullState.midiConfigValue[MIDICONFIG_TFT_AUTO_REINIT] == 1);

    // Pfm3 Misc tics
    switch (tftCpt & 0xff) {
    case 0:
        synthState.tempoClick();
        fmDisplay3.tempoClick();
        break;
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

void preenfm3DecodeMidiIn() {
    while (usartBufferIn.getCount() > 0) {
        midiDecoder.newByte(usartBufferIn.remove());
    }
    while (usbMidi.getCount() > 0) {
        midiDecoder.newByte(usbMidi.remove());
    }
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
    if (hsai == &hsai_BlockA1) {
        preenfm3DecodeMidiIn();

        saturatedOutput |= synth.buildNewSampleBlock(&waveform1[64], &waveform2[64], &waveform3[64]);
        tft.oscilloRecord32Samples(timbreSamples);
    }
}

/**
 * @brief Rx Transfer half completed callback.
 * @param  hsai pointer to a SAI_HandleTypeDef structure that contains
 *              the configuration information for SAI module.
 * @retval None
 */
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
    if (hsai == &hsai_BlockA1) {
        preenfm3DecodeMidiIn();

        saturatedOutput |= synth.buildNewSampleBlock(waveform1, waveform2, waveform3);
        tft.oscilloRecord32Samples(timbreSamples);
    }
}

/**
  * @brief Tx Transfer completed callback.
  * @param  hspi: pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @retval None
  */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    // Unselect TFT as soon as the transfer is finished to avoid noise on data line
    if (hspi == &hspi1) {
        tft.pushToTftFinished();
    }
}


/*
 * Deal with Receive fifo not empty => new midi byte to read
 * Deal with Transmit fito not full => send new midi byte if any
 */
void preenfm3_USART1_IRQHandler() {
    // huart1
    // while fifo not empty, we have something to read
    while (READ_BIT(huart1.Instance->ISR, USART_ISR_RXNE_RXFNE) != 0) {
        uint8_t midiByte = huart1.Instance->RDR;
        usartBufferIn.insert(midiByte);
        // Insert in usartBufferOut if midi through
        if (synthState.mixerState.midiThru_ != 0) {
            // Can we insert now ? if TX fifo not full
            if (READ_BIT(huart1.Instance->ISR, USART_ISR_TXE_TXFNF) != 0) {
                huart1.Instance->TDR = midiByte;
            } else {
                usartBufferOut.insert(midiByte);
                SET_BIT(huart1.Instance->CR1, USART_CR1_TXEIE_TXFNFIE);
            }
        }
    }
    // Are we supposed to send midi
    if (READ_BIT(huart1.Instance->CR1, USART_CR1_TXEIE_TXFNFIE) != 0) {
        while (((huart1.Instance->ISR & USART_ISR_TXE_TXFNF) != 0U) && (usartBufferOut.getCount() > 0)) {
            huart1.Instance->TDR = usartBufferOut.remove();
        }
        // If out buffer empty, we're not supposed anymore to send midi
        if (usartBufferOut.getCount() == 0) {
            CLEAR_BIT(huart1.Instance->CR1, USART_CR1_TXEIE_TXFNFIE);
        }
    }

    // Must clear Overflow IRQ if any to avoid freeze (should never happen)
    if (READ_BIT(huart1.Instance->ISR, USART_ISR_ORE) != 0) {
        SET_BIT(huart1.Instance->ICR, USART_ICR_ORECF);
    }

}

void dependencyInjection() {
    // ---------------------------------------
    // Dependencies Injection

    // to SynthStateAware Class
    // MidiDecoder, Synth (Env,Osc, Lfo, Matrix, Voice ), tft, PresetUtil...

    synthState.setDisplays(&displayMixer, &displayEditor, &displayMenu, &displaySequencer);

    synth.setSynthState(&synthState);

    // synth and sequencer must know each other
    synth.setSequencer(&sequencer);
    sequencer.setSynth(&synth);
    sequencer.setDisplaySequencer(&displaySequencer);
    displaySequencer.setSequencer(&sequencer);

    fmDisplay3.setSynthState(&synthState);
    fmDisplay3.init(&tft);
    fmDisplay3.setDisplays(&displayMixer, &displayEditor, &displayMenu, &displaySequencer);
    midiDecoder.setSynthState(&synthState);
    midiDecoder.setVisualInfo(&fmDisplay3);
    midiDecoder.setSynth(&synth);
    midiDecoder.setStorage(&sdCard);


    // Init child display
    displayMixer.init(&synthState, &tft);
    displayMenu.init(&synthState, &tft, &sdCard);
    displayEditor.init(&synthState, &tft);
    displaySequencer.init(&synthState, &tft);


    // ---------------------------------------
    // Register listener

    // synthstate is updated by encoder change
    encoders.insertListener(&synthState);

    // tft and synth needs to be aware of synthState changes
    // synth must be first one, can modify param new value
    /// order of param listener is important... synth must be called first so it's inserted last.
    synthState.insertParamListener(&fmDisplay3);
    synthState.insertParamListener(&midiDecoder);
    synthState.insertParamListener(&synth);
    synthState.insertMenuListener(&fmDisplay3);
    //    synthState.insertMenuListener(&synth); <====== CVIN ONLY

    synthState.setStorage(&sdCard);
    synthState.setHexter(&hexter);

    // pfm3 : synthState needs to know all timbre params names
    synthState.setTimbres(synth.getTimbres());

    sdCard.init(synth.getTimbre(0)->getParamRaw(), synth.getTimbre(1)->getParamRaw(), synth.getTimbre(2)->getParamRaw(),
            synth.getTimbre(3)->getParamRaw(), synth.getTimbre(4)->getParamRaw(), synth.getTimbre(5)->getParamRaw());

    sdCard.getMixerBank()->setMixerState(&synthState.mixerState);
    sdCard.getMixerBank()->setScalaFile(sdCard.getScalaFile());
    sdCard.getMixerBank()->setSequencer(&sequencer);
    sdCard.getSequenceBank()->setSequencer(&sequencer);

    // Load preferences
    sdCard.getConfigurationFile()->loadConfig(synthState.fullState.midiConfigValue);
    sdCard.getMixerBank()->loadDefaultMixer();
    sdCard.getSequenceBank()->loadDefaultSequence();
    sdCard.getUserWaveform()->loadUserWaveforms();
    synthState.propagateAfterNewMixerLoad();


    // To call after config loaded
    sdCard.getPatchBank()->setArpeggiatorPartOfThePreset(&synthState.fullState.midiConfigValue[MIDICONFIG_ARPEGGIATOR_IN_PRESET]);
    displayMixer.setReverbParamVisible(synthState.fullState.midiConfigValue[MIDICONFIG_REVERB_PARAMS] > 0);

}

void preenfm3_usbDataReceive(uint8_t *buffer) {
    int usbr = 0;
    while (buffer[usbr] != 0 && usbr < 64) {
        // Cable 0
        if ((buffer[usbr] >> 4) == 0) {
            switch (buffer[usbr] & 0xf) {
            // ========= 3 bytes =======================
            case 0x9:
            case 0x8:
            case 0xb:
            case 0xa:
            case 0xe:
            case 0x3:
                if (synthState.mixerState.midiThru_ != 0) {
                    usartBufferOut.insert(buffer[usbr+1]);
                    usartBufferOut.insert(buffer[usbr+2]);
                    usartBufferOut.insert(buffer[usbr+3]);
                    SET_BIT(huart1.Instance->CR1, USART_CR1_TXEIE_TXFNFIE);
                }
                // Sysex - No thru
            case 0x4:
            case 0x7:
                usbMidi.insert(buffer[usbr + 1]);
                usbMidi.insert(buffer[usbr + 2]);
                usbMidi.insert(buffer[usbr + 3]);
                break;
                // ========= 2 bytes =======================
            case 0xc:
            case 0xd:
            case 0x2:
                if (synthState.mixerState.midiThru_ != 0) {
                    usartBufferOut.insert(buffer[usbr+1]);
                    usartBufferOut.insert(buffer[usbr+2]);
                    SET_BIT(huart1.Instance->CR1, USART_CR1_TXEIE_TXFNFIE);
                }
                // Sysex - No thru
            case 0x6:
                usbMidi.insert(buffer[usbr + 1]);
                usbMidi.insert(buffer[usbr + 2]);
                break;
                // ========= 1 byte =======================
            case 0xF:
                usbMidi.insert(buffer[usbr + 1]);
                if (synthState.mixerState.midiThru_ != 0) {
                    usartBufferOut.insert(buffer[usbr+1]);
                    SET_BIT(huart1.Instance->CR1, USART_CR1_TXEIE_TXFNFIE);
                }
                // Sysex - No thru
            case 0x5:
                break;
            }
        }
        buffer[usbr] = 0;
        usbr += 4;
    }
}

float getCompInstrumentVolume(int t) {
    return synth.getCompInstrument(t).getCurrentVolume();
}

float getCompInstrumentGainReduction(int t) {
    return synth.getCompInstrument(t).getCurrentGainReduction();
}


#ifdef __cplusplus
}
#endif

