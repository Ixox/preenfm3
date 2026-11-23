/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Drum909Engine.h"

#include <string.h>

#include "PersistenceMagic.h"
#include "Timbre.h"
#include "Voice.h"

extern "C" {
#include "drum909.h"
}

static_assert(DRUM909_DRUM_COUNT == DR_NUM, "Drum909Params drum count out of sync");
static_assert(DRUM909_MAX_PARAMS == DR_MAX_PARAMS, "Drum909Params param count out of sync");
// ALIGNED_PATCH_SIZE (1024) minus the 5-byte magic.
static_assert(sizeof(Drum909Params) + 16 + sizeof(TimbreSharedParams) <= 1024 - 5, "Drum909 storage blob exceeds the preset payload");

// One kit per timbre, off DTCMRAM (which is tight).
static drum909_t drum909Kits_[NUMBER_OF_TIMBRES] __attribute__((section(".ram_d1")));

// Standard GM drum map; unknown notes are ignored.
static int gmNoteToDrum(int note) {
    switch (note) {
    case 35:
    case 36:
        return DR_BD;
    case 38:
    case 40:
        return DR_SD;
    case 41:
    case 43:
        return DR_LT;
    case 45:
    case 47:
        return DR_MT;
    case 48:
    case 50:
        return DR_HT;
    case 37:
        return DR_RS;
    case 39:
        return DR_CP;
    case 42:
    case 44:
        return DR_CH;
    case 46:
        return DR_OH;
    case 49:
    case 57:
        return DR_CR;
    case 51:
    case 59:
        return DR_RD;
    default:
        return -1;
    }
}

Drum909Engine::Drum909Engine() : timbre_(nullptr), synthState_(nullptr), timbreNumber_(0), carrier_(nullptr), kit_(nullptr) {
    memset(&params_, 0, sizeof(params_));
    resetResampler();
}

void Drum909Engine::init(Timbre* timbre, SynthState* synthState) {
    timbre_ = timbre;
    synthState_ = synthState;
    timbreNumber_ = timbre->getTimbreNumber();
    carrier_ = nullptr;
    kit_ = &drum909Kits_[timbreNumber_];

    memset(&params_, 0, sizeof(params_));
    for (int voice = 0; voice <= DR_KIT; voice++) {
        const int count = drum909_nparams(voice);
        for (int i = 0; i < count; i++) {
            params_.pots[voice][i] = drum909_param(voice, i)->def;
        }
    }

    drum909_init(kit_);
    applyParams();
    resetResampler();
}

void Drum909Engine::resetResampler() {
    ringHead_ = 0;
    ringTail_ = 0;
    phase_ = 0.0f;
}

void Drum909Engine::applyParams() {
    if (kit_ == nullptr) {
        return;
    }
    for (int voice = 0; voice <= DR_KIT; voice++) {
        const int count = drum909_nparams(voice);
        for (int i = 0; i < count; i++) {
            drum909_set(kit_, voice, i, params_.pots[voice][i]);
        }
    }
    drum909_pan_settle(kit_);
}

void Drum909Engine::afterNewParamsLoad() {
    applyParams();
}

void Drum909Engine::bindVoice(Voice* voice) {
    const bool reClaim = carrier_ != nullptr;
    carrier_ = voice;
    voice->bindTimbre(timbre_);
    voice->playing_ = true;
    voice->released_ = false;

    // A re-claim after a steal starts from a clean kit and resampler.
    if (reClaim) {
        drum909_init(kit_);
        applyParams();
        resetResampler();
    }
}

void Drum909Engine::noteOn(char note, char velocity) {
    const int drum = gmNoteToDrum(note & 0x7f);
    if (drum < 0 || timbre_->maxVoices_ == 0) {
        return;
    }

    const int voiceIndex = timbre_->allocateVoice(note & 0x7f);
    if (voiceIndex == -1) {
        return;
    }

    carrier_ = timbre_->voices_[voiceIndex];
    carrier_->note_ = (char)(note & 0x7f);
    carrier_->playing_ = true;
    carrier_->released_ = false;

    drum909_trigger(kit_, drum, (float)(velocity & 0x7f) / 127.0f);
}

void Drum909Engine::noteOff(char note) {
    (void)note;
}

void Drum909Engine::killVoice(Voice* voice) {
    if (voice == carrier_) {
        carrier_ = nullptr;
        drum909_init(kit_);
        applyParams();
        resetResampler();
    }
    voice->playing_ = false;
    voice->released_ = false;
}

void Drum909Engine::noteOffVoice(Voice* voice) {
    // One-shots: a note-off does not stop the drum.
    (void)voice;
}

void Drum909Engine::noteOffVoiceQuick(Voice* voice) {
    (void)voice;
}

uint8_t Drum909Engine::voicesNextBlock() {
    if (carrier_ == nullptr) {
        return 0;
    }

    const float step = 44100.0f / (float)PREENFM_FREQUENCY;
    const bool wasActive = drum909_active(kit_) != 0;

    // Make sure the ring holds the two samples the last output frame needs.
    const float lastOffset = phase_ + (float)(BLOCK_SIZE - 1) * step;
    while ((float)(ringHead_ - ringTail_) <= lastOffset + 1.0f) {
        float tmpL[kResampleChunk];
        float tmpR[kResampleChunk];
        memset(tmpL, 0, sizeof(tmpL));
        memset(tmpR, 0, sizeof(tmpR));
        drum909_render_st(kit_, tmpL, tmpR, nullptr, nullptr, kResampleChunk);
        for (int i = 0; i < kResampleChunk; i++) {
            ringL_[ringHead_ & kResampleMask] = tmpL[i];
            ringR_[ringHead_ & kResampleMask] = tmpR[i];
            ringHead_++;
        }
    }

    float* out = carrier_->getSampleBlock();
    for (int o = 0; o < BLOCK_SIZE; o++) {
        const float p = phase_ + (float)o * step;
        const uint32_t i = (uint32_t)p;
        const float f = p - (float)i;
        const float l0 = ringL_[(ringTail_ + i) & kResampleMask];
        const float l1 = ringL_[(ringTail_ + i + 1) & kResampleMask];
        const float r0 = ringR_[(ringTail_ + i) & kResampleMask];
        const float r1 = ringR_[(ringTail_ + i + 1) & kResampleMask];
        *out++ = l0 + (l1 - l0) * f;
        *out++ = r0 + (r1 - r0) * f;
    }

    const float advance = phase_ + (float)BLOCK_SIZE * step;
    const uint32_t whole = (uint32_t)advance;
    phase_ = advance - (float)whole;
    ringTail_ += whole;

    const bool isActive = drum909_active(kit_) != 0;
    carrier_->fxAfterBlock(0.0f, 0.0f, 0.0f);
    if (!wasActive && !isActive) {
        carrier_->emptyBuffer();
    }
    return (wasActive || isActive) ? 1 : 0;
}

void Drum909Engine::noteOnMpe(uint8_t channel, uint8_t note, uint8_t velocity) {
    (void)channel;
    (void)note;
    (void)velocity;
}

void Drum909Engine::noteOffMpe(uint8_t channel, uint8_t note, uint8_t velocity) {
    (void)channel;
    (void)note;
    (void)velocity;
}

void Drum909Engine::engineParamChanged(int paramId, int context, float oldValue, float newValue) {
    (void)oldValue;
    if (context < 0 || context > DR_KIT || paramId < 0 || paramId >= drum909_nparams(context)) {
        return;
    }
    const x0x_param_t* desc = drum909_param(context, paramId);
    int value = (int)(newValue + 0.5f);
    if (value < 0) {
        value = 0;
    } else if (value > (int)desc->max) {
        value = (int)desc->max;
    }
    params_.pots[context][paramId] = (uint8_t)value;
    drum909_set(kit_, context, paramId, value);
}

uint8_t* Drum909Engine::getPatchMemory() {
    return (uint8_t*)&params_;
}

void Drum909Engine::patchMemoryChanged() {
}

uint32_t Drum909Engine::getStorageMagic() const {
    return PRESET_DRUM909;
}

int Drum909Engine::getStorageNameOffset(uint32_t magic) const {
    return magic == PRESET_DRUM909 ? (int)sizeof(Drum909Params) : -1;
}

const char* Drum909Engine::getStorageDefaultName() const {
    return "909";
}

uint32_t Drum909Engine::getStorageBlob(uint8_t* out, uint32_t maxSize, const TimbreSharedParams& shared) {
    const uint32_t nameOffset = sizeof(Drum909Params);
    const uint32_t sharedOffset = nameOffset + 16;
    if (maxSize < sharedOffset + sizeof(TimbreSharedParams)) {
        return 0;
    }
    memcpy(out, &params_, sizeof(Drum909Params));
    memcpy(out + nameOffset, shared.presetName, 13);
    memcpy(out + sharedOffset, &shared, sizeof(TimbreSharedParams));
    return sharedOffset + sizeof(TimbreSharedParams);
}

bool Drum909Engine::loadStorageBlob(const uint8_t* in, uint32_t size, uint32_t magic) {
    if (magic != PRESET_DRUM909) {
        return false;
    }

    const uint32_t nameOffset = sizeof(Drum909Params);
    const uint32_t sharedOffset = nameOffset + 16;
    if (size < sharedOffset + sizeof(TimbreSharedParams)) {
        return false;
    }

    memcpy(&params_, in, sizeof(Drum909Params));
    for (int voice = 0; voice <= DR_KIT; voice++) {
        const int count = drum909_nparams(voice);
        for (int i = 0; i < count; i++) {
            const uint8_t max = drum909_param(voice, i)->max;
            if (params_.pots[voice][i] > max) {
                params_.pots[voice][i] = max;
            }
        }
    }

    TimbreSharedParams shared;
    memcpy(&shared, in + sharedOffset, sizeof(shared));
    timbre_->setSharedParams(shared);
    afterNewParamsLoad();
    return true;
}
