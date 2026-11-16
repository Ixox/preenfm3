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

#include "Timbre.h"
#include "Pfm3VoiceState.h"
#include "Env.h"
#include "Osc.h"

/** \brief  Unsigned Saturate
 This function saturates an unsigned value.
 \param [in]  value  Value to be saturated
 \param [in]    sat  Bit position to saturate to (0..31)
 \return             Saturated value
 */
#define __USAT(ARG1,ARG2) \
({                          \
  uint32_t __RES, __ARG1 = (ARG1); \
  asm ("usat %0, %1, %2" : "=r" (__RES) :  "I" (ARG2), "r" (__ARG1) ); \
  __RES; \
 })

class Timbre;
class VoicePool;
struct MsfaVoiceState;
struct MsfaTimbreState;

class Voice {
    friend class Timbre;
    friend class Preenfm3Engine;

public:
    Voice();
    ~Voice(void);

    void init();
    void nextBlock();
    void emptyBuffer();
    void fxAfterBlock();
    void endNoteOrBeginNextOne();
    void setNewEffectParam(int encoder);

    void noteOnWithoutPop(short note, float newNoteFrequency, short velocity, uint32_t index, float phase = 0.25f);
    void noteOn(short note, float newNoteFrequency, short velocity, uint32_t index, float phase = 0.25f);
    void glideToNote(short newNote, float newNoteFrequency);
    void killNow();
    void noteOff();
    void noteOffQuick();
    void glideFirstNoteOff();
    void glide();

    bool isReleased() {
        return this->released;
    }
    bool isPlaying() {
        return this->playing;
    }
    bool isNewNotePending() {
        return this->newNotePending;
    }
    uint32_t getIndex() {
        return this->index;
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
    float getNoteRealFrequencyEstimation(float newNoteFrequency);
    char getNextPendingNote() {
        return this->pendingNote;
    }
    char getNextGlidingNote() {
        return this->nextGlidingNote;
    }
    bool isHoldedByPedal() {
        return this->holdedByPedal;
    }
    void setHoldedByPedal(bool holded) {
        this->holdedByPedal = holded;
    }
    void bindTimbre(const Pfm3TimbreContext *context, Timbre *timbre);
    void bindMsfaTimbre(Timbre *timbre) {
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
    void completePendingTransfer(int poolIndex, VoicePool *pool);

    void setMsfaState(MsfaVoiceState *state) {
        msfaState_ = state;
    }
    void msfaNoteOn(const uint8_t *program, uint8_t note, uint8_t velocity, uint32_t index, MsfaTimbreState *ts);
    void msfaNoteOff();
    void msfaNextBlock(MsfaTimbreState *ts);
    void msfaUpdateProgram(const uint8_t *program);

    Timbre* getCurrentTimbre() {
        return currentTimbre;
    }
    bool isGliding() {
        return gliding;
    }

    void updateAllModulationIndexes() {
        int numberOfIMs = algoInformation[(int) (currentTimbre->getParamRaw()->engine1.algo)].im;

        // Feedback range is [0:1] compared to [0:16] of other modulation, let's divide the modulation impact by 16 (* 0.0625)
        pfm3State_.feedbackModulation = currentTimbre->getParamRaw()->engineIm3.modulationIndex6 + this->pfm3State_.velIm6
            + pfm3State_.matrix.getDestination(MTX_DEST_FEEDBACK) * 0.0625f;
        if (unlikely(pfm3State_.feedbackModulation < 0.0f)) {
            pfm3State_.feedbackModulation = 0.0f;
        } else if (unlikely(pfm3State_.feedbackModulation > 1.0f)) {
            pfm3State_.feedbackModulation = 1.0f;
        }

        pfm3State_.modulationIndex1 = currentTimbre->getParamRaw()->engineIm1.modulationIndex1 + pfm3State_.matrix.getDestination(INDEX_MODULATION1)
            + pfm3State_.matrix.getDestination(INDEX_ALL_MODULATION) + this->pfm3State_.velIm1;
        if (unlikely(pfm3State_.modulationIndex1 < 0.0f)) {
            pfm3State_.modulationIndex1 = 0.0f;
        }
        pfm3State_.modulationIndex2 = currentTimbre->getParamRaw()->engineIm1.modulationIndex2 + pfm3State_.matrix.getDestination(INDEX_MODULATION2)
            + pfm3State_.matrix.getDestination(INDEX_ALL_MODULATION) + this->pfm3State_.velIm2;
        if (unlikely(pfm3State_.modulationIndex2 < 0.0f)) {
            pfm3State_.modulationIndex2 = 0.0f;
        }

        pfm3State_.modulationIndex3 = currentTimbre->getParamRaw()->engineIm2.modulationIndex3 + pfm3State_.matrix.getDestination(INDEX_MODULATION3)
            + pfm3State_.matrix.getDestination(INDEX_ALL_MODULATION) + this->pfm3State_.velIm3;
        if (unlikely(pfm3State_.modulationIndex3 < 0.0f)) {
            pfm3State_.modulationIndex3 = 0.0f;
        }

        if (likely(numberOfIMs < 3)) {
            return;
        }

        pfm3State_.modulationIndex4 = currentTimbre->getParamRaw()->engineIm2.modulationIndex4 + pfm3State_.matrix.getDestination(INDEX_MODULATION4)
            + pfm3State_.matrix.getDestination(INDEX_ALL_MODULATION) + this->pfm3State_.velIm4;
        if (unlikely(pfm3State_.modulationIndex4 < 0.0f)) {
            pfm3State_.modulationIndex4 = 0.0f;
        }

        pfm3State_.modulationIndex5 = currentTimbre->getParamRaw()->engineIm3.modulationIndex5 + pfm3State_.matrix.getDestination(INDEX_ALL_MODULATION) + this->pfm3State_.velIm5;
        if (unlikely(pfm3State_.modulationIndex5 < 0.0f)) {
            pfm3State_.modulationIndex5 = 0.0f;
        }
    }

    void updateAllMixOscsAndPans() {
        float inv65535 = .0000152587890625; // 1/ 65535
        int pan;
        int numberOfMix = algoInformation[(int) (currentTimbre->getParamRaw()->engine1.algo)].mix;
        // add a LP on mix when MPE to mitigate noise dur to midi CC being [0-127]
        bool notMPE = currentTimbre->timbreNumber_ != 0 || currentTimbre->getMPESetting() == 0;


        pfm3State_.mix1 = currentTimbre->getParamRaw()->engineMix1.mixOsc1 + pfm3State_.matrix.getDestination(MIX_OSC1) + pfm3State_.matrix.getDestination(ALL_MIX);
        // Optimization to check pfm3State_.mix1 is between 0 and 1
        if (likely(notMPE)) {
            pfm3State_.mix1 = __USAT((int)(pfm3State_.mix1 * 65536) , 16) * inv65535;
        } else {
            pfm3State_.mix1 = __USAT((int)((pfm3State_.mix1 * .05 + pfm3State_.mix1Previous * .95) * 65536) , 16) * inv65535;
            pfm3State_.mix1Previous = pfm3State_.mix1;
        }
        float pan1 = currentTimbre->getParamRaw()->engineMix1.panOsc1 + pfm3State_.matrix.getDestination(PAN_OSC1) + pfm3State_.matrix.getDestination(ALL_PAN) + 1.0f;
        // pan1 is between -1 and 1 : Scale from 0.0 to 256
        pan = __USAT((int )(pan1 * 128), 8);
        pfm3State_.pan1Left = panTable[pan] * .05f + pfm3State_.pan1Left * .95f;
        pfm3State_.pan1Right = panTable[256 - pan] * .05f + pfm3State_.pan1Right * .95f;

        if (unlikely(numberOfMix == 1)) {
            return;
        }

        pfm3State_.mix2 = currentTimbre->getParamRaw()->engineMix1.mixOsc2 + pfm3State_.matrix.getDestination(MIX_OSC2) + pfm3State_.matrix.getDestination(ALL_MIX);
        if (likely(notMPE)) {
            pfm3State_.mix2 = __USAT((int)(pfm3State_.mix2 * 65535) , 16) * inv65535;
        } else {
            pfm3State_.mix2 = __USAT((int)((pfm3State_.mix2 * .05 + pfm3State_.mix2Previous * .95) * 65535) , 16) * inv65535;
            pfm3State_.mix2Previous = pfm3State_.mix2;
        }
        float pan2 = currentTimbre->getParamRaw()->engineMix1.panOsc2 + pfm3State_.matrix.getDestination(PAN_OSC2) + pfm3State_.matrix.getDestination(ALL_PAN) + 1.0f;
        pan = __USAT((int )(pan2 * 128), 8);
        pfm3State_.pan2Left = panTable[pan] * .05f + pfm3State_.pan2Left * .95f;
        pfm3State_.pan2Right = panTable[256 - pan] * .05f + pfm3State_.pan2Right * .95f;

        if (unlikely(numberOfMix == 2)) {
            return;
        }

        pfm3State_.mix3 = currentTimbre->getParamRaw()->engineMix2.mixOsc3 + pfm3State_.matrix.getDestination(MIX_OSC3) + pfm3State_.matrix.getDestination(ALL_MIX);
        if (likely(notMPE)) {
            pfm3State_.mix3 = __USAT((int)(pfm3State_.mix3 * 65535) , 16) * inv65535;
        } else {
            pfm3State_.mix3 = __USAT((int)((pfm3State_.mix3 * .05 + pfm3State_.mix3Previous * .95) * 65535) , 16) * inv65535;
            pfm3State_.mix3Previous = pfm3State_.mix3;
        }
        float pan3 = currentTimbre->getParamRaw()->engineMix2.panOsc3 + pfm3State_.matrix.getDestination(PAN_OSC3) + pfm3State_.matrix.getDestination(ALL_PAN) + 1.0f;
        pan = __USAT((int )(pan3 * 128), 8);
        pfm3State_.pan3Left = panTable[pan] * .05f + pfm3State_.pan3Left * .95f;
        pfm3State_.pan3Right = panTable[256 - pan] * .05f + pfm3State_.pan3Right * .95f;

        if (numberOfMix == 3) {
            return;
        }

        // No matrix for pfm3State_.mix4 and pan4
        pfm3State_.mix4 = currentTimbre->getParamRaw()->engineMix2.mixOsc4 + pfm3State_.matrix.getDestination(MIX_OSC4) + pfm3State_.matrix.getDestination(ALL_MIX);
        if (likely(notMPE)) {
            pfm3State_.mix4 = __USAT((int)(pfm3State_.mix4 * 65535) , 16) * inv65535;
        } else {
            pfm3State_.mix4 = __USAT((int)((pfm3State_.mix4 * .05 + pfm3State_.mix4Previous * .95) * 65535) , 16) * inv65535;
            pfm3State_.mix4Previous = pfm3State_.mix4;
        }
        float pan4 = currentTimbre->getParamRaw()->engineMix2.panOsc4 + pfm3State_.matrix.getDestination(PAN_OSC4) + pfm3State_.matrix.getDestination(ALL_PAN) + 1.0f;
        pan = __USAT((int )(pan4 * 128), 8);
        pfm3State_.pan4Left = panTable[pan] * .05f + pfm3State_.pan4Left * .95f;
        pfm3State_.pan4Right = panTable[256 - pan] * .05f + pfm3State_.pan4Right * .95f;

        if (numberOfMix == 4) {
            return;
        }

        pfm3State_.mix5 = currentTimbre->getParamRaw()->engineMix3.mixOsc5 + pfm3State_.matrix.getDestination(ALL_MIX);
        if (likely(notMPE)) {
            pfm3State_.mix5 = __USAT((int)(pfm3State_.mix5 * 65535) , 16) * inv65535;
        } else {
            pfm3State_.mix5 = __USAT((int)((pfm3State_.mix5 * .05 + pfm3State_.mix5Previous * .95) * 65535) , 16) * inv65535;
            pfm3State_.mix5Previous = pfm3State_.mix5;
        }

        float pan5 = currentTimbre->getParamRaw()->engineMix3.panOsc5 + pfm3State_.matrix.getDestination(ALL_PAN) + 1.0f;
        pan = __USAT((int )(pan5 * 128), 8);
        pfm3State_.pan5Left = panTable[pan] * .05f + pfm3State_.pan5Left * .95f;
        pfm3State_.pan5Right = panTable[256 - pan] * .05f + pfm3State_.pan5Right * .95f;

        pfm3State_.mix6 = currentTimbre->getParamRaw()->engineMix3.mixOsc6 + pfm3State_.matrix.getDestination(ALL_MIX);
        if (likely(notMPE)) {
            pfm3State_.mix6 = __USAT((int)(pfm3State_.mix6 * 65535) , 16) * inv65535;
        } else {
            pfm3State_.mix6 = __USAT((int)((pfm3State_.mix6 * .05 + pfm3State_.mix6Previous * .95) * 65535) , 16) * inv65535;
            pfm3State_.mix6Previous = pfm3State_.mix6;
        }

        float pan6 = currentTimbre->getParamRaw()->engineMix3.panOsc6 + pfm3State_.matrix.getDestination(ALL_PAN) + 1.0f;
        pan = __USAT((int )(pan6 * 128), 8);
        pfm3State_.pan6Left = panTable[pan] * .05f + pfm3State_.pan6Left * .95f;
        pfm3State_.pan6Right = panTable[256 - pan] * .05f + pfm3State_.pan6Right * .95f;
    }

    void midiClockSongPositionStep(int songPosition, bool recomputeNext);
    void midiClockContinue(int songPosition);
    void midiClockStart();

    void lfoNoteOn() {
        if (likely(currentTimbre->isLfoUsed(0))) {
            pfm3State_.lfoOsc[0].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(1))) {
            pfm3State_.lfoOsc[1].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(2))) {
            pfm3State_.lfoOsc[2].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(3))) {
            pfm3State_.lfoEnv[0].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(4))) {
            pfm3State_.lfoEnv2[0].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(5))) {
            pfm3State_.lfoStepSeq[0].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(6))) {
            pfm3State_.lfoStepSeq[1].noteOn();
        }
    }

    void lfoNoteOff() {
        if (likely(currentTimbre->isLfoUsed(0))) {
            pfm3State_.lfoOsc[0].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(1))) {
            pfm3State_.lfoOsc[1].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(2))) {
            pfm3State_.lfoOsc[2].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(3))) {
            pfm3State_.lfoEnv[0].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(4))) {
            pfm3State_.lfoEnv2[0].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(5))) {
            pfm3State_.lfoStepSeq[0].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(6))) {
            pfm3State_.lfoStepSeq[1].noteOff();
        }
    }

    void prepareMatrixForNewBlock() {
        // first 3 LFO can be free running
        if (likely(currentTimbre->isLfoUsed(0))) {
            this->pfm3State_.lfoOsc[0].nextValueInMatrix();
        }
        if (likely(currentTimbre->isLfoUsed(1))) {
            this->pfm3State_.lfoOsc[1].nextValueInMatrix();
        }
        if (likely(currentTimbre->isLfoUsed(2))) {
            this->pfm3State_.lfoOsc[2].nextValueInMatrix();
        }

        // Only compute the rest if playing
        if (likely(isPlaying())) {
            if (likely(currentTimbre->isLfoUsed(3))) {
                this->pfm3State_.lfoEnv[0].nextValueInMatrix();
            }
            if (likely(currentTimbre->isLfoUsed(4))) {
                this->pfm3State_.lfoEnv2[0].nextValueInMatrix();
            }
            if (likely(currentTimbre->isLfoUsed(5))) {
                this->pfm3State_.lfoStepSeq[0].nextValueInMatrix();
            }
            if (likely(currentTimbre->isLfoUsed(6))) {
                this->pfm3State_.lfoStepSeq[1].nextValueInMatrix();
            }

            this->pfm3State_.matrix.computeAllDestinations();
            updateAllModulationIndexes();
        }
    }

    void afterNewParamsLoad() {
        resetMatrix();
        for (int j = 0; j < NUMBER_OF_ENCODERS_PFM2; j++) {
            this->pfm3State_.lfoOsc[0].valueChanged(j);
            this->pfm3State_.lfoOsc[1].valueChanged(j);
            this->pfm3State_.lfoOsc[2].valueChanged(j);
            this->pfm3State_.lfoEnv[0].valueChanged(j);
            this->pfm3State_.lfoEnv2[0].valueChanged(j);
            this->pfm3State_.lfoStepSeq[0].valueChanged(j);
            this->pfm3State_.lfoStepSeq[1].valueChanged(j);

        }
        v0L = v1L = 0.0f;
        v0R = v1R = 0.0f;
    }

    void resetMatrix() {
        this->pfm3State_.matrix.resetSources();
        this->pfm3State_.matrix.resetAllDestination();
    }

    void lfoValueChange(int currentRow, int encoder, float newValue) {
        switch (currentRow) {
            case ROW_LFOOSC1:
            case ROW_LFOOSC2:
            case ROW_LFOOSC3:
                pfm3State_.lfoOsc[currentRow - ROW_LFOOSC1].valueChanged(encoder);
                break;
            case ROW_LFOENV1:
                pfm3State_.lfoEnv[0].valueChanged(encoder);
                break;
            case ROW_LFOENV2:
                pfm3State_.lfoEnv2[0].valueChanged(encoder);
                break;
            case ROW_LFOSEQ1:
            case ROW_LFOSEQ2:
                pfm3State_.lfoStepSeq[currentRow - ROW_LFOSEQ1].valueChanged(encoder);
                break;
        }
    }

    float* const getSampleBlock() {
        return sampleBlock;
    }

    Pfm3VoiceState* getPfm3State() {
        return &pfm3State_;
    }

    bool isNewGlide() {
        return newGlide;
    }

private:
    // private function for BP filter
    void recomputeBPValues(float q, float fSquare);

    // PFM3 per-voice DSP state
    Pfm3VoiceState pfm3State_;

    // voice status
    bool released;
    bool playing;
    bool isFullOfZero;
    uint32_t index;
    char note;
    char midiVelocity;
    float noteFrequency;
    float velocity;
    bool newNotePlayed;

    bool holdedByPedal;
    // Fixing the "plop" when all notes are buisy...
    bool newNotePending;
    char pendingNote;
    char pendingNoteVelocity;
    float pendingNoteFrequency;
    unsigned int nextIndex;
    float phase_;

    // Gliding ?
    bool gliding;
    float glidePhase;
    char nextGlidingNote;
    float nextGlidingNoteFrequency;
    bool newGlide; // Firs gliding note in "Always" mode
    // Used by 'glide always'
    float nextMainFrequency;
    // Used when note Off is received during Quick Release
    uint8_t noteAlreadyFinished;

    Timbre *currentTimbre;
    Timbre *pendingTimbre_;
    bool transferReady_;
    MsfaVoiceState *msfaState_;

    // glide phase increment
    static float glidePhaseInc[13];

    // preenfm3
    // Voice needs their own sample block to apply effects
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

    static float mpeBitchBend[6];

};

#endif
