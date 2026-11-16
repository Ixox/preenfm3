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
    friend class MsfaSynthEngine;
    friend class ByosEngine;

public:
    Voice();
    ~Voice(void);

    void init(int slot);
    void emptyBuffer();
    void fxAfterBlock(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterAmp);
    void setNewEffectParam(int encoder);


    bool isReleased() {
        return this->released;
    }
    bool isPlaying() {
        return this->playing;
    }
    uint32_t getIndex() {
        return this->index;
    }
    uint8_t slot() const {
        return slot_;
    }
    char getNote() {
        return this->note;
    }
    char getMidiVelocity() {
        return this->midiVelocity;
    }
    float getNoteFrequency() {
        return this->noteFrequency;
    }
    bool isHoldedByPedal() {
        return this->holdedByPedal;
    }
    void setHoldedByPedal(bool holded) {
        this->holdedByPedal = holded;
    }
    void bindTimbre(Timbre *timbre) {
        currentTimbre = timbre;
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
        return currentTimbre;
    }


    void resetVoiceState();

    float* const getSampleBlock() {
        return sampleBlock;
    }


private:
    // private function for BP filter
    void recomputeBPValues(float q, float fSquare);


    // voice status
    bool released;
    bool playing;
    bool isFullOfZero;
    uint32_t index;
    uint8_t slot_;
    char note;
    char midiVelocity;
    float noteFrequency;
    float velocity;

    bool holdedByPedal;
    // Fixing the "plop" when all notes are buisy...
    Timbre *currentTimbre;
    Timbre *pendingTimbre_;
    bool transferReady_;


    // Voice needs its own sample block to apply effects
    float sampleBlock[BLOCK_SIZE * 2];
    float mixerGain;

    // Filter
    float fxParam1, fxParam2, fxParam3;
    float fxParamA1, fxParamA2;
    float fxParamB1, fxParamB2;
    float v0L, v1L, v2L, v3L, v4L, v5L, v6L, v7L, v8L;
    float v0R, v1R, v2R, v3R, v4R, v5R, v6R, v7R, v8R;
    float fxPhase;
    // save float fxParam1 to detect modification
    float fxParam1PlusMatrix;


};

#endif
