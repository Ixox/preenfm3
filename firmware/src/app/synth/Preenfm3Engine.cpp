/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Preenfm3Engine.h"
#include "Pfm3FlashFormat.h"
#include "Timbre.h"
#include "Voice.h"

static Pfm3TimbreContext pfm3Contexts[NUMBER_OF_TIMBRES] __attribute__((section(".ram_d1")));
Pfm3TimbreParams Preenfm3Engine::pfm3Params_[NUMBER_OF_TIMBRES] __attribute__((section(".ram_d1")));
OneSynthParams oneSynthParamsTmp __attribute__((section(".ram_d1")));

const float Preenfm3Engine::unisonPhase[MAX_UNISON_VOICES] = {
    .37f, .11f, .495f, .53f, .03f, .19f, .89f
};

#define INV127 .00787401574803149606f
#define INV16 .0625

extern float midiNoteScale[2][NUMBER_OF_TIMBRES][128];
extern float noise[32];

void Preenfm3Engine::init(Timbre *timbre, SynthState *synthState) {
    timbre_ = timbre;
    timbreNumber_ = timbre->getTimbreNumber();
    context_ = &pfm3Contexts[timbreNumber_];
    for (int lfo = 0; lfo < NUMBER_OF_LFO; lfo++) {
        lfoUSed_[lfo] = 0;
    }
    recomputeNext_ = true;
    currentGate_ = 0.0f;
    initOscEnv(synthState);
    initContext();
    refreshContext();
}

void Preenfm3Engine::initOscEnv(SynthState *synthState) {
    struct OneSynthParams *p = timbre_->getParamRaw();

    env_[0].init(&p->env1Time, &p->env1Level, 0, &p->engine1.algo, &p->env1Curve);
    env_[1].init(&p->env2Time, &p->env2Level, 1, &p->engine1.algo, &p->env2Curve);
    env_[2].init(&p->env3Time, &p->env3Level, 2, &p->engine1.algo, &p->env3Curve);
    env_[3].init(&p->env4Time, &p->env4Level, 3, &p->engine1.algo, &p->env4Curve);
    env_[4].init(&p->env5Time, &p->env5Level, 4, &p->engine1.algo, &p->env5Curve);
    env_[5].init(&p->env6Time, &p->env6Level, 5, &p->engine1.algo, &p->env6Curve);

    osc_[0].init(synthState, &p->osc1, OSC1_FREQ);
    osc_[1].init(synthState, &p->osc2, OSC2_FREQ);
    osc_[2].init(synthState, &p->osc3, OSC3_FREQ);
    osc_[3].init(synthState, &p->osc4, OSC4_FREQ);
    osc_[4].init(synthState, &p->osc5, OSC5_FREQ);
    osc_[5].init(synthState, &p->osc6, OSC6_FREQ);
}

void Preenfm3Engine::afterNewParamsLoad() {
    for (int e = 0; e < 6; e++) {
        env_[e].applyCurves();
    }

    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        timbre_->voices_[timbre_->voiceNumber_[k]]->afterNewParamsLoad();
    }

    for (int j = 0; j < NUMBER_OF_ENCODERS_PFM2; j++) {
        for (int e = 0; e < 6; e++) {
            env_[e].reloadADSR(j);
        }
    }
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
    if (unlikely(timbre_->maxVoices_ == 0
        || timbre_->mixerState_->instrumentState_[timbre_->timbreNumber_].scaleFrequencies[(int) note] == 0.0f)) {
        return;
    }

    note &= 0x7f;

    float noteFrequency = timbre_->mixerState_->instrumentState_[timbre_->timbreNumber_].scaleFrequencies[(int) note];
    bool unison = timbre_->params_.engine1.playMode == PLAY_MODE_UNISON;
    bool mono = timbre_->params_.engine1.playMode == PLAY_MODE_MONO;
    int unisonWidth = timbre_->maxVoices_ < MAX_UNISON_VOICES ? (int) timbre_->maxVoices_ : MAX_UNISON_VOICES;

    timbre_->numberOfVoiceInverse_ = 1.0f / (float) (unison ? unisonWidth : (timbre_->maxVoices_ > 0 ? timbre_->maxVoices_ : 1));

    if (unison) {
        timbre_->trimActiveVoices(unisonWidth);
        if (!timbre_->ensureActiveVoices(unisonWidth)) {
            return;
        }
        float noteFrequencyUnison = noteFrequency + noteFrequency * timbre_->params_.engine2.unisonDetune * .05f;
        float noteFrequencyUnisonInc = noteFrequency * timbre_->params_.engine2.unisonDetune * timbre_->numberOfVoiceInverse_ * .1f;
        for (int k = 0; k < unisonWidth; k++) {
            int n = timbre_->voiceNumber_[k];
            preenNoteOnUpdateMatrix(n, note, velocity);
            if (timbre_->voices_[n]->isPlaying()) {
                timbre_->voices_[n]->noteOnWithoutPop(note, noteFrequencyUnison, velocity, timbre_->voiceIndex_++, unisonPhase[k]);
            } else {
                timbre_->voices_[n]->noteOn(note, noteFrequencyUnison, velocity, timbre_->voiceIndex_++, unisonPhase[k]);
            }
            noteFrequencyUnison += noteFrequencyUnisonInc;
        }
        timbre_->lastPlayedNote_ = timbre_->voiceNumber_[0];
        timbre_->lowerNote_ = note;
        timbre_->lowerNoteReleased_ = false;
        timbre_->lowerNoteFrequency = timbre_->voices_[timbre_->voiceNumber_[0]]->getNoteRealFrequencyEstimation(noteFrequency);
        return;
    }

    int voiceToUse;
    if (mono && timbre_->numberOfVoices_ > 0 && !timbre_->voices_[timbre_->voiceNumber_[0]]->isTransferPending()) {
        // Mono uses a single voice; noteOff only checks voice 0.
        voiceToUse = timbre_->voiceNumber_[0];
    } else {
        voiceToUse = timbre_->allocateVoice(note);
    }
    if (voiceToUse == -1) {
        return;
    }
    timbre_->clearMpeMapping(voiceToUse);

    bool transferPending = timbre_->voices_[voiceToUse]->isTransferPending();
    if (!transferPending) {
        preenNoteOnUpdateMatrix(voiceToUse, note, velocity);
    }
    if (timbre_->voices_[voiceToUse]->isPlaying()) {
        timbre_->voices_[voiceToUse]->noteOnWithoutPop(note, noteFrequency, velocity, timbre_->voiceIndex_++);
    } else {
        timbre_->voices_[voiceToUse]->noteOn(note, noteFrequency, velocity, timbre_->voiceIndex_++);
    }

    if (!transferPending) {
        timbre_->lastPlayedNote_ = voiceToUse;
    }
    if (note <= timbre_->lowerNote_ || timbre_->lowerNoteReleased_ || mono) {
        timbre_->lowerNote_ = note;
        timbre_->lowerNoteReleased_ = false;
        if (!transferPending) {
            timbre_->lowerNoteFrequency = timbre_->voices_[voiceToUse]->getNoteRealFrequencyEstimation(noteFrequency);
        }
    }
}

void Preenfm3Engine::noteOff(char note) {
    bool isUnison = timbre_->params_.engine1.playMode == PLAY_MODE_UNISON;

    if (note == timbre_->lowerNote_) {
        timbre_->lowerNoteReleased_ = true;
    }

    // Unison we must check all voices.... (different from noteOn)
    int iNov = timbre_->params_.engine1.playMode == PLAY_MODE_MONO ? 1 : (int) timbre_->numberOfVoices_;
    for (int k = 0; k < iNov; k++) {
        // voice number k of timbre
        int n = timbre_->voiceNumber_[k];

        // Not playing = free CPU
        if (unlikely(!timbre_->voices_[n]->isPlaying())) {
            continue;
        }

        if (likely(timbre_->voices_[n]->getNextGlidingNote() == 0 || timbre_->voices_[n]->isNewGlide())) {
            if (timbre_->voices_[n]->getNote() == note) {
                if (unlikely(timbre_->holdPedal_)) {
                    timbre_->voices_[n]->setHoldedByPedal(true);
                    if (likely(!isUnison)) {
                        return;
                    }
                } else {
                    timbre_->voices_[n]->noteOff();
                    if (likely(!isUnison)) {
                        return;
                    }
                }
            }
        } else {
            // if gliding and releasing first note
            if (timbre_->voices_[n]->getNote() == note) {
                timbre_->voices_[n]->glideFirstNoteOff();
                if (likely(!isUnison)) {
                    return;
                }
            }
            // if gliding and releasing next note
            if (timbre_->voices_[n]->getNextGlidingNote() == note) {
                timbre_->voices_[n]->glideToNote(timbre_->voices_[n]->getNote(), timbre_->voices_[n]->getNoteFrequency());
                timbre_->voices_[n]->glideFirstNoteOff();
                // Sync Osccilo
                timbre_->lowerNoteFrequency = timbre_->voices_[n]->getNoteRealFrequencyEstimation(timbre_->voices_[n]->getNoteFrequency());
                if (likely(!isUnison)) {
                    return;
                }
            }
        }
    }
}

void Preenfm3Engine::preenNoteOnUpdateMatrix(int voiceToUse, int note, int velocity) {
    // Update voice matrix with midi note and velocity
    timbre_->voices_[voiceToUse]->pfm3State_.matrix.setSource(MATRIX_SOURCE_NOTE1, midiNoteScale[0][timbre_->timbreNumber_][note]);
    timbre_->voices_[voiceToUse]->pfm3State_.matrix.setSource(MATRIX_SOURCE_NOTE2, midiNoteScale[1][timbre_->timbreNumber_][note]);
    timbre_->voices_[voiceToUse]->pfm3State_.matrix.setSource(MATRIX_SOURCE_VELOCITY, INV127 * velocity);
    timbre_->voices_[voiceToUse]->pfm3State_.matrix.setSource(MATRIX_SOURCE_RANDOM, noise[voiceToUse]);
}

uint8_t Preenfm3Engine::voicesNextBlock() {
    uint8_t numberOfPlayingVoices = 0;
    if (unlikely(timbre_->params_.engine1.playMode == PLAY_MODE_UNISON)) {
        timbre_->cleanNextBlock();
        // UNISON
        float pansSav[6];
        pansSav[0] = timbre_->params_.engineMix1.panOsc1;
        pansSav[1] = timbre_->params_.engineMix1.panOsc2;
        pansSav[2] = timbre_->params_.engineMix2.panOsc3;
        pansSav[3] = timbre_->params_.engineMix2.panOsc4;
        pansSav[4] = timbre_->params_.engineMix3.panOsc5;
        pansSav[5] = timbre_->params_.engineMix3.panOsc6;

        float* pans[] = {
            &timbre_->params_.engineMix1.panOsc1,
            &timbre_->params_.engineMix1.panOsc2,
            &timbre_->params_.engineMix2.panOsc3,
            &timbre_->params_.engineMix2.panOsc4,
            &timbre_->params_.engineMix3.panOsc5,
            &timbre_->params_.engineMix3.panOsc6
        };

        int currentAlgo = (int) timbre_->params_.engine1.algo;
        float algoNumberOfMix = algoInformation[currentAlgo].mix;
        float numberOfCarrierOp = timbre_->numberOfVoices_ * algoNumberOfMix;
        float opPan = - timbre_->params_.engine2.unisonSpread;
        float opPanInc = 2.0f / numberOfCarrierOp * timbre_->params_.engine2.unisonSpread;

        if (likely(timbre_->voices_[timbre_->voiceNumber_[0]]->isPlaying())) {
            for (int vv = 0; vv < (int) timbre_->numberOfVoices_; vv++) {
                int v = timbre_->voiceNumber_[vv];
                if (unlikely(v < 0)) {
                    continue;
                }
                bool otherSide = false;
                for (int op = 0; op < 6; op ++) {
                    if (algoOpInformation[currentAlgo][op] == 1) {
                        if (otherSide) {
                            *pans[op] = -opPan;
                        } else {
                            *pans[op] = opPan;
                        }
                        opPan += opPanInc;
                        otherSide =! otherSide;
                    }
                }
                timbre_->voices_[v]->nextBlock();

                if (vv > 0) {
                    // We accumulate in the first voice buffer
                    float *source = timbre_->voices_[v]->getSampleBlock();
                    float *voice0SampleBlock = timbre_->voices_[timbre_->voiceNumber_[0]]->getSampleBlock();
                    for (int s = 0; s < BLOCK_SIZE; s++) {
                        *voice0SampleBlock++ += *source++;
                        *voice0SampleBlock++ += *source++;
                    }
                }
                numberOfPlayingVoices++;
            }
            timbre_->voices_[timbre_->voiceNumber_[0]]->fxAfterBlock();

        } else {
            timbre_->voices_[timbre_->voiceNumber_[0]]->emptyBuffer();
        }

        timbre_->params_.engineMix1.panOsc1 = pansSav[0];
        timbre_->params_.engineMix1.panOsc2 = pansSav[1];
        timbre_->params_.engineMix2.panOsc3 = pansSav[2];
        timbre_->params_.engineMix2.panOsc4 = pansSav[3];
        timbre_->params_.engineMix3.panOsc5 = pansSav[4];
        timbre_->params_.engineMix3.panOsc6 = pansSav[5];
    } else {
        for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
            int v = timbre_->voiceNumber_[k];
            if (likely(timbre_->voices_[v]->isPlaying())) {
                timbre_->voices_[v]->nextBlock();
                timbre_->voices_[v]->fxAfterBlock();
                numberOfPlayingVoices++;
            } else {
                timbre_->voices_[v]->emptyBuffer();
            }
        }
    }

    return numberOfPlayingVoices;
}

void Preenfm3Engine::prepareBlock() {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        timbre_->voices_[timbre_->voiceNumber_[k]]->prepareMatrixForNewBlock();
    }
}

void Preenfm3Engine::glide() {
    if (unlikely(timbre_->params_.engine1.playMode == PLAY_MODE_UNISON)) {
        for (int vv = 0; vv < (int) timbre_->numberOfVoices_; vv++) {
            int v = timbre_->voiceNumber_[vv];
            if (v != -1 && timbre_->voices_[v]->isGliding()) {
                timbre_->voices_[v]->glide();
            }
        }
    } else {
        if (timbre_->voiceNumber_[0] != -1 && timbre_->voices_[timbre_->voiceNumber_[0]]->isGliding()) {
            timbre_->voices_[timbre_->voiceNumber_[0]]->glide();
        }
    }
}

void Preenfm3Engine::gateFx() {
    // Gate algo !!
    float gate = 0.0f;
    if ((int) timbre_->numberOfVoices_ > 0) {
        gate = timbre_->voices_[timbre_->lastPlayedNote_]->pfm3State_.matrix.getDestination(MAIN_GATE);
    }
    if (unlikely(gate > 0 || currentGate_ > 0)) {
        gate *= .72547132656922730694f; // 0 < gate < 1.0
        if (gate > 1.0f) {
            gate = 1.0f;
        }
        float incGate = (gate - currentGate_) * .03125f; // ( *.03125f = / 32)
        // limit the speed.
        if (incGate > 0.002f) {
            incGate = 0.002f;
        } else if (incGate < -0.002f) {
            incGate = -0.002f;
        }

        float *sp = timbre_->sampleBlock_;
        float coef;
        for (int k = 0; k < BLOCK_SIZE; k++) {
            currentGate_ += incGate;
            coef = 1.0f - currentGate_;
            *sp = *sp * coef;
            sp++;
            *sp = *sp * coef;
            sp++;
        }
    }
}

void Preenfm3Engine::setMatrixSource(SourceEnum source, float value) {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        timbre_->voices_[timbre_->voiceNumber_[k]]->pfm3State_.matrix.setSource(source, value);
    }
}

void Preenfm3Engine::setMatrixSourceMPE(uint8_t channel, SourceEnum source, float value) {
    int voiceToUse = timbre_->mpeChannelVoice_[channel - 1];
    if (unlikely(voiceToUse == -1)) {
        return;
    }
    timbre_->voices_[voiceToUse]->pfm3State_.matrix.setSource(source, value);
}

void Preenfm3Engine::resetMatrixDestination(float oldValue) {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        timbre_->voices_[timbre_->voiceNumber_[k]]->pfm3State_.matrix.resetDestination(oldValue);
    }
}

void Preenfm3Engine::setMatrixPolyAfterTouch(uint8_t note, float value) {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        int voiceIndex = timbre_->voiceNumber_[k];
        Voice *voice = timbre_->voices_[voiceIndex];
        if (unlikely(voice->isPlaying() && voice->getNote() == note)) {
            voice->pfm3State_.matrix.setSource(MATRIX_SOURCE_POLYPHONIC_AFTERTOUCH, value);
            break;
        }
    }
}

void Preenfm3Engine::noteOnMpe(uint8_t channel, uint8_t note, uint8_t velocity) {
    int channelIndex = channel - 1;
    if (unlikely(channelIndex < 0 || channelIndex >= 16)) {
        return;
    }

    int voiceToUse = timbre_->mpeChannelVoice_[channelIndex];
    if (voiceToUse == -1
        || (timbre_->voices_[voiceToUse]->getCurrentTimbre() != timbre_
            && timbre_->voices_[voiceToUse]->getPendingTimbre() != timbre_)) {
        voiceToUse = timbre_->allocateVoice(note);
        if (unlikely(voiceToUse == -1)) {
            return;
        }
        timbre_->clearMpeMapping(voiceToUse);
        timbre_->mpeChannelVoice_[channelIndex] = voiceToUse;
    }

    bool transferPending = timbre_->voices_[voiceToUse]->isTransferPending();
    if (!transferPending) {
        preenNoteOnUpdateMatrix(voiceToUse, note, velocity);
    }
    float noteFrequency = timbre_->mixerState_->instrumentState_[0].scaleFrequencies[(int) note];

    if (timbre_->voices_[voiceToUse]->isPlaying()) {
        timbre_->voices_[voiceToUse]->noteOnWithoutPop(note, noteFrequency, velocity, timbre_->voiceIndex_++);
    } else {
        timbre_->voices_[voiceToUse]->noteOn(note, noteFrequency, velocity, timbre_->voiceIndex_++);
    }
}

void Preenfm3Engine::noteOffMpe(uint8_t channel, uint8_t note, uint8_t velocity) {
    (void) velocity;

    int channelIndex = channel - 1;
    if (unlikely(channelIndex < 0 || channelIndex >= 16)) {
        return;
    }

    int voiceToUse = timbre_->mpeChannelVoice_[channelIndex];
    if (unlikely(voiceToUse == -1)) {
        return;
    }

    if (timbre_->voices_[voiceToUse]->isPlaying()) {
        timbre_->voices_[voiceToUse]->noteOff();
    }
}

void Preenfm3Engine::newParamValue(int row, int encoder) {
    if ((row == ROW_ENGINE && encoder == ENCODER_ENGINE_ALGO)
        || ((row == ROW_OSC_MIX1 || row == ROW_OSC_MIX2 || row == ROW_OSC_MIX3)
            && (encoder == 1 || encoder == 3))
        || (row == ROW_EFFECT1 && encoder == ENCODER_EFFECT_PARAM3)) {
        refreshContext();
    }
}

void Preenfm3Engine::setParamValue(int index, float value) {
    if (index < TimbreParamLayout::sharedBlockStart) {
        ((float*) &pfm3Params())[index] = value;
    } else if (index >= TimbreParamLayout::pfm3BlockBStart && index < TimbreParamLayout::effect2Start) {
        ((float*) &pfm3Params())[index - TimbreParamLayout::sharedGap] = value;
    }
}

void Preenfm3Engine::prepareFxMatrix(Voice *voice) {
    if (!voice->isPlaying()) {
        voice->pfm3State_.matrix.computeAllDestinations();
    }
}

void Preenfm3Engine::updateVoiceMatrix(Voice *voice, int note, int velocity) {
    int voiceIndex = 0;
    for (int i = 0; i < MAX_NUMBER_OF_VOICES; i++) {
        if (timbre_->voices_[i] == voice) {
            voiceIndex = i;
            break;
        }
    }
    preenNoteOnUpdateMatrix(voiceIndex, note, velocity);
}

void Preenfm3Engine::lfoValueChange(int currentRow, int encoder, float newValue) {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        timbre_->voices_[timbre_->voiceNumber_[k]]->lfoValueChange(currentRow, encoder, newValue);
    }
}

bool Preenfm3Engine::isLfoUsed(int lfo) {
    return lfoUSed_[lfo] > 0;
}

void Preenfm3Engine::midiClockContinue(int songPosition) {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        timbre_->voices_[timbre_->voiceNumber_[k]]->midiClockContinue(songPosition);
    }

    recomputeNext_ = ((songPosition & 0x1) == 0);
    timbre_->OnMidiContinue();
}

void Preenfm3Engine::midiClockStart() {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        timbre_->voices_[timbre_->voiceNumber_[k]]->midiClockStart();
    }

    recomputeNext_ = true;
    timbre_->OnMidiStart();
}

void Preenfm3Engine::midiClockSongPositionStep(int songPosition) {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        timbre_->voices_[timbre_->voiceNumber_[k]]->midiClockSongPositionStep(songPosition, recomputeNext_);
    }

    if ((songPosition & 0x1) == 0) {
        recomputeNext_ = true;
    }
}

float Preenfm3Engine::getMatrixDestination(int destination) {
    if ((int) timbre_->numberOfVoices_ > 0) {
        return timbre_->voices_[timbre_->lastPlayedNote_]->pfm3State_.matrix.getDestination((DestinationEnum) destination);
    }
    return 0.0f;
}

void Preenfm3Engine::updateMidiNoteScale(int scale) {
    int intBreakNote;
    int curveBefore;
    int curveAfter;
    if (scale == 0) {
        intBreakNote = timbre_->params_.midiNote1Curve.breakNote;
        curveBefore = timbre_->params_.midiNote1Curve.curveBefore;
        curveAfter = timbre_->params_.midiNote1Curve.curveAfter;
    } else {
        intBreakNote = timbre_->params_.midiNote2Curve.breakNote;
        curveBefore = timbre_->params_.midiNote2Curve.curveBefore;
        curveAfter = timbre_->params_.midiNote2Curve.curveAfter;
    }
    float floatBreakNote = intBreakNote;
    float multiplier = 1.0f;

    switch (curveBefore) {
        case MIDI_NOTE_CURVE_FLAT:
            for (int n = 0; n < intBreakNote; n++) {
                midiNoteScale[scale][timbre_->timbreNumber_][n] = 0;
            }
            break;
        case MIDI_NOTE_CURVE_M_LINEAR:
            multiplier = -1.0f;
            goto linearBefore;
        case MIDI_NOTE_CURVE_M_LINEAR2:
            multiplier = -8.0f;
            goto linearBefore;
        case MIDI_NOTE_CURVE_LINEAR2:
            multiplier = 8.0f;
            goto linearBefore;
        case MIDI_NOTE_CURVE_LINEAR:
linearBefore: for (int n = 0; n < intBreakNote; n++) {
                float fn = (floatBreakNote - n);
                midiNoteScale[scale][timbre_->timbreNumber_][n] = fn * INV127 * multiplier;
            }
            break;
        case MIDI_NOTE_CURVE_M_EXP:
            multiplier = -1.0f;
        case MIDI_NOTE_CURVE_EXP:
            if (floatBreakNote == 0) {
                floatBreakNote = 1;
            }
            for (int n = 0; n < intBreakNote; n++) {
                float fn = (floatBreakNote - n);
                fn = fn * fn / floatBreakNote;
                midiNoteScale[scale][timbre_->timbreNumber_][n] = fn * INV16 * multiplier;
            }
            break;
    }

    midiNoteScale[scale][timbre_->timbreNumber_][intBreakNote] = 0;

    switch (curveAfter) {
        case MIDI_NOTE_CURVE_FLAT:
            for (int n = intBreakNote + 1; n < 128; n++) {
                midiNoteScale[scale][timbre_->timbreNumber_][n] = 0;
            }
            break;
        case MIDI_NOTE_CURVE_M_LINEAR:
            multiplier = -1.0f;
            goto linearAfter;
        case MIDI_NOTE_CURVE_M_LINEAR2:
            multiplier = -8.0f;
            goto linearAfter;
        case MIDI_NOTE_CURVE_LINEAR2:
            multiplier = 8.0f;
            goto linearAfter;
        case MIDI_NOTE_CURVE_LINEAR:
linearAfter: for (int n = intBreakNote + 1; n < 128; n++) {
                float fn = n - floatBreakNote;
                midiNoteScale[scale][timbre_->timbreNumber_][n] = fn * INV127 * multiplier;
            }
            break;
        case MIDI_NOTE_CURVE_M_EXP:
            multiplier = -1.0f;
        case MIDI_NOTE_CURVE_EXP:
            if (floatBreakNote == 0) {
                floatBreakNote = 1;
            }
            for (int n = intBreakNote + 1; n < 128; n++) {
                float fn = n - floatBreakNote;
                fn = fn * fn / floatBreakNote;
                midiNoteScale[scale][timbre_->timbreNumber_][n] = fn * INV16 * multiplier;
            }
            break;
    }
}

void Preenfm3Engine::verifyLfoUsed(int encoder, float oldValue, float newValue) {
    // No need to recompute
    if (timbre_->numberOfVoices_ == 0.0f) {
        return;
    }
    // We used it and still use it
    if (encoder == ENCODER_MATRIX_MUL && oldValue != 0.0f && newValue != 0.0f) {
        return;
    }

    for (int lfo = 0; lfo < NUMBER_OF_LFO; lfo++) {
        lfoUSed_[lfo] = 0;
    }

    MatrixRowParams *matrixRows = &timbre_->params_.matrixRowState1;

    for (int r = 0; r < MATRIX_SIZE; r++) {
        if ((matrixRows[r].source >= MATRIX_SOURCE_LFO1 && matrixRows[r].source <= MATRIX_SOURCE_LFOSEQ2) && matrixRows[r].mul != 0.0f
            && (matrixRows[r].dest1 != 0.0f || matrixRows[r].dest2 != 0.0f)) {
            lfoUSed_[(int) matrixRows[r].source - MATRIX_SOURCE_LFO1]++;
        }

        // Check if we have a Mtx* that would require LFO even if mul is 0
        if (matrixRows[r].mul != 0.0f && matrixRows[r].source != 0.0f) {
            if (matrixRows[r].dest1 >= MTX1_MUL && matrixRows[r].dest1 <= MTX4_MUL) {
                int index = matrixRows[r].dest1 - MTX1_MUL;
                if (matrixRows[index].source >= MATRIX_SOURCE_LFO1 && matrixRows[index].source <= MATRIX_SOURCE_LFOSEQ2) {
                    lfoUSed_[(int) matrixRows[index].source - MATRIX_SOURCE_LFO1]++;
                }
            }
            // same test for dest2
            if (matrixRows[r].dest2 >= MTX1_MUL && matrixRows[r].dest2 <= MTX4_MUL) {
                int index = matrixRows[r].dest2 - MTX1_MUL;
                if (matrixRows[index].source >= MATRIX_SOURCE_LFO1 && matrixRows[index].source <= MATRIX_SOURCE_LFOSEQ2) {
                    lfoUSed_[(int) matrixRows[index].source - MATRIX_SOURCE_LFO1]++;
                }
            }
        }
    }
}

uint32_t Preenfm3Engine::getStorageMagic() const {
    return PRESET_VERSION1;
}

int Preenfm3Engine::getStorageNameOffset(uint32_t magic) const {
    if (magic == PRESET_VERSION2) {
        return offsetof(OneSynthParams, presetName);
    }
    return offsetof(FlashSynthParams, presetName);
}

const char* Preenfm3Engine::getStorageDefaultName() const {
    return "";
}

uint32_t Preenfm3Engine::getStorageBlob(uint8_t *out, uint32_t maxSize, const TimbreSharedParams &shared) {
    timbre_->serializeParams(oneSynthParamsTmp);
    pfm3ParamsToFlash(&oneSynthParamsTmp, (FlashSynthParams*) out, true);
    return sizeof(FlashSynthParams);
}

bool Preenfm3Engine::loadStorageBlob(const uint8_t *in, uint32_t size, uint32_t magic) {
    if (magic == PRESET_VERSION2) {
        memcpy(&oneSynthParamsTmp, in, sizeof(OneSynthParams));
    } else {
        pfm3FlashToParams((const FlashSynthParams*) in, &oneSynthParamsTmp, true);
    }
    timbre_->applyFlatParams(oneSynthParamsTmp);
    return true;
}
