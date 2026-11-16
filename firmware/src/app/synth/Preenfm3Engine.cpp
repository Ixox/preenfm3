/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Preenfm3Engine.h"
#include "Timbre.h"
#include "Voice.h"

static Pfm3TimbreContext pfm3Contexts[NUMBER_OF_TIMBRES] __attribute__((section(".ram_d1")));

void Preenfm3Engine::init(Timbre *timbre) {
    timbre_ = timbre;
    context_ = &pfm3Contexts[timbre->getTimbreNumber()];
    initContext();
    refreshContext();
}

void Preenfm3Engine::afterNewParamsLoad() {
    timbre_->pfm3AfterNewParamsLoad();
    refreshContext();
}

void Preenfm3Engine::initContext() {
    struct OneSynthParams *p = timbre_->getParamRaw();

    context_->matrixRows = &p->matrixRowState1;

    context_->lfoOscParams[0] = &p->lfoOsc1;
    context_->lfoOscParams[1] = &p->lfoOsc2;
    context_->lfoOscParams[2] = &p->lfoOsc3;
    for (int k = 0; k < NUMBER_OF_LFO_OSC; k++) {
        context_->lfoPhases[k] = &((float*) &p->lfoPhases.phaseLfo1)[k];
    }

    context_->lfoEnv = &p->lfoEnv1;
    context_->lfoEnv2 = &p->lfoEnv2;

    context_->lfoStepSeqParams[0] = &p->lfoSeq1;
    context_->lfoStepSeqParams[1] = &p->lfoSeq2;
    context_->lfoStepSeqSteps[0] = &p->lfoSteps1;
    context_->lfoStepSeqSteps[1] = &p->lfoSteps2;
}

void Preenfm3Engine::refreshContext() {
    struct OneSynthParams *p = timbre_->getParamRaw();

    float pans[6] = {
        p->engineMix1.panOsc1,
        p->engineMix1.panOsc2,
        p->engineMix2.panOsc3,
        p->engineMix2.panOsc4,
        p->engineMix3.panOsc5,
        p->engineMix3.panOsc6
    };
    int *opInfo = algoOpInformation[(int) p->engine1.algo];
    for (int op = 0; op < 6; op++) {
        if (likely(opInfo[op] == 1)) {
            float pan1 = pans[op] + 1.0f;
            int pan = __USAT((int) (pan1 * 128), 8);
            context_->panLeft[op] = panTable[pan];
            context_->panRight[op] = panTable[256 - pan];
        } else {
            context_->panLeft[op] = 0.0f;
            context_->panRight[op] = 0.0f;
        }
    }

    context_->mixerGain = p->effect1.param3;
}

void Preenfm3Engine::bindVoice(Voice *voice) {
    voice->bindTimbre(context_, timbre_);
}

void Preenfm3Engine::noteOn(char note, char velocity) {
    timbre_->preenNoteOn(note, velocity);
}

void Preenfm3Engine::noteOff(char note) {
    timbre_->preenNoteOff(note);
}

uint8_t Preenfm3Engine::voicesNextBlock() {
    return timbre_->pfm3VoicesNextBlock();
}

void Preenfm3Engine::prepareBlock() {
    timbre_->pfm3PrepareMatrixForNewBlock();
}

void Preenfm3Engine::glide() {
    timbre_->pfm3Glide();
}

void Preenfm3Engine::gateFx() {
    timbre_->pfm3GateFx();
}
