/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef VOICE_POOL_H_
#define VOICE_POOL_H_

#include "Common.h"

class Voice;

/*
 * Global pool of the 16 voices. Tracks which timbre currently owns each
 * voice; the allocation policy itself lives in Timbre.
 */
class VoicePool {
public:
    VoicePool();

    void init();

    int findFree(Voice **voices);
    int findOldestReleased(Voice **voices) const;
    int findOldestPlaying(Voice **voices, int excludeTimbre) const;

    void claim(int voiceIndex, int timbre);
    void release(int voiceIndex);
    int owner(int voiceIndex) const {
        return owner_[voiceIndex];
    }

private:
    int8_t owner_[MAX_NUMBER_OF_VOICES];
    int8_t oneFreeVoice_;
};

#endif
