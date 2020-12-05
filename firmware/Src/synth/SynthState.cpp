/*
 * Copyright 2013 Xavier Hosxe
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
#include "FMDisplayMixer.h"
#include "FMDisplayMenu.h"
#include "FMDisplayEditor.h"
#include "FMDisplaySequencer.h"
#include "SynthState.h"
#include "Hexter.h"
#include "Timbre.h"
#include "Synth.h"
#include "preenfm3lib.h"

extern Synth synth;
extern RNG_HandleTypeDef hrng;
extern float diatonicScaleFrequency[];



SynthState::SynthState() {
    operatorNumber = 0;

    // First default preset
    fullState.synthMode = SYNTH_MODE_MIXER;
    fullState.preenFMBankNumber = 0;
    fullState.preenFMPresetNumber = 0;
    fullState.preenFMMixerNumber = 0;
    fullState.preenFMMixerPresetNumber = 0;
    fullState.dx7BankNumber = 0;
    fullState.dx7PresetNumber = 0;
    // Default pfm3 edit page
    fullState.mainPage = -1;
    fullState.editPage = 0;

    // https://en.wikipedia.org/wiki/C_(musical_note)
    // Frequency of note 60 C4 :
    fullState.midiConfigValue[MIDICONFIG_USB] = 2;
    fullState.midiConfigValue[MIDICONFIG_RECEIVES] = 3;
    fullState.midiConfigValue[MIDICONFIG_SENDS] = 1;
    fullState.midiConfigValue[MIDICONFIG_PROGRAM_CHANGE] = 1;
    fullState.midiConfigValue[MIDICONFIG_TEST_NOTE] = 60;
    fullState.midiConfigValue[MIDICONFIG_TEST_VELOCITY] = 120;
    fullState.midiConfigValue[MIDICONFIG_ENCODER] = 1;
    fullState.midiConfigValue[MIDICONFIG_ARPEGGIATOR_IN_PRESET] = 0;
    fullState.midiConfigValue[MIDICONFIG_CPU_USAGE] = 0;
    fullState.midiConfigValue[MIDICONFIG_TFT_AUTO_REINIT] = 0;
    fullState.midiConfigValue[MIDICONFIG_ENCODER_PUSH] = 0;
    // Init randomizer values to 1
    fullState.randomizer.Oper = 1;
    fullState.randomizer.EnvT = 1;
    fullState.randomizer.IM = 1;
    fullState.randomizer.Modl = 1;

    // Mixer
    fullState.mixerCurrentEdit = 0;
    fullState.menuCurrentEdit = 0;

    for (int k = 0; k < 12; k++) {
        fullState.name[k] = 0;
    }

    // edit with timbre 0
    currentTimbre = 0;
    stepSelect[0] = 0;
    stepSelect[1] = 0;
    patternSelect = 0;

    isPlayingNote = false;

    for (int row = 0; row < NUMBER_OF_ROWS; row++) {
        for (int param = 0; param < NUMBER_OF_ENCODERS_PFM2; param++) {
            struct ParameterDisplay* paramDisplay = &(allParameterRows.row[row]->params[param]);
            if (paramDisplay->numberOfValues > 1.0) {
                paramDisplay->incValue = ((paramDisplay->maxValue - paramDisplay->minValue) / (paramDisplay->numberOfValues - 1.0f));
            } else {
                paramDisplay->incValue = 0.0f;
            }
        }
    }

    // Init mixer state with default values
    char mixerStateChars[sizeof(mixerState)];
    uint32_t size;
    mixerState.getFullDefaultState(mixerStateChars, &size, 0);
    mixerState.restoreFullState(mixerStateChars);

    for (int b = 0; b < NUMBER_OF_BUTTONIDS; b++) {
        fullState.buttonState[b] = 0;
    }
    fullState.operatorNumber = 0;
}

void SynthState::setDisplays(FMDisplayMixer* displayMixer, FMDisplayEditor* displayEditor, FMDisplayMenu* displayMenu, FMDisplaySequencer* displaySequencer) {
    this->displayMixer = displayMixer;
    this->displayEditor = displayEditor;
    this->displayMenu = displayMenu;
    this->displaySequencer = displaySequencer;
}


void SynthState::encoderTurnedForStepSequencer(int row, int encoder4, int encoder6, int ticks) {
    int whichStepSeq = row - ROW_LFOSEQ1;
    StepSequencerSteps * seqSteps = &((StepSequencerSteps *) (&params->lfoSteps1))[whichStepSeq];

    if (encoder6 == 3) {
        int oldPos = stepSelect[whichStepSeq];
        int newValue = stepSelect[whichStepSeq] + (ticks > 0 ? 1 : -1);

        if (newValue > 15) {
            newValue = 0;
        } else if (newValue < 0) {
            newValue = 15;
        }

        stepSelect[whichStepSeq] = newValue;
        propagateNewParamValue(currentTimbre, row, encoder4, (ParameterDisplay*) NULL, oldPos, stepSelect[whichStepSeq]);

    } else if (encoder6 == 4) {
        char *step = &seqSteps->steps[(int) stepSelect[whichStepSeq]];
        int oldValue = (int) (*step);

        int newValue = (*step) + ticks;

        if (newValue > 15) {
            newValue = 15;
        } else if (newValue < 0) {
            newValue = 0;
        }
        if ((*step) != newValue) {
            (*step) = newValue;
            propagateNewParamValue(currentTimbre, row, encoder4, (ParameterDisplay*) NULL, oldValue, (int) (*step));
        }
    }
}

void SynthState::encoderTurnedForArpPattern(int row, int encoder, int ticks) {
    if (encoder == 0) {
        // Encoder 0: move cursor
        int oldPos = patternSelect;
        int newPos = oldPos;
        newPos += (ticks > 0 ? 1 : -1);

        if (newPos > 15) {
            newPos = 0;
        } else if (newPos < 0) {
            newPos = 15;
        }
        if (newPos != oldPos) {
            patternSelect = newPos;
            propagateNewParamValue(currentTimbre, row, encoder, (ParameterDisplay*) NULL, oldPos, patternSelect);
        }

    } else if (encoder == 1) {
        // Change value(s)
        if (params->engineArp2.pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT) {
            return;
        }
        arp_pattern_t pattern = params->engineArpUserPatterns.patterns[(int) params->engineArp2.pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT];
        const uint16_t oldMask = ARP_PATTERN_GETMASK(pattern);
        uint16_t newMask = oldMask;

        uint16_t bitsToModify = 0;
        switch (encoder) {
        case 1:
            bitsToModify = 0x1 << patternSelect;
            break;	   // modify single note
        case 2:
            bitsToModify = 0x1111 << (patternSelect & 3);
            break; // modify all
        case 3:
            bitsToModify = 0xf << ((patternSelect >> 2) << 2);
            break; // modify entire bar
        }
        if (ticks > 0) {
            newMask |= bitsToModify;
        } else {
            newMask &= ~bitsToModify;
        }

        if (oldMask != newMask) {
            ARP_PATTERN_SETMASK(pattern, newMask);
            params->engineArpUserPatterns.patterns[(int) params->engineArp2.pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT] = pattern;
            propagateNewParamValue(currentTimbre, row, encoder, (ParameterDisplay*) NULL, oldMask, newMask);
        }
    }
}

void SynthState::twoButtonsPressed(int button1, int button2) {
    switch (button1) {
    case BUTTON_PFM3_MENU:
        switch (button2) {
            case BUTTON_PFM3_1:
                propagateNoteOn(-2);
                break;
            case BUTTON_PFM3_2:
                propagateNoteOn(0);
                break;
            case BUTTON_PFM3_3:
                propagateNoteOn(2);
                break;
            case BUTTON_PFM3_4:
                propagateNoteOn(-14);
                break;
            case BUTTON_PFM3_5:
                propagateNoteOn(-12);
                break;
            case BUTTON_PFM3_6:
                propagateNoteOn(-10);
                break;
            case BUTTON_PREVIOUS_INSTRUMENT: {
            	SynthEditMode previousMode = fullState.synthMode;
                fullState.synthMode = SYNTH_MODE_REINIT_TFT;
                propagateNewPfm3Page();
                fullState.synthMode = previousMode;
                propagateNewPfm3Page();
                break;
            }
        }
        break;
    case BUTTON_NEXT_INSTRUMENT:
        if (button2 == BUTTON_PREVIOUS_INSTRUMENT) {
            propagateNoteOff();
            propagateBeforeNewParamsLoad(currentTimbre);
            propagateAfterNewParamsLoad(currentTimbre);
        }
        break;
    case BUTTON_PREVIOUS_INSTRUMENT:
        if (button2 == BUTTON_NEXT_INSTRUMENT) {
            propagateNoteOff();
            propagateBeforeNewParamsLoad(currentTimbre);
            propagateAfterNewParamsLoad(currentTimbre);
        }
        break;
    }



#ifdef PPMIMAGE_ENABLE
    // Screenshot !!
    if (button1 == BUTTON_PFM3_MENU && button2 == BUTTON_PFM3_SEQUENCER) {
        storage->getPPMImage()->saveImage();
        propagateNewPfm3Page();
    }
#endif

#ifdef DEBUG__KO
    if (button1 == BUTTON_LFO) {
        if (button2 == BUTTON_MATRIX) {
            synth.debugVoice();
        }
        if (button2 == BUTTON_BACK) {
            synth.showCycles();
        }
        if (button2 == BUTTON_MENUSELECT) {
            storage->getPatchBank()->testMemoryPreset();
        }
    }
#endif

}

void SynthState::encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) {
    if (button == BUTTON_NEXT_INSTRUMENT) {
        encoderTurned(encoder, ticks * 10);
        return;
    }

    if (fullState.synthMode == SYNTH_MODE_EDIT_PFM3) {
        displayEditor->encoderTurnedWhileButtonPressed(encoder, ticks, button);
    }
}

bool SynthState::newRandomizerValue(int encoder, int ticks) {
    int8_t oldValue = 0;
    int8_t newValue = 0;
    switch (encoder) {
    case 0:
        oldValue = fullState.randomizer.Oper;
        fullState.randomizer.Oper += (ticks > 0 ? 1 : -1);
        fullState.randomizer.Oper = fullState.randomizer.Oper > 3 ? 3 : fullState.randomizer.Oper;
        fullState.randomizer.Oper = fullState.randomizer.Oper < 0 ? 0 : fullState.randomizer.Oper;
        newValue = fullState.randomizer.Oper;
        break;
    case 1:
    case 2:
        oldValue = fullState.randomizer.EnvT;
        fullState.randomizer.EnvT += (ticks > 0 ? 1 : -1);
        fullState.randomizer.EnvT = fullState.randomizer.EnvT > 3 ? 3 : fullState.randomizer.EnvT;
        fullState.randomizer.EnvT = fullState.randomizer.EnvT < 0 ? 0 : fullState.randomizer.EnvT;
        newValue = fullState.randomizer.EnvT;
        break;
    case 3:
        oldValue = fullState.randomizer.IM;
        fullState.randomizer.IM += (ticks > 0 ? 1 : -1);
        fullState.randomizer.IM = fullState.randomizer.IM > 3 ? 3 : fullState.randomizer.IM;
        fullState.randomizer.IM = fullState.randomizer.IM < 0 ? 0 : fullState.randomizer.IM;
        newValue = fullState.randomizer.IM;
        break;
    case 4:
    case 5:
        oldValue = fullState.randomizer.Modl;
        fullState.randomizer.Modl += (ticks > 0 ? 1 : -1);
        fullState.randomizer.Modl = fullState.randomizer.Modl > 3 ? 3 : fullState.randomizer.Modl;
        fullState.randomizer.Modl = fullState.randomizer.Modl < 0 ? 0 : fullState.randomizer.Modl;
        newValue = fullState.randomizer.Modl;
        break;
    }
    return newValue != oldValue;
}

void SynthState::encoderTurned(int encoder, int ticks) {
    switch (fullState.synthMode) {
    case SYNTH_MODE_EDIT_PFM3:
        displayEditor->encoderTurnedPfm3(encoder, ticks);
        break;
    case SYNTH_MODE_MENU:
        displayMenu->encoderTurned(currentTimbre, encoder, ticks);
        break;
    case SYNTH_MODE_MIXER:
        displayMixer->encoderTurned(encoder, ticks);
        break;
    case SYNTH_MODE_SEQUENCER: {
        if (encoder == 2) {
            setCurrentInstrument(ticks > 0 ? 0 : -1);
        }
        displaySequencer->encoderTurned(currentTimbre, encoder, ticks);
        break;
    }
    }
}

void SynthState::loadNewPreset(int timbre) {
    storeTestNote();
    propagateNoteOff();
    propagateBeforeNewParamsLoad(timbre);
    storage->getPatchBank()->copyNewPreset(params);
    propagateAfterNewParamsLoad(timbre);
    restoreTestNote();
}

void SynthState::loadPreset(int timbre, PFM3File const *bank, int patchNumber, struct OneSynthParams* params) {
    storeTestNote();
    propagateNoteOff();
    propagateBeforeNewParamsLoad(timbre);
    storage->getPatchBank()->loadPatch(bank, patchNumber, params);
    propagateAfterNewParamsLoad(timbre);
    restoreTestNote();
}

void SynthState::loadDx7Patch(int timbre, PFM3File const *bank, int patchNumber, struct OneSynthParams* params) {
    storeTestNote();
    propagateNoteOff();
    propagateBeforeNewParamsLoad(timbre);
    hexter->loadHexterPatch(storage->getDX7SysexFile()->dx7LoadPatch(bank, patchNumber), params);
    propagateAfterNewParamsLoad(timbre);
    restoreTestNote();
}

void SynthState::loadMixer(PFM3File const *bank, int patchNumber) {
    propagateBeforeNewParamsLoad(currentTimbre);
    storage->getMixerBank()->loadMixer(bank, patchNumber);
    // Update and clean all timbres
    this->currentTimbre = 0;
    propagateNewTimbre(currentTimbre);
    propagateAfterNewMixerLoad();
}

void SynthState::loadPresetFromMidi(int timbre, int bank, int bankLSB, int patchNumber, struct OneSynthParams *params) {
    switch (bank) {
        case 0: {
            PFM3File const *bank = storage->getPatchBank()->getFile(bankLSB);
            if (bank->fileType != FILE_EMPTY) {
                fullState.preenFMBankNumber = bankLSB;
                fullState.preenFMPresetNumber = patchNumber;
                loadPreset(timbre, bank, patchNumber, params);
            }
            break;
        }
        case 1: {
            PFM3File const *bank = storage->getMixerBank()->getFile(bankLSB);
            if (bank->fileType != FILE_EMPTY) {
                fullState.preenFMMixerNumber = bankLSB;
                fullState.preenFMMixerPresetNumber = patchNumber;
                loadMixer(bank, patchNumber);
            }
            break;
        }
        case 2:
        case 3:
        case 4: {
            int dx7bank = bank - 2;
            PFM3File const *bank = storage->getDX7SysexFile()->getFile(bankLSB + dx7bank * 128);
            if (bank->fileType != FILE_EMPTY) {
                fullState.dx7BankNumber = bankLSB;
                fullState.dx7PresetNumber = patchNumber;
                loadDx7Patch(timbre, bank, patchNumber, params);
            }
            break;
        }
    }
}



void SynthState::setCurrentInstrument(int value) {
    if (value == -1) {
        currentTimbre = currentTimbre - 1;
        if (unlikely(currentTimbre < 0)) {
            currentTimbre = NUMBER_OF_TIMBRES - 1;
        }
    } else if (value == 0) {
        currentTimbre = (currentTimbre + 1) % NUMBER_OF_TIMBRES;
    } else if (value <= (NUMBER_OF_TIMBRES + 1)) {
        currentTimbre = value - 1;
    } else {
        return;
    }
    propagateNewTimbre(currentTimbre);
}

void SynthState::buttonLongPressed(int button) {

    if (fullState.synthMode == SYNTH_MODE_SEQUENCER) {
        displaySequencer->buttonLongPressed(currentTimbre, button);
    }
}

void SynthState::buttonPressed(int button) {
    SynthEditMode oldSynthMode = fullState.synthMode;

    bool swithToMenuIfBackButtonPressed = true;

    if (fullState.synthMode == SYNTH_MODE_EDIT_PFM3) {
        if (fullState.mainPage != -1) {
            // Back must not go to MENU mode if not in the main page
            swithToMenuIfBackButtonPressed = false;
        }
        displayEditor->buttonPressed(button);
    } else if (fullState.synthMode == SYNTH_MODE_MENU) {
        displayMenu->buttonPressed(currentTimbre, button);
        swithToMenuIfBackButtonPressed = false;
    } else if (fullState.synthMode == SYNTH_MODE_MIXER) {
        displayMixer->buttonPressed(button);
    } else if (fullState.synthMode == SYNTH_MODE_SEQUENCER) {
        if (displaySequencer->getSequencerMode() != SEQ_MODE_NORMAL
            && displaySequencer->getSequencerMode() != SEQ_MODE_STEP) {
            swithToMenuIfBackButtonPressed = false;
        }
        displaySequencer->buttonPressed(currentTimbre, button);
    }

    // Anywhere these are main functions !!!!
    switch (button) {
    case BUTTON_PFM3_EDIT:
        fullState.synthMode = SYNTH_MODE_EDIT_PFM3;
        break;
    case BUTTON_PFM3_SEQUENCER:
        fullState.synthMode = SYNTH_MODE_SEQUENCER;
        break;
    case BUTTON_PFM3_MIXER:
        fullState.synthMode = SYNTH_MODE_MIXER;
        break;
    case BUTTON_PFM3_MENU:
        if (swithToMenuIfBackButtonPressed) {
            if (fullState.synthModeBeforeMenu != SYNTH_MODE_MENU) {
                fullState.synthModeBeforeMenu = fullState.synthMode;
            }
            // Make sure to propagate synth mode
            oldSynthMode = SYNTH_MODE_EDIT_PFM3;
            fullState.synthMode = SYNTH_MODE_MENU;
            fullState.previousChoice = fullState.previousMenuChoice.main;
            fullState.currentMenuItem = MenuItemUtil::getMenuItem(MAIN_MENU);
            propagateNewSynthMode();
            return;
        }
        break;
    case BUTTON_PREVIOUS_INSTRUMENT:
        // select next instrument as current one
        if (fullState.synthMode != SYNTH_MODE_MENU) {
            setCurrentInstrument(-1);
        }
        break;
    case BUTTON_NEXT_INSTRUMENT:
        // select next instrument as current one
        if (fullState.synthMode != SYNTH_MODE_MENU) {
            setCurrentInstrument(0);
        }
        break;
    case BUTTON_ENCODER_1:
    case BUTTON_ENCODER_2:
    case BUTTON_ENCODER_3:
    case BUTTON_ENCODER_4:
    case BUTTON_ENCODER_5:
    case BUTTON_ENCODER_6:
        setCurrentInstrument(button - BUTTON_ENCODER_1 + 1);
        break;
    }

    if (oldSynthMode != fullState.synthMode) {
        propagateNewSynthMode();
        return;
    }
}


void SynthState::propagateAfterNewParamsLoad(int timbre) {
    for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
        listener->afterNewParamsLoad(timbre);
    }
}

void SynthState::propagateAfterNewMixerLoad() {
    for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
        listener->afterNewMixerLoad();
    }
}

void SynthState::propagateNewTimbre(int timbre) {
    propagateNoteOff();
    for (SynthParamListener* listener = firstParamListener; listener != 0; listener = listener->nextListener) {
        listener->newTimbre(timbre);
    }
}

void SynthState::tempoClick() {
    if (fullState.synthMode == SYNTH_MODE_MENU) {
        if (fullState.currentMenuItem->menuType == MENUTYPE_TEMPORARY) {
            if (doneClick > 4) {
                fullState.synthMode = fullState.synthModeBeforeMenu;
                propagateNewSynthMode();
            }
            doneClick++;
        }
    } else {
        doneClick = 0;
    }
}

void SynthState::setParamsAndTimbre(struct OneSynthParams *newParams, int newCurrentTimbre) {
    this->params = newParams;
    this->currentTimbre = newCurrentTimbre;
}




/*
 * Randomizer
 */

int getRandomInt(int max) {
    uint32_t rnd;
    HAL_RNG_GenerateRandomNumber(&hrng, &rnd);
    return rnd % max;
}

float getRandomFloat(float min, float max) {
    uint32_t rnd;
    HAL_RNG_GenerateRandomNumber(&hrng, &rnd);
    float f = ((float) (rnd % 100000)) / 100000.0f;
    return f * (max - min) + min;
}

float getRandomShape(int operatorRandom) {
    int shape = getRandomInt(7);
    switch (operatorRandom) {
    case 1:
        if (shape > 2) {
            shape = 0;
        }
        break;
    case 2:
        shape = getRandomInt(8);
        if (shape == 6) { // Rand
            shape = 0;
        }
        break;
    case 3:
        break;
    }
    return shape;
}

float getRandomFrequencyType(int operatorRandom) {
    int freqType = getRandomInt(32);
    if (freqType > 1) { // Keyboard 5 times out of 6
        freqType = 0;
    }
    return freqType;
}

float getRandomFrequency(int operatorRandom) {
    float random1Frequency[] = { .5f, 1.0f, 2.0f, 4.0f };
    float random2Frequency[] = { .25, .5f, 1.0f, 1.5, 2.0f, 3.0f, 4.0f };
    float freq = 0;
    switch (operatorRandom) {
    case 1:
        freq = random1Frequency[getRandomInt(4)];
        break;
    case 2:
        freq = random2Frequency[getRandomInt(7)];
        break;
    case 3:
        freq = getRandomInt(24) * .25 + .25;
        break;
    }
    return freq;
}

float getFineTune(int operatorRandom) {
    float fineTune = 0;
    switch (operatorRandom) {
    case 2:
        fineTune = getRandomInt(10) * .01 - .05;
        if (fineTune < 0.03 || fineTune > 0.03) {
            fineTune = 0;
        }
        break;
    case 3:
        fineTune = getRandomInt(20) * .01 - .05;
        if (fineTune < 0.07 || fineTune > 0.07) {
            fineTune = 0;
        }
        break;
    }
    return fineTune;
}

void SynthState::randomizePreset() {
    int operatorRandom = fullState.randomizer.Oper;
    int envelopeTypeRandom = fullState.randomizer.EnvT;
    int imRandom = fullState.randomizer.IM;
    int modulationRandom = fullState.randomizer.Modl;

    // general

    params->engine1.velocity = 8;

    if (operatorRandom > 0) {
        params->engineMix1.mixOsc1 = getRandomFloat(0.6f, 1.0f);
        params->engineMix1.mixOsc2 = getRandomFloat(0.6f, 1.0f);
        params->engineMix2.mixOsc3 = getRandomFloat(0.6f, 1.0f);
        params->engineMix2.mixOsc4 = getRandomFloat(0.6f, 1.0f);
        params->engineMix3.mixOsc5 = getRandomFloat(0.6f, 1.0f);
        params->engineMix3.mixOsc6 = getRandomFloat(0.6f, 1.0f);

        params->engineMix1.panOsc1 = 0.0;
        params->engineMix1.panOsc2 = getRandomFloat(-0.3f, 0.3f);
        params->engineMix2.panOsc3 = getRandomFloat(-0.3f, 0.3f);
        params->engineMix2.panOsc4 = getRandomFloat(-0.5f, 0.5f);
        params->engineMix3.panOsc5 = getRandomFloat(-0.5f, 0.5f);
        params->engineMix3.panOsc6 = getRandomFloat(-0.7f, 0.7f);

        params->engine1.algo = getRandomInt(ALGO_END);

        for (int o = 0; o < 6; o++) {
            struct OscillatorParams* currentOsc = &((struct OscillatorParams*) &params->osc1)[o];
            currentOsc->shape = getRandomShape(operatorRandom);
            currentOsc->frequencyMul = getRandomFrequency(operatorRandom);
            currentOsc->frequencyType = getRandomFrequencyType(operatorRandom);
            currentOsc->detune = getFineTune(operatorRandom);
        }

        // FX
        params->effect.param1 = getRandomFloat(0.2f, 0.8f);
        params->effect.param2 = getRandomFloat(0.2f, 0.8f);
        int effect = getRandomInt(15);
        if (effect == 1 || effect > 6) {
            params->effect.param3 = 1.0;
            params->effect.type = 0;
        } else {
            params->effect.param3 = 0.6;
            params->effect.type = effect;
        }
    }

    for (int e = 0; e < 6; e++) {
        struct EnvelopeParamsA* enva = &((struct EnvelopeParamsA*) &params->env1a)[e * 2];
        struct EnvelopeParamsB* envb = &((struct EnvelopeParamsB*) &params->env1b)[e * 2];

        switch (envelopeTypeRandom) {
        case 1:
            enva->attackLevel = 1.0;
            enva->attackTime = getRandomFloat(0, 0.3f);
            enva->decayLevel = getRandomFloat(0.5f, 1.0f);
            enva->decayTime = getRandomFloat(0.05, 0.5f);

            envb->sustainLevel = getRandomFloat(0.0f, 1.0f);
            envb->sustainTime = getRandomFloat(0.02, 1.0f);
            envb->releaseLevel = 0.0f;
            envb->releaseTime = getRandomFloat(0.2, 5.0f);
            ;

            break;
        case 2:
            enva->attackLevel = getRandomFloat(0.25f, 1.0f);
            enva->attackTime = getRandomFloat(0.5f, 3.0f);
            enva->decayLevel = getRandomFloat(0.5f, 1.0f);
            enva->decayTime = getRandomFloat(1.0f, 5.0f);

            envb->sustainLevel = getRandomFloat(0.0f, 1.0f);
            envb->sustainTime = getRandomFloat(2.0f, 5.0f);
            envb->releaseLevel = 0.0f;
            envb->releaseTime = getRandomFloat(1.0f, 8.0f);
            break;
        case 3:
            enva->attackLevel = getRandomFloat(0, 1.0f);
            enva->attackTime = getRandomFloat(0, 1.0f);
            enva->decayLevel = getRandomFloat(0, 1.0f);
            enva->decayTime = getRandomFloat(0, 1.0f);

            envb->sustainLevel = getRandomFloat(0, 1.0f);
            envb->sustainTime = getRandomFloat(0, 1.0f);
            envb->releaseLevel = getRandomFloat(0, 1.0f);
            envb->releaseTime = getRandomFloat(0, 4.0f);
            break;
        }
    }

    if (imRandom > 0) {
        struct EngineIm1* im1 = (struct EngineIm1*) &params->engineIm1;
        struct EngineIm2* im2 = (struct EngineIm2*) &params->engineIm2;
        struct EngineIm3* im3 = (struct EngineIm3*) &params->engineIm3;

        float min = 0;
        float max = 0;
        float minVelo = 0;
        float maxVelo = 0;

        switch (imRandom) {
        case 1:
            min = 0.25f;
            max = 2.0f;
            minVelo = 0.2f;
            maxVelo = 1.0f;
            break;
        case 2:
            min = .5f;
            max = 3.0f;
            minVelo = 1.0f;
            maxVelo = 2.0f;
            break;
        case 3:
            min = 1.0f;
            max = 5.0f;
            minVelo = 1.0f;
            maxVelo = 4.0f;
            break;
        }
        im1->modulationIndex1 = getRandomFloat(min, max);
        im1->modulationIndexVelo1 = getRandomFloat(minVelo, maxVelo);
        im1->modulationIndex2 = getRandomFloat(min, max);
        im1->modulationIndexVelo2 = getRandomFloat(minVelo, maxVelo);
        im2->modulationIndex3 = getRandomFloat(min, max);
        im2->modulationIndexVelo3 = getRandomFloat(minVelo, maxVelo);
        im2->modulationIndex4 = getRandomFloat(min, max);
        im2->modulationIndexVelo4 = getRandomFloat(minVelo, maxVelo);
        im3->modulationIndex5 = getRandomFloat(min, max);
        im3->modulationIndexVelo5 = getRandomFloat(minVelo, maxVelo);
    }

    if (modulationRandom > 0) {

        params->matrixRowState1.source = MATRIX_SOURCE_LFO1;
        params->matrixRowState2.source = MATRIX_SOURCE_LFO1;
        params->matrixRowState3.source = MATRIX_SOURCE_LFO2;
        params->matrixRowState4.source = MATRIX_SOURCE_LFO2;
        params->matrixRowState5.source = MATRIX_SOURCE_LFO3;
        params->matrixRowState6.source = MATRIX_SOURCE_LFOENV1;
        params->matrixRowState7.source = MATRIX_SOURCE_LFOENV2;
        params->matrixRowState8.source = MATRIX_SOURCE_LFOSEQ1;
        params->matrixRowState9.source = MATRIX_SOURCE_LFOSEQ2;

        params->matrixRowState10.source = MATRIX_SOURCE_MODWHEEL;
        params->matrixRowState10.mul = 2.0f;
        params->matrixRowState10.dest1 = INDEX_ALL_MODULATION;

        params->matrixRowState11.source = MATRIX_SOURCE_PITCHBEND;
        params->matrixRowState11.mul = 1.0f;
        params->matrixRowState11.dest1 = ALL_OSC_FREQ;

        params->matrixRowState12.source = MATRIX_SOURCE_AFTERTOUCH;
        params->matrixRowState12.mul = 1.0f;
        params->matrixRowState12.dest1 = INDEX_MODULATION1;

        for (int m = 1; m <= 9; m++) {
            struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*) &params->matrixRowState1)[m - 1];
            matrixRow->mul = 0;
            matrixRow->dest1 = 0;
        }

        float dest[] = { INDEX_ALL_MODULATION, OSC1_FREQ, ALL_PAN, OSC2_FREQ, INDEX_MODULATION1, PAN_OSC1, INDEX_ALL_MODULATION };
        for (int i = 0; i < 2; i++) {
            struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*) &params->matrixRowState1)[getRandomInt(10)];
            matrixRow->mul = getRandomFloat(0.2f, 3.0f);
            matrixRow->dest1 = dest[getRandomInt(7)];
        }

        if (modulationRandom >= 2) {
            for (int i = 0; i < 3; i++) {
                struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*) &params->matrixRowState1)[getRandomInt(10)];
                matrixRow->mul = getRandomFloat(1.0f, 3.0f);
                matrixRow->dest1 = getRandomInt(DESTINATION_MAX);
            }
        }

        if (modulationRandom >= 3) {
            for (int i = 0; i < 6; i++) {
                struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*) &params->matrixRowState1)[getRandomInt(10)];
                float mm = getRandomFloat(2.0f, 5.0f);
                matrixRow->mul = getRandomFloat(-mm, mm);
                matrixRow->dest1 = getRandomInt(DESTINATION_MAX);
            }
        }

        for (int o = 0; o < 3; o++) {
            struct LfoParams* osc = &((struct LfoParams*) &params->lfoOsc1)[o];
            osc->shape = getRandomInt(5);
            osc->freq = getRandomFloat(0.2, 3 + modulationRandom * 2);
            if (getRandomInt(4) > 1) {
                osc->bias = 0;
            } else {
                osc->bias = getRandomFloat(-1.0f, 1.0f);
            }
            // PAD
            if (envelopeTypeRandom == 2) {
                osc->keybRamp = getRandomFloat(0.0f, 4.0f);
            } else {
                osc->keybRamp = getRandomFloat(0.0f, 1.0f);
            }
        }
        for (int e = 0; e < 2; e++) {
            struct EnvelopeParams* env = &((struct EnvelopeParams*) &params->lfoEnv1)[e];
            env->attack = getRandomFloat(0.05f, 1.0f);
            env->decay = getRandomFloat(0.05f, 1.0f);
            env->sustain = getRandomFloat(0.05f, 1.0f);
            if (e == 0) {
                env->release = getRandomFloat(0.05f, 1.0f);
            } else {
                params->lfoEnv2.loop = getRandomInt(2) + 1;
            }
        }

        int bpm = getRandomInt(120) + 60;
        for (int s = 0; s < 2; s++) {
            struct StepSequencerParams* stepSeq = &((struct StepSequencerParams*) &params->lfoSeq1)[s];
            stepSeq->bpm = bpm;
            stepSeq->gate = getRandomFloat(0.25, 1);

            struct StepSequencerSteps* steps = &((struct StepSequencerSteps*) &params->lfoSteps1)[s];
            for (int k = 0; k < 16; k++) {
                if ((k % 4) == 0) {
                    steps->steps[k] = getRandomInt(5) + 11;
                } else {
                    steps->steps[k] = getRandomInt(10);
                }
            }
        }
    }
}

char* SynthState::getTimbreName(int t) {
    return timbres[t].getPresetName();
}

uint8_t SynthState::getTimbrePolyMono(int t) {
    return timbres[t].getParamRaw()->engine1.polyMono;
}


bool SynthState::scalaSettingsChanged(int timbre) {
    if (!mixerState.instrumentState_[timbre].scalaEnable) {
        mixerState.instrumentState_[timbre].scaleFrequencies = diatonicScaleFrequency;
    } else {
        if (storage->getScalaFile()->getFile(mixerState.instrumentState_[timbre].scaleScaleNumber)->fileType == FILE_EMPTY) {
            return false;
        }
        float* newScaleFrequencies = storage->getScalaFile()->loadScalaScale(&mixerState, timbre);
        if (newScaleFrequencies != 0) {
            mixerState.instrumentState_[timbre].scaleFrequencies = newScaleFrequencies;
        } else {
            // File Size returned 0
            return false;
        }
    }
    return true;
}

const char* SynthState::getSequenceName() {
	displaySequencer->getSequenceName();
}


