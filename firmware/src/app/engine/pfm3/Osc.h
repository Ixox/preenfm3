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

#pragma once

#include "Matrix.h"
#include "SynthStateAware.h"

extern "C" {
extern float sinTable[];
}

struct OscState {
    float index;
    float frequency;
    float mainFrequencyPlusMatrix;
    float mainFrequency;
    float fromFrequency;
    float nextFrequency;
};

extern struct WaveTable waveTables[];
extern float exp2_harm[];

class Osc : public SynthStateAware {
public:
    Osc() {};
    virtual ~Osc() {};

    void init(SynthState* synthState, struct OscillatorParams* oscParams, DestinationEnum df);

    void newNote(struct OscState* oscState, float newNoteFrequency, float phase);
    float getNoteRealFrequencyEstimation(struct OscState* oscState, float newNoteFrequency);
    void glideToNote(struct OscState* oscState, float newNoteFrequency);
    void glideStep(struct OscState* oscState, float phase);

    inline void calculateFrequencyWithMatrix(struct OscState* oscState, Matrix* matrix, float expHarm) {
        oscState->mainFrequencyPlusMatrix = oscState->mainFrequency;
        oscState->mainFrequencyPlusMatrix *= expHarm;
        oscState->mainFrequencyPlusMatrix +=
            (oscState->mainFrequency * (matrix->getDestination(destFreq_) + matrix->getDestination(ALL_OSC_FREQ)) * .1f);
    }

    inline const struct WaveTable* getWaveTable() {
        return &waveTables[(int)oscillator_->shape];
    }

    inline float getNextSample(struct OscState* oscState, const struct WaveTable* waveTable) {
        oscState->index += oscState->frequency * waveTable->precomputedValue + waveTable->floatToAdd;

        // convert to int;
        int indexInteger = oscState->index;
        // keep decimal part;
        oscState->index -= indexInteger;
        // Put it back inside the table
        indexInteger &= waveTable->max;
        // Readjust the floating pont inside the table
        oscState->index += indexInteger;

        return waveTable->table[indexInteger];
    }

    inline float getPhase(struct OscState* oscState) {
        struct WaveTable* waveTable = &waveTables[(int)oscillator_->shape];
        return oscState->index * waveTable->phaseMul;
    }

    inline float geIndexFromtPhase(float phase) {
        struct WaveTable* waveTable = &waveTables[(int)oscillator_->shape];
        return phase * waveTable->max;
    }

    inline __attribute__((always_inline)) float getPhaseIncrement(struct OscState* oscState) {
        struct WaveTable* waveTable = &waveTables[(int)oscillator_->shape];
        return oscState->frequency * waveTable->precomputedValue + waveTable->floatToAdd;
    }

    // Sub-sample accurate hard sync of this (slave) oscillator on a master wrap.
    // Must be called after the master getNextSample() that wrapped and after this
    // slave frequency is set, before this slave getNextSample().
    inline __attribute__((always_inline)) void hardSyncToMaster(struct OscState* oscState, Osc* masterOsc, struct OscState* masterState,
                                                                float masterPhase) {
        struct WaveTable* waveTable = &waveTables[(int)oscillator_->shape];
        float masterIncrement = masterOsc->getPhaseIncrement(masterState) * waveTables[(int)masterOsc->oscillator_->shape].phaseMul;

        if (unlikely(!(masterIncrement > 0.0f))) {
            // Reversed phase: keep the legacy sample-rounded reset.
            oscState->index = masterPhase * waveTable->max;
            return;
        }

        // Fraction of the current sample elapsed since the master wrap (0..1).
        float elapsed = masterPhase / masterIncrement;
        if (elapsed > 1.0f) {
            elapsed = 1.0f;
        }
        // getNextSample() adds one full increment: pre-compensate so that the slave
        // phase after that call is exactly elapsed * increment. Offset by one table
        // length to keep the index positive.
        float slaveIncrement = getPhaseIncrement(oscState);
        oscState->index = (float)(waveTable->max + 1) + (elapsed - 1.0f) * slaveIncrement;
    }

    inline float* getNextBlock(struct OscState* oscState) {
        int shape = (int)oscillator_->shape;
        int max = waveTables[shape].max;
        const float* wave = waveTables[shape].table;
        float freq = oscState->frequency * waveTables[shape].precomputedValue + waveTables[shape].floatToAdd;
        float fIndex = oscState->index;
        int iIndex;
        float* oscValuesToFill = oscValues[oscValuesCpt];
        oscValuesCpt++;
        oscValuesCpt &= 0x3;

        for (int k = 0; k < 32;) {
            fIndex += freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            iIndex &= max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex];

            fIndex += freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            iIndex &= max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex];

            fIndex += freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            iIndex &= max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex];

            fIndex += freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            iIndex &= max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex];
        }
        oscState->index = fIndex;
        return oscValuesToFill;
    };

    inline float* getNextBlockWithFeedbackAndEnveloppe(struct OscState* oscState, float feedback, float& env, float envInc,
                                                       float freqMultiplier, float* lastValue) {
        int shape = (int)oscillator_->shape;
        int max = waveTables[shape].max;
        const float* wave = waveTables[shape].table;
        float freq = oscState->frequency * waveTables[shape].precomputedValue + waveTables[shape].floatToAdd;
        float fIndex = oscState->index;
        int iIndex;
        float* oscValuesToFill = oscValues[4];

        lastValue[2] = .95f * lastValue[2] + feedback * .05f;
        float phaseModulationAmplitude = lastValue[2] * ((float)max) * .5f;

        float localLastValue0 = lastValue[0];
        float localLastValue1 = lastValue[1];

        // Optimisation to avoid multiple freqMultiplier in the loop
        float localEnvM = env * freqMultiplier;
        float envIncM = envInc * freqMultiplier;
        for (int k = 0; k < 32; k++) {
            fIndex += freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            iIndex &= max;
            fIndex += iIndex;

            int index = iIndex + (localLastValue0 * phaseModulationAmplitude);
            index &= max;

            // Get rid of DC offset
            float newValue = wave[index];
            localLastValue0 = newValue - localLastValue1 + .99525f * localLastValue0;
            localLastValue1 = newValue;

            oscValuesToFill[k] = localLastValue0 * localEnvM;
            localEnvM += envIncM;
        }
        lastValue[0] = localLastValue0;
        lastValue[1] = localLastValue1;

        // update env
        env += envInc * 32;

        oscState->index = fIndex;
        return oscValuesToFill;
    }

    float* getNextBlockHQ(struct OscState* oscState) {
        int shape = (int)oscillator_->shape;
        int max = waveTables[shape].max;
        const float* wave = waveTables[shape].table;
        float freq = oscState->frequency * waveTables[shape].precomputedValue + waveTables[shape].floatToAdd;
        float fIndex = oscState->index;
        int iIndex;
        float fp;
        float* oscValuesToFill = oscValues[oscValuesCpt];
        oscValuesCpt++;
        oscValuesCpt &= 0x3;
        for (int k = 0; k < 32;) {
            fIndex += freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            fp = fIndex;
            iIndex &= max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex] * (1 - fp) + wave[iIndex] * fp;

            fIndex += freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            fp = fIndex;
            iIndex &= max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex] * (1 - fp) + wave[iIndex] * fp;

            fIndex += freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            fp = fIndex;
            iIndex &= max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex] * (1 - fp) + wave[iIndex] * fp;

            fIndex += freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            fp = fIndex;
            iIndex &= max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex] * (1 - fp) + wave[iIndex] * fp;
        }
        oscState->index = fIndex;
        return oscValuesToFill;
    };

private:
    DestinationEnum destFreq_;
    static float* oscValues[5];
    static int oscValuesCpt;
    OscillatorParams* oscillator_;
};
