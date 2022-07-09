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

class Voice {
    friend class Timbre;

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
    void setCurrentTimbre(Timbre *timbre);

    Timbre* getCurrentTimbre() {
        return currentTimbre;
    }
    bool isGliding() {
        return gliding;
    }

    void updateAllModulationIndexes() {
        int numberOfIMs = algoInformation[(int) (currentTimbre->getParamRaw()->engine1.algo)].im;

        // Feedback range is [0:1] compared to [0:16] of other modulation, let's divide the modulation impact by 16 (* 0.0625)
        feedbackModulation = currentTimbre->getParamRaw()->engineIm3.modulationIndex6 + this->velIm6
            + matrix.getDestination(MTX_DEST_FEEDBACK) * 0.0625f;
        if (unlikely(feedbackModulation < 0.0f)) {
            feedbackModulation = 0.0f;
        } else if (unlikely(feedbackModulation > 1.0f)) {
            feedbackModulation = 1.0f;
        }

        modulationIndex1 = currentTimbre->getParamRaw()->engineIm1.modulationIndex1 + matrix.getDestination(INDEX_MODULATION1)
            + matrix.getDestination(INDEX_ALL_MODULATION) + this->velIm1;
        if (unlikely(modulationIndex1 < 0.0f)) {
            modulationIndex1 = 0.0f;
        }
        modulationIndex2 = currentTimbre->getParamRaw()->engineIm1.modulationIndex2 + matrix.getDestination(INDEX_MODULATION2)
            + matrix.getDestination(INDEX_ALL_MODULATION) + this->velIm2;
        if (unlikely(modulationIndex2 < 0.0f)) {
            modulationIndex2 = 0.0f;
        }

        modulationIndex3 = currentTimbre->getParamRaw()->engineIm2.modulationIndex3 + matrix.getDestination(INDEX_MODULATION3)
            + matrix.getDestination(INDEX_ALL_MODULATION) + this->velIm3;
        if (unlikely(modulationIndex3 < 0.0f)) {
            modulationIndex3 = 0.0f;
        }

        if (likely(numberOfIMs < 3)) {
            return;
        }

        modulationIndex4 = currentTimbre->getParamRaw()->engineIm2.modulationIndex4 + matrix.getDestination(INDEX_MODULATION4)
            + matrix.getDestination(INDEX_ALL_MODULATION) + this->velIm4;
        if (unlikely(modulationIndex4 < 0.0f)) {
            modulationIndex4 = 0.0f;
        }

        modulationIndex5 = currentTimbre->getParamRaw()->engineIm3.modulationIndex5 + matrix.getDestination(INDEX_ALL_MODULATION) + this->velIm5;
        if (unlikely(modulationIndex5 < 0.0f)) {
            modulationIndex5 = 0.0f;
        }
    }

    void updateAllMixOscsAndPans() {
        float inv65535 = .0000152587890625; // 1/ 65535
        int pan;
        int numberOfMix = algoInformation[(int) (currentTimbre->getParamRaw()->engine1.algo)].mix;


        mix1 = currentTimbre->getParamRaw()->engineMix1.mixOsc1 + matrix.getDestination(MIX_OSC1) + matrix.getDestination(ALL_MIX);
        // Optimization to check mix1 is between 0 and 1
        mix1 = __USAT((int)((mix1 * .05 + mix1Previous * .95) * 65536) , 16) * inv65535;
        mix1Previous = mix1;
        float pan1 = currentTimbre->getParamRaw()->engineMix1.panOsc1 + matrix.getDestination(PAN_OSC1) + matrix.getDestination(ALL_PAN) + 1.0f;
        // pan1 is between -1 and 1 : Scale from 0.0 to 256
        pan = __USAT((int )(pan1 * 128), 8);
        pan1Left = panTable[pan] * .05f + pan1Left * .95f;
        pan1Right = panTable[256 - pan] * .05f + pan1Right * .95f;

        if (numberOfMix == 1) {
            return;
        }

        mix2 = currentTimbre->getParamRaw()->engineMix1.mixOsc2 + matrix.getDestination(MIX_OSC2) + matrix.getDestination(ALL_MIX);
        mix2 = __USAT((int)((mix2 * .05 + mix2Previous * .95) * 65535) , 16) * inv65535;
        mix2Previous = mix2;
        float pan2 = currentTimbre->getParamRaw()->engineMix1.panOsc2 + matrix.getDestination(PAN_OSC2) + matrix.getDestination(ALL_PAN) + 1.0f;
        pan = __USAT((int )(pan2 * 128), 8);
        pan2Left = panTable[pan] * .05f + pan2Left * .95f;
        pan2Right = panTable[256 - pan] * .05f + pan2Right * .95f;

        if (numberOfMix == 2) {
            return;
        }

        mix3 = currentTimbre->getParamRaw()->engineMix2.mixOsc3 + matrix.getDestination(MIX_OSC3) + matrix.getDestination(ALL_MIX);
        mix3 = __USAT((int)((mix3 * .05 + mix3Previous * .95) * 65535) , 16) * inv65535;
        mix3Previous = mix3;
        float pan3 = currentTimbre->getParamRaw()->engineMix2.panOsc3 + matrix.getDestination(PAN_OSC3) + matrix.getDestination(ALL_PAN) + 1.0f;
        pan = __USAT((int )(pan3 * 128), 8);
        pan3Left = panTable[pan] * .05f + pan3Left * .95f;
        pan3Right = panTable[256 - pan] * .05f + pan3Right * .95f;

        if (numberOfMix == 3) {
            return;
        }

        // No matrix for mix4 and pan4
        mix4 = currentTimbre->getParamRaw()->engineMix2.mixOsc4 + matrix.getDestination(MIX_OSC4) + matrix.getDestination(ALL_MIX);
        mix4 = __USAT((int)((mix4 * .05 + mix4Previous * .95) * 65535) , 16) * inv65535;
        mix4Previous = mix4;
        float pan4 = currentTimbre->getParamRaw()->engineMix2.panOsc4 + matrix.getDestination(PAN_OSC4) + matrix.getDestination(ALL_PAN) + 1.0f;
        pan = __USAT((int )(pan4 * 128), 8);
        pan4Left = panTable[pan] * .05f + pan4Left * .95f;
        pan4Right = panTable[256 - pan] * .05f + pan4Right * .95f;

        if (numberOfMix == 4) {
            return;
        }

        mix5 = currentTimbre->getParamRaw()->engineMix3.mixOsc5 + matrix.getDestination(ALL_MIX);
        mix5 = __USAT((int)((mix5 * .05 + mix5Previous * .95) * 65535) , 16) * inv65535;
        mix5Previous = mix5;

        float pan5 = currentTimbre->getParamRaw()->engineMix3.panOsc5 + matrix.getDestination(ALL_PAN) + 1.0f;
        pan = __USAT((int )(pan5 * 128), 8);
        pan5Left = panTable[pan] * .05f + pan5Left * .95f;
        pan5Right = panTable[256 - pan] * .05f + pan5Right * .95f;

        mix6 = currentTimbre->getParamRaw()->engineMix3.mixOsc6 + matrix.getDestination(ALL_MIX);
        mix6 = __USAT((int)((mix6 * .05 + mix6Previous * .95) * 65535) , 16) * inv65535;
        mix6Previous = mix6;

        float pan6 = currentTimbre->getParamRaw()->engineMix3.panOsc6 + matrix.getDestination(ALL_PAN) + 1.0f;
        pan = __USAT((int )(pan6 * 128), 8);
        pan6Left = panTable[pan] * .05f + pan6Left * .95f;
        pan6Right = panTable[256 - pan] * .05f + pan6Right * .95f;
    }

    void midiClockSongPositionStep(int songPosition, bool recomputeNext);
    void midiClockContinue(int songPosition);
    void midiClockStart();

    void lfoNoteOn() {
        if (likely(currentTimbre->isLfoUsed(0))) {
            lfoOsc[0].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(1))) {
            lfoOsc[1].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(2))) {
            lfoOsc[2].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(3))) {
            lfoEnv[0].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(4))) {
            lfoEnv2[0].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(5))) {
            lfoStepSeq[0].noteOn();
        }
        if (likely(currentTimbre->isLfoUsed(6))) {
            lfoStepSeq[1].noteOn();
        }
    }

    void lfoNoteOff() {
        if (likely(currentTimbre->isLfoUsed(0))) {
            lfoOsc[0].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(1))) {
            lfoOsc[1].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(2))) {
            lfoOsc[2].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(3))) {
            lfoEnv[0].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(4))) {
            lfoEnv2[0].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(5))) {
            lfoStepSeq[0].noteOff();
        }
        if (likely(currentTimbre->isLfoUsed(6))) {
            lfoStepSeq[1].noteOff();
        }
    }

    void prepareMatrixForNewBlock() {

        // first 3 LFO can be free running
        if (likely(currentTimbre->isLfoUsed(0))) {
            this->lfoOsc[0].nextValueInMatrix();
        }
        if (likely(currentTimbre->isLfoUsed(1))) {
            this->lfoOsc[1].nextValueInMatrix();
        }
        if (likely(currentTimbre->isLfoUsed(2))) {
            this->lfoOsc[2].nextValueInMatrix();
        }

        // Only compute the rest if playing
        if (likely(isPlaying())) {
            if (likely(currentTimbre->isLfoUsed(3))) {
                this->lfoEnv[0].nextValueInMatrix();
            }
            if (likely(currentTimbre->isLfoUsed(4))) {
                this->lfoEnv2[0].nextValueInMatrix();
            }
            if (likely(currentTimbre->isLfoUsed(5))) {
                this->lfoStepSeq[0].nextValueInMatrix();
            }
            if (likely(currentTimbre->isLfoUsed(6))) {
                this->lfoStepSeq[1].nextValueInMatrix();
            }

            this->matrix.computeAllDestinations();
            updateAllModulationIndexes();
        }
    }

    void afterNewParamsLoad() {
        this->matrix.resetSources();
        this->matrix.resetAllDestination();
        for (int j = 0; j < NUMBER_OF_ENCODERS_PFM2; j++) {
            this->lfoOsc[0].valueChanged(j);
            this->lfoOsc[1].valueChanged(j);
            this->lfoOsc[2].valueChanged(j);
            this->lfoEnv[0].valueChanged(j);
            this->lfoEnv2[0].valueChanged(j);
            this->lfoStepSeq[0].valueChanged(j);
            this->lfoStepSeq[1].valueChanged(j);

        }
        v0L = v1L = 0.0f;
        v0R = v1R = 0.0f;
    }

    void lfoValueChange(int currentRow, int encoder, float newValue) {
        switch (currentRow) {
            case ROW_LFOOSC1:
            case ROW_LFOOSC2:
            case ROW_LFOOSC3:
                lfoOsc[currentRow - ROW_LFOOSC1].valueChanged(encoder);
                break;
            case ROW_LFOENV1:
                lfoEnv[0].valueChanged(encoder);
                break;
            case ROW_LFOENV2:
                lfoEnv2[0].valueChanged(encoder);
                break;
            case ROW_LFOSEQ1:
            case ROW_LFOSEQ2:
                lfoStepSeq[currentRow - ROW_LFOSEQ1].valueChanged(encoder);
                break;
        }
    }

    float* const getSampleBlock() {
        return sampleBlock;
    }

    bool isNewGlide() {
        return newGlide;
    }

private:
    // private function for BP filter
    void recomputeBPValues(float q, float fSquare);

    // voice status
    bool released;
    bool playing;
    bool isFullOfZero;
    uint32_t index;
    char note;
    char midiVelocity;
    float noteFrequency;
    float velocity;
    float velIm1, velIm2, velIm3, velIm4, velIm5, velIm6;
    bool newNotePlayed;
    //
    float freqAi, freqAo;
    float freqBi, freqBo;
    // Harm freq shifting
    float freqHarm, targetFreqHarm;

    EnvData envState1_;
    EnvData envState2_;
    EnvData envState3_;
    EnvData envState4_;
    EnvData envState5_;
    EnvData envState6_;

    OscState oscState1_;
    OscState oscState2_;
    OscState oscState3_;
    OscState oscState4_;
    OscState oscState5_;
    OscState oscState6_;

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


    // env Value
    float env1ValueMem;
    float env2ValueMem;
    float env3ValueMem;
    float env4ValueMem;
    float env5ValueMem;
    float env6ValueMem;

    Timbre *currentTimbre;

    // glide phase increment
    static float glidePhaseInc[13];

    // Matrix....
    Matrix matrix;

    // optimization
    float modulationIndex1, modulationIndex2, modulationIndex3, modulationIndex4, modulationIndex5;
    float feedbackModulation, fdbLastValue[3];
    float mix1, mix2, mix3, mix4, mix5, mix6;
    float mix1Previous, mix2Previous, mix3Previous, mix4Previous, mix5Previous, mix6Previous;
    float pan1Left, pan2Left, pan3Left, pan4Left, pan5Left, pan6Left;
    float pan1Right, pan2Right, pan3Right, pan4Right, pan5Right, pan6Right;
    //
    LfoOsc lfoOsc[NUMBER_OF_LFO_OSC];
    LfoEnv lfoEnv[NUMBER_OF_LFO_ENV];
    LfoEnv2 lfoEnv2[NUMBER_OF_LFO_ENV2];
    LfoStepSeq lfoStepSeq[NUMBER_OF_LFO_STEP];

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
