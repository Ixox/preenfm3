/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Preenfm3Engine.h"
#include "Hexter.h"
#include "MidiControlChange.h"
#include "Pfm3ControlChange.h"
#include "Pfm3FlashFormat.h"
#include "Pfm3Param.h"
#include "Pfm3Preset.h"
#include "SynthState.h"
#include "Timbre.h"
#include "Voice.h"
#include "VoicePool.h"

extern volatile bool gPfm3ParamTablesValid;

extern RNG_HandleTypeDef hrng;

/* Unsigned saturation, used by the PFM3 oscillator mix code. */
#define __USAT(ARG1, ARG2)                                                                                                                 \
    ({                                                                                                                                     \
        uint32_t __RES, __ARG1 = (ARG1);                                                                                                   \
        asm("usat %0, %1, %2" : "=r"(__RES) : "I"(ARG2), "r"(__ARG1));                                                                     \
        __RES;                                                                                                                             \
    })

static Pfm3TimbreContext pfm3Contexts[NUMBER_OF_TIMBRES] __attribute__((section(".ram_d1")));
Pfm3TimbreParams Preenfm3Engine::pfm3Params_[NUMBER_OF_TIMBRES] __attribute__((section(".ram_d1")));
Pfm3Preset pfm3PresetTmp __attribute__((section(".ram_d1")));
static Pfm3Preset pfm3Presets_[NUMBER_OF_TIMBRES] __attribute__((section(".ram_d3")));
static Hexter hexter;

float Preenfm3Engine::glidePhaseInc[13] = {1.0f / 2.0f,   1.0f / 5.0f,    1.0f / 9.0f,   1.0f / 15.0f,  1.0f / 22.0f,
                                           1.0f / 35.0f,  1.0f / 50.0f,   1.0f / 90.0f,  1.0f / 140.0f, 1.0f / 200.0f,
                                           1.0f / 500.0f, 1.0f / 1200.0f, 1.0f / 2700.0f};

float Preenfm3Engine::mpeBitchBend[6] = {0.0f, 480.0f, 360.0f, 240.0f, 120.0f, 0.0f};

Pfm3VoiceState Preenfm3Engine::pfm3VoiceStates_[MAX_NUMBER_OF_VOICES] __attribute__((section(".ram_d1")));

Pfm3VoiceState& Preenfm3Engine::voiceState(int slot) {
    return pfm3VoiceStates_[slot];
}

Pfm3VoiceState& Preenfm3Engine::pfm3State(Voice* voice) {
    return pfm3VoiceStates_[voice->slot()];
}

void Preenfm3Engine::initVoiceState(int slot) {
    Pfm3VoiceState& state = pfm3VoiceStates_[slot];
    state.freqAi = state.freqAo = 0.0f;
    state.freqBi = state.freqBo = 0.0f;
    state.freqHarm = 1.0f;
    state.targetFreqHarm = 0.0f;
}

const float Preenfm3Engine::unisonPhase[MAX_UNISON_VOICES] = {.37f, .11f, .495f, .53f, .03f, .19f, .89f};

extern float midiNoteScale[2][NUMBER_OF_TIMBRES][128];
extern float noise[32];

void Preenfm3Engine::init(Timbre* timbre, SynthState* synthState) {
    timbre_ = timbre;
    synthState_ = synthState;
    timbreNumber_ = timbre->getTimbreNumber();
    pfm3Preset_ = &pfm3Presets_[timbreNumber_];
    applyPfm3Preset(preenMainPreset);
    stepSelect_[0] = 0;
    stepSelect_[1] = 0;
    hexter_ = &hexter;
    context_ = &pfm3Contexts[timbreNumber_];
    for (int lfo = 0; lfo < NUMBER_OF_LFO; lfo++) {
        lfoUSed_[lfo] = 0;
    }
    recomputeNext_ = true;
    currentGate_ = 0.0f;
    initOscEnv(synthState);
    initContext();
    refreshContext();
    if (timbreNumber_ == 0) {
        validateParamTables();
    }

    for (int slot = 0; slot < MAX_NUMBER_OF_VOICES; slot++) {
        initVoiceState(slot);
    }
}

bool Preenfm3Engine::validateParamTables() {
    Pfm3Preset saved;
    serializePfm3Preset(saved);
    deserializePfm3Preset(defaultPreset);
    bool ok = pfm3ValidateTables() && pfm3ValidateDefaults(pfm3Params(), timbre_->sharedParams());
    gPfm3ParamTablesValid = ok;
    deserializePfm3Preset(saved);
    return ok;
}

void Preenfm3Engine::initOscEnv(SynthState* synthState) {
    Pfm3TimbreParams* p = &pfm3Params();

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

    for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
        afterParamsLoadVoice(timbre_->voices_[timbre_->voiceNumber_[k]]);
    }

    for (int j = 0; j < PFM3_PARAMS_PER_ROW; j++) {
        for (int e = 0; e < 6; e++) {
            env_[e].reloadADSR(j);
        }
    }
    refreshContext();
}

void Preenfm3Engine::initContext() {
    Pfm3TimbreParams* p = &pfm3Params();

    context_->params = p;
    context_->matrixRows = &p->matrixRowState1;

    context_->lfoOscParams[0] = &p->lfoOsc1;
    context_->lfoOscParams[1] = &p->lfoOsc2;
    context_->lfoOscParams[2] = &p->lfoOsc3;
    for (int k = 0; k < NUMBER_OF_LFO_OSC; k++) {
        context_->lfoPhases[k] = &((float*)&p->lfoPhases.phaseLfo1)[k];
    }

    context_->lfoEnv = &p->lfoEnv1;
    context_->lfoEnv2 = &p->lfoEnv2;

    context_->lfoStepSeqParams[0] = &p->lfoSeq1;
    context_->lfoStepSeqParams[1] = &p->lfoSeq2;
    context_->lfoStepSeqSteps[0] = &p->lfoSteps1;
    context_->lfoStepSeqSteps[1] = &p->lfoSteps2;
}

void Preenfm3Engine::refreshContext() {
    Pfm3TimbreParams* p = &pfm3Params();

    float pans[6] = {p->engineMix1.panOsc1, p->engineMix1.panOsc2, p->engineMix2.panOsc3,
                     p->engineMix2.panOsc4, p->engineMix3.panOsc5, p->engineMix3.panOsc6};
    int* opInfo = algoOpInformation[(int)p->engine1.algo];
    for (int op = 0; op < 6; op++) {
        if (likely(opInfo[op] == 1)) {
            float pan1 = pans[op] + 1.0f;
            int pan = __USAT((int)(pan1 * 128), 8);
            context_->panLeft[op] = panTable[pan];
            context_->panRight[op] = panTable[256 - pan];
        } else {
            context_->panLeft[op] = 0.0f;
            context_->panRight[op] = 0.0f;
        }
    }

    context_->mixerGain = timbre_->sharedParams().effect1.param3;
}

void Preenfm3Engine::bindVoice(Voice* voice) {
    bindVoiceState(voice);
}

void Preenfm3Engine::noteOn(char note, char velocity) {
    if (unlikely(timbre_->maxVoices_ == 0 ||
                 timbre_->mixerState_->instrumentState_[timbre_->timbreNumber_].scaleFrequencies[(int)note] == 0.0f)) {
        return;
    }

    note &= 0x7f;

    float noteFrequency = timbre_->mixerState_->instrumentState_[timbre_->timbreNumber_].scaleFrequencies[(int)note];
    bool unison = pfm3Params().engine1.playMode == PLAY_MODE_UNISON;
    bool mono = pfm3Params().engine1.playMode == PLAY_MODE_MONO;
    int unisonWidth = timbre_->maxVoices_ < MAX_UNISON_VOICES ? (int)timbre_->maxVoices_ : MAX_UNISON_VOICES;

    timbre_->numberOfVoiceInverse_ = 1.0f / (float)(unison ? unisonWidth : (timbre_->maxVoices_ > 0 ? timbre_->maxVoices_ : 1));

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
    int iNov = pfm3Params().engine1.playMode == PLAY_MODE_MONO ? 1 : (int)timbre_->numberOfVoices_;
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

void Preenfm3Engine::bindVoiceState(Voice* voice) {
    resetPfm3VoiceState(voice);
    voice->currentTimbre_ = timbre_;
    Pfm3VoiceState& state = pfm3State(voice);

    state.matrix.init(context_->matrixRows);

    for (int k = 0; k < NUMBER_OF_LFO_OSC; k++) {
        state.lfoOsc[k].init(context_->lfoOscParams[k], context_->lfoPhases[k], &state.matrix, (SourceEnum)(MATRIX_SOURCE_LFO1 + k),
                             (DestinationEnum)(LFO1_FREQ + k));
    }

    state.lfoEnv[0].init(context_->lfoEnv, &state.matrix, MATRIX_SOURCE_LFOENV1, (DestinationEnum)0);
    state.lfoEnv2[0].init(context_->lfoEnv2, &state.matrix, MATRIX_SOURCE_LFOENV2, (DestinationEnum)LFOENV2_SILENCE);

    for (int k = 0; k < NUMBER_OF_LFO_STEP; k++) {
        state.lfoStepSeq[k].init(context_->lfoStepSeqParams[k], context_->lfoStepSeqSteps[k], &state.matrix,
                                 (SourceEnum)(MATRIX_SOURCE_LFOSEQ1 + k), (DestinationEnum)(LFOSEQ1_GATE + k));
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

void Preenfm3Engine::glideFirstNoteOff(Voice* voice) {
    voice->note_ = pfm3State(voice).nextGlidingNote;
    voice->noteFrequency_ = pfm3State(voice).nextGlidingNoteFrequency;
    pfm3State(voice).nextGlidingNote = 0;
}

void Preenfm3Engine::glideToNote(Voice* voice, short newNote, float newNoteFrequency) {
    pfm3State(voice).gliding = true;
    pfm3State(voice).glidePhase = 0.0f;
    pfm3State(voice).nextGlidingNote = newNote;
    pfm3State(voice).nextGlidingNoteFrequency = newNoteFrequency;
    if (voice->holdedByPedal_) {
        glideFirstNoteOff(voice);
    }

    Pfm3VoiceState& state = pfm3State(voice);
    osc_[0].glideToNote(&state.oscState1_, newNoteFrequency);
    osc_[1].glideToNote(&state.oscState2_, newNoteFrequency);
    osc_[2].glideToNote(&state.oscState3_, newNoteFrequency);
    osc_[3].glideToNote(&state.oscState4_, newNoteFrequency);
    osc_[4].glideToNote(&state.oscState5_, newNoteFrequency);
    osc_[5].glideToNote(&state.oscState6_, newNoteFrequency);
}

void Preenfm3Engine::glideVoice(Voice* voice) {
    pfm3State(voice).glidePhase += glidePhaseInc[(int)(pfm3Params().engine1.glideSpeed + .05f)];
    Pfm3VoiceState& state = pfm3State(voice);
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
float Preenfm3Engine::getNoteRealFrequencyEstimation(Voice* voice, float newNoteFrequency) {
    return osc_[0].getNoteRealFrequencyEstimation(&pfm3State(voice).oscState1_, newNoteFrequency);
}

void Preenfm3Engine::noteOnVoiceWithoutPop(Voice* voice, short newNote, float newNoteFrequency, short velocity, uint32_t index,
                                           float phase) {
    voice->index_ = index;
    if (voice->pendingTimbre_ == nullptr && !voice->released_ && pfm3Params().engine1.playMode != PLAY_MODE_POLY &&
        pfm3Params().engine2.glideType != GLIDE_TYPE_OFF) {
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

        Pfm3VoiceState& state = pfm3State(voice);
        env_[0].noteOffQuick(&state.envState1_);
        env_[1].noteOffQuick(&state.envState2_);
        env_[2].noteOffQuick(&state.envState3_);
        env_[3].noteOffQuick(&state.envState4_);
        env_[4].noteOffQuick(&state.envState5_);
        env_[5].noteOffQuick(&state.envState6_);
        pfm3State(voice).newNotePlayed = false;
    }
}

void Preenfm3Engine::noteOnVoice(Voice* voice, short newNote, float newNoteFrequency, short velocity, uint32_t index, float phase) {

    Pfm3TimbreParams& params = pfm3Params();
    Pfm3VoiceState& state = pfm3State(voice);

    if (unlikely(params.engine1.playMode != PLAY_MODE_POLY && params.engine2.glideType == GLIDE_TYPE_ALWAYS)) {
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
    voice->fxNoteTrigger_ = 1;
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

    state.velIm1 = params.engineIm1.modulationIndexVelo1 * (float)velocity * .0078125f;
    state.velIm2 = params.engineIm1.modulationIndexVelo2 * (float)velocity * .0078125f;
    state.velIm3 = params.engineIm2.modulationIndexVelo3 * (float)velocity * .0078125f;
    state.velIm4 = params.engineIm2.modulationIndexVelo4 * (float)velocity * .0078125f;
    state.velIm5 = params.engineIm3.modulationIndexVelo5 * (float)velocity * .0078125f;
    state.velIm6 = params.engineIm3.modulationIndexVelo6 * (float)velocity * .0078125f;

    int zeroVelo = (16 - params.engine1.velocity) * 8;
    int newVelocity = zeroVelo + ((velocity * (128 - zeroVelo)) >> 7);
    voice->velocity_ = newVelocity * .0078125f;

    pfm3State(voice).newNotePlayed = true;
    lfoNoteOn(voice);
}

void Preenfm3Engine::endNoteOrBeginNextOne(Voice* voice) {
    if (pfm3State(voice).newNotePending) {
        if (voice->pendingTimbre_ != nullptr) {
            pfm3State(voice).newNotePending = false;
            voice->transferReady_ = true;
        } else if (pfm3State(voice).pendingNote <= 127) {
            noteOnVoice(voice, pfm3State(voice).pendingNote, pfm3State(voice).pendingNoteFrequency, pfm3State(voice).pendingNoteVelocity,
                        voice->index_, pfm3State(voice).phase_);
        } else {
            noteOnVoice(voice, pfm3State(voice).pendingNote - 128, pfm3State(voice).pendingNoteFrequency,
                        pfm3State(voice).pendingNoteVelocity, voice->index_, pfm3State(voice).phase_);
            pfm3State(voice).noteAlreadyFinished = 1;
        }
    } else {
        voice->playing_ = false;
        voice->released_ = false;
    }
    Pfm3VoiceState& state = pfm3State(voice);
    state.freqAi = 0.0f;
    state.freqAo = 0.0f;
    state.freqBi = 0.0f;
    state.freqBo = 0.0f;
}

void Preenfm3Engine::lfoNoteOn(Voice* voice) {
    Pfm3VoiceState& state = pfm3State(voice);
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

void Preenfm3Engine::lfoNoteOff(Voice* voice) {
    Pfm3VoiceState& state = pfm3State(voice);
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

void Preenfm3Engine::afterParamsLoadVoice(Voice* voice) {
    resetMatrix(voice);
    Pfm3VoiceState& state = pfm3State(voice);
    for (int j = 0; j < PFM3_PARAMS_PER_ROW; j++) {
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

void Preenfm3Engine::resetMatrix(Voice* voice) {
    Pfm3VoiceState& state = pfm3State(voice);
    state.matrix.resetSources();
    state.matrix.resetAllDestination();
}

void Preenfm3Engine::lfoValueChangeOnVoice(Voice* voice, Pfm3Param id, int field, float newValue) {
    Pfm3VoiceState& state = pfm3State(voice);
    switch ((Pfm3Block)pfm3ParamBlockOf(id)) {
    case Pfm3Block::LfoOsc:
        state.lfoOsc[pfm3ParamElementOf(id)].valueChanged(field);
        break;
    case Pfm3Block::LfoEnv1:
        state.lfoEnv[0].valueChanged(field);
        break;
    case Pfm3Block::LfoEnv2:
        state.lfoEnv2[0].valueChanged(field);
        break;
    case Pfm3Block::LfoSeq:
        state.lfoStepSeq[pfm3ParamElementOf(id)].valueChanged(field);
        break;
    default:
        break;
    }
}

void Preenfm3Engine::noteOffVoice(Voice* voice) {
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

        Pfm3VoiceState& state = pfm3State(voice);
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

void Preenfm3Engine::noteOffVoiceQuick(Voice* voice) {
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

    Pfm3VoiceState& state = pfm3State(voice);
    env_[0].noteOffQuick(&state.envState1_);
    env_[1].noteOffQuick(&state.envState2_);
    env_[2].noteOffQuick(&state.envState3_);
    env_[3].noteOffQuick(&state.envState4_);
    env_[4].noteOffQuick(&state.envState5_);
    env_[5].noteOffQuick(&state.envState6_);

    lfoNoteOff(voice);
}

void Preenfm3Engine::killVoice(Voice* voice) {
    voice->resetVoiceState();
    resetPfm3VoiceState(voice);
    Pfm3VoiceState& state = pfm3State(voice);
    state.env1ValueMem = 0;
    state.env2ValueMem = 0;
    state.env3ValueMem = 0;
    state.env4ValueMem = 0;
    state.env5ValueMem = 0;
    state.env6ValueMem = 0;
}

void Preenfm3Engine::startVoiceAfterTransfer(Voice* voice, int note, float noteFrequency, uint8_t velocity, float phase, bool alreadyOff) {
    noteOnVoice(voice, note, noteFrequency, velocity, voice->index_, phase);
    if (alreadyOff) {
        pfm3State(voice).noteAlreadyFinished = 1;
    }
}

void Preenfm3Engine::completeVoiceTransfer(Voice* voice, int poolIndex, VoicePool* pool) {
    Timbre* target = voice->pendingTimbre_;
    if (target == nullptr) {
        return;
    }

    Pfm3VoiceState& state = pfm3State(voice);
    int note = state.pendingNote & 0x7f;
    bool alreadyOff = state.pendingNote > 127;
    uint8_t velocity = (uint8_t)state.pendingNoteVelocity;
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

void Preenfm3Engine::resetPfm3VoiceState(Voice* voice) {
    Pfm3VoiceState& state = pfm3State(voice);
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

void Preenfm3Engine::updateAllModulationIndexes(Voice* voice) {
    Pfm3VoiceState& state = pfm3State(voice);
    Pfm3TimbreParams& pfm3 = pfm3Params();
    int numberOfIMs = algoInformation[(int)(pfm3.engine1.algo)].im;

    // Feedback range is [0:1] compared to [0:16] of other modulation, let's divide the modulation impact by 16 (* 0.0625)
    state.feedbackModulation = pfm3.engineIm3.modulationIndex6 + state.velIm6 + state.matrix.getDestination(MTX_DEST_FEEDBACK) * 0.0625f;
    if (unlikely(state.feedbackModulation < 0.0f)) {
        state.feedbackModulation = 0.0f;
    } else if (unlikely(state.feedbackModulation > 1.0f)) {
        state.feedbackModulation = 1.0f;
    }

    state.modulationIndex1 = pfm3.engineIm1.modulationIndex1 + state.matrix.getDestination(INDEX_MODULATION1) +
                             state.matrix.getDestination(INDEX_ALL_MODULATION) + state.velIm1;
    if (unlikely(state.modulationIndex1 < 0.0f)) {
        state.modulationIndex1 = 0.0f;
    }
    state.modulationIndex2 = pfm3.engineIm1.modulationIndex2 + state.matrix.getDestination(INDEX_MODULATION2) +
                             state.matrix.getDestination(INDEX_ALL_MODULATION) + state.velIm2;
    if (unlikely(state.modulationIndex2 < 0.0f)) {
        state.modulationIndex2 = 0.0f;
    }

    state.modulationIndex3 = pfm3.engineIm2.modulationIndex3 + state.matrix.getDestination(INDEX_MODULATION3) +
                             state.matrix.getDestination(INDEX_ALL_MODULATION) + state.velIm3;
    if (unlikely(state.modulationIndex3 < 0.0f)) {
        state.modulationIndex3 = 0.0f;
    }

    if (likely(numberOfIMs < 3)) {
        return;
    }

    state.modulationIndex4 = pfm3.engineIm2.modulationIndex4 + state.matrix.getDestination(INDEX_MODULATION4) +
                             state.matrix.getDestination(INDEX_ALL_MODULATION) + state.velIm4;
    if (unlikely(state.modulationIndex4 < 0.0f)) {
        state.modulationIndex4 = 0.0f;
    }

    state.modulationIndex5 = pfm3.engineIm3.modulationIndex5 + state.matrix.getDestination(INDEX_ALL_MODULATION) + state.velIm5;
    if (unlikely(state.modulationIndex5 < 0.0f)) {
        state.modulationIndex5 = 0.0f;
    }
}

void Preenfm3Engine::prepareMatrixForNewBlock(Voice* voice) {
    Pfm3VoiceState& state = pfm3State(voice);
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

void Preenfm3Engine::updateAllMixOscsAndPans(Voice* voice) {
    Pfm3VoiceState& state = pfm3State(voice);
    Pfm3TimbreParams& pfm3 = pfm3Params();
    float inv65535 = .0000152587890625; // 1/ 65535
    int pan;
    int numberOfMix = algoInformation[(int)(pfm3.engine1.algo)].mix;
    // add a LP on mix when MPE to mitigate noise dur to midi CC being [0-127]
    bool notMPE = timbre_->timbreNumber_ != 0 || timbre_->getMPESetting() == 0;

    state.mix1 = pfm3.engineMix1.mixOsc1 + state.matrix.getDestination(MIX_OSC1) + state.matrix.getDestination(ALL_MIX);
    // Optimization to check state.mix1 is between 0 and 1
    if (likely(notMPE)) {
        state.mix1 = __USAT((int)(state.mix1 * 65536), 16) * inv65535;
    } else {
        state.mix1 = __USAT((int)((state.mix1 * .05 + state.mix1Previous * .95) * 65536), 16) * inv65535;
        state.mix1Previous = state.mix1;
    }
    float pan1 = pfm3.engineMix1.panOsc1 + state.matrix.getDestination(PAN_OSC1) + state.matrix.getDestination(ALL_PAN) + 1.0f;
    // pan1 is between -1 and 1 : Scale from 0.0 to 256
    pan = __USAT((int)(pan1 * 128), 8);
    state.pan1Left = panTable[pan] * .05f + state.pan1Left * .95f;
    state.pan1Right = panTable[256 - pan] * .05f + state.pan1Right * .95f;

    if (unlikely(numberOfMix == 1)) {
        return;
    }

    state.mix2 = pfm3.engineMix1.mixOsc2 + state.matrix.getDestination(MIX_OSC2) + state.matrix.getDestination(ALL_MIX);
    if (likely(notMPE)) {
        state.mix2 = __USAT((int)(state.mix2 * 65535), 16) * inv65535;
    } else {
        state.mix2 = __USAT((int)((state.mix2 * .05 + state.mix2Previous * .95) * 65535), 16) * inv65535;
        state.mix2Previous = state.mix2;
    }
    float pan2 = pfm3.engineMix1.panOsc2 + state.matrix.getDestination(PAN_OSC2) + state.matrix.getDestination(ALL_PAN) + 1.0f;
    pan = __USAT((int)(pan2 * 128), 8);
    state.pan2Left = panTable[pan] * .05f + state.pan2Left * .95f;
    state.pan2Right = panTable[256 - pan] * .05f + state.pan2Right * .95f;

    if (unlikely(numberOfMix == 2)) {
        return;
    }

    state.mix3 = pfm3.engineMix2.mixOsc3 + state.matrix.getDestination(MIX_OSC3) + state.matrix.getDestination(ALL_MIX);
    if (likely(notMPE)) {
        state.mix3 = __USAT((int)(state.mix3 * 65535), 16) * inv65535;
    } else {
        state.mix3 = __USAT((int)((state.mix3 * .05 + state.mix3Previous * .95) * 65535), 16) * inv65535;
        state.mix3Previous = state.mix3;
    }
    float pan3 = pfm3.engineMix2.panOsc3 + state.matrix.getDestination(PAN_OSC3) + state.matrix.getDestination(ALL_PAN) + 1.0f;
    pan = __USAT((int)(pan3 * 128), 8);
    state.pan3Left = panTable[pan] * .05f + state.pan3Left * .95f;
    state.pan3Right = panTable[256 - pan] * .05f + state.pan3Right * .95f;

    if (numberOfMix == 3) {
        return;
    }

    // No matrix for state.mix4 and pan4
    state.mix4 = pfm3.engineMix2.mixOsc4 + state.matrix.getDestination(MIX_OSC4) + state.matrix.getDestination(ALL_MIX);
    if (likely(notMPE)) {
        state.mix4 = __USAT((int)(state.mix4 * 65535), 16) * inv65535;
    } else {
        state.mix4 = __USAT((int)((state.mix4 * .05 + state.mix4Previous * .95) * 65535), 16) * inv65535;
        state.mix4Previous = state.mix4;
    }
    float pan4 = pfm3.engineMix2.panOsc4 + state.matrix.getDestination(PAN_OSC4) + state.matrix.getDestination(ALL_PAN) + 1.0f;
    pan = __USAT((int)(pan4 * 128), 8);
    state.pan4Left = panTable[pan] * .05f + state.pan4Left * .95f;
    state.pan4Right = panTable[256 - pan] * .05f + state.pan4Right * .95f;

    if (numberOfMix == 4) {
        return;
    }

    state.mix5 = pfm3.engineMix3.mixOsc5 + state.matrix.getDestination(ALL_MIX);
    if (likely(notMPE)) {
        state.mix5 = __USAT((int)(state.mix5 * 65535), 16) * inv65535;
    } else {
        state.mix5 = __USAT((int)((state.mix5 * .05 + state.mix5Previous * .95) * 65535), 16) * inv65535;
        state.mix5Previous = state.mix5;
    }

    float pan5 = pfm3.engineMix3.panOsc5 + state.matrix.getDestination(ALL_PAN) + 1.0f;
    pan = __USAT((int)(pan5 * 128), 8);
    state.pan5Left = panTable[pan] * .05f + state.pan5Left * .95f;
    state.pan5Right = panTable[256 - pan] * .05f + state.pan5Right * .95f;

    state.mix6 = pfm3.engineMix3.mixOsc6 + state.matrix.getDestination(ALL_MIX);
    if (likely(notMPE)) {
        state.mix6 = __USAT((int)(state.mix6 * 65535), 16) * inv65535;
    } else {
        state.mix6 = __USAT((int)((state.mix6 * .05 + state.mix6Previous * .95) * 65535), 16) * inv65535;
        state.mix6Previous = state.mix6;
    }

    float pan6 = pfm3.engineMix3.panOsc6 + state.matrix.getDestination(ALL_PAN) + 1.0f;
    pan = __USAT((int)(pan6 * 128), 8);
    state.pan6Left = panTable[pan] * .05f + state.pan6Left * .95f;
    state.pan6Right = panTable[256 - pan] * .05f + state.pan6Right * .95f;
}

void Preenfm3Engine::renderVoice(Voice* voice) {
    Pfm3VoiceState& pfm3State_ = pfm3State(voice);
    Timbre* currentTimbre = timbre_;
    float& velocity = voice->velocity_;

    // After matrix
    if (unlikely(pfm3State(voice).newNotePlayed)) {
        pfm3State(voice).newNotePlayed = false;
        pfm3State_.env1ValueMem = env_[0].noteOnAfterMatrixCompute(&pfm3State_.envState1_, &pfm3State_.matrix);
        pfm3State_.env2ValueMem = env_[1].noteOnAfterMatrixCompute(&pfm3State_.envState2_, &pfm3State_.matrix);
        pfm3State_.env3ValueMem = env_[2].noteOnAfterMatrixCompute(&pfm3State_.envState3_, &pfm3State_.matrix);
        pfm3State_.env4ValueMem = env_[3].noteOnAfterMatrixCompute(&pfm3State_.envState4_, &pfm3State_.matrix);
        pfm3State_.env5ValueMem = env_[4].noteOnAfterMatrixCompute(&pfm3State_.envState5_, &pfm3State_.matrix);
        pfm3State_.env6ValueMem = env_[5].noteOnAfterMatrixCompute(&pfm3State_.envState6_, &pfm3State_.matrix);
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

    float* sample = voice->sampleBlock_;
    float inv32 = .03125f;

    if (unlikely(pfm3State_.matrix.getDestination(ALL_OSC_FREQ_HARM) != pfm3State_.targetFreqHarm) || currentTimbre->getMPESetting() > 0) {
        // * 20 so that we have a full tone for full pitchbend
        pfm3State_.targetFreqHarm = pfm3State_.matrix.getDestination(ALL_OSC_FREQ_HARM) * 20;
        pfm3State_.targetFreqHarm +=
            (pfm3State_.matrix.getSource(MATRIX_SOURCE_PITCHBEND_MPE) * mpeBitchBend[currentTimbre->getMPESetting()]);

        float findex = 512 + pfm3State_.targetFreqHarm;
        int index = findex;
        // Max = 1024
        index &= 0x3ff;
        float fp = findex - index;
        pfm3State_.freqHarm = (exp2_harm[index] * (1.0f - fp) + exp2_harm[index + 1] * fp);
    }

    switch ((int)pfm3Params().engine1.algo) {

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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc3Values =
                osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc3Values =
                osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float* osc3Values =
                osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
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

                pfm3State_.oscState1_.frequency =
                    freq2 * voiceIm1 + freq3 * voiceIm2 + freq4 * voiceIm3 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc4Values =
                osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc4Values =
                osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc4Values =
                osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
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

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) &&
                         env_[2].isDead(&pfm3State_.envState3_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float* osc4Values =
                osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
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

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_) &&
                         env_[4].isDead(&pfm3State_.envState5_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float* osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float* osc4Values =
                osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
                                                             pfm3State_.oscState4_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

            float div2TimesVelocity = .5f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float freq6 = osc6Values[k] * env6Value;
                freq6 *= pfm3State_.oscState6_.frequency;

                float freq4 = osc4Values[k];

                float freq3 = osc3Values[k] * env3Value;
                freq3 *= pfm3State_.oscState3_.frequency;

                float freq2 = osc2Values[k] * env2Value;
                freq2 *= pfm3State_.oscState2_.frequency;

                pfm3State_.oscState1_.frequency =
                    freq2 * voiceIm1 + freq3 * voiceIm2 + freq4 * voiceIm3 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float* osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values =
                osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values =
                osc_[1].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState2_, pfm3State_.feedbackModulation, env2Value, env2Inc,
                                                             pfm3State_.oscState2_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values =
                osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float* osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values =
                osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
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
            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_) &&
                         env_[4].isDead(&pfm3State_.envState5_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float* osc4Values =
                osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
                                                             pfm3State_.oscState4_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float* osc5Values = osc_[4].getNextBlock(&pfm3State_.oscState5_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values =
                osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float* osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float* osc5Values = osc_[4].getNextBlock(&pfm3State_.oscState5_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values =
                osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
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

                pfm3State_.oscState3_.frequency =
                    freq4 * voiceIm2 + freq5 * voiceIm3 + freq6 * voiceIm4 + pfm3State_.oscState3_.mainFrequencyPlusMatrix;
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values =
                osc_[1].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState2_, pfm3State_.feedbackModulation, env2Value, env2Inc,
                                                             pfm3State_.oscState2_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float* osc5Values = osc_[4].getNextBlock(&pfm3State_.oscState5_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values =
                osc_[1].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState2_, pfm3State_.feedbackModulation, env2Value, env2Inc,
                                                             pfm3State_.oscState2_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float* osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

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

                pfm3State_.oscState1_.frequency =
                    freq2 * voiceIm1 + freq3 * voiceIm2 + freq5 * voiceIm4 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float* osc3Values =
                osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
                                                             pfm3State_.oscState3_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

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

                pfm3State_.oscState1_.frequency =
                    freq2 * voiceIm1 + freq3 * voiceIm2 + freq4 * voiceIm3 + pfm3State_.oscState1_.mainFrequencyPlusMatrix;
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values =
                osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
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
                *sample++ = car1 * pfm3State_.pan1Left + car4 * pfm3State_.pan2Left + car5 * pfm3State_.pan3Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }

            pfm3State_.freqAi = f2xm1;
            pfm3State_.freqAo = freq2;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[3].isDead(&pfm3State_.envState4_) &&
                         env_[4].isDead(&pfm3State_.envState5_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc3Values =
                osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
                                                             pfm3State_.oscState3_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float* osc5Values = osc_[4].getNextBlock(&pfm3State_.oscState5_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

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

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) &&
                         env_[3].isDead(&pfm3State_.envState4_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc3Values =
                osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
                                                             pfm3State_.oscState3_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

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

                *sample++ =
                    car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car4 * pfm3State_.pan3Right + car5 * pfm3State_.pan4Right;
                *sample++ =
                    car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car4 * pfm3State_.pan3Left + car5 * pfm3State_.pan4Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
                env6Value += env6Inc;
            }
            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) &&
                         env_[3].isDead(&pfm3State_.envState4_) && env_[4].isDead(&pfm3State_.envState5_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values =
                osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
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

                *sample++ =
                    car1 * pfm3State_.pan1Right + car3 * pfm3State_.pan2Right + car4 * pfm3State_.pan3Right + car5 * pfm3State_.pan4Right;
                *sample++ =
                    car1 * pfm3State_.pan1Left + car3 * pfm3State_.pan2Left + car4 * pfm3State_.pan3Left + car5 * pfm3State_.pan4Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_) &&
                         env_[3].isDead(&pfm3State_.envState4_) && env_[4].isDead(&pfm3State_.envState5_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc1Values = osc_[0].getNextBlock(&pfm3State_.oscState1_);

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values =
                osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
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

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right +
                            car4 * pfm3State_.pan4Right + car5 * pfm3State_.pan5Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left +
                            car4 * pfm3State_.pan4Left + car5 * pfm3State_.pan5Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) &&
                         env_[3].isDead(&pfm3State_.envState4_) && env_[4].isDead(&pfm3State_.envState5_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float* osc5Values =
                osc_[4].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState5_, pfm3State_.feedbackModulation, env5Value, env5Inc,
                                                             pfm3State_.oscState5_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

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

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[2].isDead(&pfm3State_.envState3_) &&
                         env_[5].isDead(&pfm3State_.envState6_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc1Values = osc_[0].getNextBlock(&pfm3State_.oscState1_);

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float* osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values =
                osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
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

                *sample++ =
                    car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right + car5 * pfm3State_.pan4Right;
                *sample++ =
                    car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left + car5 * pfm3State_.pan4Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) &&
                         env_[2].isDead(&pfm3State_.envState3_) && env_[4].isDead(&pfm3State_.envState5_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc1Values = osc_[0].getNextBlock(&pfm3State_.oscState1_);

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            float* osc5Values =
                osc_[4].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState5_, pfm3State_.feedbackModulation, env5Value, env5Inc,
                                                             pfm3State_.oscState5_.frequency, pfm3State_.fdbLastValue);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values = osc_[5].getNextBlock(&pfm3State_.oscState6_);

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

                *sample++ =
                    car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right + car6 * pfm3State_.pan4Right;
                *sample++ =
                    car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left + car6 * pfm3State_.pan4Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env6Value += env6Inc;
            }

            pfm3State_.freqAi = f4xm1;
            pfm3State_.freqAo = freq4;

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) &&
                         env_[2].isDead(&pfm3State_.envState3_) && env_[5].isDead(&pfm3State_.envState6_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc1Values = osc_[0].getNextBlock(&pfm3State_.oscState1_);

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float* osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float* osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            // Not enough float arrays for a fifth optimization
            pfm3State_.oscState5_.frequency = pfm3State_.oscState5_.mainFrequencyPlusMatrix;
            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;

            // Multiplier is 1 because op6 is a carrier
            float* osc6Values = osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation,
                                                                             env6Value, env6Inc, 1.0f, pfm3State_.fdbLastValue);

            float div6TimesVelocity = .16f * velocity;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                float car1 = osc1Values[k] * env1Value * pfm3State_.mix1;
                float car2 = osc2Values[k] * env2Value * pfm3State_.mix2;
                float car3 = osc3Values[k] * env3Value * pfm3State_.mix3;
                float car4 = osc4Values[k] * env4Value * pfm3State_.mix4;
                // getNextBlockWithFeedbackAndEnveloppe already applied envelope
                float car6 = osc6Values[k] * pfm3State_.mix6;

                float car5 = osc_[4].getNextSample(&pfm3State_.oscState5_, waveTable5) * env5Value * pfm3State_.mix5;

                *sample++ = (car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right +
                             car4 * pfm3State_.pan4Right + car5 * pfm3State_.pan5Right + car6 * pfm3State_.pan6Right) *
                            div6TimesVelocity;
                *sample++ = (car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left +
                             car4 * pfm3State_.pan4Left + car5 * pfm3State_.pan5Left + car6 * pfm3State_.pan6Left) *
                            div6TimesVelocity;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) &&
                         env_[2].isDead(&pfm3State_.envState3_) && env_[3].isDead(&pfm3State_.envState4_) &&
                         env_[4].isDead(&pfm3State_.envState5_) && env_[5].isDead(&pfm3State_.envState6_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc1Values = osc_[0].getNextBlock(&pfm3State_.oscState1_);

            pfm3State_.oscState2_.frequency = pfm3State_.oscState2_.mainFrequencyPlusMatrix;
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float* osc3Values = osc_[2].getNextBlock(&pfm3State_.oscState3_);

            pfm3State_.oscState4_.frequency = pfm3State_.oscState4_.mainFrequencyPlusMatrix;
            float* osc4Values = osc_[3].getNextBlock(&pfm3State_.oscState4_);

            pfm3State_.oscState6_.frequency = pfm3State_.oscState6_.mainFrequencyPlusMatrix;
            float* osc6Values =
                osc_[5].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState6_, pfm3State_.feedbackModulation, env6Value, env6Inc,
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

                *sample++ = (car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right +
                             car4 * pfm3State_.pan4Right + car5 * pfm3State_.pan5Right) *
                            div5TimesVelocity;
                *sample++ = (car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left +
                             car4 * pfm3State_.pan4Left + car5 * pfm3State_.pan5Left) *
                            div5TimesVelocity;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
                env5Value += env5Inc;
            }
            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) &&
                         env_[2].isDead(&pfm3State_.envState3_) && env_[3].isDead(&pfm3State_.envState4_) &&
                         env_[4].isDead(&pfm3State_.envState5_) && env_[5].isDead(&pfm3State_.envState6_))) {
                endNoteOrBeginNextOne(voice);
            }
        }
        break;

    case ALG29:
        /* Windowed sync AM, 1 synced by 3
        inspiration from
        https://electro-music.com/nm_classic/015_workshops/Clavia/NordModularWorkshops&Threads/WerkMap/WorkShops/Hordijk1999-2000/VOSIM.html
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc4Values =
                osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
                                                             pfm3State_.oscState4_.frequency, pfm3State_.fdbLastValue);

            float div2TimesVelocity = velocity * .5f;

            float mix1V = pfm3State_.mix1 * div2TimesVelocity;
            float mix2V = pfm3State_.mix2 * div2TimesVelocity;

            float osc1Matrix = pfm3State_.oscState1_.mainFrequencyPlusMatrix - pfm3State_.oscState1_.mainFrequency;
            float osc1FrequencyPlusMatrix = pfm3State_.oscState1_.mainFrequency + osc1Matrix * fabsf(osc1Matrix) * 0.25f;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                float freq4 = osc4Values[k];

                pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix + voiceIm3 * freq4;
                float osc3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3);
                float phase = osc_[2].getPhase(&pfm3State_.oscState3_);
                float freq3 = osc3 * env3Value * pfm3State_.oscState3_.frequency;

                // sync check
                bool isSync = pfm3State_.prevPhase > phase;
                pfm3State_.prevPhase = phase;
                float window = 1.f - phase;

                pfm3State_.oscState1_.frequency = freq3 * voiceIm1 + osc1FrequencyPlusMatrix;

                // sync slave osc (sub-sample accurate)
                if (unlikely(isSync)) {
                    osc_[0].hardSyncToMaster(&pfm3State_.oscState1_, &osc_[2], &pfm3State_.oscState3_, phase);
                }

                float carSample1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * window;
                carSample1 *= fabsf(carSample1); // vosim shape
                float car1 = carSample1 * env1Value * mix1V;

                pfm3State_.oscState2_.frequency = freq4 * voiceIm2 + pfm3State_.oscState2_.mainFrequencyPlusMatrix;
                float car2 = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * env2Value * mix2V;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left;

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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc4Values =
                osc_[3].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState4_, pfm3State_.feedbackModulation, env4Value, env4Inc,
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

                pfm3State_.oscState1_.frequency = freq3 * voiceIm1 + osc1FrequencyPlusMatrix + freq4 * voiceIm3;
                pfm3State_.oscState2_.frequency = freq3 * voiceIm2 + osc2FrequencyPlusMatrix + freq4 * voiceIm4;

                // sync slave osc (sub-sample accurate)
                if (unlikely(isSync)) {
                    osc_[0].hardSyncToMaster(&pfm3State_.oscState1_, &osc_[2], &pfm3State_.oscState3_, phase);
                    osc_[1].hardSyncToMaster(&pfm3State_.oscState2_, &osc_[2], &pfm3State_.oscState3_, phase);
                }

                float carSample1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * window;
                carSample1 *= fabsf(carSample1); // vosim shape
                float car1 = carSample1 * env1Value * mix1V;
                float carSample2 = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * window;
                carSample2 *= fabsf(carSample2); // vosim shape
                float car2 = carSample2 * env2Value * mix2V;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left;

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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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

                pfm3State_.oscState3_.frequency = freq4 * voiceIm3 + osc3FrequencyPlusMatrix;
                pfm3State_.oscState2_.frequency = freq4 * voiceIm2 + osc2FrequencyPlusMatrix;
                pfm3State_.oscState1_.frequency = freq4 * voiceIm1 + osc1FrequencyPlusMatrix;

                // sync slave oscs (sub-sample accurate)
                if (unlikely(isSync)) {
                    osc_[0].hardSyncToMaster(&pfm3State_.oscState1_, &osc_[3], &pfm3State_.oscState4_, phase);
                    osc_[1].hardSyncToMaster(&pfm3State_.oscState2_, &osc_[3], &pfm3State_.oscState4_, phase);
                    osc_[2].hardSyncToMaster(&pfm3State_.oscState3_, &osc_[3], &pfm3State_.oscState4_, phase);
                }

                float carSample3 = osc_[2].getNextSample(&pfm3State_.oscState3_, waveTable3) * window;
                carSample3 *= fabsf(carSample3); // vosim shape
                float car3 = carSample3 * env3Value * mix3V;

                float carSample2 = osc_[1].getNextSample(&pfm3State_.oscState2_, waveTable2) * window;
                carSample2 *= fabsf(carSample2); // vosim shape
                float car2 = carSample2 * env2Value * mix2V;

                float carSample1 = osc_[0].getNextSample(&pfm3State_.oscState1_, waveTable1) * window;
                carSample1 *= fabsf(carSample1); // vosim shape
                float car1 = carSample1 * env1Value * mix1V;

                *sample++ = car1 * pfm3State_.pan1Right + car2 * pfm3State_.pan2Right + car3 * pfm3State_.pan3Right;
                *sample++ = car1 * pfm3State_.pan1Left + car2 * pfm3State_.pan2Left + car3 * pfm3State_.pan3Left;

                env1Value += env1Inc;
                env2Value += env2Inc;
                env3Value += env3Inc;
                env4Value += env4Inc;
            }

            if (unlikely(env_[0].isDead(&pfm3State_.envState1_) && env_[1].isDead(&pfm3State_.envState2_) &&
                         env_[2].isDead(&pfm3State_.envState3_))) {
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
            env1Inc = (envNextValue - env1Value) * inv32; // divide by 32
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
            float* osc2Values = osc_[1].getNextBlock(&pfm3State_.oscState2_);

            pfm3State_.oscState3_.frequency = pfm3State_.oscState3_.mainFrequencyPlusMatrix;
            float* osc3Values =
                osc_[2].getNextBlockWithFeedbackAndEnveloppe(&pfm3State_.oscState3_, pfm3State_.feedbackModulation, env3Value, env3Inc,
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

                pfm3State_.oscState1_.frequency = freq2 * voiceIm1 + freq3 * voiceIm2 + freq4 * voiceIm3 + osc1FrequencyPlusMatrix;

                // sync slave osc (sub-sample accurate)
                if (unlikely(isSync)) {
                    osc_[0].hardSyncToMaster(&pfm3State_.oscState1_, &osc_[3], &pfm3State_.oscState4_, phase);
                }

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

        float* pans[] = {&pfm3Params().engineMix1.panOsc1, &pfm3Params().engineMix1.panOsc2, &pfm3Params().engineMix2.panOsc3,
                         &pfm3Params().engineMix2.panOsc4, &pfm3Params().engineMix3.panOsc5, &pfm3Params().engineMix3.panOsc6};

        int currentAlgo = (int)pfm3Params().engine1.algo;
        float algoNumberOfMix = algoInformation[currentAlgo].mix;
        float numberOfCarrierOp = timbre_->numberOfVoices_ * algoNumberOfMix;
        float opPan = -pfm3Params().engine2.unisonSpread;
        float opPanInc = 2.0f / numberOfCarrierOp * pfm3Params().engine2.unisonSpread;

        if (likely(timbre_->voices_[timbre_->voiceNumber_[0]]->isPlaying())) {
            for (int vv = 0; vv < (int)timbre_->numberOfVoices_; vv++) {
                int v = timbre_->voiceNumber_[vv];
                if (unlikely(v < 0)) {
                    continue;
                }
                bool otherSide = false;
                for (int op = 0; op < 6; op++) {
                    if (algoOpInformation[currentAlgo][op] == 1) {
                        if (otherSide) {
                            *pans[op] = -opPan;
                        } else {
                            *pans[op] = opPan;
                        }
                        opPan += opPanInc;
                        otherSide = !otherSide;
                    }
                }
                renderVoice(timbre_->voices_[v]);

                if (vv > 0) {
                    // We accumulate in the first voice buffer
                    float* source = timbre_->voices_[v]->getSampleBlock();
                    float* voice0SampleBlock = timbre_->voices_[timbre_->voiceNumber_[0]]->getSampleBlock();
                    for (int s = 0; s < BLOCK_SIZE; s++) {
                        *voice0SampleBlock++ += *source++;
                        *voice0SampleBlock++ += *source++;
                    }
                }
                numberOfPlayingVoices++;
            }
            Voice* voice0 = timbre_->voices_[timbre_->voiceNumber_[0]];
            Pfm3VoiceState& state0 = pfm3State(voice0);
            voice0->fxAfterBlock(state0.matrix.getDestination(FILTER1_PARAM1), state0.matrix.getDestination(FILTER1_PARAM2),
                                 state0.matrix.getDestination(FILTER1_AMP));

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
        for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
            int v = timbre_->voiceNumber_[k];
            if (likely(timbre_->voices_[v]->isPlaying())) {
                renderVoice(timbre_->voices_[v]);
                Voice* voice = timbre_->voices_[v];
                Pfm3VoiceState& state = pfm3State(voice);
                voice->fxAfterBlock(state.matrix.getDestination(FILTER1_PARAM1), state.matrix.getDestination(FILTER1_PARAM2),
                                    state.matrix.getDestination(FILTER1_AMP));
                numberOfPlayingVoices++;
            } else {
                timbre_->voices_[v]->emptyBuffer();
            }
        }
    }

    return numberOfPlayingVoices;
}

void Preenfm3Engine::prepareBlock() {
    for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
        prepareMatrixForNewBlock(timbre_->voices_[timbre_->voiceNumber_[k]]);
    }
}

void Preenfm3Engine::glide() {
    if (unlikely(pfm3Params().engine1.playMode == PLAY_MODE_UNISON)) {
        for (int vv = 0; vv < (int)timbre_->numberOfVoices_; vv++) {
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
    if ((int)timbre_->numberOfVoices_ > 0) {
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

        float* sp = timbre_->sampleBlock_;
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
    for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
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
    for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
        pfm3State(timbre_->voices_[timbre_->voiceNumber_[k]]).matrix.resetDestination(oldValue);
    }
}

void Preenfm3Engine::setMatrixPolyAfterTouch(uint8_t note, float value) {
    for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
        int voiceIndex = timbre_->voiceNumber_[k];
        Voice* voice = timbre_->voices_[voiceIndex];
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
    if (voiceToUse == -1 ||
        (timbre_->voices_[voiceToUse]->getCurrentTimbre() != timbre_ && timbre_->voices_[voiceToUse]->getPendingTimbre() != timbre_)) {
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
    float noteFrequency = timbre_->mixerState_->instrumentState_[timbre_->timbreNumber_].scaleFrequencies[(int)note];

    if (timbre_->voices_[voiceToUse]->isPlaying()) {
        noteOnVoiceWithoutPop(timbre_->voices_[voiceToUse], note, noteFrequency, velocity, timbre_->voiceIndex_++);
    } else {
        noteOnVoice(timbre_->voices_[voiceToUse], note, noteFrequency, velocity, timbre_->voiceIndex_++);
    }
}

void Preenfm3Engine::noteOffMpe(uint8_t channel, uint8_t note, uint8_t velocity) {
    (void)velocity;

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

void Preenfm3Engine::paramChanged(int index, float oldValue, float newValue) {
    Pfm3Param id = pfm3FlatToParam((uint16_t)index);
    if ((uint16_t)id != PFM3_PARAM_NONE) {
        paramChanged(id, oldValue, newValue);
        return;
    }
    // Shared effect gain still needs a context refresh.
    CommonParamId cid = commonFlatToParam((uint16_t)index);
    if (cid != COMMON_PARAM_NONE && commonBlockOf(cid) == (uint8_t)CommonBlock::Effect1 && commonFieldOf(cid) == 3) {
        refreshContext();
    }
}

void Preenfm3Engine::engineParamChanged(int paramId, int context, float oldValue, float newValue) {
    uint16_t flat = (context == 0) ? pfm3FlatIndex((Pfm3Param)paramId) : commonFlatIndex((CommonParamId)paramId);
    if (flat != 0xFFFF) {
        timbre_->setParamValue(flat, oldValue, newValue);
    }
}

void Preenfm3Engine::paramChanged(Pfm3Param id, float oldValue, float newValue) {
    const uint8_t block = pfm3ParamBlockOf(id);
    const uint8_t element = pfm3ParamElementOf(id);
    const uint8_t field = pfm3ParamFieldOf(id);

    switch ((Pfm3Block)block) {
    case Pfm3Block::EnvTime:
    case Pfm3Block::EnvLevel:
        env_[element].reloadADSR(field);
        break;
    case Pfm3Block::EnvCurve:
        env_[element].applyCurves();
        break;
    case Pfm3Block::Matrix:
        verifyLfoUsed(field, oldValue, newValue);
        if (field == 2 || field == 3) { // Dest1 / Dest2
            resetMatrixDestination(oldValue);
        }
        break;
    case Pfm3Block::LfoOsc:
    case Pfm3Block::LfoEnv1:
    case Pfm3Block::LfoEnv2:
    case Pfm3Block::LfoSeq: {
        uint16_t flat = pfm3FlatIndex(id);
        lfoValueChange(flat / PFM3_PARAMS_PER_ROW, flat % PFM3_PARAMS_PER_ROW, newValue);
        break;
    }
    case Pfm3Block::Performance:
        setMatrixSource((SourceEnum)(MATRIX_SOURCE_CC1 + field), newValue);
        break;
    case Pfm3Block::MidiCurve:
        updateMidiNoteScale(element);
        break;
    case Pfm3Block::Engine1:
        if (field == 0) { // Algo
            refreshContext();
        }
        break;
    case Pfm3Block::OscMix:
        if (field == 1) { // Pan
            refreshContext();
        }
        break;
    default:
        break;
    }
}

void Preenfm3Engine::setParamValue(int index, float value) {
    if (index < Pfm3PresetLayout::sharedBlockStart) {
        ((float*)&pfm3Params())[index] = value;
    } else if (index >= Pfm3PresetLayout::pfm3BlockBStart && index < Pfm3PresetLayout::effect2Start) {
        ((float*)&pfm3Params())[index - Pfm3PresetLayout::sharedGap] = value;
    }
}

bool Preenfm3Engine::isUnisonMode() const {
    return pfm3Params().engine1.playMode == PLAY_MODE_UNISON;
}

void Preenfm3Engine::setStepSequencerSteps(int which, const StepSequencerSteps& steps) {
    if (which == 0) {
        pfm3Params().lfoSteps1 = steps;
    } else {
        pfm3Params().lfoSteps2 = steps;
    }
}

void Preenfm3Engine::prepareFxMatrix(Voice* voice) {
    if (!voice->isPlaying()) {
        pfm3State(voice).matrix.computeAllDestinations();
    }
}

void Preenfm3Engine::updateVoiceMatrix(Voice* voice, int note, int velocity) {
    int voiceIndex = 0;
    for (int i = 0; i < MAX_NUMBER_OF_VOICES; i++) {
        if (timbre_->voices_[i] == voice) {
            voiceIndex = i;
            break;
        }
    }
    preenNoteOnUpdateMatrix(voiceIndex, note, velocity);
}

void Preenfm3Engine::lfoValueChange(int paramId, int field, float newValue) {
    for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
        lfoValueChangeOnVoice(timbre_->voices_[timbre_->voiceNumber_[k]], (Pfm3Param)paramId, field, newValue);
    }
}

bool Preenfm3Engine::isLfoUsed(int lfo) {
    return lfoUSed_[lfo] > 0;
}

float Preenfm3Engine::getMatrixDestination(int destination) {
    if ((int)timbre_->numberOfVoices_ > 0) {
        return pfm3State(timbre_->voices_[timbre_->lastPlayedNote_]).matrix.getDestination((DestinationEnum)destination);
    }
    return 0.0f;
}

void Preenfm3Engine::verifyLfoUsed(int field, float oldValue, float newValue) {
    // No need to recompute
    if (timbre_->numberOfVoices_ == 0.0f) {
        return;
    }
    // We used it and still use it
    if (field == 1 && oldValue != 0.0f && newValue != 0.0f) { // Mul
        return;
    }

    for (int lfo = 0; lfo < NUMBER_OF_LFO; lfo++) {
        lfoUSed_[lfo] = 0;
    }

    MatrixRowParams* matrixRows = &pfm3Params().matrixRowState1;

    for (int r = 0; r < MATRIX_SIZE; r++) {
        if ((matrixRows[r].source >= MATRIX_SOURCE_LFO1 && matrixRows[r].source <= MATRIX_SOURCE_LFOSEQ2) && matrixRows[r].mul != 0.0f &&
            (matrixRows[r].dest1 != 0.0f || matrixRows[r].dest2 != 0.0f)) {
            lfoUSed_[(int)matrixRows[r].source - MATRIX_SOURCE_LFO1]++;
        }

        // Check if we have a Mtx* that would require LFO even if mul is 0
        if (matrixRows[r].mul != 0.0f && matrixRows[r].source != 0.0f) {
            if (matrixRows[r].dest1 >= MTX1_MUL && matrixRows[r].dest1 <= MTX4_MUL) {
                int index = matrixRows[r].dest1 - MTX1_MUL;
                if (matrixRows[index].source >= MATRIX_SOURCE_LFO1 && matrixRows[index].source <= MATRIX_SOURCE_LFOSEQ2) {
                    lfoUSed_[(int)matrixRows[index].source - MATRIX_SOURCE_LFO1]++;
                }
            }
            // same test for dest2
            if (matrixRows[r].dest2 >= MTX1_MUL && matrixRows[r].dest2 <= MTX4_MUL) {
                int index = matrixRows[r].dest2 - MTX1_MUL;
                if (matrixRows[index].source >= MATRIX_SOURCE_LFO1 && matrixRows[index].source <= MATRIX_SOURCE_LFOSEQ2) {
                    lfoUSed_[(int)matrixRows[index].source - MATRIX_SOURCE_LFO1]++;
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

uint32_t Preenfm3Engine::getStorageBlob(uint8_t* out, uint32_t maxSize, const TimbreSharedParams& shared) {
    serializePfm3Preset(pfm3PresetTmp);
    pfm3ParamsToFlash(&pfm3PresetTmp, (FlashSynthParams*)out, true);
    return sizeof(FlashSynthParams);
}

bool Preenfm3Engine::loadStorageBlob(const uint8_t* in, uint32_t size, uint32_t magic) {
    if (magic == PRESET_VERSION2) {
        memcpy(&pfm3PresetTmp, in, sizeof(Pfm3Preset));
    } else {
        pfm3FlashToParams((const FlashSynthParams*)in, &pfm3PresetTmp, true);
    }
    applyPfm3Preset(pfm3PresetTmp);
    return true;
}

void Preenfm3Engine::loadHexterPatch(const uint8_t* packedPatch) {
    hexter_->loadHexterPatch(const_cast<uint8_t*>(packedPatch), getPfm3Preset());
    deserializePfm3Preset(*getPfm3Preset());
}

void Preenfm3Engine::loadNewPreset() {
    applyPfm3Preset(newPresetParams);
}

int Preenfm3Engine::getStepSelection(int which) const {
    return (which == 0 || which == 1) ? stepSelect_[which] : 0;
}

void Preenfm3Engine::setStepSelection(int which, int step) {
    if (which == 0 || which == 1) {
        stepSelect_[which] = step;
    }
}

static int getRandomInt(int max) {
    uint32_t rnd;
    HAL_RNG_GenerateRandomNumber(&hrng, &rnd);
    return rnd % max;
}

static float getRandomFloat(float min, float max) {
    uint32_t rnd;
    HAL_RNG_GenerateRandomNumber(&hrng, &rnd);
    float f = ((float)(rnd % 100000)) / 100000.0f;
    return f * (max - min) + min;
}

static float getRandomShape(int operatorRandom) {
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

static float getRandomFrequencyType(int operatorRandom) {
    int freqType = getRandomInt(32);
    if (freqType > 1) { // Keyboard 5 times out of 6
        freqType = 0;
    }
    return freqType;
}

static float getRandomFrequency(int operatorRandom) {
    float random1Frequency[] = {.5f, 1.0f, 2.0f, 4.0f};
    float random2Frequency[] = {.25, .5f, 1.0f, 1.5, 2.0f, 3.0f, 4.0f};
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

static float getFineTune(int operatorRandom) {
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

void Preenfm3Engine::randomizePreset(int operatorRandom, int envelopeTypeRandom, int imRandom, int modulationRandom) {
    Pfm3Preset* preset = getPfm3Preset();

    // general

    preset->engine1.velocity = 8;

    if (operatorRandom > 0) {
        preset->engineMix1.mixOsc1 = getRandomFloat(0.6f, 1.0f);
        preset->engineMix1.mixOsc2 = getRandomFloat(0.6f, 1.0f);
        preset->engineMix2.mixOsc3 = getRandomFloat(0.6f, 1.0f);
        preset->engineMix2.mixOsc4 = getRandomFloat(0.6f, 1.0f);
        preset->engineMix3.mixOsc5 = getRandomFloat(0.6f, 1.0f);
        preset->engineMix3.mixOsc6 = getRandomFloat(0.6f, 1.0f);

        preset->engineMix1.panOsc1 = 0.0;
        preset->engineMix1.panOsc2 = getRandomFloat(-0.3f, 0.3f);
        preset->engineMix2.panOsc3 = getRandomFloat(-0.3f, 0.3f);
        preset->engineMix2.panOsc4 = getRandomFloat(-0.5f, 0.5f);
        preset->engineMix3.panOsc5 = getRandomFloat(-0.5f, 0.5f);
        preset->engineMix3.panOsc6 = getRandomFloat(-0.7f, 0.7f);

        preset->engine1.algo = getRandomInt(ALGO_END);

        for (int o = 0; o < 6; o++) {
            struct OscillatorParams* currentOsc = &((struct OscillatorParams*)&preset->osc1)[o];
            currentOsc->shape = getRandomShape(operatorRandom);
            currentOsc->frequencyMul = getRandomFrequency(operatorRandom);
            currentOsc->frequencyType = getRandomFrequencyType(operatorRandom);
            currentOsc->detune = getFineTune(operatorRandom);
        }

        // FX
        preset->effect1.param1 = getRandomFloat(0.2f, 0.8f);
        preset->effect1.param2 = getRandomFloat(0.2f, 0.8f);
        int effect = getRandomInt(15);
        if (effect == 1 || effect > 6) {
            preset->effect1.param3 = 1.0;
            preset->effect1.type = 0;
        } else {
            preset->effect1.param3 = 0.6;
            preset->effect1.type = effect;
        }
    }

    for (int e = 0; e < 6; e++) {
        struct EnvelopeParamsA* enva = &((struct EnvelopeParamsA*)&preset->env1Time)[e * 2];
        struct EnvelopeParamsB* envb = &((struct EnvelopeParamsB*)&preset->env1Level)[e * 2];

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
        struct EngineIm1* im1 = (struct EngineIm1*)&preset->engineIm1;
        struct EngineIm2* im2 = (struct EngineIm2*)&preset->engineIm2;
        struct EngineIm3* im3 = (struct EngineIm3*)&preset->engineIm3;

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

        preset->matrixRowState1.source = MATRIX_SOURCE_LFO1;
        preset->matrixRowState2.source = MATRIX_SOURCE_LFO1;
        preset->matrixRowState3.source = MATRIX_SOURCE_LFO2;
        preset->matrixRowState4.source = MATRIX_SOURCE_LFO2;
        preset->matrixRowState5.source = MATRIX_SOURCE_LFO3;
        preset->matrixRowState6.source = MATRIX_SOURCE_LFOENV1;
        preset->matrixRowState7.source = MATRIX_SOURCE_LFOENV2;
        preset->matrixRowState8.source = MATRIX_SOURCE_LFOSEQ1;
        preset->matrixRowState9.source = MATRIX_SOURCE_LFOSEQ2;

        preset->matrixRowState10.source = MATRIX_SOURCE_MODWHEEL;
        preset->matrixRowState10.mul = 2.0f;
        preset->matrixRowState10.dest1 = INDEX_ALL_MODULATION;

        preset->matrixRowState11.source = MATRIX_SOURCE_PITCHBEND;
        preset->matrixRowState11.mul = 1.0f;
        preset->matrixRowState11.dest1 = ALL_OSC_FREQ;

        preset->matrixRowState12.source = MATRIX_SOURCE_AFTERTOUCH;
        preset->matrixRowState12.mul = 1.0f;
        preset->matrixRowState12.dest1 = INDEX_MODULATION1;

        for (int m = 1; m <= 9; m++) {
            struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*)&preset->matrixRowState1)[m - 1];
            matrixRow->mul = 0;
            matrixRow->dest1 = 0;
        }

        float dest[] = {INDEX_ALL_MODULATION, OSC1_FREQ, ALL_PAN, OSC2_FREQ, INDEX_MODULATION1, PAN_OSC1, INDEX_ALL_MODULATION};
        for (int i = 0; i < 2; i++) {
            struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*)&preset->matrixRowState1)[getRandomInt(10)];
            matrixRow->mul = getRandomFloat(0.2f, 3.0f);
            matrixRow->dest1 = dest[getRandomInt(7)];
        }

        if (modulationRandom >= 2) {
            for (int i = 0; i < 3; i++) {
                struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*)&preset->matrixRowState1)[getRandomInt(10)];
                matrixRow->mul = getRandomFloat(1.0f, 3.0f);
                matrixRow->dest1 = getRandomInt(DESTINATION_MAX);
            }
        }

        if (modulationRandom >= 3) {
            for (int i = 0; i < 6; i++) {
                struct MatrixRowParams* matrixRow = &((struct MatrixRowParams*)&preset->matrixRowState1)[getRandomInt(10)];
                float mm = getRandomFloat(2.0f, 5.0f);
                matrixRow->mul = getRandomFloat(-mm, mm);
                matrixRow->dest1 = getRandomInt(DESTINATION_MAX);
            }
        }

        for (int o = 0; o < 3; o++) {
            struct LfoParams* osc = &((struct LfoParams*)&preset->lfoOsc1)[o];
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
            struct EnvelopeLfoParams* env = &((struct EnvelopeLfoParams*)&preset->lfoEnv1)[e];
            env->attack = getRandomFloat(0.05f, 1.0f);
            env->decay = getRandomFloat(0.05f, 1.0f);
            env->sustain = getRandomFloat(0.05f, 1.0f);
            if (e == 0) {
                env->release = getRandomFloat(0.05f, 1.0f);
            } else {
                preset->lfoEnv2.loop = getRandomInt(2) + 1;
            }
        }

        int bpm = getRandomInt(120) + 60;
        for (int s = 0; s < 2; s++) {
            struct StepSequencerParams* stepSeq = &((struct StepSequencerParams*)&preset->lfoSeq1)[s];
            stepSeq->bpm = bpm;
            stepSeq->gate = getRandomFloat(0.25, 1);

            struct StepSequencerSteps* steps = &((struct StepSequencerSteps*)&preset->lfoSteps1)[s];
            for (int k = 0; k < 16; k++) {
                if ((k % 4) == 0) {
                    steps->steps[k] = getRandomInt(5) + 11;
                } else {
                    steps->steps[k] = getRandomInt(10);
                }
            }
        }
    }

    deserializePfm3Preset(*getPfm3Preset());
}

void Preenfm3Engine::applyPfm3Preset(const Pfm3Preset& flat) {
    *pfm3Preset_ = flat;
    deserializePfm3Preset(flat);
}

void Preenfm3Engine::syncSharedParams(const TimbreSharedParams& shared) {
    pfm3Preset_->engineArp1 = shared.engineArp1;
    pfm3Preset_->engineArp2 = shared.engineArp2;
    pfm3Preset_->engineArpUserPatterns = shared.engineArpUserPatterns;
    pfm3Preset_->effect1 = shared.effect1;
    pfm3Preset_->effect2 = shared.effect2;
    memcpy(pfm3Preset_->presetName, shared.presetName, sizeof(pfm3Preset_->presetName));
}

int Preenfm3Engine::getSeqStepValue(int which, int step) const {
    return which == 0 ? pfm3Preset_->lfoSteps1.steps[step] : pfm3Preset_->lfoSteps2.steps[step];
}

void Preenfm3Engine::setSeqStepValue(int which, int step, int value) {
    if (which == 0) {
        pfm3Preset_->lfoSteps1.steps[step] = value;
    } else {
        pfm3Preset_->lfoSteps2.steps[step] = value;
    }
}

void Preenfm3Engine::serializePfm3Preset(struct Pfm3Preset& out) const {
    const Pfm3TimbreParams& pfm3 = pfm3Params();

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

    const TimbreSharedParams& shared = timbre_->sharedParams();
    out.engineArp1 = shared.engineArp1;
    out.engineArp2 = shared.engineArp2;
    out.engineArpUserPatterns = shared.engineArpUserPatterns;
    out.effect1 = shared.effect1;
    out.effect2 = shared.effect2;
    memcpy(out.presetName, shared.presetName, sizeof(out.presetName));
}

void Preenfm3Engine::deserializePfm3Preset(const struct Pfm3Preset& in) {
    Pfm3TimbreParams& pfm3 = pfm3Params();

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

    *pfm3Preset_ = in;
    TimbreSharedParams shared;
    shared.engineArp1 = in.engineArp1;
    shared.engineArp2 = in.engineArp2;
    shared.engineArpUserPatterns = in.engineArpUserPatterns;
    shared.effect1 = in.effect1;
    shared.effect2 = in.effect2;
    memcpy(shared.presetName, in.presetName, sizeof(shared.presetName));
    timbre_->setSharedParams(shared);
}
