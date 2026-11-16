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
#include "SynthState.h"
#include "Pfm3ParameterRows.h"
#include "MidiControlChange.h"
#include "Pfm3ControlChange.h"
#include "Pfm3Midi.h"
#include "Timbre.h"
#include "Voice.h"
#include "VoicePool.h"
#include "Pfm3Preset.h"

/* Unsigned saturation, used by the PFM3 oscillator mix code. */
#define __USAT(ARG1,ARG2) \
({                          \
  uint32_t __RES, __ARG1 = (ARG1); \
  asm ("usat %0, %1, %2" : "=r" (__RES) :  "I" (ARG2), "r" (__ARG1) ); \
  __RES; \
})

static Pfm3TimbreContext pfm3Contexts[NUMBER_OF_TIMBRES] __attribute__((section(".ram_d1")));
Pfm3TimbreParams Preenfm3Engine::pfm3Params_[NUMBER_OF_TIMBRES] __attribute__((section(".ram_d1")));
Pfm3Preset pfm3PresetTmp __attribute__((section(".ram_d1")));

float Preenfm3Engine::glidePhaseInc[13] = {
    1.0f/2.0f, 1.0f/5.0f, 1.0f/9.0f, 1.0f/15.0f, 1.0f/22.0f, 1.0f/35.0f, 1.0f/50.0f,
    1.0f/90.0f, 1.0f/140.0f, 1.0f/200.0f, 1.0f/500.0f, 1.0f/1200.0f, 1.0f/2700.0f
};

float Preenfm3Engine::mpeBitchBend[6] = { 0.0f, 480.0f, 360.0f, 240.0f, 120.0f, 0.0f };

Pfm3VoiceState Preenfm3Engine::pfm3VoiceStates_[MAX_NUMBER_OF_VOICES];

Pfm3VoiceState& Preenfm3Engine::voiceState(int slot) {
    return pfm3VoiceStates_[slot];
}

Pfm3VoiceState& Preenfm3Engine::pfm3State(Voice *voice) {
    return pfm3VoiceStates_[voice->slot()];
}

void Preenfm3Engine::initVoiceState(int slot) {
    Pfm3VoiceState &state = pfm3VoiceStates_[slot];
    state.freqAi = state.freqAo = 0.0f;
    state.freqBi = state.freqBo = 0.0f;
    state.freqHarm = 1.0f;
    state.targetFreqHarm = 0.0f;
}

const float Preenfm3Engine::unisonPhase[MAX_UNISON_VOICES] = {
    .37f, .11f, .495f, .53f, .03f, .19f, .89f
};

#define INV127 .00787401574803149606f
#define INV16 .0625

extern float midiNoteScale[2][NUMBER_OF_TIMBRES][128];
extern float noise[32];

void Preenfm3Engine::init(Timbre *timbre, SynthState *synthState) {
    timbre_ = timbre;
    synthState_ = synthState;
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

    for (int slot = 0; slot < MAX_NUMBER_OF_VOICES; slot++) {
        initVoiceState(slot);
    }
}

void Preenfm3Engine::initOscEnv(SynthState *synthState) {
    Pfm3TimbreParams *p = &pfm3Params();

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
        afterParamsLoadVoice(timbre_->voices_[timbre_->voiceNumber_[k]]);
    }

    for (int j = 0; j < NUMBER_OF_ENCODERS_PFM2; j++) {
        for (int e = 0; e < 6; e++) {
            env_[e].reloadADSR(j);
        }
    }
    refreshContext();
}

void Preenfm3Engine::initContext() {
    Pfm3TimbreParams *p = &pfm3Params();

    context_->params = p;
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
    Pfm3TimbreParams *p = &pfm3Params();

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

    context_->mixerGain = timbre_->sharedParams().effect1.param3;
}

void Preenfm3Engine::bindVoice(Voice *voice) {
    bindVoiceState(voice);
}

void Preenfm3Engine::noteOn(char note, char velocity) {
    if (unlikely(timbre_->maxVoices_ == 0
        || timbre_->mixerState_->instrumentState_[timbre_->timbreNumber_].scaleFrequencies[(int) note] == 0.0f)) {
        return;
    }

    note &= 0x7f;

    float noteFrequency = timbre_->mixerState_->instrumentState_[timbre_->timbreNumber_].scaleFrequencies[(int) note];
    bool unison = pfm3Params().engine1.playMode == PLAY_MODE_UNISON;
    bool mono = pfm3Params().engine1.playMode == PLAY_MODE_MONO;
    int unisonWidth = timbre_->maxVoices_ < MAX_UNISON_VOICES ? (int) timbre_->maxVoices_ : MAX_UNISON_VOICES;

    timbre_->numberOfVoiceInverse_ = 1.0f / (float) (unison ? unisonWidth : (timbre_->maxVoices_ > 0 ? timbre_->maxVoices_ : 1));

    if (unison) {
        timbre_->trimActiveVoices(unisonWidth);
        if (!timbre_->ensureActiveVoices(unisonWidth)) {
            return;
        }
        float noteFrequencyUnison = noteFrequency + noteFrequency * pfm3Params().engine2.unisonDetune * .05f;
        float noteFrequencyUnisonInc = noteFrequency * pfm3Params().engine2.unisonDetune * timbre_->numberOfVoiceInverse_ * .1f;
        for (int k = 0; k < unisonWidth; k++) {
            int n = timbre_->voiceNumber_[k];
            preenNoteOnUpdateMatrix(n, note, velocity);
            if (timbre_->voices_[n]->isPlaying()) {
                noteOnVoiceWithoutPop(timbre_->voices_[n], note, noteFrequencyUnison, velocity, timbre_->voiceIndex_++, unisonPhase[k]);
            } else {
                noteOnVoice(timbre_->voices_[n], note, noteFrequencyUnison, velocity, timbre_->voiceIndex_++, unisonPhase[k]);
            }
            noteFrequencyUnison += noteFrequencyUnisonInc;
        }
        timbre_->lastPlayedNote_ = timbre_->voiceNumber_[0];
        timbre_->lowerNote_ = note;
        timbre_->lowerNoteReleased_ = false;
        timbre_->lowerNoteFrequency_ = getNoteRealFrequencyEstimation(timbre_->voices_[timbre_->voiceNumber_[0]], noteFrequency);
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
        noteOnVoiceWithoutPop(timbre_->voices_[voiceToUse], note, noteFrequency, velocity, timbre_->voiceIndex_++);
    } else {
        noteOnVoice(timbre_->voices_[voiceToUse], note, noteFrequency, velocity, timbre_->voiceIndex_++);
    }

    if (!transferPending) {
        timbre_->lastPlayedNote_ = voiceToUse;
    }
    if (note <= timbre_->lowerNote_ || timbre_->lowerNoteReleased_ || mono) {
        timbre_->lowerNote_ = note;
        timbre_->lowerNoteReleased_ = false;
        if (!transferPending) {
            timbre_->lowerNoteFrequency_ = getNoteRealFrequencyEstimation(timbre_->voices_[voiceToUse], noteFrequency);
        }
    }
}

void Preenfm3Engine::noteOff(char note) {
    bool isUnison = pfm3Params().engine1.playMode == PLAY_MODE_UNISON;

    if (note == timbre_->lowerNote_) {
        timbre_->lowerNoteReleased_ = true;
    }

    // Unison we must check all voices.... (different from noteOn)
    int iNov = pfm3Params().engine1.playMode == PLAY_MODE_MONO ? 1 : (int) timbre_->numberOfVoices_;
    for (int k = 0; k < iNov; k++) {
        // voice number k of timbre
        int n = timbre_->voiceNumber_[k];

        // Not playing = free CPU
        if (unlikely(!timbre_->voices_[n]->isPlaying())) {
            continue;
        }

        if (likely(pfm3State(timbre_->voices_[n]).nextGlidingNote == 0 || pfm3State(timbre_->voices_[n]).newGlide)) {
            if (timbre_->voices_[n]->getNote() == note) {
                if (unlikely(timbre_->holdPedal_)) {
                    timbre_->voices_[n]->setHoldedByPedal(true);
                    if (likely(!isUnison)) {
                        return;
                    }
                } else {
                    noteOffVoice(timbre_->voices_[n]);
                    if (likely(!isUnison)) {
                        return;
                    }
                }
            }
        } else {
            // if gliding and releasing first note
            if (timbre_->voices_[n]->getNote() == note) {
                glideFirstNoteOff(timbre_->voices_[n]);
                if (likely(!isUnison)) {
                    return;
                }
            }
            // if gliding and releasing next note
            if (pfm3State(timbre_->voices_[n]).nextGlidingNote == note) {
                glideToNote(timbre_->voices_[n], timbre_->voices_[n]->getNote(), timbre_->voices_[n]->getNoteFrequency());
                glideFirstNoteOff(timbre_->voices_[n]);
                // Sync Osccilo
                timbre_->lowerNoteFrequency_ = getNoteRealFrequencyEstimation(timbre_->voices_[n], timbre_->voices_[n]->getNoteFrequency());
                if (likely(!isUnison)) {
                    return;
                }
            }
        }
    }
}

void Preenfm3Engine::preenNoteOnUpdateMatrix(int voiceToUse, int note, int velocity) {
    // Update voice matrix with midi note and velocity
    pfm3State(timbre_->voices_[voiceToUse]).matrix.setSource(MATRIX_SOURCE_NOTE1, midiNoteScale[0][timbre_->timbreNumber_][note]);
    pfm3State(timbre_->voices_[voiceToUse]).matrix.setSource(MATRIX_SOURCE_NOTE2, midiNoteScale[1][timbre_->timbreNumber_][note]);
    pfm3State(timbre_->voices_[voiceToUse]).matrix.setSource(MATRIX_SOURCE_VELOCITY, INV127 * velocity);
    pfm3State(timbre_->voices_[voiceToUse]).matrix.setSource(MATRIX_SOURCE_RANDOM, noise[voiceToUse]);
}

void Preenfm3Engine::bindVoiceState(Voice *voice) {
    resetPfm3VoiceState(voice);
    voice->currentTimbre_ = timbre_;
    Pfm3VoiceState &state = pfm3State(voice);

    state.matrix.init(context_->matrixRows);

    for (int k = 0; k < NUMBER_OF_LFO_OSC; k++) {
        state.lfoOsc[k].init(context_->lfoOscParams[k], context_->lfoPhases[k], &state.matrix,
            (SourceEnum) (MATRIX_SOURCE_LFO1 + k), (DestinationEnum) (LFO1_FREQ + k));
    }

    state.lfoEnv[0].init(context_->lfoEnv, &state.matrix, MATRIX_SOURCE_LFOENV1, (DestinationEnum) 0);
    state.lfoEnv2[0].init(context_->lfoEnv2, &state.matrix, MATRIX_SOURCE_LFOENV2, (DestinationEnum) LFOENV2_SILENCE);

    for (int k = 0; k < NUMBER_OF_LFO_STEP; k++) {
        state.lfoStepSeq[k].init(context_->lfoStepSeqParams[k], context_->lfoStepSeqSteps[k], &state.matrix,
            (SourceEnum) (MATRIX_SOURCE_LFOSEQ1 + k), (DestinationEnum) (LFOSEQ1_GATE + k));
    }

    voice->setNewEffectParam(0);

    state.pan1Left = context_->panLeft[0];
    state.pan1Right = context_->panRight[0];
    state.pan2Left = context_->panLeft[1];
    state.pan2Right = context_->panRight[1];
    state.pan3Left = context_->panLeft[2];
    state.pan3Right = context_->panRight[2];
    state.pan4Left = context_->panLeft[3];
    state.pan4Right = context_->panRight[3];
    state.pan5Left = context_->panLeft[4];
    state.pan5Right = context_->panRight[4];
    state.pan6Left = context_->panLeft[5];
    state.pan6Right = context_->panRight[5];

    voice->mixerGain_ = context_->mixerGain;

    state.env1ValueMem = 0;
    state.env2ValueMem = 0;
    state.env3ValueMem = 0;
    state.env4ValueMem = 0;
    state.env5ValueMem = 0;
    state.env6ValueMem = 0;
}

void Preenfm3Engine::glideFirstNoteOff(Voice *voice) {
    voice->note_ = pfm3State(voice).nextGlidingNote;
    voice->noteFrequency_ = pfm3State(voice).nextGlidingNoteFrequency;
    pfm3State(voice).nextGlidingNote = 0;
}

void Preenfm3Engine::glideToNote(Voice *voice, short newNote, float newNoteFrequency) {
    pfm3State(voice).gliding = true;
    pfm3State(voice).glidePhase = 0.0f;
    pfm3State(voice).nextGlidingNote = newNote;
    pfm3State(voice).nextGlidingNoteFrequency = newNoteFrequency;
    if (voice->holdedByPedal_) {
        glideFirstNoteOff(voice);
    }

    Pfm3VoiceState &state = pfm3State(voice);
    osc_[0].glideToNote(&state.oscState1_, newNoteFrequency);
    osc_[1].glideToNote(&state.oscState2_, newNoteFrequency);
    osc_[2].glideToNote(&state.oscState3_, newNoteFrequency);
    osc_[3].glideToNote(&state.oscState4_, newNoteFrequency);
    osc_[4].glideToNote(&state.oscState5_, newNoteFrequency);
    osc_[5].glideToNote(&state.oscState6_, newNoteFrequency);
}

void Preenfm3Engine::glideVoice(Voice *voice) {
    pfm3State(voice).glidePhase += glidePhaseInc[(int) (pfm3Params().engine1.glideSpeed + .05f)];
    Pfm3VoiceState &state = pfm3State(voice);
    if (pfm3State(voice).glidePhase < 1.0f) {
        osc_[0].glideStep(&state.oscState1_, pfm3State(voice).glidePhase);
        osc_[1].glideStep(&state.oscState2_, pfm3State(voice).glidePhase);
        osc_[2].glideStep(&state.oscState3_, pfm3State(voice).glidePhase);
        osc_[3].glideStep(&state.oscState4_, pfm3State(voice).glidePhase);
        osc_[4].glideStep(&state.oscState5_, pfm3State(voice).glidePhase);
        osc_[5].glideStep(&state.oscState6_, pfm3State(voice).glidePhase);
    } else {
        osc_[0].glideStep(&state.oscState1_, 1);
        osc_[1].glideStep(&state.oscState2_, 1);
        osc_[2].glideStep(&state.oscState3_, 1);
        osc_[3].glideStep(&state.oscState4_, 1);
        osc_[4].glideStep(&state.oscState5_, 1);
        osc_[5].glideStep(&state.oscState6_, 1);
        pfm3State(voice).gliding = false;
    }
}

// Frequency used for oscilloscope sync, based on the first oscillator state.
float Preenfm3Engine::getNoteRealFrequencyEstimation(Voice *voice, float newNoteFrequency) {
    return osc_[0].getNoteRealFrequencyEstimation(&pfm3State(voice).oscState1_, newNoteFrequency);
}

void Preenfm3Engine::noteOnVoiceWithoutPop(Voice *voice, short newNote, float newNoteFrequency, short velocity, uint32_t index, float phase) {
    voice->index_ = index;
    if (voice->pendingTimbre_ == nullptr && !voice->released_
        && pfm3Params().engine1.playMode != PLAY_MODE_POLY
        && pfm3Params().engine2.glideType != GLIDE_TYPE_OFF) {
        glideToNote(voice, newNote, newNoteFrequency);
        voice->holdedByPedal_ = false;
        pfm3State(voice).newGlide = false;
    } else {
        voice->note_ = newNote;
        voice->midiVelocity_ = velocity;
        voice->noteFrequency_ = newNoteFrequency;
        pfm3State(voice).newNotePending = true;
        pfm3State(voice).pendingNoteVelocity = velocity;
        pfm3State(voice).pendingNote = newNote;
        pfm3State(voice).pendingNoteFrequency = newNoteFrequency;
        pfm3State(voice).phase_ = phase;
        voice->released_ = false;

        Pfm3VoiceState &state = pfm3State(voice);
        env_[0].noteOffQuick(&state.envState1_);
        env_[1].noteOffQuick(&state.envState2_);
        env_[2].noteOffQuick(&state.envState3_);
        env_[3].noteOffQuick(&state.envState4_);
        env_[4].noteOffQuick(&state.envState5_);
        env_[5].noteOffQuick(&state.envState6_);
        pfm3State(voice).newNotePlayed = false;
    }
}

void Preenfm3Engine::noteOnVoice(Voice *voice, short newNote, float newNoteFrequency, short velocity, uint32_t index, float phase) {

    Pfm3TimbreParams &params = pfm3Params();
    Pfm3VoiceState &state = pfm3State(voice);

    if (unlikely(params.engine1.playMode != PLAY_MODE_POLY
        && params.engine2.glideType == GLIDE_TYPE_ALWAYS)) {
        if (unlikely(pfm3State(voice).nextMainFrequency != 0.0f)) {
            voice->noteFrequency_ = pfm3State(voice).nextMainFrequency;
        } else {
            voice->noteFrequency_ = newNoteFrequency;
        }
        glideToNote(voice, newNote, newNoteFrequency);
        pfm3State(voice).newGlide = true;
        voice->note_ = newNote;
        pfm3State(voice).nextMainFrequency = newNoteFrequency;
    } else {
        voice->note_ = newNote;
        voice->noteFrequency_ = newNoteFrequency;

        if (unlikely(params.engine2.unisonDetune < 0.0f)) {
            phase = 0.25f;
        }

        osc_[0].newNote(&state.oscState1_, newNoteFrequency, phase);
        osc_[0].newNote(&state.oscState1_, newNoteFrequency, phase);
        osc_[1].newNote(&state.oscState2_, newNoteFrequency, phase);
        osc_[2].newNote(&state.oscState3_, newNoteFrequency, phase);
        osc_[3].newNote(&state.oscState4_, newNoteFrequency, phase);
        osc_[4].newNote(&state.oscState5_, newNoteFrequency, phase);
        osc_[5].newNote(&state.oscState6_, newNoteFrequency, phase);
    }

    voice->midiVelocity_ = velocity;
    voice->released_ = false;
    voice->playing_ = true;
    voice->isFullOfZero_ = false;
    pfm3State(voice).pendingNote = 0;
    pfm3State(voice).newNotePending = false;
    voice->holdedByPedal_ = false;
    voice->index_ = index;
    state.fdbLastValue[0] = 0.0f;
    state.fdbLastValue[1] = 0.0f;
    state.fdbLastValue[2] = 0.0f;

    state.velIm1 = params.engineIm1.modulationIndexVelo1 * (float) velocity * .0078125f;
    state.velIm2 = params.engineIm1.modulationIndexVelo2 * (float) velocity * .0078125f;
    state.velIm3 = params.engineIm2.modulationIndexVelo3 * (float) velocity * .0078125f;
    state.velIm4 = params.engineIm2.modulationIndexVelo4 * (float) velocity * .0078125f;
    state.velIm5 = params.engineIm3.modulationIndexVelo5 * (float) velocity * .0078125f;
    state.velIm6 = params.engineIm3.modulationIndexVelo6 * (float) velocity * .0078125f;

    int zeroVelo = (16 - params.engine1.velocity) * 8;
    int newVelocity = zeroVelo + ((velocity * (128 - zeroVelo)) >> 7);
    voice->velocity_ = newVelocity * .0078125f;

    pfm3State(voice).newNotePlayed = true;
    lfoNoteOn(voice);
}

void Preenfm3Engine::endNoteOrBeginNextOne(Voice *voice) {
    if (pfm3State(voice).newNotePending) {
        if (voice->pendingTimbre_ != nullptr) {
            pfm3State(voice).newNotePending = false;
            voice->transferReady_ = true;
        } else if (pfm3State(voice).pendingNote <= 127) {
            noteOnVoice(voice, pfm3State(voice).pendingNote, pfm3State(voice).pendingNoteFrequency, pfm3State(voice).pendingNoteVelocity, voice->index_, pfm3State(voice).phase_);
        } else {
            noteOnVoice(voice, pfm3State(voice).pendingNote - 128, pfm3State(voice).pendingNoteFrequency, pfm3State(voice).pendingNoteVelocity, voice->index_, pfm3State(voice).phase_);
            pfm3State(voice).noteAlreadyFinished = 1;
        }
    } else {
        voice->playing_ = false;
        voice->released_ = false;
    }
    Pfm3VoiceState &state = pfm3State(voice);
    state.freqAi = 0.0f;
    state.freqAo = 0.0f;
    state.freqBi = 0.0f;
    state.freqBo = 0.0f;
}

void Preenfm3Engine::lfoNoteOn(Voice *voice) {
    Pfm3VoiceState &state = pfm3State(voice);
    if (likely(timbre_->isLfoUsed(0))) {
        state.lfoOsc[0].noteOn();
    }
    if (likely(timbre_->isLfoUsed(1))) {
        state.lfoOsc[1].noteOn();
    }
    if (likely(timbre_->isLfoUsed(2))) {
        state.lfoOsc[2].noteOn();
    }
    if (likely(timbre_->isLfoUsed(3))) {
        state.lfoEnv[0].noteOn();
    }
    if (likely(timbre_->isLfoUsed(4))) {
        state.lfoEnv2[0].noteOn();
    }
    if (likely(timbre_->isLfoUsed(5))) {
        state.lfoStepSeq[0].noteOn();
    }
    if (likely(timbre_->isLfoUsed(6))) {
        state.lfoStepSeq[1].noteOn();
    }
}

void Preenfm3Engine::lfoNoteOff(Voice *voice) {
    Pfm3VoiceState &state = pfm3State(voice);
    if (likely(timbre_->isLfoUsed(0))) {
        state.lfoOsc[0].noteOff();
    }
    if (likely(timbre_->isLfoUsed(1))) {
        state.lfoOsc[1].noteOff();
    }
    if (likely(timbre_->isLfoUsed(2))) {
        state.lfoOsc[2].noteOff();
    }
    if (likely(timbre_->isLfoUsed(3))) {
        state.lfoEnv[0].noteOff();
    }
    if (likely(timbre_->isLfoUsed(4))) {
        state.lfoEnv2[0].noteOff();
    }
    if (likely(timbre_->isLfoUsed(5))) {
        state.lfoStepSeq[0].noteOff();
    }
    if (likely(timbre_->isLfoUsed(6))) {
        state.lfoStepSeq[1].noteOff();
    }
}

void Preenfm3Engine::afterParamsLoadVoice(Voice *voice) {
    resetMatrix(voice);
    Pfm3VoiceState &state = pfm3State(voice);
    for (int j = 0; j < NUMBER_OF_ENCODERS_PFM2; j++) {
        state.lfoOsc[0].valueChanged(j);
        state.lfoOsc[1].valueChanged(j);
        state.lfoOsc[2].valueChanged(j);
        state.lfoEnv[0].valueChanged(j);
        state.lfoEnv2[0].valueChanged(j);
        state.lfoStepSeq[0].valueChanged(j);
        state.lfoStepSeq[1].valueChanged(j);
    }
    voice->v0L_ = voice->v1L_ = 0.0f;
    voice->v0R_ = voice->v1R_ = 0.0f;
}

void Preenfm3Engine::resetMatrix(Voice *voice) {
    Pfm3VoiceState &state = pfm3State(voice);
    state.matrix.resetSources();
    state.matrix.resetAllDestination();
}

void Preenfm3Engine::lfoValueChangeOnVoice(Voice *voice, int currentRow, int encoder, float newValue) {
    Pfm3VoiceState &state = pfm3State(voice);
    switch (currentRow) {
        case ROW_LFOOSC1:
        case ROW_LFOOSC2:
        case ROW_LFOOSC3:
            state.lfoOsc[currentRow - ROW_LFOOSC1].valueChanged(encoder);
            break;
        case ROW_LFOENV1:
            state.lfoEnv[0].valueChanged(encoder);
            break;
        case ROW_LFOENV2:
            state.lfoEnv2[0].valueChanged(encoder);
            break;
        case ROW_LFOSEQ1:
        case ROW_LFOSEQ2:
            state.lfoStepSeq[currentRow - ROW_LFOSEQ1].valueChanged(encoder);
            break;
    }
}

void Preenfm3Engine::noteOffVoice(Voice *voice) {
    if (unlikely(!voice->playing_)) {
        return;
    }
    if (likely(!pfm3State(voice).newNotePending)) {
        if (pfm3State(voice).newNotePlayed) {
            pfm3State(voice).noteAlreadyFinished = 1;
            return;
        }
        voice->released_ = true;
        pfm3State(voice).gliding = false;
        voice->holdedByPedal_ = false;

        Pfm3VoiceState &state = pfm3State(voice);
        env_[0].noteOff(&state.envState1_, &state.matrix);
        env_[1].noteOff(&state.envState2_, &state.matrix);
        env_[2].noteOff(&state.envState3_, &state.matrix);
        env_[3].noteOff(&state.envState4_, &state.matrix);
        env_[4].noteOff(&state.envState5_, &state.matrix);
        env_[5].noteOff(&state.envState6_, &state.matrix);

        lfoNoteOff(voice);
    } else {
        pfm3State(voice).pendingNote += 128;
    }
}

void Preenfm3Engine::noteOffVoiceQuick(Voice *voice) {
    if (unlikely(!voice->playing_)) {
        return;
    }
    if (pfm3State(voice).newNotePlayed) {
        killVoice(voice);
        return;
    }
    voice->released_ = true;
    pfm3State(voice).gliding = false;
    voice->holdedByPedal_ = false;

    Pfm3VoiceState &state = pfm3State(voice);
    env_[0].noteOffQuick(&state.envState1_);
    env_[1].noteOffQuick(&state.envState2_);
    env_[2].noteOffQuick(&state.envState3_);
    env_[3].noteOffQuick(&state.envState4_);
    env_[4].noteOffQuick(&state.envState5_);
    env_[5].noteOffQuick(&state.envState6_);

    lfoNoteOff(voice);
}

void Preenfm3Engine::killVoice(Voice *voice) {
    voice->resetVoiceState();
    resetPfm3VoiceState(voice);
    Pfm3VoiceState &state = pfm3State(voice);
    state.env1ValueMem = 0;
    state.env2ValueMem = 0;
    state.env3ValueMem = 0;
    state.env4ValueMem = 0;
    state.env5ValueMem = 0;
    state.env6ValueMem = 0;
}

void Preenfm3Engine::startVoiceAfterTransfer(Voice *voice, int note, float noteFrequency, uint8_t velocity, float phase, bool alreadyOff) {
    noteOnVoice(voice, note, noteFrequency, velocity, voice->index_, phase);
    if (alreadyOff) {
        pfm3State(voice).noteAlreadyFinished = 1;
    }
}

void Preenfm3Engine::completeVoiceTransfer(Voice *voice, int poolIndex, VoicePool *pool) {
    Timbre *target = voice->pendingTimbre_;
    if (target == nullptr) {
        return;
    }

    Pfm3VoiceState &state = pfm3State(voice);
    int note = state.pendingNote & 0x7f;
    bool alreadyOff = state.pendingNote > 127;
    uint8_t velocity = (uint8_t) state.pendingNoteVelocity;
    float noteFrequency = state.pendingNoteFrequency;
    float phase = state.phase_;

    timbre_->dropVoiceToPool(poolIndex);
    if (!target->adoptVoiceFromPool(voice, poolIndex)) {
        pool->release(poolIndex);
        voice->resetVoiceState();
        resetPfm3VoiceState(voice);
        return;
    }

    target->updateVoiceMatrix(voice, note, velocity);
    voice->pendingTimbre_ = nullptr;
    voice->transferReady_ = false;
    target->startVoiceAfterTransfer(voice, note, noteFrequency, velocity, phase, alreadyOff);
}

void Preenfm3Engine::resetPfm3VoiceState(Voice *voice) {
    Pfm3VoiceState &state = pfm3State(voice);
    state.newNotePlayed = false;
    state.newNotePending = false;
    state.pendingNote = 0;
    state.pendingNoteVelocity = 0;
    state.pendingNoteFrequency = 0.0f;
    state.nextIndex = 0;
    state.phase_ = 0.0f;
    state.gliding = false;
    state.glidePhase = 0.0f;
    state.nextGlidingNote = 0;
    state.nextGlidingNoteFrequency = 0.0f;
    state.newGlide = false;
    state.nextMainFrequency = 0.0f;
    state.noteAlreadyFinished = 0;
}

void Preenfm3Engine::updateAllModulationIndexes(Voice *voice) {
    Pfm3VoiceState &state = pfm3State(voice);
    Pfm3TimbreParams &pfm3 = pfm3Params();
    int numberOfIMs = algoInformation[(int) (pfm3.engine1.algo)].im;

    // Feedback range is [0:1] compared to [0:16] of other modulation, let's divide the modulation impact by 16 (* 0.0625)
    state.feedbackModulation = pfm3.engineIm3.modulationIndex6 + state.velIm6
        + state.matrix.getDestination(MTX_DEST_FEEDBACK) * 0.0625f;
    if (unlikely(state.feedbackModulation < 0.0f)) {
        state.feedbackModulation = 0.0f;
    } else if (unlikely(state.feedbackModulation > 1.0f)) {
        state.feedbackModulation = 1.0f;
    }

    state.modulationIndex1 = pfm3.engineIm1.modulationIndex1 + state.matrix.getDestination(INDEX_MODULATION1)
        + state.matrix.getDestination(INDEX_ALL_MODULATION) + state.velIm1;
    if (unlikely(state.modulationIndex1 < 0.0f)) {
        state.modulationIndex1 = 0.0f;
    }
    state.modulationIndex2 = pfm3.engineIm1.modulationIndex2 + state.matrix.getDestination(INDEX_MODULATION2)
        + state.matrix.getDestination(INDEX_ALL_MODULATION) + state.velIm2;
    if (unlikely(state.modulationIndex2 < 0.0f)) {
        state.modulationIndex2 = 0.0f;
    }

    state.modulationIndex3 = pfm3.engineIm2.modulationIndex3 + state.matrix.getDestination(INDEX_MODULATION3)
        + state.matrix.getDestination(INDEX_ALL_MODULATION) + state.velIm3;
    if (unlikely(state.modulationIndex3 < 0.0f)) {
        state.modulationIndex3 = 0.0f;
    }

    if (likely(numberOfIMs < 3)) {
        return;
    }

    state.modulationIndex4 = pfm3.engineIm2.modulationIndex4 + state.matrix.getDestination(INDEX_MODULATION4)
        + state.matrix.getDestination(INDEX_ALL_MODULATION) + state.velIm4;
    if (unlikely(state.modulationIndex4 < 0.0f)) {
        state.modulationIndex4 = 0.0f;
    }

    state.modulationIndex5 = pfm3.engineIm3.modulationIndex5 + state.matrix.getDestination(INDEX_ALL_MODULATION) + state.velIm5;
    if (unlikely(state.modulationIndex5 < 0.0f)) {
        state.modulationIndex5 = 0.0f;
    }
}

void Preenfm3Engine::prepareMatrixForNewBlock(Voice *voice) {
    Pfm3VoiceState &state = pfm3State(voice);
    // first 3 LFO can be free running
    if (likely(isLfoUsed(0))) {
        state.lfoOsc[0].nextValueInMatrix();
    }
    if (likely(isLfoUsed(1))) {
        state.lfoOsc[1].nextValueInMatrix();
    }
    if (likely(isLfoUsed(2))) {
        state.lfoOsc[2].nextValueInMatrix();
    }

    // Only compute the rest if playing
    if (likely(voice->isPlaying())) {
        if (likely(isLfoUsed(3))) {
            state.lfoEnv[0].nextValueInMatrix();
        }
        if (likely(isLfoUsed(4))) {
            state.lfoEnv2[0].nextValueInMatrix();
        }
        if (likely(isLfoUsed(5))) {
            state.lfoStepSeq[0].nextValueInMatrix();
        }
        if (likely(isLfoUsed(6))) {
            state.lfoStepSeq[1].nextValueInMatrix();
        }

        state.matrix.computeAllDestinations();
        updateAllModulationIndexes(voice);
    }
}

void Preenfm3Engine::updateAllMixOscsAndPans(Voice *voice) {
    Pfm3VoiceState &state = pfm3State(voice);
    Pfm3TimbreParams &pfm3 = pfm3Params();
    float inv65535 = .0000152587890625; // 1/ 65535
    int pan;
    int numberOfMix = algoInformation[(int) (pfm3.engine1.algo)].mix;
    // add a LP on mix when MPE to mitigate noise dur to midi CC being [0-127]
    bool notMPE = timbre_->timbreNumber_ != 0 || timbre_->getMPESetting() == 0;

    state.mix1 = pfm3.engineMix1.mixOsc1 + state.matrix.getDestination(MIX_OSC1) + state.matrix.getDestination(ALL_MIX);
    // Optimization to check state.mix1 is between 0 and 1
    if (likely(notMPE)) {
        state.mix1 = __USAT((int)(state.mix1 * 65536) , 16) * inv65535;
    } else {
        state.mix1 = __USAT((int)((state.mix1 * .05 + state.mix1Previous * .95) * 65536) , 16) * inv65535;
        state.mix1Previous = state.mix1;
    }
    float pan1 = pfm3.engineMix1.panOsc1 + state.matrix.getDestination(PAN_OSC1) + state.matrix.getDestination(ALL_PAN) + 1.0f;
    // pan1 is between -1 and 1 : Scale from 0.0 to 256
    pan = __USAT((int )(pan1 * 128), 8);
    state.pan1Left = panTable[pan] * .05f + state.pan1Left * .95f;
    state.pan1Right = panTable[256 - pan] * .05f + state.pan1Right * .95f;

    if (unlikely(numberOfMix == 1)) {
        return;
    }

    state.mix2 = pfm3.engineMix1.mixOsc2 + state.matrix.getDestination(MIX_OSC2) + state.matrix.getDestination(ALL_MIX);
    if (likely(notMPE)) {
        state.mix2 = __USAT((int)(state.mix2 * 65535) , 16) * inv65535;
    } else {
        state.mix2 = __USAT((int)((state.mix2 * .05 + state.mix2Previous * .95) * 65535) , 16) * inv65535;
        state.mix2Previous = state.mix2;
    }
    float pan2 = pfm3.engineMix1.panOsc2 + state.matrix.getDestination(PAN_OSC2) + state.matrix.getDestination(ALL_PAN) + 1.0f;
    pan = __USAT((int )(pan2 * 128), 8);
    state.pan2Left = panTable[pan] * .05f + state.pan2Left * .95f;
    state.pan2Right = panTable[256 - pan] * .05f + state.pan2Right * .95f;

    if (unlikely(numberOfMix == 2)) {
        return;
    }

    state.mix3 = pfm3.engineMix2.mixOsc3 + state.matrix.getDestination(MIX_OSC3) + state.matrix.getDestination(ALL_MIX);
    if (likely(notMPE)) {
        state.mix3 = __USAT((int)(state.mix3 * 65535) , 16) * inv65535;
    } else {
        state.mix3 = __USAT((int)((state.mix3 * .05 + state.mix3Previous * .95) * 65535) , 16) * inv65535;
        state.mix3Previous = state.mix3;
    }
    float pan3 = pfm3.engineMix2.panOsc3 + state.matrix.getDestination(PAN_OSC3) + state.matrix.getDestination(ALL_PAN) + 1.0f;
    pan = __USAT((int )(pan3 * 128), 8);
    state.pan3Left = panTable[pan] * .05f + state.pan3Left * .95f;
    state.pan3Right = panTable[256 - pan] * .05f + state.pan3Right * .95f;

    if (numberOfMix == 3) {
        return;
    }

    // No matrix for state.mix4 and pan4
    state.mix4 = pfm3.engineMix2.mixOsc4 + state.matrix.getDestination(MIX_OSC4) + state.matrix.getDestination(ALL_MIX);
    if (likely(notMPE)) {
        state.mix4 = __USAT((int)(state.mix4 * 65535) , 16) * inv65535;
    } else {
        state.mix4 = __USAT((int)((state.mix4 * .05 + state.mix4Previous * .95) * 65535) , 16) * inv65535;
        state.mix4Previous = state.mix4;
    }
    float pan4 = pfm3.engineMix2.panOsc4 + state.matrix.getDestination(PAN_OSC4) + state.matrix.getDestination(ALL_PAN) + 1.0f;
    pan = __USAT((int )(pan4 * 128), 8);
    state.pan4Left = panTable[pan] * .05f + state.pan4Left * .95f;
    state.pan4Right = panTable[256 - pan] * .05f + state.pan4Right * .95f;

    if (numberOfMix == 4) {
        return;
    }

    state.mix5 = pfm3.engineMix3.mixOsc5 + state.matrix.getDestination(ALL_MIX);
    if (likely(notMPE)) {
        state.mix5 = __USAT((int)(state.mix5 * 65535) , 16) * inv65535;
    } else {
        state.mix5 = __USAT((int)((state.mix5 * .05 + state.mix5Previous * .95) * 65535) , 16) * inv65535;
        state.mix5Previous = state.mix5;
    }

    float pan5 = pfm3.engineMix3.panOsc5 + state.matrix.getDestination(ALL_PAN) + 1.0f;
    pan = __USAT((int )(pan5 * 128), 8);
    state.pan5Left = panTable[pan] * .05f + state.pan5Left * .95f;
    state.pan5Right = panTable[256 - pan] * .05f + state.pan5Right * .95f;

    state.mix6 = pfm3.engineMix3.mixOsc6 + state.matrix.getDestination(ALL_MIX);
    if (likely(notMPE)) {
        state.mix6 = __USAT((int)(state.mix6 * 65535) , 16) * inv65535;
    } else {
        state.mix6 = __USAT((int)((state.mix6 * .05 + state.mix6Previous * .95) * 65535) , 16) * inv65535;
        state.mix6Previous = state.mix6;
    }

    float pan6 = pfm3.engineMix3.panOsc6 + state.matrix.getDestination(ALL_PAN) + 1.0f;
    pan = __USAT((int )(pan6 * 128), 8);
    state.pan6Left = panTable[pan] * .05f + state.pan6Left * .95f;
    state.pan6Right = panTable[256 - pan] * .05f + state.pan6Right * .95f;
}

void Preenfm3Engine::renderVoice(Voice *voice) {
    Pfm3VoiceState &pfm3State_ = pfm3State(voice);
    Timbre *currentTimbre = timbre_;
    float &velocity = voice->velocity_;

    // After matrix
    if (unlikely(pfm3State(voice).newNotePlayed)) {
        pfm3State(voice).newNotePlayed = false;
        pfm3State_.env1ValueMem  = env_[0].noteOnAfterMatrixCompute(&pfm3State_.envState1_, &pfm3State_.matrix);
        pfm3State_.env2ValueMem  = env_[1].noteOnAfterMatrixCompute(&pfm3State_.envState2_, &pfm3State_.matrix);
        pfm3State_.env3ValueMem  = env_[2].noteOnAfterMatrixCompute(&pfm3State_.envState3_, &pfm3State_.matrix);
        pfm3State_.env4ValueMem  = env_[3].noteOnAfterMatrixCompute(&pfm3State_.envState4_, &pfm3State_.matrix);
        pfm3State_.env5ValueMem  = env_[4].noteOnAfterMatrixCompute(&pfm3State_.envState5_, &pfm3State_.matrix);
        pfm3State_.env6ValueMem  = env_[5].noteOnAfterMatrixCompute(&pfm3State_.envState6_, &pfm3State_.matrix);
    }

    updateAllMixOscsAndPans(voice);

    // Hoist the wave tables once per block; getNextSample() runs per sample.
    const WaveTable* waveTable1 = osc_[0].getWaveTable();
    const WaveTable* waveTable2 = osc_[1].getWaveTable();
    const WaveTable* waveTable3 = osc_[2].getWaveTable();
    const WaveTable* waveTable4 = osc_[3].getWaveTable();
    const WaveTable* waveTable5 = osc_[4].getWaveTable();
    const WaveTable* waveTable6 = osc_[5].getWaveTable();

    float env1Value;
    float env2Value;
    float env3Value;
    float env4Value;
    float env5Value;
    float env6Value;
    float envNextValue;

    float env1Inc;
    float env2Inc;
    float env3Inc;
    float env4Inc;
    float env5Inc;
    float env6Inc;

    float *sample = voice->sampleBlock_;
    float inv32 = .03125f;

    if (unlikely( pfm3State_.matrix.getDestination(ALL_OSC_FREQ_HARM) != pfm3State_.targetFreqHarm) || currentTimbre->getMPESetting() > 0) {
        // * 20 so that we have a full tone for full pitchbend
        pfm3State_.targetFreqHarm = pfm3State_.matrix.getDestination(ALL_OSC_FREQ_HARM) * 20;
        pfm3State_.targetFreqHarm += (pfm3State_.matrix.getSource(MATRIX_SOURCE_PITCHBEND_MPE) * mpeBitchBend[currentTimbre->getMPESetting()]);

        float findex = 512 + pfm3State_.targetFreqHarm;
        int index = findex;
        // Max = 1024
        index &= 0x3ff;
        float fp = findex - index;
        pfm3State_.freqHarm = (exp2_harm[index] * (1.0f - fp) + exp2_harm[index + 1] * fp);
    }

    switch ((int) pfm3Params().engine1.algo) {

        case ALGO1:
            /*
             IM3
             <----
             .---.  .---.
             | 2 |  | 3*|
             '---'  '---'
             \IM1   /IM2
             .---.
             | 1 |
             '---'
             */

        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
                pfm3State_.oscState3_.frequency, pfm3State_.fdbLastValue);

            float f2x;
            float f2xm1 = pfm3State_.freqAi;
            float freq2 = pfm3State_.freqAo;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                // getNextBlockWithFeedbackAndEnveloppe already applied envelope and multiplier
                float freq3 = osc3Values[k];

                pfm3State_.oscState2_.frequency = freq3 * voiceIm3 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                f2x = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * pfm3State_.oscState2_.frequency;
                freq2 = f2x - f2xm1 + 0.99525f * freq2;
                f2xm1 = f2x;

                pfm3State_.oscState1_.frequency = pfm3State_.oscState1_.mainFrequencyPlusMatrix + voiceIm1 * freq2 + voiceIm2 * freq3;
                float currentSample = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * velocity * env1Value * pfm3State_.mix1;

                *sample++ = currentSample * pfm3State_.pan1Right;
                *sample++ = currentSample * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
            }

            pfm3State_.freqAi = f2xm1;
            pfm3State_.freqAo = freq2;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }

        case ALGO2:
            /*
             .---.
             | 3 |
             '---'
             |
             .------.
             |IM1   |IM2
             .---.  .---.
             | 1 |  | 2 |
             '---'  '---'
             |Mix1  |Mix2
             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
                pfm3State_.oscState3_.frequency, pfm3State_.fdbLastValue);

            float div2TimesVelocity = velocity * .5f;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                // getNextBlockWithFeedbackAndEnveloppe already applied envelope and multiplier
                float freq3 = osc3Values[k];

                pfm3State_.oscState2_.frequency = freq3 * voiceIm2 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                float car2 = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * pfm3State_.mix2 * div2TimesVelocity;

                pfm3State_.oscState1_.frequency = freq3 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div2TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
            }

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;

        case ALGO3:
            /*

             IM4
             ---->
             .---.  .---.     .---.
             | 2 |  | 3 |     | 4*|
             '---'  '---'     '---'
             \IM1  |IM2    /IM3
             .---.
             | 1 |
             '---'
             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
                pfm3State_.oscState3_.frequency, pfm3State_.fdbLastValue);

            float f4x;
            float f4xm1 = pfm3State_.freqAi;
            float freq4 = pfm3State_.freqAo;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                float freq2 = osc2Values[k] * env2Value * pfm3State_.oscState2_.frequency;
                float freq3 = osc3Values[k];

                pfm3State_.oscState4_.frequency = freq3 * voiceIm4 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;

                f4x = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.oscState4_.frequency;
                freq4 = f4x - f4xm1 + 0.99525f * freq4;
                f4xm1 = f4x;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + freq3 * voiceIm2 + freq4 * voiceIm3 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float currentSample = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * velocity;

                *sample++ = currentSample * pfm3State_.pan1Right;
                *sample++ = currentSample * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env4Value += env4Inc;

            }

            pfm3State_.freqAi = f4xm1;
            pfm3State_.freqAo = freq4;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }
        case ALGO4:
            /*           IM4
             .---. <----   .---.
             | 3 |         | 4 |
             '---'         '---'
             |IM1 \IM3     |IM2
             .---.          .---.
             | 1 |          | 2 |
             '---'          '---'
             |Mix1          |Mix2

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
                pfm3State_.oscState4_.frequency, pfm3State_.fdbLastValue);

            float div2TimesVelocity = velocity * .5f;

            float f3x;
            float f3xm1 = pfm3State_.freqAi;
            float freq3 = pfm3State_.freqAo;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                float freq4 = osc4Values[k];

                pfm3State_.oscState3_.frequency = freq4 * voiceIm4 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                f3x = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.oscState3_.frequency;
                freq3 = f3x - f3xm1 + 0.99525f * freq3;
                f3xm1 = f3x;

                pfm3State_.oscState2_.frequency = freq4 * voiceIm2 + freq3 * voiceIm3 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                float car2 = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * pfm3State_.mix2 * div2TimesVelocity;

                pfm3State_.oscState1_.frequency = freq3 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div2TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
            }

            pfm3State_.freqAi = f3xm1;
            pfm3State_.freqAo = freq3;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_))) {
                endNoteOrBeginNextOne(voice);
            }
            break;
        }
        case ALGO5:
            /*
             .---.
             | 4 |
             '---'  \
                   |IM3  |
             .---.   |
             | 3 |   | IM4
             '---'   |
             |IM2  |
             .---.  /
             | 2 |
             '---'
             |IM1
             .---.
             | 1 |
             '---'

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
                pfm3State_.oscState4_.frequency, pfm3State_.fdbLastValue);

            float f2x;
            float f2xm1 = pfm3State_.freqAi;
            float freq2 = pfm3State_.freqAo;
            float f3x;
            float f3xm1 = pfm3State_.freqBi;
            float freq3 = pfm3State_.freqBo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq4 = osc4Values[k];

                pfm3State_.oscState3_.frequency = freq4 * voiceIm3 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                f3x = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.oscState3_.frequency;
                freq3 = f3x - f3xm1 + 0.99535f * freq3;
                f3xm1 = f3x;

                pfm3State_.oscState2_.frequency = freq3 * voiceIm2 + freq4 * voiceIm4 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                f2x = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * pfm3State_.oscState2_.frequency;
                freq2 = f2x - f2xm1 + 0.99525f * freq2;
                f2xm1 = f2x;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * velocity * pfm3State_.mix1;

                *sample++ = car1 * pfm3State_.pan1Right;
                *sample++ = car1 * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
            }
            pfm3State_.freqAi = f2xm1;
            pfm3State_.freqAo = freq2;
            pfm3State_.freqBi = f3xm1;
            pfm3State_.freqBo = freq3;
            if (unlikely(env_[0].isDead(&pfm3State_.envState1_))) {
                endNoteOrBeginNextOne(voice);
            }
            break;
        }
        case ALGO6:
            /*
             .---.
             | 4 |
             '---'
             /IM1 |IM2 \IM3
             .---.  .---.  .---.
             | 1 |  | 2 |  | 3 |
             '---'  '---'  '---'
             |Mix1  |Mix2  | Mix3

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
                pfm3State_.oscState4_.frequency, pfm3State_.fdbLastValue);

            float div3TimesVelocity = .33f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq4 = osc4Values[k];

                pfm3State_.oscState3_.frequency = freq4 * voiceIm3 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix3 * div3TimesVelocity;

                pfm3State_.oscState2_.frequency = freq4 * voiceIm2 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                float car2 = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * pfm3State_.mix2 * div3TimesVelocity;

                pfm3State_.oscState1_.frequency = freq4 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div3TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
            }

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) && env_[2].isDead(&pfm3State_.envState3_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }

        case ALGO7:
            /*
             IM4
             ---->
             .---.  .---.  .---.
             | 2 |  | 4 |  | 6 |
             '---'  '---'  '---'
             |IM1   |IM2   |IM3
             .---.  .---.  .---.
             | 1 |  | 3 |  | 5 |
             '---'  '---'  '---'
             |Mix1  |Mix2  |Mix3

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
                pfm3State_.oscState4_.frequency, pfm3State_.fdbLastValue);

            float div3TimesVelocity = .33f * velocity;

            // F2 will be for OP6
            float f6x;
            float f6xm1 = pfm3State_.freqAi;
            float freq6 = pfm3State_.freqAo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq4 = osc4Values[k];
                float freq2 = osc2Values[k] * env2Value * pfm3State_.oscState2_.frequency;

                pfm3State_.oscState6_.frequency = freq4 * voiceIm4 + pfm3State_.oscState6_.mainFrequencyPlusMatrix;
                f6x = osc_[5].getNextSample(&pfm3State_.oscState6_, waveTable6) * env6Value * pfm3State_.oscState6_.frequency;
                freq6 = f6x - f6xm1 + 0.99525f * freq6;
                f6xm1 = f6x;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div3TimesVelocity;

                pfm3State_.oscState3_.frequency = freq4 * voiceIm2 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix2 * div3TimesVelocity;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm3 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                float car5 = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.mix3 * div3TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car3 * pfm3State_.pan2Right + car5 * pfm3State_.pan3Right;
                *sample++ = car1 * pfm3State_.pan1Left + car3 * pfm3State_.pan2Left + car5 * pfm3State_.pan3Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env5Value += env5Inc;
                env6Value += env6Inc;
            }

            pfm3State_.freqAi = f6xm1;
            pfm3State_.freqAo = freq6;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_) && env_[4].isDead(&pfm3State_.envState5_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;
        case ALGO8:
            /*
             .---.  .---.  .---.       .---.
             | 2 |  | 3 |  | 4 |       | 6 |
             '---'  '---'  '---'       '---'
             \IM1  |IM2  /IM3         | IM4
             .---.              .---.
             | 1 |              | 5 |
             '---'              '---'
             |Mix1              | Mix2

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
                pfm3State_.oscState4_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

            float div2TimesVelocity = .5f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq6 = osc6Values[k] * env6Value;
                freq6 *= pfm3State_.oscState6_.frequency;

                float freq4 = osc4Values[k];

                float freq3 = osc3Values[k] * env3Value;
                freq3 *= pfm3State_.oscState3_.frequency;

                float freq2 = osc2Values[k] * env2Value;
                freq2 *= pfm3State_.oscState2_.frequency;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + freq3 * voiceIm2 + freq4 * voiceIm3 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div2TimesVelocity;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm4 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                float car5 = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.mix2 * div2TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car5 * pfm3State_.pan2Right;
                *sample++ = car1 * pfm3State_.pan1Left + car5 * pfm3State_.pan2Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env5Value += env5Inc;
                env6Value += env6Inc;

            }

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[4].isDead(&pfm3State_.envState5_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }

        case ALGO9:
            /*
             .---.
             | 6 |
             '---'
             |IM4
             .---.      .---.       .---.
             | 2 |      | 3 |       | 5 |
             '---'      '---'       '---'
             \IM1    /IM2         | IM3
             .---.            .---.
             | 1 |            | 4 |
             '---'            '---'
             |Mix1            | Mix2

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
                pfm3State_.oscState6_.frequency, pfm3State_.fdbLastValue);

            float div2TimesVelocity = .5f * velocity;

            // F2 will be for OP5
            float f5x;
            float f5xm1 = pfm3State_.freqAi;
            float freq5 = pfm3State_.freqAo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq6 = osc6Values[k];

                float freq3 = osc3Values[k] * env3Value;
                freq3 *= pfm3State_.oscState3_.frequency;

                float freq2 = osc2Values[k] * env2Value;
                freq2 *= pfm3State_.oscState2_.frequency;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + freq3 * voiceIm2 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div2TimesVelocity;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm4 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                f5x = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.oscState5_.frequency;
                freq5 = f5x - f5xm1 + 0.99525f * freq5;
                f5xm1 = f5x;

                pfm3State_.oscState4_.frequency = freq5 * voiceIm3 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                float car4 = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.mix2 * div2TimesVelocity;

                *sample++ = car4 * pfm3State_.pan2Right + car1 * pfm3State_.pan1Right;
                *sample++ = car4 * pfm3State_.pan2Left + car1 * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            pfm3State_.freqAi = f5xm1;
            pfm3State_.freqAo = freq5;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[3].isDead(&pfm3State_.envState4_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }

            // =========== BELOW THIS LINE DX7 ALGORITHMS !!! =================

        case ALG10:
            /* DX7 Algo 1 & 2
             .---.
             | 6 |
             '---'
             |IM4
             .---.
             | 5 |
             '---'
             |IM3
             .---.            .---.
             | 2 |            | 4 |
             '---'            '---'
             |IM1             | IM2
             .---.            .---.
             | 1 |            | 3 |
             '---'            '---'
             |Mix1            | Mix2

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState2_, pfm3State_.feedbackModulation, env2Value, env2Inc,
                pfm3State_.oscState2_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

            float div2TimesVelocity = .5f * velocity;

            float f4x;
            float f4xm1 = pfm3State_.freqAi;
            float freq4 = pfm3State_.freqAo;

            float f5x;
            float f5xm1 = pfm3State_.freqBi;
            float freq5 = pfm3State_.freqBo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq2 = osc2Values[k];

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div2TimesVelocity;

                float freq6 = osc6Values[k] * env6Value;
                freq6 *= pfm3State_.oscState6_.frequency;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm4 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                f5x = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.oscState5_.frequency;
                freq5 = f5x - f5xm1 + 0.99525f * freq5;
                f5xm1 = f5x;

                pfm3State_.oscState4_.frequency = freq5 * voiceIm3 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                f4x = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.oscState4_.frequency;
                freq4 = f4x - f4xm1 + 0.99525f * freq4;
                f4xm1 = f4x;

                pfm3State_.oscState3_.frequency = freq4 * voiceIm2 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix2 * div2TimesVelocity;

                *sample++ = car3 * pfm3State_.pan2Right + car1 * pfm3State_.pan1Right;
                *sample++ = car3 * pfm3State_.pan2Left + car1 * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
                env6Value += env6Inc;
            }

            pfm3State_.freqBi = f5xm1;
            pfm3State_.freqBo = freq5;

            pfm3State_.freqAi = f4xm1;
            pfm3State_.freqAo = freq4;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;

        }
        case ALG11:
            /*
             * DX7 algo 3 & 4
             .---.            .---.
             | 3 |            | 6 |
             '---'            '---'
             |IM2             |IM4
             .---.            .---.
             | 2 |            | 5 |
             '---'            '---'
             |IM1            | IM3
             .---.            .---.
             | 1 |            | 4 |
             '---'            '---'
             |Mix1            | Mix2

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
                pfm3State_.oscState6_.frequency, pfm3State_.fdbLastValue);

            float div2TimesVelocity = .5f * velocity;

            float f2x;
            float f2xm1 = pfm3State_.freqAi;
            float freq2 = pfm3State_.freqAo;

            float f5x;
            float f5xm1 = pfm3State_.freqBi;
            float freq5 = pfm3State_.freqBo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq3 = osc3Values[k] * env3Value;
                freq3 *= pfm3State_.oscState3_.frequency;

                pfm3State_.oscState2_.frequency = freq3 * voiceIm2 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                f2x = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * pfm3State_.oscState2_.frequency;
                freq2 = f2x - f2xm1 + 0.99525f * freq2;
                f2xm1 = f2x;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div2TimesVelocity;

                float freq6 = osc6Values[k];

                pfm3State_.oscState5_.frequency = freq6 * voiceIm4 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                f5x = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.oscState5_.frequency;
                freq5 = f5x - f5xm1 + 0.99525f * freq5;
                f5xm1 = f5x;

                pfm3State_.oscState4_.frequency = freq5 * voiceIm3 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                float car4 = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.mix2 * div2TimesVelocity;

                *sample++ = car4 * pfm3State_.pan2Right + car1 * pfm3State_.pan1Right;
                *sample++ = car4 * pfm3State_.pan2Left + car1 * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }

            pfm3State_.freqAi = f2xm1;
            pfm3State_.freqAo = freq2;

            pfm3State_.freqBi = f5xm1;
            pfm3State_.freqBo = freq5;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[3].isDead(&pfm3State_.envState4_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }
        case ALG12:
            /*
             * DX7 algo 5 & 6

             .---.  .---.  .---.
             | 2 |  | 4 |  | 6 |
             '---'  '---'  '---'
             |IM1   |IM2   |IM3
             .---.  .---.  .---.
             | 1 |  | 3 |  | 5 |
             '---'  '---'  '---'
             |Mix1  |Mix2  |Mix3

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
                pfm3State_.oscState6_.frequency, pfm3State_.fdbLastValue);

            float div3TimesVelocity = .33f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq4 = osc4Values[k] * env4Value;
                freq4 *= pfm3State_.oscState4_.frequency;

                float freq2 = osc2Values[k] * env2Value;
                freq2 *= pfm3State_.oscState2_.frequency;

                float freq6 = osc6Values[k];

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div3TimesVelocity;

                pfm3State_.oscState3_.frequency = freq4 * voiceIm2 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix2 * div3TimesVelocity;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm3 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                float car5 = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.mix3 * div3TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car3 * pfm3State_.pan2Right + car5 * pfm3State_.pan3Right;
                *sample++ = car1 * pfm3State_.pan1Left + car3 * pfm3State_.pan2Left + car5 * pfm3State_.pan3Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_) && env_[4].isDead(&pfm3State_.envState5_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;
        case ALG13:
            /* DX7 Algo 7, 8, 9

             .---.
             | 6 |
             '---'
             |IM4
             .---.     .---.  .---.
             | 2 |     | 4*|  | 5 |
             '---'     '---'  '---'
             |IM1      |IM2 /IM3
             .---.     .---.
             | 1 |     | 3 |
             '---'     '---'
             |Mix1     |Mix2
             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
                pfm3State_.oscState4_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

            float div2TimesVelocity = velocity * .5f;

            float f5x;
            float f5xm1 = pfm3State_.freqAi;
            float freq5 = pfm3State_.freqAo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq6 = osc6Values[k] * env6Value;
                freq6 *= pfm3State_.oscState6_.frequency;

                float freq4 = osc4Values[k];

                float freq2 = osc2Values[k] * env2Value;
                freq2 *= pfm3State_.oscState2_.frequency;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div2TimesVelocity;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm4 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                f5x = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.oscState5_.frequency;
                freq5 = f5x - f5xm1 + 0.99525f * freq5;
                f5xm1 = f5x;

                pfm3State_.oscState3_.frequency = freq4 * voiceIm2 + freq5 * voiceIm3 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix2 * div2TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car3 * pfm3State_.pan2Right;
                *sample++ = car1 * pfm3State_.pan1Left + car3 * pfm3State_.pan2Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env5Value += env5Inc;
                env6Value += env6Inc;
            }

            pfm3State_.freqAi = f5xm1;
            pfm3State_.freqAo = freq5;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }
        case ALG14:
            /* DX Algo 10 & 11
             .---.
             | 3 |
             '---'
             |IM2
             .---.            .---.   .---.
             | 2 |            | 5 |   | 6*|
             '---'            '---'   '---'
             |IM1            | IM3 / IM4
             .---.            .---.
             | 1 |            | 4 |
             '---'            '---'
             |Mix1            | Mix2

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float *osc5Values = osc_[4].getNextBlock(&pfm3State_.oscState5_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
                pfm3State_.oscState6_.frequency, pfm3State_.fdbLastValue);

            float div2TimesVelocity = .5f * velocity;

            float f2x;
            float f2xm1 = pfm3State_.freqAi;
            float freq2 = pfm3State_.freqAo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq3 = osc3Values[k] * env3Value;
                freq3 *= pfm3State_.oscState3_.frequency;

                float freq5 = osc5Values[k] * env5Value;
                freq5 *= pfm3State_.oscState5_.frequency;

                float freq6 = osc6Values[k];

                pfm3State_.oscState2_.frequency = freq3 * voiceIm2 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                f2x = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * pfm3State_.oscState2_.frequency;
                freq2 = f2x - f2xm1 + 0.99525f * freq2;
                f2xm1 = f2x;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div2TimesVelocity;

                pfm3State_.oscState4_.frequency = freq5 * voiceIm3 + freq6 * voiceIm4 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                float car4 = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.mix2 * div2TimesVelocity;

                *sample++ = car4 * pfm3State_.pan2Right + car1 * pfm3State_.pan1Right;
                *sample++ = car4 * pfm3State_.pan2Left + car1 * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }

            pfm3State_.freqAi = f2xm1;
            pfm3State_.freqAo = freq2;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[3].isDead(&pfm3State_.envState4_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }
        case ALG15:
            /*
             * DX Algo 12 & 13

             .---.     .---.   .---.   .---.
             | 2 |     | 4 |   | 5 |   | 6*|
             '---'     '---'   '---'   '---'
             |IM1        \IM2  | IM3 / IM4
             .---.             .---.
             | 1 |             | 3 |
             '---'             '---'
             |Mix1             | Mix2

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float *osc5Values = osc_[4].getNextBlock(&pfm3State_.oscState5_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
                pfm3State_.oscState6_.frequency, pfm3State_.fdbLastValue);

            float div2TimesVelocity = .5f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq2 = osc2Values[k] * env2Value;
                freq2 *= pfm3State_.oscState2_.frequency;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div2TimesVelocity;

                float freq4 = osc4Values[k] * env4Value;
                freq4 *= pfm3State_.oscState4_.frequency;
                float freq5 = osc5Values[k] * env5Value;
                freq5 *= pfm3State_.oscState5_.frequency;
                float freq6 = osc6Values[k];

                pfm3State_.oscState3_.frequency = freq4 * voiceIm2 + freq5 * voiceIm3 + freq6 * voiceIm4 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix2 * div2TimesVelocity;

                *sample++ = car3 * pfm3State_.pan2Right + car1 * pfm3State_.pan1Right;
                *sample++ = car3 * pfm3State_.pan2Left + car1 * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }
        case ALG16:
            /* DX7 Algo 14 and 15
             .---.  .---.
             | 5 |  | 6 |
             '---'  '---'
             |IM3/IM4
             .---.            .---.
             | 2*|            | 4 |
             '---'            '---'
             |IM1             | IM2
             .---.            .---.
             | 1 |            | 3 |
             '---'            '---'
             |Mix1            | Mix2

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState2_, pfm3State_.feedbackModulation, env2Value, env2Inc,
                pfm3State_.oscState2_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float *osc5Values = osc_[4].getNextBlock(&pfm3State_.oscState5_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

            float div2TimesVelocity = .5f * velocity;

            float f4x;
            float f4xm1 = pfm3State_.freqAi;
            float freq4 = pfm3State_.freqAo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq2 = osc2Values[k];

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div2TimesVelocity;

                float freq6 = osc6Values[k] * env6Value;
                freq6 *= pfm3State_.oscState6_.frequency;

                float freq5 = osc5Values[k] * env5Value;
                freq5 *= pfm3State_.oscState5_.frequency;

                pfm3State_.oscState4_.frequency = freq5 * voiceIm3 + freq6 * voiceIm4 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                f4x = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.oscState4_.frequency;
                freq4 = f4x - f4xm1 + 0.99525f * freq4;
                f4xm1 = f4x;

                pfm3State_.oscState3_.frequency = freq4 * voiceIm2 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix2 * div2TimesVelocity;

                *sample++ = car3 * pfm3State_.pan2Right + car1 * pfm3State_.pan1Right;
                *sample++ = car3 * pfm3State_.pan2Left + car1 * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
                env6Value += env6Inc;
            }

            pfm3State_.freqAi = f4xm1;
            pfm3State_.freqAo = freq4;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }
        case ALG17:
            /*
             * DX ALGO 16 & 17
             .---.     .---.
             | 4 |     | 6 |
             '---'     '---'
             |IM3      |IM5
             .---.     .---.     .---.
             | 2*|     | 3 |     | 5 |
             '---'     '---'     '---'
             \IM1    |IM2   / IM4
             .---.
             | 1 |
             '---'
             |Mix1

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;
            float voiceIm5 = pfm3State_.modulationIndex5;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState2_, pfm3State_.feedbackModulation, env2Value, env2Inc,
                pfm3State_.oscState2_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

            float f3x;
            float f3xm1 = pfm3State_.freqAi;
            float freq3 = pfm3State_.freqAo;

            float f5x;
            float f5xm1 = pfm3State_.freqBi;
            float freq5 = pfm3State_.freqBo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq6 = osc6Values[k] * env6Value;
                freq6 *= pfm3State_.oscState6_.frequency;

                float freq4 = osc4Values[k] * env4Value;
                freq4 *= pfm3State_.oscState4_.frequency;

                float freq2 = osc2Values[k];

                pfm3State_.oscState3_.frequency = freq4 * voiceIm3 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                f3x = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.oscState3_.frequency;
                freq3 = f3x - f3xm1 + 0.99525f * freq3;
                f3xm1 = f3x;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm5 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                f5x = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.oscState5_.frequency;
                freq5 = f5x - f5xm1 + 0.99525f * freq5;
                f5xm1 = f5x;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + freq3 * voiceIm2 + freq5 * voiceIm4 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * velocity;

                *sample++ = car1 * pfm3State_.pan1Right;
                *sample++ = car1 * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
                env6Value += env6Inc;
            }

            pfm3State_.freqAi = f3xm1;
            pfm3State_.freqAo = freq3;

            pfm3State_.freqBi = f5xm1;
            pfm3State_.freqBo = freq5;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }
        case ALG18:
            /*
             * DX ALGO 18
             .---.
             | 6 |
             '---'
             |IM5
             .---.
             | 5 |
             '---'
             |IM4
             .---.     .---.     .---.
             | 2 |     | 3*|     | 4 |
             '---'     '---'     '---'
             \IM1    |IM2   / IM3
             .---.
             | 1 |
             '---'
             |Mix1
             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;
            float voiceIm5 = pfm3State_.modulationIndex5;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
                pfm3State_.oscState3_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

            float f4x;
            float f4xm1 = pfm3State_.freqAi;
            float freq4 = pfm3State_.freqAo;

            float f5x;
            float f5xm1 = pfm3State_.freqBi;
            float freq5 = pfm3State_.freqBo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq6 = osc6Values[k] * env6Value;
                freq6 *= pfm3State_.oscState6_.frequency;

                float freq3 = osc3Values[k];

                float freq2 = osc2Values[k] * env2Value;
                freq2 *= pfm3State_.oscState2_.frequency;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm5 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                f5x = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.oscState5_.frequency;
                freq5 = f5x - f5xm1 + 0.99525f * freq5;
                f5xm1 = f5x;

                pfm3State_.oscState4_.frequency = freq5 * voiceIm4 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                f4x = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.oscState4_.frequency;
                freq4 = f4x - f4xm1 + 0.99525f * freq4;
                f4xm1 = f4x;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + freq3 * voiceIm2 + freq4 * voiceIm3 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * velocity;

                *sample++ = car1 * pfm3State_.pan1Right;
                *sample++ = car1 * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
                env6Value += env6Inc;
            }

            pfm3State_.freqAi = f4xm1;
            pfm3State_.freqAo = freq4;

            pfm3State_.freqBi = f5xm1;
            pfm3State_.freqBo = freq5;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }

        case ALG19:
            /*
             * DX7 algo 19

             .---.
             | 3 |
             '---'
             |IM2
             .---.        .---.
             | 2 |        | 6*|
             '---'        '---'
             |IM1    IM3/   \IM4
             .---.  .---.     .---.
             | 1 |  | 4 |     | 5 |
             '---'  '---'     '---'
             |Mix1  |Mix2     |Mix3

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
                pfm3State_.oscState6_.frequency, pfm3State_.fdbLastValue);

            float div3TimesVelocity = .33f * velocity;

            float f2x;
            float f2xm1 = pfm3State_.freqAi;
            float freq2 = pfm3State_.freqAo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq3 = osc3Values[k] * env3Value;
                freq3 *= pfm3State_.oscState3_.frequency;

                float freq6 = osc6Values[k];

                pfm3State_.oscState2_.frequency = freq3 * voiceIm2 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                f2x = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * pfm3State_.oscState2_.frequency;
                freq2 = f2x - f2xm1 + 0.99525f * freq2;
                f2xm1 = f2x;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div3TimesVelocity;

                pfm3State_.oscState4_.frequency = freq6 * voiceIm3 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                float car4 = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.mix2 * div3TimesVelocity;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm4 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                float car5 = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.mix3 * div3TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car4 * pfm3State_.pan2Right + car5 * pfm3State_.pan3Right;
                *sample++ = car1 * pfm3State_.pan1Left  + car4 * pfm3State_.pan2Left  + car5 * pfm3State_.pan3Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }

            pfm3State_.freqAi = f2xm1;
            pfm3State_.freqAo = freq2;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[3].isDead(&pfm3State_.envState4_) && env_[4].isDead(&pfm3State_.envState5_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;
        case ALG20:
            /*
             * DX7 algo 20, 26 & 27

             .---.     .---.  .---.
             | 3*|     | 5 |  | 6 |
             '---'     '---'  '---'
             /IM1 \IM2     \IM3 /IM4
             .---.  .---.    .---.
             | 1 |  | 2 |    | 4 |
             '---'  '---'    '---'
             |Mix1  |Mix2    |Mix3
             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
                pfm3State_.oscState3_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float *osc5Values = osc_[4].getNextBlock(&pfm3State_.oscState5_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

            float div3TimesVelocity = .33f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq3 = osc3Values[k];

                float freq5 = osc5Values[k] * env5Value;
                freq5 *= pfm3State_.oscState5_.frequency;

                float freq6 = osc6Values[k] * env6Value;
                freq6 *= pfm3State_.oscState6_.frequency;

                pfm3State_.oscState1_.frequency = freq3 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div3TimesVelocity;

                pfm3State_.oscState2_.frequency = freq3 * voiceIm2 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                float car2 = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * pfm3State_.mix2 * div3TimesVelocity;

                pfm3State_.oscState4_.frequency = freq5 * voiceIm3 + freq6 * voiceIm4 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                float car4 = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.mix3 * div3TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car4 * pfm3State_.pan3Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car4 * pfm3State_.pan3Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
                env6Value += env6Inc;
            }

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) && env_[3].isDead(&pfm3State_.envState4_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;
        case ALG21:
            /* DX ALGO 21 & 23
             .---.         .---.
             | 3*|         | 6 |
             '---'         '---'
             /IM1  \IM2   /IM3  \IM4
             .---.  .---.  .---.  .---.
             | 1 |  | 2 |  | 4 |  | 5 |
             '---'  '---'  '---'  '---'
             |Mix1  |Mix2  |Mix3  |Mix4

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
                pfm3State_.oscState3_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

            float div4TimesVelocity = .25f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq6 = osc6Values[k] * env6Value;
                freq6 *= pfm3State_.oscState6_.frequency;

                float freq3 = osc3Values[k];

                pfm3State_.oscState1_.frequency = freq3 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div4TimesVelocity;

                pfm3State_.oscState2_.frequency = freq3 * voiceIm2 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                float car2 = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * pfm3State_.mix2 * div4TimesVelocity;

                pfm3State_.oscState4_.frequency = freq6 * voiceIm3 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                float car4 = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.mix3 * div4TimesVelocity;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm4 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                float car5 = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.mix4 * div4TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car4 * pfm3State_.pan3Right + car5 * pfm3State_.pan4Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car4 * pfm3State_.pan3Left + car5 * pfm3State_.pan4Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
                env6Value += env6Inc;
            }
            if (unlikely(
                env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) && env_[3].isDead(&pfm3State_.envState4_)
                    && env_[4].isDead(&pfm3State_.envState5_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;
        case ALG22:
            /* DX7 algo 22
             .---.         .---.
             | 2 |         | 6*|
             '---'         '---'
             |IM1      /IM2 |IM3 \IM4
             .---.  .---.  .---.  .---.
             | 1 |  | 3 |  | 4 |  | 5 |
             '---'  '---'  '---'  '---'
             |Mix1  |Mix2  |Mix3  |Mix4

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
                pfm3State_.oscState6_.frequency, pfm3State_.fdbLastValue);

            float div4TimesVelocity = .25f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq6 = osc6Values[k];

                float freq2 = osc2Values[k] * env2Value;
                freq2 *= pfm3State_.oscState2_.frequency;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div4TimesVelocity;

                pfm3State_.oscState3_.frequency = freq6 * voiceIm2 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix2 * div4TimesVelocity;

                pfm3State_.oscState4_.frequency = freq6 * voiceIm3 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                float car4 = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.mix3 * div4TimesVelocity;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm4 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                float car5 = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.mix4 * div4TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car3 * pfm3State_.pan2Right + car4 * pfm3State_.pan3Right + car5 * pfm3State_.pan4Right;
                *sample++ = car1 * pfm3State_.pan1Left  + car3 * pfm3State_.pan2Left  + car4 * pfm3State_.pan3Left  + car5 * pfm3State_.pan4Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            if (unlikely(
                env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_) && env_[3].isDead(&pfm3State_.envState4_)
                    && env_[4].isDead(&pfm3State_.envState5_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;
        case ALG23:
            /* DX ALGO 24, 25 & 31

             .---.
             | 6 |
             '---'
             /IM1|IM2\IM3
             .---.  .---.  .---.  .---.   .---.
             | 1 |  | 2 |  | 3 |  | 4 |   | 5 |
             '---'  '---'  '---'  '---'   '---'
             |Mix1  |Mix2  |Mix3  |Mix4   |Mix5

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState1_.frequency = pfm3State_.oscState1_.mainFrequencyPlusMatrix;
            float *osc1Values = osc_[0].getNextBlock(&pfm3State_.oscState1_);

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
                pfm3State_.oscState6_.frequency, pfm3State_.fdbLastValue);

            float div5TimesVelocity = .2f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq6 = osc6Values[k];

                float car1 = osc1Values[k] * env1Value * pfm3State_.mix1 * div5TimesVelocity;

                float car2 = osc2Values[k] * env2Value * pfm3State_.mix2 * div5TimesVelocity;

                pfm3State_.oscState3_.frequency = freq6 * voiceIm1 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix3 * div5TimesVelocity;

                pfm3State_.oscState4_.frequency = freq6 * voiceIm2 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                float car4 = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.mix4 * div5TimesVelocity;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm3 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                float car5 = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.mix5 * div5TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right + car4 * pfm3State_.pan4Right + car5 * pfm3State_.pan5Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left + car4 * pfm3State_.pan4Left + car5 * pfm3State_.pan5Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            if (unlikely(
                env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) && env_[3].isDead(&pfm3State_.envState4_)
                    && env_[4].isDead(&pfm3State_.envState5_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;
        case ALG24:
            /*
             * DX ALGO 28

             .---.
             | 5*|
             '---'
             |IM3
             .---.  .---.
             | 2 |  | 4 |
             '---'  '---'
             |IM1   |IM2
             .---.  .---.  .---.
             | 1 |  | 3 |  | 6 |
             '---'  '---'  '---'
             |Mix1  |Mix2  |Mix3

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float *osc5Values = osc_[4].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState5_, pfm3State_.feedbackModulation, env5Value, env5Inc,
                pfm3State_.oscState5_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

            float div3TimesVelocity = .33f * velocity;

            float f4x;
            float f4xm1 = pfm3State_.freqAi;
            float freq4 = pfm3State_.freqAo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq2 = osc2Values[k] * env2Value;
                freq2 *= pfm3State_.oscState2_.frequency;

                float freq5 = osc5Values[k];

                pfm3State_.oscState4_.frequency = freq5 * voiceIm3 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                f4x = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.oscState4_.frequency;
                freq4 = f4x - f4xm1 + 0.99525f * freq4;
                f4xm1 = f4x;

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
                float car1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * env1Value * pfm3State_.mix1 * div3TimesVelocity;

                pfm3State_.oscState3_.frequency = freq4 * voiceIm2 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix2 * div3TimesVelocity;

                float car6 = osc6Values[k] * env6Value * pfm3State_.mix3 * div3TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car3 * pfm3State_.pan2Right + car6 * pfm3State_.pan3Right;
                *sample++ = car1 * pfm3State_.pan1Left + car3 * pfm3State_.pan2Left + car6 * pfm3State_.pan3Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env6Value += env6Inc;
            }

            pfm3State_.freqAi = f4xm1;
            pfm3State_.freqAo = freq4;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_) && env_[5].isDead(&pfm3State_.envState6_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;
        case ALG25:
            /*
             * DX ALGO 29
             *

             .---.  .---.
             | 4 |  | 6*|
             '---'  '---'
             |IM1   |IM2
             .---.  .---.  .---.  .---.
             | 1 |  | 2 |  | 3 |  | 5 |
             '---'  '---'  '---'  '---'
             |Mix1  |Mix2  |Mix3  |Mix4

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState1_.frequency = pfm3State_.oscState1_.mainFrequencyPlusMatrix;
            float *osc1Values = osc_[0].getNextBlock(&pfm3State_.oscState1_);

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
                pfm3State_.oscState6_.frequency, pfm3State_.fdbLastValue);

            float div4TimesVelocity = .25f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq6 = osc6Values[k];

                float freq4 = osc4Values[k] * env4Value;
                freq4 *= pfm3State_.oscState4_.frequency;

                float car1 = osc1Values[k] * env1Value * pfm3State_.mix1 * div4TimesVelocity;

                float car2 = osc2Values[k] * env2Value * pfm3State_.mix2 * div4TimesVelocity;

                pfm3State_.oscState3_.frequency = freq4 * voiceIm1 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix3 * div4TimesVelocity;

                pfm3State_.oscState5_.frequency = freq6 * voiceIm2 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                float car5 = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.mix4 * div4TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right + car5 * pfm3State_.pan4Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left + car5 * pfm3State_.pan4Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            if (unlikely(
                env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) && env_[2].isDead(&pfm3State_.envState3_)
                    && env_[4].isDead(&pfm3State_.envState5_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;
        case ALG26:
            /*
             * DX Algo 30
             .---.
             | 5*|
             '---'
             |IM2
             .---.
             | 4 |
             '---'
             |IM1
             .---.  .---.  .---.  .---.
             | 1 |  | 2 |  | 3 |  | 6 |
             '---'  '---'  '---'  '---'
             |Mix1  |Mix2  |Mix3  |Mix4

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState1_.frequency = pfm3State_.oscState1_.mainFrequencyPlusMatrix;
            float *osc1Values = osc_[0].getNextBlock(&pfm3State_.oscState1_);

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float *osc5Values = osc_[4].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState5_, pfm3State_.feedbackModulation, env5Value, env5Inc,
                pfm3State_.oscState5_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

            float div4TimesVelocity = .25f * velocity;

            float f4x;
            float f4xm1 = pfm3State_.freqAi;
            float freq4 = pfm3State_.freqAo;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq5 = osc5Values[k];

                float car1 = osc1Values[k] * env1Value * pfm3State_.mix1 * div4TimesVelocity;

                float car2 = osc2Values[k] * env2Value * pfm3State_.mix2 * div4TimesVelocity;

                pfm3State_.oscState4_.frequency = freq5 * voiceIm2 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                f4x = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.oscState4_.frequency;
                freq4 = f4x - f4xm1 + 0.99525f * freq4;
                f4xm1 = f4x;

                pfm3State_.oscState3_.frequency = freq4 * voiceIm1 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float car3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * env3Value * pfm3State_.mix3 * div4TimesVelocity;

                float car6 = osc6Values[k] * env6Value * pfm3State_.mix4 * div4TimesVelocity;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right + car6 * pfm3State_.pan4Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left + car6 * pfm3State_.pan4Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env6Value += env6Inc;
            }

            pfm3State_.freqAi = f4xm1;
            pfm3State_.freqAo = freq4;

            if (unlikely(
                env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) && env_[2].isDead(&pfm3State_.envState3_)
                    && env_[5].isDead(&pfm3State_.envState6_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;

        case ALG27:
            /*
             * DX ALGO 32
             *
             .---.  .---.  .---.  .---.   .---.   .---.
             | 1 |  | 2 |  | 3 |  | 4 |   | 5 |   | 6*|
             '---'  '---'  '---'  '---'   '---'   '---'
             |Mix1  |Mix2  |Mix3  |Mix4   |Mix5   |Mix6

             */
        {
            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState1_.frequency = pfm3State_.oscState1_.mainFrequencyPlusMatrix;
            float *osc1Values = osc_[0].getNextBlock(&pfm3State_.oscState1_);

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            // Not enough float arrays for a fifth optimization
            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;

            // Multiplier is 1 because op6 is a carrier
            float *osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc, 1.0f,
                pfm3State_.fdbLastValue);

            float div6TimesVelocity = .16f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float car1 = osc1Values[k] * env1Value * pfm3State_.mix1;
                float car2 = osc2Values[k] * env2Value * pfm3State_.mix2;
                float car3 = osc3Values[k] * env3Value * pfm3State_.mix3;
                float car4 = osc4Values[k] * env4Value * pfm3State_.mix4;
                // getNextBlockWithFeedbackAndEnveloppe already applied envelope
                float car6 = osc6Values[k] * pfm3State_.mix6;

                float car5 = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.mix5;

                *sample++ = (car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right + car4 * pfm3State_.pan4Right + car5 * pfm3State_.pan5Right + car6 * pfm3State_.pan6Right)
                    * div6TimesVelocity;
                *sample++ = (car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left + car4 * pfm3State_.pan4Left + car5 * pfm3State_.pan5Left + car6 * pfm3State_.pan6Left) * div6TimesVelocity;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            if (unlikely(
                env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) && env_[2].isDead(&pfm3State_.envState3_)
                    && env_[3].isDead(&pfm3State_.envState4_) && env_[4].isDead(&pfm3State_.envState5_) && env_[5].isDead(&pfm3State_.envState6_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;
        case ALG28:
            /*
             * DX ALGO 31

             .---.
             | 6*|
             '---'
             |IM1
             .---.  .---.  .---.  .---.   .---.
             | 1 |  | 2 |  | 3 |  | 4 |   | 5 |
             '---'  '---'  '---'  '---'   '---'
             |Mix1  |Mix2  |Mix3  |Mix4   |Mix5

             */
        {
            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[4].calculateFrequencyWithMatrix(&pfm3State_.oscState5_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[5].calculateFrequencyWithMatrix(&pfm3State_.oscState6_, &pfm3State_.matrix, pfm3State_.freqHarm);

            float voiceIm1 = pfm3State_.modulationIndex1;

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            env5Value = pfm3State_.env5ValueMem;
            envNextValue = env_[4].getNextAmpExp(&pfm3State_.envState5_);
            env5Inc = (envNextValue - env5Value) * inv32;
            pfm3State_.env5ValueMem = envNextValue;

            env6Value = pfm3State_.env6ValueMem;
            envNextValue = env_[5].getNextAmpExp(&pfm3State_.envState6_);
            env6Inc = (envNextValue - env6Value) * inv32;
            pfm3State_.env6ValueMem = envNextValue;

            pfm3State_.oscState1_.frequency = pfm3State_.oscState1_.mainFrequencyPlusMatrix;
            float *osc1Values = osc_[0].getNextBlock(&pfm3State_.oscState1_);

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float *osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
                pfm3State_.oscState6_.frequency, pfm3State_.fdbLastValue);

            float div5TimesVelocity = .2f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float car1 = osc1Values[k] * env1Value * pfm3State_.mix1;
                float car2 = osc2Values[k] * env2Value * pfm3State_.mix2;
                float car3 = osc3Values[k] * env3Value * pfm3State_.mix3;
                float car4 = osc4Values[k] * env4Value * pfm3State_.mix4;

                float freq6 = osc6Values[k];

                pfm3State_.oscState5_.frequency = freq6 * voiceIm1 + pfm3State_.oscState5_.mainFrequencyPlusMatrix;
                float car5 = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.mix5;

                *sample++ = (car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right + car4 * pfm3State_.pan4Right + car5 * pfm3State_.pan5Right) * div5TimesVelocity;
                *sample++ = (car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left + car4 * pfm3State_.pan4Left + car5 * pfm3State_.pan5Left) * div5TimesVelocity;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            if (unlikely(
                env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) && env_[2].isDead(&pfm3State_.envState3_)
                    && env_[3].isDead(&pfm3State_.envState4_) && env_[4].isDead(&pfm3State_.envState5_) && env_[5].isDead(&pfm3State_.envState6_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
            break;

        case ALG29:
            /* Windowed sync AM, 1 synced by 3
            inspiration from https://electro-music.com/nm_classic/015_workshops/Clavia/NordModularWorkshops&Threads/WerkMap/WorkShops/Hordijk1999-2000/VOSIM.html
                             https://scsynth.org/t/fake-resonance-aka-windowed-sync/2720

                     IM3
                    <----
             .---.         .---.
             | 3*|         | 4 |
             '---'         '---'
             |IM1           |IM2
             .---.          .---.
             |*1 |          | 2 |
             '---'          '---'
             |Mix1          |Mix2

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
                pfm3State_.oscState4_.frequency, pfm3State_.fdbLastValue);

            float div2TimesVelocity = velocity * .5f;

            float mix1V = pfm3State_.mix1 * div2TimesVelocity;
            float mix2V = pfm3State_.mix2 * div2TimesVelocity;

            float osc1Matrix = pfm3State_.oscState1_.mainFrequencyPlusMatrix - pfm3State_.oscState1_.mainFrequency;
            float osc1FrequencyPlusMatrix = pfm3State_.oscState1_.mainFrequency + osc1Matrix * fabsf(osc1Matrix) * 0.25f;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                float freq4 = osc4Values[k];

                pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix + voiceIm3 * freq4;
                float osc3  = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3);
                float phase = osc_[2].getPhase(&pfm3State_.oscState3_);
                float freq3 = osc3 * env3Value * pfm3State_.oscState3_.frequency;
  
                // sync check
                bool isSync = pfm3State_.prevPhase > phase;
                pfm3State_.prevPhase = phase;
                float window = 1.f - phase;

                // sync slave osc
                if (unlikely(isSync)) {
                    pfm3State_.oscState1_.index = osc_[0].geIndexFromtPhase(phase);
                }

                pfm3State_.oscState1_.frequency = freq3 * voiceIm1 + osc1FrequencyPlusMatrix;
                float carSample1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * window;
                carSample1 *= fabsf(carSample1); // vosim shape
                float car1 = carSample1 * env1Value * mix1V;

                pfm3State_.oscState2_.frequency = freq4 * voiceIm2 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                float car2 = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * mix2V;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right;
                *sample++ = car1 * pfm3State_.pan1Left  + car2 * pfm3State_.pan2Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
            }

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_))) {
                endNoteOrBeginNextOne(voice);
            }
            break;
        }
        case ALG30:
            /* Windowed sync AM, 1 & 2 synced by 3
                             
             .---.         .---.
             | 3*|         | 4 |
             '---'         '---'
             |IM1 \IM2 IM3/ |IM4
             .---.          .---.
             |*1 |          |*2 |
             '---'          '---'
             |Mix1          |Mix2

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float *osc4Values = osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
                pfm3State_.oscState4_.frequency, pfm3State_.fdbLastValue);

            float div2TimesVelocity = velocity * .5f;

            float mix1V = pfm3State_.mix1 * div2TimesVelocity;
            float mix2V = pfm3State_.mix2 * div2TimesVelocity;

            float osc1Matrix = pfm3State_.oscState1_.mainFrequencyPlusMatrix - pfm3State_.oscState1_.mainFrequency;
            float osc1FrequencyPlusMatrix = pfm3State_.oscState1_.mainFrequency + osc1Matrix * fabsf(osc1Matrix) * 0.25f;

            float osc2Matrix = pfm3State_.oscState2_.mainFrequencyPlusMatrix - pfm3State_.oscState2_.mainFrequency;
            float osc2FrequencyPlusMatrix = pfm3State_.oscState2_.mainFrequency + osc2Matrix * fabsf(osc2Matrix) * 0.25f;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                float freq4 = osc4Values[k];

                pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
                float osc3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3);
                float phase = osc_[2].getPhase(&pfm3State_.oscState3_);
                float freq3 = osc3 * env3Value * pfm3State_.oscState3_.frequency;
  
                // sync check
                bool isSync = pfm3State_.prevPhase > phase;
                pfm3State_.prevPhase = phase;
                float window = 1.f - phase;

                // sync slave osc
                if (unlikely(isSync)) {
                    pfm3State_.oscState1_.index = osc_[0].geIndexFromtPhase(phase);
                    pfm3State_.oscState2_.index = osc_[1].geIndexFromtPhase(phase);
                }

                pfm3State_.oscState1_.frequency = freq3 * voiceIm1 + osc1FrequencyPlusMatrix + freq4 * voiceIm3;
                float carSample1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * window;
                carSample1 *= fabsf(carSample1); // vosim shape
                float car1 = carSample1 * env1Value * mix1V;

                pfm3State_.oscState2_.frequency = freq3 * voiceIm2 + osc2FrequencyPlusMatrix + freq4 * voiceIm4;
                float carSample2 = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * window;
                carSample2 *= fabsf(carSample2); // vosim shape
                float car2 = carSample2 * env2Value * mix2V;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right;
                *sample++ = car1 * pfm3State_.pan1Left  + car2 * pfm3State_.pan2Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
            }

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_))) {
                endNoteOrBeginNextOne(voice);
            }
            break;
        }
        case ALG31:
            /* Windowed sync AM, 1 2 3 synced by 4

             .---.
             | 4*|
             '---'
             /IM1 |IM2 \IM3
             .---.  .---.  .---.
             |*1 |  |*2 |  |*3 |
             '---'  '---'  '---'
             |Mix1  |Mix2  | Mix3

             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            float div3TimesVelocity = .33f * velocity;

            float mix1V = pfm3State_.mix1 * div3TimesVelocity;
            float mix2V = pfm3State_.mix2 * div3TimesVelocity;
            float mix3V = pfm3State_.mix3 * div3TimesVelocity;

            float osc1Matrix = pfm3State_.oscState1_.mainFrequencyPlusMatrix - pfm3State_.oscState1_.mainFrequency;
            float osc1FrequencyPlusMatrix = pfm3State_.oscState1_.mainFrequency + osc1Matrix * fabsf(osc1Matrix) * 0.25f;

            float osc2Matrix = pfm3State_.oscState2_.mainFrequencyPlusMatrix - pfm3State_.oscState2_.mainFrequency;
            float osc2FrequencyPlusMatrix = pfm3State_.oscState2_.mainFrequency + osc2Matrix * fabsf(osc2Matrix) * 0.25f;

            float osc3Matrix = pfm3State_.oscState3_.mainFrequencyPlusMatrix - pfm3State_.oscState3_.mainFrequency;
            float osc3FrequencyPlusMatrix = pfm3State_.oscState3_.mainFrequency + osc3Matrix * fabsf(osc3Matrix) * 0.25f;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
                float osc4 = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4);
                float phase = osc_[3].getPhase(&pfm3State_.oscState4_);
                float freq4 = osc4 * env4Value * pfm3State_.oscState4_.frequency;

                bool isSync = pfm3State_.prevPhase > phase;
                pfm3State_.prevPhase = phase;
                float window = 1.f - phase;

                // sync slave osc
                if (unlikely(isSync)) {
                    pfm3State_.oscState1_.index = osc_[0].geIndexFromtPhase(phase);
                    pfm3State_.oscState2_.index = osc_[1].geIndexFromtPhase(phase);
                    pfm3State_.oscState3_.index = osc_[2].geIndexFromtPhase(phase);
                }

                pfm3State_.oscState3_.frequency = freq4 * voiceIm3 + osc3FrequencyPlusMatrix;
                float carSample3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * window;
                carSample3 *= fabsf(carSample3); // vosim shape
                float car3 = carSample3 * env3Value * mix3V;

                pfm3State_.oscState2_.frequency = freq4 * voiceIm2 + osc2FrequencyPlusMatrix;
                float carSample2 = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * window;
                carSample2 *= fabsf(carSample2); // vosim shape
                float car2 = carSample2 * env2Value * mix2V;

                pfm3State_.oscState1_.frequency = freq4 * voiceIm1 + osc1FrequencyPlusMatrix;
                float carSample1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * window;
                carSample1 *= fabsf(carSample1); // vosim shape
                float car1 = carSample1 * env1Value * mix1V;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right;
                *sample++ = car1 * pfm3State_.pan1Left  + car2 * pfm3State_.pan2Left  + car3 * pfm3State_.pan3Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
            }

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) && env_[2].isDead(&pfm3State_.envState3_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }

        case ALG32:
            /* Windowed sync AM, 1 synced by 4

                          IM4
                         ---->
             .---.  .---.     .---.
             | 2 |  | 3 |     | 4*|
             '---'  '---'     '---'
             \IM1  |IM2    /IM3
             .---.
             |*1 |
             '---'
             */
        {
            float voiceIm1 = pfm3State_.modulationIndex1;
            float voiceIm2 = pfm3State_.modulationIndex2;
            float voiceIm3 = pfm3State_.modulationIndex3;
            float voiceIm4 = pfm3State_.modulationIndex4;

            osc_[0].calculateFrequencyWithMatrix(&pfm3State_.oscState1_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[1].calculateFrequencyWithMatrix(&pfm3State_.oscState2_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[2].calculateFrequencyWithMatrix(&pfm3State_.oscState3_, &pfm3State_.matrix, pfm3State_.freqHarm);
            osc_[3].calculateFrequencyWithMatrix(&pfm3State_.oscState4_, &pfm3State_.matrix, pfm3State_.freqHarm);

            env1Value = pfm3State_.env1ValueMem;
            envNextValue = env_[0].getNextAmpExp(&pfm3State_.envState1_);
            env1Inc = (envNextValue - env1Value) * inv32;  // divide by 32
            pfm3State_.env1ValueMem = envNextValue;

            env2Value = pfm3State_.env2ValueMem;
            envNextValue = env_[1].getNextAmpExp(&pfm3State_.envState2_);
            env2Inc = (envNextValue - env2Value) * inv32;
            pfm3State_.env2ValueMem = envNextValue;

            env3Value = pfm3State_.env3ValueMem;
            envNextValue = env_[2].getNextAmpExp(&pfm3State_.envState3_);
            env3Inc = (envNextValue - env3Value) * inv32;
            pfm3State_.env3ValueMem = envNextValue;

            env4Value = pfm3State_.env4ValueMem;
            envNextValue = env_[3].getNextAmpExp(&pfm3State_.envState4_);
            env4Inc = (envNextValue - env4Value) * inv32;
            pfm3State_.env4ValueMem = envNextValue;

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float *osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float *osc3Values = osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
                pfm3State_.oscState3_.frequency, pfm3State_.fdbLastValue);

            float f4x;
            float f4xm1 = pfm3State_.freqAi;
            float freq4 = pfm3State_.freqAo;
            float mix1V = pfm3State_.mix1 * velocity;

            float osc1Matrix = pfm3State_.oscState1_.mainFrequencyPlusMatrix - pfm3State_.oscState1_.mainFrequency;
            float osc1FrequencyPlusMatrix = pfm3State_.oscState1_.mainFrequency + osc1Matrix * fabsf(osc1Matrix) * 0.25f;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                float freq2 = osc2Values[k] * env2Value * pfm3State_.oscState2_.frequency;
                float freq3 = osc3Values[k];

                pfm3State_.oscState4_.frequency = freq3 * voiceIm4 + pfm3State_.oscState4_.mainFrequencyPlusMatrix;

                f4x = osc_[3].getNextSample(&pfm3State_.oscState4_, waveTable4) * env4Value * pfm3State_.oscState4_.frequency;
                float phase = osc_[3].getPhase(&pfm3State_.oscState4_);
                freq4 = f4x - f4xm1 + 0.99525f * freq4;
                f4xm1 = f4x;

                bool isSync = pfm3State_.prevPhase > phase;
                pfm3State_.prevPhase = phase;
                float window = 1.f - phase;

                // sync slave osc
                if (unlikely(isSync)) {
                    pfm3State_.oscState1_.index = osc_[0].geIndexFromtPhase(phase);
                }

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + freq3 * voiceIm2 + freq4 * voiceIm3 + osc1FrequencyPlusMatrix;
                float carSample1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * window;
                carSample1 *= fabsf(carSample1); // vosim shape
                float currentSample = carSample1 * env1Value * mix1V;

                *sample++ = currentSample * pfm3State_.pan1Right;
                *sample++ = currentSample * pfm3State_.pan1Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env4Value += env4Inc;

            }

            pfm3State_.freqAi = f4xm1;
            pfm3State_.freqAo = freq4;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_))) {
                endNoteOrBeginNextOne(voice);
            }

            break;
        }

    } // End switch

    if (unlikely(pfm3State(voice).noteAlreadyFinished > 0)) {
        if (pfm3State(voice).noteAlreadyFinished == 2) {
            pfm3State(voice).noteAlreadyFinished = 0;
            noteOffVoice(voice);
        } else {
            pfm3State(voice).noteAlreadyFinished++;
        }
    }

}

uint8_t Preenfm3Engine::voicesNextBlock() {
    uint8_t numberOfPlayingVoices = 0;
    if (unlikely(pfm3Params().engine1.playMode == PLAY_MODE_UNISON)) {
        timbre_->cleanNextBlock();
        // UNISON
        float pansSav[6];
        pansSav[0] = pfm3Params().engineMix1.panOsc1;
        pansSav[1] = pfm3Params().engineMix1.panOsc2;
        pansSav[2] = pfm3Params().engineMix2.panOsc3;
        pansSav[3] = pfm3Params().engineMix2.panOsc4;
        pansSav[4] = pfm3Params().engineMix3.panOsc5;
        pansSav[5] = pfm3Params().engineMix3.panOsc6;

        float* pans[] = {
            &pfm3Params().engineMix1.panOsc1,
            &pfm3Params().engineMix1.panOsc2,
            &pfm3Params().engineMix2.panOsc3,
            &pfm3Params().engineMix2.panOsc4,
            &pfm3Params().engineMix3.panOsc5,
            &pfm3Params().engineMix3.panOsc6
        };

        int currentAlgo = (int) pfm3Params().engine1.algo;
        float algoNumberOfMix = algoInformation[currentAlgo].mix;
        float numberOfCarrierOp = timbre_->numberOfVoices_ * algoNumberOfMix;
        float opPan = - pfm3Params().engine2.unisonSpread;
        float opPanInc = 2.0f / numberOfCarrierOp * pfm3Params().engine2.unisonSpread;

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
                renderVoice(timbre_->voices_[v]);

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
            Voice *voice0 = timbre_->voices_[timbre_->voiceNumber_[0]];
            Pfm3VoiceState &state0 = pfm3State(voice0);
            voice0->fxAfterBlock(state0.matrix.getDestination(FILTER1_PARAM1), state0.matrix.getDestination(FILTER1_PARAM2), state0.matrix.getDestination(FILTER1_AMP));

        } else {
            timbre_->voices_[timbre_->voiceNumber_[0]]->emptyBuffer();
        }

        pfm3Params().engineMix1.panOsc1 = pansSav[0];
        pfm3Params().engineMix1.panOsc2 = pansSav[1];
        pfm3Params().engineMix2.panOsc3 = pansSav[2];
        pfm3Params().engineMix2.panOsc4 = pansSav[3];
        pfm3Params().engineMix3.panOsc5 = pansSav[4];
        pfm3Params().engineMix3.panOsc6 = pansSav[5];
    } else {
        for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
            int v = timbre_->voiceNumber_[k];
            if (likely(timbre_->voices_[v]->isPlaying())) {
                renderVoice(timbre_->voices_[v]);
                Voice *voice = timbre_->voices_[v];
                Pfm3VoiceState &state = pfm3State(voice);
                voice->fxAfterBlock(state.matrix.getDestination(FILTER1_PARAM1), state.matrix.getDestination(FILTER1_PARAM2), state.matrix.getDestination(FILTER1_AMP));
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
        prepareMatrixForNewBlock(timbre_->voices_[timbre_->voiceNumber_[k]]);
    }
}

void Preenfm3Engine::glide() {
    if (unlikely(pfm3Params().engine1.playMode == PLAY_MODE_UNISON)) {
        for (int vv = 0; vv < (int) timbre_->numberOfVoices_; vv++) {
            int v = timbre_->voiceNumber_[vv];
            if (v != -1 && pfm3State(timbre_->voices_[v]).gliding) {
                glideVoice(timbre_->voices_[v]);
            }
        }
    } else {
        if (timbre_->voiceNumber_[0] != -1 && pfm3State(timbre_->voices_[timbre_->voiceNumber_[0]]).gliding) {
            glideVoice(timbre_->voices_[timbre_->voiceNumber_[0]]);
        }
    }
}

void Preenfm3Engine::gateFx() {
    // Gate algo !!
    float gate = 0.0f;
    if ((int) timbre_->numberOfVoices_ > 0) {
        gate = pfm3State(timbre_->voices_[timbre_->lastPlayedNote_]).matrix.getDestination(MAIN_GATE);
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
        pfm3State(timbre_->voices_[timbre_->voiceNumber_[k]]).matrix.setSource(source, value);
    }
}

void Preenfm3Engine::setMatrixSourceMPE(uint8_t channel, SourceEnum source, float value) {
    int voiceToUse = timbre_->mpeChannelVoice_[channel - 1];
    if (unlikely(voiceToUse == -1)) {
        return;
    }
    pfm3State(timbre_->voices_[voiceToUse]).matrix.setSource(source, value);
}

void Preenfm3Engine::resetMatrixDestination(float oldValue) {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        pfm3State(timbre_->voices_[timbre_->voiceNumber_[k]]).matrix.resetDestination(oldValue);
    }
}

void Preenfm3Engine::setMatrixPolyAfterTouch(uint8_t note, float value) {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        int voiceIndex = timbre_->voiceNumber_[k];
        Voice *voice = timbre_->voices_[voiceIndex];
        if (unlikely(voice->isPlaying() && voice->getNote() == note)) {
            pfm3State(voice).matrix.setSource(MATRIX_SOURCE_POLYPHONIC_AFTERTOUCH, value);
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
    float noteFrequency = timbre_->mixerState_->instrumentState_[timbre_->timbreNumber_].scaleFrequencies[(int) note];

    if (timbre_->voices_[voiceToUse]->isPlaying()) {
        noteOnVoiceWithoutPop(timbre_->voices_[voiceToUse], note, noteFrequency, velocity, timbre_->voiceIndex_++);
    } else {
        noteOnVoice(timbre_->voices_[voiceToUse], note, noteFrequency, velocity, timbre_->voiceIndex_++);
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
        noteOffVoice(timbre_->voices_[voiceToUse]);
    }
}

void Preenfm3Engine::paramChanged(int row, int encoder, float oldValue, float newValue) {
    switch (row) {
        case ROW_ENV1_TIME:
        case ROW_ENV1_LEVEL:
            env_[0].reloadADSR(encoder);
            break;
        case ROW_ENV2_TIME:
        case ROW_ENV2_LEVEL:
            env_[1].reloadADSR(encoder);
            break;
        case ROW_ENV3_TIME:
        case ROW_ENV3_LEVEL:
            env_[2].reloadADSR(encoder);
            break;
        case ROW_ENV4_TIME:
        case ROW_ENV4_LEVEL:
            env_[3].reloadADSR(encoder);
            break;
        case ROW_ENV5_TIME:
        case ROW_ENV5_LEVEL:
            env_[4].reloadADSR(encoder);
            break;
        case ROW_ENV6_TIME:
        case ROW_ENV6_LEVEL:
            env_[5].reloadADSR(encoder);
            break;
        case ROW_ENV1_CURVE:
            env_[0].applyCurves();
            break;
        case ROW_ENV2_CURVE:
            env_[1].applyCurves();
            break;
        case ROW_ENV3_CURVE:
            env_[2].applyCurves();
            break;
        case ROW_ENV4_CURVE:
            env_[3].applyCurves();
            break;
        case ROW_ENV5_CURVE:
            env_[4].applyCurves();
            break;
        case ROW_ENV6_CURVE:
            env_[5].applyCurves();
            break;
        case ROW_MATRIX_FIRST ... ROW_MATRIX_LAST:
            verifyLfoUsed(encoder, oldValue, newValue);
            if (encoder == ENCODER_MATRIX_DEST1 || encoder == ENCODER_MATRIX_DEST2) {
                resetMatrixDestination(oldValue);
            }
            break;
        case ROW_LFOOSC1 ... ROW_LFOOSC3:
        case ROW_LFOENV1 ... ROW_LFOENV2:
        case ROW_LFOSEQ1 ... ROW_LFOSEQ2:
            lfoValueChange(row, encoder, newValue);
            break;
        case ROW_PERFORMANCE1:
            setMatrixSource((SourceEnum) (MATRIX_SOURCE_CC1 + encoder), newValue);
            break;
        case ROW_MIDINOTE1CURVE:
            updateMidiNoteScale(0);
            break;
        case ROW_MIDINOTE2CURVE:
            updateMidiNoteScale(1);
            break;
        case ROW_ENGINE:
            if (encoder == ENCODER_ENGINE_ALGO) {
                refreshContext();
            }
            break;
        case ROW_OSC_MIX1:
        case ROW_OSC_MIX2:
        case ROW_OSC_MIX3:
            if (encoder == 1 || encoder == 3) {
                refreshContext();
            }
            break;
        case ROW_EFFECT1:
            if (encoder == ENCODER_EFFECT_PARAM3) {
                refreshContext();
            }
            break;
        default:
            break;
    }
}

void Preenfm3Engine::setParamValue(int index, float value) {
    if (index < Pfm3PresetLayout::sharedBlockStart) {
        ((float*) &pfm3Params())[index] = value;
    } else if (index >= Pfm3PresetLayout::pfm3BlockBStart && index < Pfm3PresetLayout::effect2Start) {
        ((float*) &pfm3Params())[index - Pfm3PresetLayout::sharedGap] = value;
    }
}

bool Preenfm3Engine::isUnisonMode() const {
    return pfm3Params().engine1.playMode == PLAY_MODE_UNISON;
}

void Preenfm3Engine::setStepSequencerSteps(int which, const StepSequencerSteps &steps) {
    if (which == 0) {
        pfm3Params().lfoSteps1 = steps;
    } else {
        pfm3Params().lfoSteps2 = steps;
    }
}

void Preenfm3Engine::prepareFxMatrix(Voice *voice) {
    if (!voice->isPlaying()) {
        pfm3State(voice).matrix.computeAllDestinations();
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
        lfoValueChangeOnVoice(timbre_->voices_[timbre_->voiceNumber_[k]], currentRow, encoder, newValue);
    }
}

bool Preenfm3Engine::isLfoUsed(int lfo) {
    return lfoUSed_[lfo] > 0;
}

float Preenfm3Engine::getMatrixDestination(int destination) {
    if ((int) timbre_->numberOfVoices_ > 0) {
        return pfm3State(timbre_->voices_[timbre_->lastPlayedNote_]).matrix.getDestination((DestinationEnum) destination);
    }
    return 0.0f;
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

    MatrixRowParams *matrixRows = &pfm3Params().matrixRowState1;

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
        return offsetof(Pfm3Preset, presetName);
    }
    return offsetof(FlashSynthParams, presetName);
}

const char* Preenfm3Engine::getStorageDefaultName() const {
    return "";
}

uint32_t Preenfm3Engine::getStorageBlob(uint8_t *out, uint32_t maxSize, const TimbreSharedParams &shared) {
    timbre_->serializePfm3Preset(pfm3PresetTmp);
    pfm3ParamsToFlash(&pfm3PresetTmp, (FlashSynthParams*) out, true);
    return sizeof(FlashSynthParams);
}

bool Preenfm3Engine::loadStorageBlob(const uint8_t *in, uint32_t size, uint32_t magic) {
    if (magic == PRESET_VERSION2) {
        memcpy(&pfm3PresetTmp, in, sizeof(Pfm3Preset));
    } else {
        pfm3FlashToParams((const FlashSynthParams*) in, &pfm3PresetTmp, true);
    }
    timbre_->applyPfm3Preset(pfm3PresetTmp);
    return true;
}

void Preenfm3Engine::serializePfm3Preset(struct Pfm3Preset &out) const {
    const Pfm3TimbreParams &pfm3 = pfm3Params();

    out.engine1 = pfm3.engine1;
    out.engineIm1 = pfm3.engineIm1;
    out.engineIm2 = pfm3.engineIm2;
    out.engineIm3 = pfm3.engineIm3;
    out.engineMix1 = pfm3.engineMix1;
    out.engineMix2 = pfm3.engineMix2;
    out.engineMix3 = pfm3.engineMix3;
    out.osc1 = pfm3.osc1;
    out.osc2 = pfm3.osc2;
    out.osc3 = pfm3.osc3;
    out.osc4 = pfm3.osc4;
    out.osc5 = pfm3.osc5;
    out.osc6 = pfm3.osc6;
    out.env1Time = pfm3.env1Time;
    out.env2Time = pfm3.env2Time;
    out.env3Time = pfm3.env3Time;
    out.env4Time = pfm3.env4Time;
    out.env5Time = pfm3.env5Time;
    out.env6Time = pfm3.env6Time;
    out.env1Level = pfm3.env1Level;
    out.env2Level = pfm3.env2Level;
    out.env3Level = pfm3.env3Level;
    out.env4Level = pfm3.env4Level;
    out.env5Level = pfm3.env5Level;
    out.env6Level = pfm3.env6Level;
    out.matrixRowState1 = pfm3.matrixRowState1;
    out.matrixRowState2 = pfm3.matrixRowState2;
    out.matrixRowState3 = pfm3.matrixRowState3;
    out.matrixRowState4 = pfm3.matrixRowState4;
    out.matrixRowState5 = pfm3.matrixRowState5;
    out.matrixRowState6 = pfm3.matrixRowState6;
    out.matrixRowState7 = pfm3.matrixRowState7;
    out.matrixRowState8 = pfm3.matrixRowState8;
    out.matrixRowState9 = pfm3.matrixRowState9;
    out.matrixRowState10 = pfm3.matrixRowState10;
    out.matrixRowState11 = pfm3.matrixRowState11;
    out.matrixRowState12 = pfm3.matrixRowState12;
    out.performance1 = pfm3.performance1;
    out.lfoOsc1 = pfm3.lfoOsc1;
    out.lfoOsc2 = pfm3.lfoOsc2;
    out.lfoOsc3 = pfm3.lfoOsc3;
    out.lfoPhases = pfm3.lfoPhases;
    out.lfoEnv1 = pfm3.lfoEnv1;
    out.lfoEnv2 = pfm3.lfoEnv2;
    out.lfoSeq1 = pfm3.lfoSeq1;
    out.lfoSeq2 = pfm3.lfoSeq2;
    out.midiNote1Curve = pfm3.midiNote1Curve;
    out.midiNote2Curve = pfm3.midiNote2Curve;
    out.lfoSteps1 = pfm3.lfoSteps1;
    out.lfoSteps2 = pfm3.lfoSteps2;
    out.engine2 = pfm3.engine2;
    out.env1Curve = pfm3.env1Curve;
    out.env2Curve = pfm3.env2Curve;
    out.env3Curve = pfm3.env3Curve;
    out.env4Curve = pfm3.env4Curve;
    out.env5Curve = pfm3.env5Curve;
    out.env6Curve = pfm3.env6Curve;
}

void Preenfm3Engine::deserializePfm3Preset(const struct Pfm3Preset &in) {
    Pfm3TimbreParams &pfm3 = pfm3Params();

    pfm3.engine1 = in.engine1;
    pfm3.engineIm1 = in.engineIm1;
    pfm3.engineIm2 = in.engineIm2;
    pfm3.engineIm3 = in.engineIm3;
    pfm3.engineMix1 = in.engineMix1;
    pfm3.engineMix2 = in.engineMix2;
    pfm3.engineMix3 = in.engineMix3;
    pfm3.osc1 = in.osc1;
    pfm3.osc2 = in.osc2;
    pfm3.osc3 = in.osc3;
    pfm3.osc4 = in.osc4;
    pfm3.osc5 = in.osc5;
    pfm3.osc6 = in.osc6;
    pfm3.env1Time = in.env1Time;
    pfm3.env2Time = in.env2Time;
    pfm3.env3Time = in.env3Time;
    pfm3.env4Time = in.env4Time;
    pfm3.env5Time = in.env5Time;
    pfm3.env6Time = in.env6Time;
    pfm3.env1Level = in.env1Level;
    pfm3.env2Level = in.env2Level;
    pfm3.env3Level = in.env3Level;
    pfm3.env4Level = in.env4Level;
    pfm3.env5Level = in.env5Level;
    pfm3.env6Level = in.env6Level;
    pfm3.matrixRowState1 = in.matrixRowState1;
    pfm3.matrixRowState2 = in.matrixRowState2;
    pfm3.matrixRowState3 = in.matrixRowState3;
    pfm3.matrixRowState4 = in.matrixRowState4;
    pfm3.matrixRowState5 = in.matrixRowState5;
    pfm3.matrixRowState6 = in.matrixRowState6;
    pfm3.matrixRowState7 = in.matrixRowState7;
    pfm3.matrixRowState8 = in.matrixRowState8;
    pfm3.matrixRowState9 = in.matrixRowState9;
    pfm3.matrixRowState10 = in.matrixRowState10;
    pfm3.matrixRowState11 = in.matrixRowState11;
    pfm3.matrixRowState12 = in.matrixRowState12;
    pfm3.performance1 = in.performance1;
    pfm3.lfoOsc1 = in.lfoOsc1;
    pfm3.lfoOsc2 = in.lfoOsc2;
    pfm3.lfoOsc3 = in.lfoOsc3;
    pfm3.lfoPhases = in.lfoPhases;
    pfm3.lfoEnv1 = in.lfoEnv1;
    pfm3.lfoEnv2 = in.lfoEnv2;
    pfm3.lfoSeq1 = in.lfoSeq1;
    pfm3.lfoSeq2 = in.lfoSeq2;
    pfm3.midiNote1Curve = in.midiNote1Curve;
    pfm3.midiNote2Curve = in.midiNote2Curve;
    pfm3.lfoSteps1 = in.lfoSteps1;
    pfm3.lfoSteps2 = in.lfoSteps2;
    pfm3.engine2 = in.engine2;
    pfm3.env1Curve = in.env1Curve;
    pfm3.env2Curve = in.env2Curve;
    pfm3.env3Curve = in.env3Curve;
    pfm3.env4Curve = in.env4Curve;
    pfm3.env5Curve = in.env5Curve;
    pfm3.env6Curve = in.env6Curve;
}

void Preenfm3Engine::setPfm3ValueFromMidi(int row, int encoder, float newValue) {
    struct ParameterDisplay *param = &(allParameterRows.row[row]->params[encoder]);
    int index = row * NUMBER_OF_ENCODERS_PFM2 + encoder;
    float oldValue = ((float*) timbre_->getPfm3Preset())[index];

    if (newValue > param->maxValue) {
        // in v2, matrix target were removed so some values are > to max value but we need to accept it
        bool mustConstraint = true;
        if (param->valueNameOrder != NULL) {
            for (int v = 0; v < param->numberOfValues; v++) {
                if ((int) param->valueNameOrder[v] == (int) (newValue + .01)) {
                    mustConstraint = false;
                }
            }
        }
        if (mustConstraint) {
            newValue = param->maxValue;
        }
    } else if (newValue < param->minValue) {
        newValue = param->minValue;
    }
    ((float*) timbre_->getPfm3Preset())[index] = newValue;

    if (oldValue != newValue) {
        synthState_->propagateNewPfm3ParamValueFromExternal(timbreNumber_, row, encoder, param, oldValue, newValue);
    }
}

bool Preenfm3Engine::midiControlChange(int timbre, uint8_t cc, uint8_t value) {
    (void) timbre;
    switch (cc) {
    case CC_ALGO:
        setPfm3ValueFromMidi(ROW_ENGINE, ENCODER_ENGINE_ALGO, (float) value);
        break;
    case CC_IM1:
    case CC_IM2:
        setPfm3ValueFromMidi(ROW_MODULATION1, ENCODER_ENGINE_IM1 + (cc - CC_IM1) * 2, (float) value * .1f);
        break;
    case CC_IM3:
    case CC_IM4:
        setPfm3ValueFromMidi(ROW_MODULATION2, ENCODER_ENGINE_IM3 + (cc - CC_IM3) * 2, (float) value * .1f);
        break;
    case CC_IM5:
        setPfm3ValueFromMidi(ROW_MODULATION3, ENCODER_ENGINE_IM5, (float) value * .1f);
        break;
    case CC_IM_FEEDBACK:
        setPfm3ValueFromMidi(ROW_MODULATION3, ENCODER_ENGINE_IM6, (float) value * INV127);
        break;
    case CC_MIX1:
    case CC_MIX2:
        setPfm3ValueFromMidi(ROW_OSC_MIX1, ENCODER_ENGINE_MIX1 + (cc - CC_MIX1), (float) value * .00787401574803149606f);
        break;
    case CC_MIX3:
    case CC_MIX4:
        setPfm3ValueFromMidi(ROW_OSC_MIX2, ENCODER_ENGINE_MIX3 + (cc - CC_MIX3), (float) value * .00787401574803149606f);
        break;
    case CC_PAN1:
    case CC_PAN2:
        setPfm3ValueFromMidi(ROW_OSC_MIX1, ENCODER_ENGINE_PAN1 + (cc - CC_PAN1),
                (float) value * .00787401574803149606f * 2.0f - 1.0f);
        break;
    case CC_PAN3:
    case CC_PAN4:
        setPfm3ValueFromMidi(ROW_OSC_MIX2, ENCODER_ENGINE_PAN3 + (cc - CC_PAN3),
                (float) value * .00787401574803149606f * 2.0f - 1.0f);
        break;
    case CC_OSC1_FREQ:
    case CC_OSC2_FREQ:
    case CC_OSC3_FREQ:
    case CC_OSC4_FREQ:
    case CC_OSC5_FREQ:
    case CC_OSC6_FREQ:
        setPfm3ValueFromMidi(ROW_OSC1 + cc - CC_OSC1_FREQ, ENCODER_OSC_FREQ, (float) value * .0833333333333333f);
        break;
    case CC_MATRIXROW1_MUL:
    case CC_MATRIXROW2_MUL:
    case CC_MATRIXROW3_MUL:
    case CC_MATRIXROW4_MUL:
        setPfm3ValueFromMidi(ROW_MATRIX1 + cc - CC_MATRIXROW1_MUL, ENCODER_MATRIX_MUL, (float) value * .2f - 10.0f);
        break;
    case CC_LFO1_FREQ:
    case CC_LFO2_FREQ:
    case CC_LFO3_FREQ:
        setPfm3ValueFromMidi(ROW_LFOOSC1 + cc - CC_LFO1_FREQ, ENCODER_LFO_FREQ, (float) value * .2f);
        break;
    case CC_LFO_ENV2_SILENCE:
        setPfm3ValueFromMidi(ROW_LFOENV2, ENCODER_LFO_ENV2_SILENCE, (float) value * .125f);
        break;
    case CC_STEPSEQ5_GATE:
    case CC_STEPSEQ6_GATE:
        setPfm3ValueFromMidi(ROW_LFOSEQ1 + cc - CC_STEPSEQ5_GATE, ENCODER_STEPSEQ_GATE, (float) value * .01f);
        break;
    case CC_MATRIX_SOURCE_CC1:
    case CC_MATRIX_SOURCE_CC2:
    case CC_MATRIX_SOURCE_CC3:
    case CC_MATRIX_SOURCE_CC4:
        setPfm3ValueFromMidi(ROW_PERFORMANCE1, cc - CC_MATRIX_SOURCE_CC1, (float) (value - 64) * INV127 * 2.0);
        break;
    case CC_ENV_ATK_OP1:
        // ATK_OP1 is not consecutive to the other ones
        setPfm3ValueFromMidi(ROW_ENV1_TIME, ENCODER_ENV_A, (float) value * .01562500000000000000f);
        break;
    case CC_ENV_ATK_OP2:
    case CC_ENV_ATK_OP3:
    case CC_ENV_ATK_OP4:
    case CC_ENV_ATK_OP5:
    case CC_ENV_ATK_OP6:
        setPfm3ValueFromMidi(ROW_ENV2_TIME + (cc - CC_ENV_ATK_OP2), ENCODER_ENV_A,
                (float) value * .01562500000000000000f);
        break;
    case CC_ENV_ATK_ALL_CARRIER:
    case CC_ENV_ATK_ALL_MODULATOR: {
        int currentAlgo = (int) timbre_->getPfm3Preset()->engine1.algo;
        int type = cc == CC_ENV_ATK_ALL_CARRIER ? OPERATOR_CARRIER : OPERATOR_MODULATOR;
        for (int e = 0; e < NUMBER_OF_OPERATORS; e++) {
            if (algoOpInformation[currentAlgo][e] == type) {
                setPfm3ValueFromMidi(ROW_ENV1_TIME + e, ENCODER_ENV_A, (float) value * .01562500000000000000f);
            }
        }
        break;
    }
    case CC_ENV_REL_OP1:
    case CC_ENV_REL_OP2:
    case CC_ENV_REL_OP3:
    case CC_ENV_REL_OP4:
    case CC_ENV_REL_OP5:
    case CC_ENV_REL_OP6:
        setPfm3ValueFromMidi(ROW_ENV1_TIME + (cc - CC_ENV_REL_OP1), ENCODER_ENV_R,
                (float) value * .03125000000000000000f);
        break;
    case CC_ENV_REL_ALL_MODULATOR:
    case CC_ENV_REL_ALL_CARRIER: {
        int currentAlgo = (int) timbre_->getPfm3Preset()->engine1.algo;
        int type = cc == CC_ENV_REL_ALL_CARRIER ? OPERATOR_CARRIER : OPERATOR_MODULATOR;
        for (int e = 0; e < NUMBER_OF_OPERATORS; e++) {
            if (algoOpInformation[currentAlgo][e] == type) {
                setPfm3ValueFromMidi(ROW_ENV1_TIME + e, ENCODER_ENV_R, (float) value * .03125000000000000000f);
            }
        }
        break;
    }
    case CC_LFO1_PHASE:
    case CC_LFO2_PHASE:
    case CC_LFO3_PHASE:
        setPfm3ValueFromMidi(ROW_LFOPHASES, ENCODER_LFO_PHASE1 + cc - CC_LFO1_PHASE, (float) value * .01f);
        break;
    case CC_LFO1_BIAS:
    case CC_LFO2_BIAS:
    case CC_LFO3_BIAS:
        setPfm3ValueFromMidi(ROW_LFOOSC1 + cc - CC_LFO1_BIAS, ENCODER_LFO_BIAS, (float) value * .01f);
        break;
    case CC_LFO1_SHAPE:
    case CC_LFO2_SHAPE:
    case CC_LFO3_SHAPE:
        setPfm3ValueFromMidi(ROW_LFOOSC1 + cc - CC_LFO1_SHAPE, ENCODER_LFO_SHAPE, (float) value);
        break;
    case CC_UNISON_DETUNE:
        setPfm3ValueFromMidi(ROW_ENGINE2, ENCODER_ENGINE2_UNISON_DETUNE, (float) value * .015625f - 1.0f);
        break;
    case CC_UNISON_SPREAD:
        setPfm3ValueFromMidi(ROW_ENGINE2, ENCODER_ENGINE2_UNISON_SPREAD, (float) value * INV127);
        break;
    default:
        return false;
    }
    return true;
}

void Preenfm3Engine::setStepValueFromMidi(int whichStepSeq, int step, int newValue) {
    if (!timbre_->supportsStepSequencer()) {
        return;
    }
    if (whichStepSeq < 0 || whichStepSeq > 1 || step < 0 || step > 15 || newValue < 0 || newValue > 15) {
        return;
    }

    int oldValue = timbre_->getSeqStepValue(whichStepSeq, step);
    if (oldValue != newValue) {
        int oldStep = synthState_->stepSelect_[whichStepSeq];
        synthState_->stepSelect_[whichStepSeq] = step;
        if (oldStep != step) {
            synthState_->propagateNewPfm3ParamValueFromExternal(timbreNumber_, ROW_LFOSEQ1 + whichStepSeq, 2, NULL, oldStep,
                step);
        }
        timbre_->setSeqStepValue(whichStepSeq, step, newValue);
        int newNewValue = timbre_->getSeqStepValue(whichStepSeq, step);
        if (oldValue != newNewValue) {
            synthState_->propagateNewPfm3ParamValueFromExternal(timbreNumber_, ROW_LFOSEQ1 + whichStepSeq, 3, NULL, oldValue,
                newValue);
        }
    }
}

bool Preenfm3Engine::midiNrpn(int timbre, uint16_t param, uint16_t value) {
    (void) timbre;
    uint8_t paramMSB = param >> 7;
    uint8_t paramLSB = param & 0x7f;
    uint8_t valueMSB = value >> 7;
    uint8_t valueLSB = value & 0x7f;

    if (paramMSB < 2) {
        float floatValue = (float) ((valueMSB << 7) + valueLSB);
        int index = param;
        int memoryIndex = Pfm3Midi::getMemoryIndexFromMidi(index);
        int row = memoryIndex >> 2;
        int encoder = memoryIndex % 4;

        if (row < NUMBER_OF_ROWS_FOR_EDITOR) {
            struct ParameterDisplay *paramDisplay = &(allParameterRows.row[row]->params[encoder]);
            if (paramDisplay->displayType == DISPLAY_TYPE_FLOAT || paramDisplay->displayType == DISPLAY_TYPE_FLOAT_OSC_FREQUENCY
                    || paramDisplay->displayType == DISPLAY_TYPE_FLOAT_LFO_FREQUENCY || paramDisplay->displayType == DISPLAY_TYPE_LFO_KSYN) {
                floatValue = floatValue * .01f + paramDisplay->minValue;
            }
            setPfm3ValueFromMidi(row, encoder, floatValue);
        } else if (index >= 228 && index < 240) {
            timbre_->setPresetNameChar(index - 228, (char) floatValue);
            if (index == 239) {
                synthState_->propagateNewPresetName(timbreNumber_);
            }
        }
    } else if (paramMSB < 4) {
        setStepValueFromMidi(paramMSB - 2, paramLSB, valueLSB);
    } else {
        return false;
    }
    return true;
}

bool Preenfm3Engine::midiPfm3ParamFeedback(int row, int encoder, float newValue, struct MidiParamMessage &out) const {
    int sendCCOrNRPN = synthState_->fullState_.midiConfigValue[MIDICONFIG_SENDS];

    if (sendCCOrNRPN == 2) {
        out.isNrpn = true;

        // Special case for the step sequencer.
        if ((row == ROW_LFOSEQ1 || row == ROW_LFOSEQ2) && encoder >= 2) {
            if (encoder == 2) {
                return false;
            }
            int currentStepSeq = row - ROW_LFOSEQ1;
            out.nrpn = ((currentStepSeq + 2) << 7) | synthState_->stepSelect_[currentStepSeq];
            out.nrpnValue = (unsigned char) newValue;
            return true;
        }

        const struct ParameterDisplay *param = &allParameterRows.row[row]->params[encoder];
        int valueToSend;
        if (param->displayType == DISPLAY_TYPE_FLOAT || param->displayType == DISPLAY_TYPE_FLOAT_OSC_FREQUENCY
                || param->displayType == DISPLAY_TYPE_FLOAT_LFO_FREQUENCY || param->displayType == DISPLAY_TYPE_LFO_KSYN) {
            valueToSend = (newValue - param->minValue) * 100.0f + .1f;
        } else {
            valueToSend = newValue + .1f;
        }
        out.nrpn = Pfm3Midi::getMidiIndexFromMemory(row * NUMBER_OF_ENCODERS_PFM2 + encoder);
        out.nrpnValue = valueToSend;
        return true;
    }

    if (sendCCOrNRPN != 1) {
        return false;
    }

    out.isNrpn = false;
    out.cc = 0;
    out.ccValue = 0;

    switch (row) {
    case ROW_ENGINE:
        if (encoder == ENCODER_ENGINE_ALGO) {
            out.ccValue = newValue;
            out.cc = CC_ALGO;
        }
        break;
    case ROW_MODULATION1:
        if ((encoder & 0x1) == 0) {
            out.ccValue = newValue * 10 + .001f;
            out.cc = CC_IM1 + (encoder >> 1);
        }
        break;
    case ROW_MODULATION2:
        if ((encoder & 0x1) == 0) {
            out.ccValue = newValue * 10 + .001f;
            out.cc = CC_IM3 + encoder;
        }
        break;
    case ROW_MODULATION3:
        if ((encoder & 0x1) == 0) {
            out.ccValue = newValue * 10 + .001f;
            out.cc = CC_IM5 + encoder;
        }
        break;
    case ROW_OSC_MIX1:
        out.cc = CC_MIX1 + encoder;
        if ((encoder & 0x1) == 0) {
            // mix
            out.ccValue = newValue * 100.0f + .1f;
        } else {
            out.ccValue = (newValue + 1.0f) * 50.0f + .1f;
        }
        break;
    case ROW_OSC_MIX2:
        out.cc = CC_MIX3 + encoder;
        if ((encoder & 0x1) == 0) {
            // mix
            out.ccValue = newValue * 100.0f + .1f;
        } else {
            out.ccValue = (newValue + 1.0f) * 50.0f + .1f;
        }
        break;
    case ROW_OSC_FIRST ... ROW_OSC_LAST:
        if (encoder == ENCODER_OSC_FREQ) {
            out.cc = CC_OSC1_FREQ + (row - ROW_OSC_FIRST);
            out.ccValue = newValue * 12.0f + .1f;
        }
        break;
    case ROW_MATRIX_FIRST ... ROW_MATRIX4:
        if (encoder == ENCODER_MATRIX_MUL) {
            out.cc = CC_MATRIXROW1_MUL + row - ROW_MATRIX_FIRST;
            out.ccValue = newValue * 5.0f + 50.1f;
        }
        break;
    case ROW_LFO_FIRST ... ROW_LFOOSC3:
        switch (encoder) {
        case ENCODER_LFO_SHAPE:
            out.cc = CC_LFO1_SHAPE + (row - ROW_LFO_FIRST);
            out.ccValue = newValue + .1f;
            break;
        case ENCODER_LFO_FREQ:
            out.cc = CC_LFO1_FREQ + (row - ROW_LFO_FIRST);
            out.ccValue = newValue * 5.0f + .1f;
            break;
        case ENCODER_LFO_BIAS:
            out.cc = CC_LFO1_BIAS + (row - ROW_LFO_FIRST);
            out.ccValue = newValue * 100.0f + .1f;
            break;
        }
        break;
    case ROW_LFOENV2:
        if (encoder == ENCODER_LFO_ENV2_SILENCE) {
            out.cc = CC_LFO_ENV2_SILENCE;
            out.ccValue = newValue * 8.0f + .1f;
        }
        break;
    case ROW_LFOSEQ1 ... ROW_LFOSEQ2:
        if (encoder == ENCODER_STEPSEQ_GATE) {
            out.cc = CC_STEPSEQ5_GATE + (row - ROW_LFOSEQ1);
            out.ccValue = newValue * 100.0f + .1f;
        }
        break;
    case ROW_EFFECT1:
        if (encoder == ENCODER_EFFECT_TYPE) {
            out.cc = CC_FILTER_TYPE;
            out.ccValue = newValue + .1f;
        } else {
            out.cc = CC_FILTER_PARAM1 + encoder - 1;
            if (encoder == ENCODER_EFFECT_PARAM3) {
                out.ccValue = newValue * 100.0f + .1f;
            } else {
                out.ccValue = newValue * 128.0f + .1f;
            }
        }
        break;
    case ROW_EFFECT2:
        if (encoder == ENCODER_EFFECT_TYPE) {
            out.cc = CC_FILTER2_TYPE;
            out.ccValue = newValue + .1f;
        } else {
            out.cc = CC_FILTER2_PARAM1 + encoder - 1;
            if (encoder == ENCODER_EFFECT_PARAM3) {
                out.ccValue = newValue * 100.0f + .1f;
            } else {
                out.ccValue = newValue * 128.0f + .1f;
            }
        }
        break;
    case ROW_ENV1_TIME:
        if (encoder == ENCODER_ENV_A) {
            // CC_ENV_ATK_OP1 is separate from the rest
            out.cc = CC_ENV_ATK_OP1;
            out.ccValue = newValue * 64.0f + .1f;
        } else if (encoder == ENCODER_ENV_R) {
            out.cc = CC_ENV_REL_OP1;
            out.ccValue = newValue * 32.0f + .1f;
        }
        break;
    case ROW_ENV2_TIME:
    case ROW_ENV3_TIME:
    case ROW_ENV4_TIME:
    case ROW_ENV5_TIME:
    case ROW_ENV6_TIME:
        if (encoder == ENCODER_ENV_A) {
            out.cc = CC_ENV_ATK_OP2 + (row - ROW_ENV2_TIME);
            out.ccValue = newValue * 64.0f + .1f;
        } else if (encoder == ENCODER_ENV_R) {
            out.cc = CC_ENV_REL_OP2 + (row - ROW_ENV2_TIME);
            out.ccValue = newValue * 32.0f + .1f;
        }
        break;
    case ROW_LFOPHASES:
        out.cc = CC_LFO1_PHASE + encoder;
        out.ccValue = newValue * 100.0f + .1f;
        break;
    case ROW_ARPEGGIATOR1:
        switch (encoder) {
        case ENCODER_ARPEGGIATOR_CLOCK:
            out.cc = CC_ARP_CLOCK;
            out.ccValue = newValue + .1f;
            break;
        case ENCODER_ARPEGGIATOR_DIRECTION:
            out.cc = CC_ARP_DIRECTION;
            out.ccValue = newValue + .1f;
            break;
        case ENCODER_ARPEGGIATOR_OCTAVE:
            out.cc = CC_ARP_OCTAVE;
            out.ccValue = newValue + .1f;
            break;
        }
        break;
    case ROW_ARPEGGIATOR2:
        switch (encoder) {
        case ENCODER_ARPEGGIATOR_PATTERN:
            out.cc = CC_ARP_PATTERN;
            out.ccValue = newValue + .1f;
            break;
        case ENCODER_ARPEGGIATOR_DIVISION:
            out.cc = CC_ARP_DIVISION;
            out.ccValue = newValue + .1f;
            break;
        case ENCODER_ARPEGGIATOR_DURATION:
            out.cc = CC_ARP_DURATION;
            out.ccValue = newValue + .1f;
            break;
        }
        break;
    }

    if (out.cc == 0) {
        return false;
    }
    // CC limited to 127
    if (out.ccValue > 127) {
        out.ccValue = 127;
    }
    return true;
}

bool Preenfm3Engine::midiPatchParam(int timbre, int index, struct MidiParamMessage &out) const {
    (void) timbre;

    const int nameCount = 12;
    const int paramCount = NUMBER_OF_ROWS_FOR_EDITOR * NUMBER_OF_ENCODERS_PFM2;
    const int stepCount = 2 * 16;

    out.isNrpn = true;

    // Preset name.
    if (index < nameCount) {
        out.nrpn = (1 << 7) | (100 + index);
        out.nrpnValue = timbre_->getPfm3Preset()->presetName[index];
        return true;
    }
    index -= nameCount;

    // Flat parameters.
    if (index < paramCount) {
        int row = index / NUMBER_OF_ENCODERS_PFM2;
        int encoder = index % NUMBER_OF_ENCODERS_PFM2;
        const struct ParameterDisplay *param = &allParameterRows.row[row]->params[encoder];
        float floatValue = ((float*) timbre_->getPfm3Preset())[index];

        int valueToSend;
        if (param->displayType == DISPLAY_TYPE_FLOAT || param->displayType == DISPLAY_TYPE_FLOAT_OSC_FREQUENCY
                || param->displayType == DISPLAY_TYPE_FLOAT_LFO_FREQUENCY || param->displayType == DISPLAY_TYPE_LFO_KSYN) {
            valueToSend = (floatValue - param->minValue) * 100.0f + .1f;
        } else {
            valueToSend = floatValue + .1f;
        }
        out.nrpn = Pfm3Midi::getMidiIndexFromMemory(index);
        out.nrpnValue = valueToSend;
        return true;
    }
    index -= paramCount;

    // Step sequencer.
    if (index < stepCount) {
        int whichStepSeq = index / 16;
        int step = index % 16;
        out.nrpn = ((whichStepSeq + 2) << 7) | step;
        out.nrpnValue = timbre_->getSeqStepValue(whichStepSeq, step);
        return true;
    }

    return false;
}

void Preenfm3Engine::midiClockSongPositionStepOnVoice(Voice *voice, int songPosition, bool recomputeNext) {
    Pfm3VoiceState &state = pfm3State(voice);
    if (likely(timbre_->isLfoUsed(0))) state.lfoOsc[0].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(1))) state.lfoOsc[1].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(2))) state.lfoOsc[2].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(3))) state.lfoEnv[0].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(4))) state.lfoEnv2[0].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(5))) state.lfoStepSeq[0].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(6))) state.lfoStepSeq[1].midiClock(songPosition, recomputeNext);
}

void Preenfm3Engine::midiClockContinueOnVoice(Voice *voice, int songPosition) {
    Pfm3VoiceState &state = pfm3State(voice);
    if (likely(timbre_->isLfoUsed(0))) state.lfoOsc[0].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(1))) state.lfoOsc[1].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(2))) state.lfoOsc[2].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(3))) state.lfoEnv[0].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(4))) state.lfoEnv2[0].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(5))) state.lfoStepSeq[0].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(6))) state.lfoStepSeq[1].midiClock(songPosition, false);
}

void Preenfm3Engine::midiClockStartOnVoice(Voice *voice) {
    Pfm3VoiceState &state = pfm3State(voice);
    if (likely(timbre_->isLfoUsed(0))) state.lfoOsc[0].midiContinue();
    if (likely(timbre_->isLfoUsed(1))) state.lfoOsc[1].midiContinue();
    if (likely(timbre_->isLfoUsed(2))) state.lfoOsc[2].midiContinue();
    if (likely(timbre_->isLfoUsed(3))) state.lfoEnv[0].midiContinue();
    if (likely(timbre_->isLfoUsed(4))) state.lfoEnv2[0].midiContinue();
    if (likely(timbre_->isLfoUsed(5))) state.lfoStepSeq[0].midiContinue();
    if (likely(timbre_->isLfoUsed(6))) state.lfoStepSeq[1].midiContinue();
}

void Preenfm3Engine::midiClockContinue(int songPosition) {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        midiClockContinueOnVoice(timbre_->voices_[timbre_->voiceNumber_[k]], songPosition);
    }

    recomputeNext_ = ((songPosition & 0x1) == 0);
    timbre_->OnMidiContinue();
}

void Preenfm3Engine::midiClockStart() {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        midiClockStartOnVoice(timbre_->voices_[timbre_->voiceNumber_[k]]);
    }

    recomputeNext_ = true;
    timbre_->OnMidiStart();
}

void Preenfm3Engine::midiClockSongPositionStep(int songPosition) {
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        midiClockSongPositionStepOnVoice(timbre_->voices_[timbre_->voiceNumber_[k]], songPosition, recomputeNext_);
    }

    if ((songPosition & 0x1) == 0) {
        recomputeNext_ = true;
    }
}

void Preenfm3Engine::updateMidiNoteScale(int scale) {
    int intBreakNote;
    int curveBefore;
    int curveAfter;
    if (scale == 0) {
        intBreakNote = pfm3Params().midiNote1Curve.breakNote;
        curveBefore = pfm3Params().midiNote1Curve.curveBefore;
        curveAfter = pfm3Params().midiNote1Curve.curveAfter;
    } else {
        intBreakNote = pfm3Params().midiNote2Curve.breakNote;
        curveBefore = pfm3Params().midiNote2Curve.curveBefore;
        curveAfter = pfm3Params().midiNote2Curve.curveAfter;
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
