/*
 * Copyright 2013 Xavier Hosxe
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

#ifndef VOICE_H_
#define VOICE_H_

#include "Common.h"

class Timbre;

class Voice {
    friend class Timbre;
    friend class Preenfm3Engine;
    friend class Dx7SynthEngine;
    friend class ByosEngine;

public:
    Voice();
    ~Voice(void);

    void init(int slot);
    void emptyBuffer();
    void fxAfterBlock(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterAmp);
    void setNewEffectParam(int encoder);


    bool isReleased() {
        return this->released_;
    }
    bool isPlaying() {
        return this->playing_;
    }
    uint32_t getIndex() {
        return this->index_;
    }
    uint8_t slot() const {
        return slot_;
    }
    char getNote() {
        return this->note_;
    }
    char getMidiVelocity() {
        return this->midiVelocity_;
    }
    float getNoteFrequency() {
        return this->noteFrequency_;
    }
    bool isHoldedByPedal() {
        return this->holdedByPedal_;
    }
    void setHoldedByPedal(bool holded) {
        this->holdedByPedal_ = holded;
    }
    void bindTimbre(Timbre *timbre) {
        currentTimbre_ = timbre;
    }
    void setPendingTimbre(Timbre *timbre) {
        pendingTimbre_ = timbre;
    }
    Timbre* getPendingTimbre() {
        return pendingTimbre_;
    }
    bool isTransferPending() {
        return pendingTimbre_ != nullptr;
    }
    bool isTransferReady() {
        return transferReady_;
    }

            
    Timbre* getCurrentTimbre() {
        return currentTimbre_;
    }


    void resetVoiceState();

    float* const getSampleBlock() {
        return sampleBlock_;
    }


private:
    // private function for BP filter
    void recomputeBPValues(float q, float fSquare);


    // voice status
    bool released_;
    bool playing_;
    bool isFullOfZero_;
    uint32_t index_;
    uint8_t slot_;
    char note_;
    char midiVelocity_;
    float noteFrequency_;
    float velocity_;

    bool holdedByPedal_;
    // Fixing the "plop" when all notes are buisy...
    Timbre *currentTimbre_;
    Timbre *pendingTimbre_;
    bool transferReady_;


    // Voice needs its own sample block to apply effects
    float sampleBlock_[BLOCK_SIZE * 2];
    float mixerGain_;

    // Filter
    float fxParam1_, fxParam2_, fxParam3_;
    float fxParamA1_, fxParamA2_;
    float fxParamB1_, fxParamB2_;
    float v0L_, v1L_, v2L_, v3L_, v4L_, v5L_, v6L_, v7L_, v8L_;
    float v0R_, v1R_, v2R_, v3R_, v4R_, v5R_, v6R_, v7R_, v8R_;
    float fxPhase_;
    // save float fxParam1 to detect modification
    float fxParam1PlusMatrix_;


};

#endif
