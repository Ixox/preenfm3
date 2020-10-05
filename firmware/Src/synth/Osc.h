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

#include "SynthStateAware.h"
#include "Matrix.h"

extern float sinTable[];



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


class Osc : public SynthStateAware
{
public:
    Osc() {};
    virtual ~Osc() {};

    void init(SynthState* synthState, struct OscillatorParams *oscParams, DestinationEnum df);

    void newNote(struct OscState* oscState, float newNoteFrequency);
    float getNoteRealFrequencyEstimation(struct OscState* oscState, float newNoteFrequency);
    void glideToNote(struct OscState* oscState, float newNoteFrequency);
    void glideStep(struct OscState* oscState, float phase);

    inline void calculateFrequencyWithMatrix(struct OscState *oscState, Matrix* matrix, float expHarm) {
        oscState->mainFrequencyPlusMatrix = oscState->mainFrequency;
        oscState->mainFrequencyPlusMatrix *= expHarm;
        oscState->mainFrequencyPlusMatrix +=  (oscState->mainFrequency  * (matrix->getDestination(destFreq) + matrix->getDestination(ALL_OSC_FREQ)) * .1f);
    }

    inline float getNextSample(struct OscState *oscState)  {
        struct WaveTable* waveTable = &waveTables[(int) oscillator->shape];

        oscState->index +=  oscState->frequency * waveTable->precomputedValue + waveTable->floatToAdd;

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



   	float* getNextBlock(struct OscState *oscState)  {
        int shape = (int) oscillator->shape;
   		int max = waveTables[shape].max;
   		float *wave = waveTables[shape].table;
   		float freq = oscState->frequency * waveTables[shape].precomputedValue + waveTables[shape].floatToAdd;
   		float fIndex = oscState->index;
   		int iIndex;
   		float* oscValuesToFill = oscValues[oscValuesCpt];
    	oscValuesCpt++;
    	oscValuesCpt &= 0x3;

   		for (int k=0; k<32; ) {
            fIndex +=  freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            iIndex &=  max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex];

            fIndex +=  freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            iIndex &=  max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex];

            fIndex +=  freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            iIndex &=  max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex];

            fIndex +=  freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            iIndex &=  max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex];

   		}
    	oscState->index = fIndex;
    	return oscValuesToFill;
    };


    float* getNextBlockWithFeedbackAndEnveloppe(struct OscState *oscState, float feedback, float& env, float envInc, float freqMultiplier, float* lastValue) {
        int shape = (int) oscillator->shape;
        int max = waveTables[shape].max;
        float *wave = waveTables[shape].table;
        float freq = oscState->frequency * waveTables[shape].precomputedValue + waveTables[shape].floatToAdd;
        float fIndex = oscState->index;
        int iIndex;
        float* oscValuesToFill = oscValues[4];
        float localEnv = env;


        float localLastValue0 = lastValue[0];
        float localLastValue1 = lastValue[1];

        if (unlikely(feedback == 0.0f)) {
            for (int k = 0; k < 32;) {
                fIndex += freq;
                iIndex = fIndex;
                fIndex -= iIndex;
                iIndex &= max;
                fIndex += iIndex;
                oscValuesToFill[k++] = wave[iIndex] * freqMultiplier * localEnv ;
                localEnv += envInc;
                // Two times less jump if done twice
                fIndex += freq;
                iIndex = fIndex;
                fIndex -= iIndex;
                iIndex &= max;
                fIndex += iIndex;
                oscValuesToFill[k++] = wave[iIndex] * freqMultiplier * localEnv ;
                localEnv += envInc;
            }
            lastValue[0] = oscValuesToFill[31];
            lastValue[1] = oscValuesToFill[31];
            lastValue[2] = 0.0f;
        } else {
            float phaseModulationAmplitude = lastValue[2] * ((float) max) * .5f;
            lastValue[2] = .95f * lastValue[2] + feedback * .05f;

            for (int k = 0; k < 32;) {
                fIndex += freq;
                iIndex = fIndex;
                fIndex -= iIndex;
                iIndex &= max;
                fIndex += iIndex;

                int index =  iIndex + (localLastValue0 * phaseModulationAmplitude);
                index &= max;


                // Get rid of DC offset
                float newValue = wave[index];
                localLastValue0 = newValue - localLastValue1 + .99525f * localLastValue0;
                localLastValue1 = newValue;

                oscValuesToFill[k++] = localLastValue0 * freqMultiplier * localEnv ;

                localEnv += envInc;
            }
            lastValue[0] = localLastValue0;
            lastValue[1] = localLastValue1;
        }


        env = localEnv;
        oscState->index = fIndex;
        return oscValuesToFill;
    }


   	float* getNextBlockHQ(struct OscState *oscState)  {
        int shape = (int) oscillator->shape;
   		int max = waveTables[shape].max;
   		float *wave = waveTables[shape].table;
   		float freq = oscState->frequency * waveTables[shape].precomputedValue + waveTables[shape].floatToAdd;
   		float fIndex = oscState->index;
   		int iIndex;
   		float fp;
   		float* oscValuesToFill = oscValues[oscValuesCpt];
    	oscValuesCpt++;
    	oscValuesCpt &= 0x3;
   		for (int k=0; k<32; ) {
            fIndex +=  freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            fp = fIndex;
            iIndex &=  max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex] * (1-fp) + wave[iIndex]* fp;

            fIndex +=  freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            fp = fIndex;
            iIndex &=  max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex] * (1-fp) + wave[iIndex]* fp;

            fIndex +=  freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            fp = fIndex;
            iIndex &=  max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex] * (1-fp) + wave[iIndex]* fp;

            fIndex +=  freq;
            iIndex = fIndex;
            fIndex -= iIndex;
            fp = fIndex;
            iIndex &=  max;
            fIndex += iIndex;
            oscValuesToFill[k++] = wave[iIndex] * (1-fp) + wave[iIndex]* fp;
   		}
    	oscState->index = fIndex;
    	return oscValuesToFill;
    };

private:
    DestinationEnum destFreq;
    Matrix* matrix;
    static float* oscValues[5];
    static int oscValuesCpt;
    OscillatorParams* oscillator;
};
