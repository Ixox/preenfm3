/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ByosEngine.h"

#include <string.h>

#include "PersistenceMagic.h"
#include "SynthState.h"
#include "Timbre.h"
#include "Voice.h"

ByosVoiceState ByosEngine::voiceStates_[MAX_NUMBER_OF_VOICES] __attribute__((section(".ram_d1")));

static float clampf(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

ByosEngine::ByosEngine() : timbre_(nullptr), synthState_(nullptr), timbreNumber_(0) {
}

void ByosEngine::init(Timbre* timbre, SynthState* synthState) {
    timbre_ = timbre;
    synthState_ = synthState;
    timbreNumber_ = timbre->getTimbreNumber();

    memset(oscParams_, 0, sizeof(oscParams_));
    oscParams_[0].frequencyType = OSC_FT_KEYBOARD;
    oscParams_[1].frequencyType = OSC_FT_KEYBOARD;

    params_.wave[0] = OSC_SHAPE_SQUARE;
    params_.wave[1] = OSC_SHAPE_SAW;
    params_.multiplier[0] = 1.0f;
    params_.multiplier[1] = 1.5f;
    params_.mix = 0.5f;
    params_.attack = 0.01f;
    params_.release = 0.3f;
    applyParams();

    osc_[0].init(synthState, &oscParams_[0], OSC1_FREQ);
    osc_[1].init(synthState, &oscParams_[1], OSC2_FREQ);

    memset(voiceStates_, 0, sizeof(voiceStates_));
    for (int slot = 0; slot < MAX_NUMBER_OF_VOICES; slot++) {
        voiceStates_[slot].envStage = BYOS_ENV_IDLE;
    }
}

void ByosEngine::applyParams() {
    oscParams_[0].shape = params_.wave[0];
    oscParams_[0].frequencyMul = params_.multiplier[0];
    oscParams_[1].shape = params_.wave[1];
    oscParams_[1].frequencyMul = params_.multiplier[1];
}

void ByosEngine::afterNewParamsLoad() {
    applyParams();
}

void ByosEngine::patchMemoryChanged() {
    applyParams();
}

uint8_t* ByosEngine::getPatchMemory() {
    return (uint8_t*)&params_;
}

void ByosEngine::bindVoice(Voice* voice) {
    voice->bindTimbre(timbre_);
    resetVoiceState(voice);
}

void ByosEngine::resetVoiceState(Voice* voice) {
    ByosVoiceState& state = voiceStates_[voice->slot()];
    state.envLevel = 0.0f;
    state.envInc = 0.0f;
    state.velocityGain = 0.0f;
    state.envStage = BYOS_ENV_IDLE;
}

void ByosEngine::startVoice(Voice* voice, int note, int velocity, uint32_t index, float noteFrequency) {
    voice->resetVoiceState();

    ByosVoiceState& state = voiceStates_[voice->slot()];
    state.envLevel = 0.0f;
    state.envStage = BYOS_ENV_ATTACK;
    state.velocityGain = (float)velocity * (1.0f / 127.0f);
    if (params_.attack <= 0.0f) {
        state.envLevel = 1.0f;
        state.envStage = BYOS_ENV_SUSTAIN;
        state.envInc = 0.0f;
    } else {
        state.envInc = 1.0f / (params_.attack * PREENFM_FREQUENCY);
    }

    osc_[0].newNote(&state.osc[0], noteFrequency, 0.0f);
    osc_[1].newNote(&state.osc[1], noteFrequency, 0.0f);

    voice->note_ = (char)note;
    voice->midiVelocity_ = (char)velocity;
    voice->noteFrequency_ = noteFrequency;
    voice->velocity_ = state.velocityGain;
    voice->index_ = index;
    voice->playing_ = true;
    voice->released_ = false;
    voice->isFullOfZero_ = false;
    voice->holdedByPedal_ = false;
}

void ByosEngine::releaseVoice(Voice* voice) {
    ByosVoiceState& state = voiceStates_[voice->slot()];
    if (params_.release <= 0.0f || state.envLevel <= 0.0f) {
        state.envLevel = 0.0f;
        state.envStage = BYOS_ENV_IDLE;
        state.envInc = 0.0f;
        voice->playing_ = false;
        voice->released_ = false;
        return;
    }
    state.envStage = BYOS_ENV_RELEASE;
    state.envInc = -state.envLevel / (params_.release * PREENFM_FREQUENCY);
    voice->released_ = true;
}

void ByosEngine::noteOn(char note, char velocity) {
    if (timbre_->maxVoices_ == 0) {
        return;
    }

    note &= 0x7f;

    int voiceToUse = timbre_->allocateVoice(note);
    if (voiceToUse == -1) {
        return;
    }
    timbre_->clearMpeMapping(voiceToUse);

    float noteFrequency = timbre_->mixerState_->instrumentState_[timbreNumber_].scaleFrequencies[(int)note];
    Voice* voice = timbre_->voices_[voiceToUse];
    startVoice(voice, note, velocity, timbre_->voiceIndex_++, noteFrequency);

    timbre_->lastPlayedNote_ = voiceToUse;
    if (note <= timbre_->lowerNote_ || timbre_->lowerNoteReleased_) {
        timbre_->lowerNote_ = note;
        timbre_->lowerNoteReleased_ = false;
        timbre_->lowerNoteFrequency_ = noteFrequency;
    }
}

void ByosEngine::noteOff(char note) {
    if (note == timbre_->lowerNote_) {
        timbre_->lowerNoteReleased_ = true;
    }

    for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
        Voice* voice = timbre_->voices_[timbre_->voiceNumber_[k]];
        if (!voice->isPlaying()) {
            continue;
        }
        if (voice->getNote() == note) {
            if (timbre_->holdPedal_) {
                voice->setHoldedByPedal(true);
            } else {
                releaseVoice(voice);
            }
        }
    }
}

void ByosEngine::killVoice(Voice* voice) {
    voice->resetVoiceState();
    resetVoiceState(voice);
    voice->playing_ = false;
    voice->released_ = false;
}

void ByosEngine::noteOffVoice(Voice* voice) {
    releaseVoice(voice);
}

void ByosEngine::noteOffVoiceQuick(Voice* voice) {
    killVoice(voice);
}

void ByosEngine::noteOnMpe(uint8_t channel, uint8_t note, uint8_t velocity) {
    (void)channel;
    noteOn(note, velocity);
}

void ByosEngine::noteOffMpe(uint8_t channel, uint8_t note, uint8_t velocity) {
    (void)channel;
    (void)velocity;
    noteOff(note);
}

void ByosEngine::renderVoice(Voice* voice) {
    if (!voice->isPlaying()) {
        voice->emptyBuffer();
        return;
    }

    ByosVoiceState& state = voiceStates_[voice->slot()];
    const struct WaveTable* waveTable0 = osc_[0].getWaveTable();
    const struct WaveTable* waveTable1 = osc_[1].getWaveTable();
    float* out = voice->getSampleBlock();

    for (int k = 0; k < BLOCK_SIZE; k++) {
        if (state.envStage == BYOS_ENV_ATTACK) {
            state.envLevel += state.envInc;
            if (state.envLevel >= 1.0f) {
                state.envLevel = 1.0f;
                state.envStage = BYOS_ENV_SUSTAIN;
            }
        } else if (state.envStage == BYOS_ENV_RELEASE) {
            state.envLevel += state.envInc;
            if (state.envLevel <= 0.0f) {
                state.envLevel = 0.0f;
                state.envStage = BYOS_ENV_IDLE;
                voice->playing_ = false;
                voice->released_ = false;
            }
        }

        float sample = 0.0f;
        if (voice->playing_) {
            float op1 = osc_[0].getNextSample(&state.osc[0], waveTable0);
            float op2 = osc_[1].getNextSample(&state.osc[1], waveTable1);
            sample = (op1 * (1.0f - params_.mix) + op2 * params_.mix) * state.envLevel * state.velocityGain;
        }
        *out++ = sample;
        *out++ = sample;
    }
}

uint8_t ByosEngine::voicesNextBlock() {
    uint8_t playing = 0;
    for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
        Voice* voice = timbre_->voices_[timbre_->voiceNumber_[k]];
        bool wasPlaying = voice->isPlaying();
        renderVoice(voice);
        if (wasPlaying) {
            voice->fxAfterBlock(0.0f, 0.0f, 0.0f);
        }
        if (voice->isPlaying()) {
            playing++;
        }
    }
    return playing;
}

void ByosEngine::engineParamChanged(int paramId, int context, float oldValue, float newValue) {
    (void)oldValue;

    switch (paramId) {
    case BYOS_PARAM_WAVE:
        if (context < 0 || context > 1) {
            return;
        }
        params_.wave[context] = clampf(newValue, 0.0f, (float)(OSC_SHAPE_LAST - 1));
        oscParams_[context].shape = params_.wave[context];
        break;
    case BYOS_PARAM_MULTIPLIER:
        if (context < 0 || context > 1) {
            return;
        }
        params_.multiplier[context] = clampf(newValue, 0.25f, 4.0f);
        oscParams_[context].frequencyMul = params_.multiplier[context];
        break;
    case BYOS_PARAM_MIX:
        params_.mix = clampf(newValue, 0.0f, 1.0f);
        break;
    case BYOS_PARAM_ATTACK:
        params_.attack = clampf(newValue, 0.0f, 5.0f);
        break;
    case BYOS_PARAM_RELEASE:
        params_.release = clampf(newValue, 0.0f, 5.0f);
        break;
    default:
        break;
    }
}

float ByosEngine::getParamValue(int paramId, int context) const {
    switch (paramId) {
    case BYOS_PARAM_WAVE:
        return (context >= 0 && context < 2) ? params_.wave[context] : 0.0f;
    case BYOS_PARAM_MULTIPLIER:
        return (context >= 0 && context < 2) ? params_.multiplier[context] : 0.0f;
    case BYOS_PARAM_MIX:
        return params_.mix;
    case BYOS_PARAM_ATTACK:
        return params_.attack;
    case BYOS_PARAM_RELEASE:
        return params_.release;
    default:
        return 0.0f;
    }
}

bool ByosEngine::midiData(int timbre, const MidiData& data) {
    (void)timbre;
    if (data.type != MIDI_DATA_CC) {
        return false;
    }

    uint8_t cc = data.cc;
    uint8_t value = data.ccValue;
    int paramId;
    int context = 0;
    float newValue;

    switch (cc) {
    case BYOS_CC_OP1_WAVE:
        paramId = BYOS_PARAM_WAVE;
        newValue = (float)((value * (OSC_SHAPE_LAST - 1) + 63) / 127);
        break;
    case BYOS_CC_OP1_MULTIPLIER:
        paramId = BYOS_PARAM_MULTIPLIER;
        newValue = 0.25f + (float)((value * 15 + 63) / 127) * 0.25f;
        break;
    case BYOS_CC_OP2_WAVE:
        paramId = BYOS_PARAM_WAVE;
        context = 1;
        newValue = (float)((value * (OSC_SHAPE_LAST - 1) + 63) / 127);
        break;
    case BYOS_CC_OP2_MULTIPLIER:
        paramId = BYOS_PARAM_MULTIPLIER;
        context = 1;
        newValue = 0.25f + (float)((value * 15 + 63) / 127) * 0.25f;
        break;
    case BYOS_CC_MIX:
        paramId = BYOS_PARAM_MIX;
        newValue = (float)value * (1.0f / 127.0f);
        break;
    case BYOS_CC_ATTACK:
        paramId = BYOS_PARAM_ATTACK;
        newValue = (float)value * (5.0f / 127.0f);
        break;
    case BYOS_CC_RELEASE:
        paramId = BYOS_PARAM_RELEASE;
        newValue = (float)value * (5.0f / 127.0f);
        break;
    default:
        return false;
    }

    float oldValue = getParamValue(paramId, context);
    if (oldValue != newValue) {
        synthState_->propagateNewEngineParamValueFromExternal(timbreNumber_, paramId, context, oldValue, newValue);
    }
    return true;
}

bool ByosEngine::midiParamFeedback(int paramId, int context, float newValue, struct MidiParamMessage& out) const {
    int cc;
    int ccValue;

    switch (paramId) {
    case BYOS_PARAM_WAVE:
        cc = context == 0 ? BYOS_CC_OP1_WAVE : BYOS_CC_OP2_WAVE;
        ccValue = (int)(newValue * 127.0f / (OSC_SHAPE_LAST - 1) + 0.5f);
        break;
    case BYOS_PARAM_MULTIPLIER:
        cc = context == 0 ? BYOS_CC_OP1_MULTIPLIER : BYOS_CC_OP2_MULTIPLIER;
        ccValue = (int)((newValue - 0.25f) / 0.25f * 127.0f / 15.0f + 0.5f);
        break;
    case BYOS_PARAM_MIX:
        cc = BYOS_CC_MIX;
        ccValue = (int)(newValue * 127.0f + 0.5f);
        break;
    case BYOS_PARAM_ATTACK:
        cc = BYOS_CC_ATTACK;
        ccValue = (int)(newValue * 127.0f / 5.0f + 0.5f);
        break;
    case BYOS_PARAM_RELEASE:
        cc = BYOS_CC_RELEASE;
        ccValue = (int)(newValue * 127.0f / 5.0f + 0.5f);
        break;
    default:
        return false;
    }

    if (ccValue < 0) {
        ccValue = 0;
    } else if (ccValue > 127) {
        ccValue = 127;
    }

    out.isNrpn = false;
    out.cc = (uint8_t)cc;
    out.ccValue = (uint8_t)ccValue;
    return true;
}

uint32_t ByosEngine::getStorageMagic() const {
    return PRESET_BYOS;
}

int ByosEngine::getStorageNameOffset(uint32_t magic) const {
    return magic == PRESET_BYOS ? (int)sizeof(ByosParams) : -1;
}

const char* ByosEngine::getStorageDefaultName() const {
    return "BYOS";
}

uint32_t ByosEngine::getStorageBlob(uint8_t* out, uint32_t maxSize, const TimbreSharedParams& shared) {
    const uint32_t nameOffset = sizeof(ByosParams);
    const uint32_t sharedOffset = nameOffset + 16;
    if (maxSize < sharedOffset + sizeof(TimbreSharedParams)) {
        return 0;
    }
    memcpy(out, &params_, sizeof(ByosParams));
    memcpy(out + nameOffset, shared.presetName, 13);
    memcpy(out + sharedOffset, &shared, sizeof(TimbreSharedParams));
    return sharedOffset + sizeof(TimbreSharedParams);
}

bool ByosEngine::loadStorageBlob(const uint8_t* in, uint32_t size, uint32_t magic) {
    (void)magic;

    const uint32_t nameOffset = sizeof(ByosParams);
    const uint32_t sharedOffset = nameOffset + 16;
    if (size < sharedOffset + sizeof(TimbreSharedParams)) {
        return false;
    }

    memcpy(&params_, in, sizeof(ByosParams));
    TimbreSharedParams shared;
    memcpy(&shared, in + sharedOffset, sizeof(shared));
    timbre_->setSharedParams(shared);
    applyParams();
    return true;
}
