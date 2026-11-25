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

#include "Encoders.h"
#include "FirmwareTftDisplay.h"
#include "MidiDecoder.h"
#include "ScreenCoordinator.h"
#include "ScreenEditor.h"
#include "ScreenMenu.h"
#include "ScreenMixer.h"
#include "ScreenSequencer.h"
#include "Sequencer.h"
#include "Storage.h"
#include "Synth.h"
#include "SynthEngineFactory.h"
#include "SynthState.h"
#include "ili9341.h"
#include "stm32h7xx_hal.h"
#include "usb_audio.h"
#include "usb_glue.h"

extern UART_HandleTypeDef huart1;
extern SAI_HandleTypeDef hsai_BlockA1;
extern SAI_HandleTypeDef hsai_BlockB1;
extern SAI_HandleTypeDef hsai_BlockA2;
extern RingBuffer<uint8_t, 64> usartBufferOut;
extern RingBuffer<uint8_t, 64> usartBufferIn;
extern TIM_HandleTypeDef htim1;

#define RAM_D1_SECTION  __attribute__((section(".ram_d1")))
#define RAM_D2_SECTION  __attribute__((section(".ram_d2")))
#define RAM_D2B_SECTION __attribute__((section(".ram_d2b")))
#define RAM_D2C_SECTION __attribute__((section(".ram_d2c")))
#define RAM_D3_SECTION  __attribute__((section(".ram_d3")))

SynthState synthState;
ScreenCoordinator fmDisplay3;
ScreenEditor displayEditor;
ScreenMenu displayMenu;
ScreenMixer displayMixer;
ScreenSequencer displaySequencer;
MidiDecoder midiDecoder;
Encoders encoders;
FirmwareTftDisplay tft RAM_D1_SECTION;
TftAlgo tftAlgo;
Synth synth;
Storage sdCard;
Sequencer sequencer;
// Set by the audio ISR when a timbre clips; the main loop displays CLIPPING.
// One bit per timbre.
uint8_t saturatedOutput = 0;
uint8_t saturatedOutputMem = 0;
bool saturatedOutputDisplayed = 0;
int ili9341NumberOfErrorsOnScreen = 0;
int ili9341NumberOfErrors = 0;

RingBuffer<uint8_t, 64> usbMidi;

// Audio output buffers, one per SAI block. Each is two 64-int32 halves
// (double buffer). The SAI DMA runs circularly and plays one half while the
// audio callbacks refill the other.
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

uint32_t ledCpt = 0;
;
float* timbreSamples;
int previousCpuUsage = 101;
uint8_t previousNumberOfPlayingVoices = 255;

// Defined bellow
void dependencyInjection();

void preenfm3Init() {

    uint32_t erreurSD = preenfm3LibInitSD();

    tft.init(&tftAlgo);
    ILI9341_Init();

    // Engine DSP lookup tables must be initialized once, before the SAI DMA
    // callbacks start, and before the engine voice/timbre state is initialized.
    SynthEngineFactory::initDsp();

    dependencyInjection();

    // The "Usb Audio" config is read at boot: it selects the duplex or the
    // MIDI-only USB descriptor. A reboot is required to apply a change.
    usbInit(synthState.fullState_.midiConfigValue[MIDICONFIG_USB_AUDIO] != 0);

    fmDisplay3.setRefreshStatus(20);

    for (int s = 0; s < 128; s++) {
        waveform1[s] = 0;
        waveform2[s] = 0;
        waveform3[s] = 0;
    }

    // Start the three circular SAI DMA streams. From this point the audio
    // callbacks run and become the clock that drives the whole synth; they
    // must never be starved.
    HAL_SAI_Transmit_DMA(&hsai_BlockA2, (uint8_t*)waveform3, 64 * 2);
    HAL_SAI_Transmit_DMA(&hsai_BlockB1, (uint8_t*)waveform2, 64 * 2);
    HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*)waveform1, 64 * 2);

    timbreSamples = synth.getTimbre(synthState.getCurrentTimbre())->getSampleBlock();

    // Let's start main tft refresh loop
    readyForTFT = true;

    // Prepare Screen before turning on the backlight. It's nicer.
    if (erreurSD == 0) {
        while (fmDisplay3.needRefresh() || tft.getNumberOfPendingActions() > 0) {
            if (fmDisplay3.needRefresh() && tft.getNumberOfPendingActions() < 100) {
                fmDisplay3.refreshAllScreenByStep();
            }
            tft.processActions(TFT_MAX_ACTIONS_PER_LOOP);
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
    // For tft.ticTimer
    HAL_Delay(400);

    // the TFT should be ready, we can turn on the led backlight
    // the TFT should be ready, we can turn on the led backlight
    uint8_t tft_bl = synthState.fullState_.midiConfigValue[MIDICONFIG_TFT_BACKLIGHT];

    TIM1->CCR2 = tft_bl < 10 ? 10 : tft_bl;
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);

    if (erreurSD > 0) {
        HAL_Delay(2000);
        // Refresh all
        fmDisplay3.newSynthMode(&synthState.fullState_);
    }
    HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_RESET);
}

// Main loop (thread mode): runs continuously, no fixed period. Handles work
// that may take variable time or can be deferred: MIDI async actions, encoder
// actions, UI refresh, oscillo, TFT reset/reinit and tft.processActions().
// Some tasks self-pace with HAL_GetTick() (oscillo every 125 ms, CPU display
// every 1 s). Audio does not run here; it runs from the SAI DMA IRQ.
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

    if (unlikely(tftHasJustBeenCleared)) {
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

    if (synthState.fullState_.synthMode == SYNTH_MODE_EDITOR) {
        // Oscillo refresh: 20 Hz = 50 ms
        if (unlikely((currentMillis - oscilloMillis) >= 100)) {
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

    if (synthState.fullState_.midiConfigValue[MIDICONFIG_TFT_AUTO_REINIT] == 1) {
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

    tft.processActions(TFT_MAX_ACTIONS_PER_LOOP);

    if (synthState.fullState_.midiConfigValue[MIDICONFIG_CPU_USAGE]) {
        if (unlikely((currentMillis - cpuUsageMillis) > 1000)) {
            cpuUsageMillis = currentMillis;
            int cpuUsage = (int)(synth.getCpuUsage() * 10.0f);
            uint8_t numberOfPlayingVoices = synth.getNumberOfPlayingVoices();
            if (cpuUsage != previousCpuUsage) {
                previousCpuUsage = cpuUsage;
                tft.setCharBackgroundColor(COLOR_BLACK);
                tft.setCharColor(COLOR_GRAY);
                tft.setCursorInPixel(90, 25);
                int cpuUsage10 = cpuUsage / 10;
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

// Two execution contexts: this 1 kHz SysTick ISR does only short, deterministic
// work that needs an exact millisecond cadence; variable or slow work is
// deferred to preenfm3Loop() in thread mode. Audio is separate: it runs from
// the SAI DMA IRQ (DMA1_Stream0 -> HAL_SAI_Tx*CpltCallback).
//
// Tick map (SysTick runs at 1 kHz):
//   preenfm3Tic()   SysTick ISR entry:
//                     encoders        every 2 ms
//                     sequencer       every 1 ms
//                     tft.ticTimer()  every 1 ms (power check every 500 ms,
//                                     TFT_WAITCYCLE countdown only)
//                     slowTick()      every 256 ms (UI/menu timing; a divider
//                                     off this same 1 kHz tick)
//   tft.processActions()  main loop only: drains TFT actions and starts the
//                         next dirty band. Never call from an ISR.
// Historical note: the old TftDisplay::tic() was split into ticTimer()
// (ISR-safe timing) and processActions() (main-loop drawing).
void preenfm3Tic() {

    if (unlikely(!readyForTFT)) {
        return;
    }

    tftCpt++;

    // check encoder/buttons status : 500 times per seconds
    if ((tftCpt & 0x1) == 0) {
        encoders.checkStatus(synthState.fullState_.midiConfigValue[MIDICONFIG_ENCODER],
                             synthState.fullState_.midiConfigValue[MIDICONFIG_ENCODER_PUSH]);
    }

    // In case of sequencer running on internal BPM
    sequencer.ticMillis();

    // TFT DMA2D opertations
    tft.ticTimer(synthState.fullState_.midiConfigValue[MIDICONFIG_TFT_AUTO_REINIT] == 1);

    // Pfm3 Misc tics
    switch (tftCpt & 0xff) {
    case 0:
        synthState.slowTick();
        fmDisplay3.slowTick();
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

// Runs from the audio ISR only: drain the MIDI byte rings without blocking.
// The rings are filled by the USART and USB interrupts.
void preenfm3DecodeMidiIn() {
    while (usartBufferIn.getCount() > 0) {
        midiDecoder.newByte(usartBufferIn.remove());
    }
    while (usbMidi.getCount() > 0) {
        midiDecoder.newByte(usbMidi.remove());
    }
}

// --- Audio ISR entry points ---------------------------------------------
// Called from the SAI DMA IRQ (DMA1_Stream0) at the audio block rate. They
// produce every sample the synth plays, so they are the hardest real-time
// path in the firmware: short, non-blocking, allocation-free.

// Add the USB playback stream (host -> device) to DAC1. The capture push is
// done before this, so the host never records itself (no loopback).
static void mixUsbPlayback(int32_t* dst) {
    int32_t pb[BLOCK_SIZE * 2];
    usbAudioPlaybackPull(pb, BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE * 2; i++) {
        int32_t v = dst[i] + pb[i];
        if (v > 0x7FFFFF00) {
            v = 0x7FFFFF00;
        } else if (v < -0x7FFFFF00) {
            v = -0x7FFFFF00;
        }
        dst[i] = v;
    }
}

// Audio ISR (DMA complete): the first half is playing, so refill the second
// half. Missing this deadline glitches the audio.
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef* hsai) {
    if (hsai == &hsai_BlockA1) {
        preenfm3DecodeMidiIn();

        saturatedOutput |= synth.buildNewSampleBlock(&waveform1[64], &waveform2[64], &waveform3[64]);
        usbAudioPush(&waveform1[64], &waveform2[64], &waveform3[64], BLOCK_SIZE);
        mixUsbPlayback(&waveform1[64]);
        __DSB();
        tft.oscilloRecord32Samples(timbreSamples);
    }
}

// Audio ISR (DMA half complete): the second half is playing, so refill the
// first half. Together with TxCplt this is the most time-critical entry point
// in the firmware; never block or allocate here.
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef* hsai) {
    if (hsai == &hsai_BlockA1) {
        preenfm3DecodeMidiIn();

        saturatedOutput |= synth.buildNewSampleBlock(waveform1, waveform2, waveform3);
        usbAudioPush(waveform1, waveform2, waveform3, BLOCK_SIZE);
        mixUsbPlayback(waveform1);
        __DSB();
        tft.oscilloRecord32Samples(timbreSamples);
    }
}

/**
 * @brief Tx Transfer completed callback.
 * @param  hspi: pointer to a SPI_HandleTypeDef structure that contains
 *               the configuration information for SPI module.
 * @retval None
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    // Unselect TFT as soon as the transfer is finished to avoid noise on data line
    if (hspi == &hspi1) {
        tft.pushToTftFinished();
    }
}

extern "C" void tftSpiError() {
    tft.pushToTftError();
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
        if (synthState.mixerState_.midiThru_ != 0) {
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

    synthState.init(&displayMixer, &displayEditor, &displayMenu, &displaySequencer);

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

    // pfm3 : synthState needs to know all timbre params names
    synthState.setTimbres(synth.getTimbres());

    sdCard.init(synth.getTimbre(0), synth.getTimbre(1), synth.getTimbre(2), synth.getTimbre(3), synth.getTimbre(4), synth.getTimbre(5));

    sdCard.getMixerBank()->setMixerState(&synthState.mixerState_);
    sdCard.getMixerBank()->setScalaFile(sdCard.getScalaFile());
    sdCard.getMixerBank()->setSequencer(&sequencer);
    sdCard.getSequenceBank()->setSequencer(&sequencer);

    // Load preferences
    sdCard.getConfigurationFile()->loadConfig(synthState.fullState_.midiConfigValue);
    sdCard.getMixerBank()->loadDefaultMixer();
    sdCard.getSequenceBank()->loadDefaultSequence();
    sdCard.getUserWaveform()->loadUserWaveforms();
    sdCard.getUserEnvCurve()->loadUserEnvCurves();
    synthState.propagateAfterNewMixerLoad();

    // To call after config loaded
    sdCard.getPatchBank()->setArpeggiatorPartOfThePreset(&synthState.fullState_.midiConfigValue[MIDICONFIG_ARPEGGIATOR_IN_PRESET]);
    displayMixer.setReverbParamVisible(synthState.fullState_.midiConfigValue[MIDICONFIG_REVERB_PARAMS] > 0);
}

void preenfm3_usbDataReceive(uint8_t* buffer) {
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
                if (synthState.mixerState_.midiThru_ != 0) {
                    usartBufferOut.insert(buffer[usbr + 1]);
                    usartBufferOut.insert(buffer[usbr + 2]);
                    usartBufferOut.insert(buffer[usbr + 3]);
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
                if (synthState.mixerState_.midiThru_ != 0) {
                    usartBufferOut.insert(buffer[usbr + 1]);
                    usartBufferOut.insert(buffer[usbr + 2]);
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
                if (synthState.mixerState_.midiThru_ != 0) {
                    usartBufferOut.insert(buffer[usbr + 1]);
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

float getCompInstrumentThreshold(int t) {
    return synth.getCompInstrument(t).getThresh();
}

#ifdef __cplusplus
}
#endif
