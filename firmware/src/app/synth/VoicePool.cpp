/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "VoicePool.h"
#include "Voice.h"

VoicePool::VoicePool() {
    init();
}

void VoicePool::init() {
    for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
        owner_[v] = -1;
    }
    oneFreeVoice_ = 0;
}

int VoicePool::findFree(Voice **voices) {
    if (likely(oneFreeVoice_ != -1)) {
        return oneFreeVoice_;
    }

    int idle = -1;
    for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
        if (owner_[v] == -1) {
            oneFreeVoice_ = (int8_t) v;
            return v;
        }
        if (idle == -1 && !voices[v]->isPlaying() && !voices[v]->isTransferPending()) {
            idle = v;
        }
    }
    return idle;
}

int VoicePool::findOldestReleased(Voice **voices) const {
    int best = -1;
    uint32_t bestIndex = UINT32_MAX;

    for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
        if (unlikely(owner_[v] == -1) || voices[v]->isTransferPending()) {
            continue;
        }
        if (unlikely(!voices[v]->isReleased())) {
            continue;
        }
        uint32_t index = voices[v]->getIndex();
        if (index < bestIndex) {
            bestIndex = index;
            best = v;
        }
    }

    return best;
}

int VoicePool::findOldestPlaying(Voice **voices, int excludeTimbre) const {
    int best = -1;
    uint32_t bestIndex = UINT32_MAX;

    for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
        if (unlikely(owner_[v] == -1) || owner_[v] == excludeTimbre || voices[v]->isTransferPending()) {
            continue;
        }
        if (unlikely(!voices[v]->isPlaying() || voices[v]->isReleased())) {
            continue;
        }
        uint32_t index = voices[v]->getIndex();
        if (index < bestIndex) {
            bestIndex = index;
            best = v;
        }
    }

    return best;
}

void VoicePool::claim(int voiceIndex, int timbre) {
    owner_[voiceIndex] = (int8_t) timbre;
    if (oneFreeVoice_ == voiceIndex) {
        oneFreeVoice_ = -1;
    }
}

void VoicePool::release(int voiceIndex) {
    owner_[voiceIndex] = -1;
    oneFreeVoice_ = (int8_t) voiceIndex;
}
