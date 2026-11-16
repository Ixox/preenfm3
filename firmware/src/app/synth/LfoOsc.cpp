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

#include "LfoOsc.h"



extern float noise[32];

void LfoOsc::init(struct LfoParams *lfoParams, float* phase, Matrix *matrix, SourceEnum source, DestinationEnum dest) {
    Lfo::init(matrix, source, dest);
    this->type_ = LFO_TRIANGLE;
    this->ramp_ = 0;
    this->initPhase_ = phase;
    this->rampInv_ = 10000000 ;
    this->currentRamp_ = 0;
    this->lfo_ = lfoParams;
    valueChanged(3);
    this->destination_ = dest;
    this->currentRandomValue_ = 0.0f;

    ticks_ = 1536;
    midiClock(0, true);
}


void LfoOsc::midiClock(int songPosition, bool computeStep) {

    ticks_ &= 0x7ff;

    switch ((int)(lfo_->freq * 10.0f + .05f)) {
    case LFO_MIDICLOCK_MC_DIV_16:
        if ((songPosition & 0x1)==0) {
            if (computeStep) {
                currentFreq_ = PREENFM_FREQUENCY / BLOCK_SIZE / 32.0f * invTab[ticks_];
                ticks_ = 0;
            }
            phase_ = (songPosition & 0x3E) * 0.015625f + *this->initPhase_;
        }
        break;
    case LFO_MIDICLOCK_MC_DIV_8:
        if ((songPosition & 0x1)==0) {
            if (computeStep) {
                currentFreq_ = PREENFM_FREQUENCY / BLOCK_SIZE / 16.0f * invTab[ticks_];
                ticks_ = 0;
            }
            phase_ = (songPosition & 0x1E) * 0.03125f + *this->initPhase_;
        }
        break;
    case LFO_MIDICLOCK_MC_DIV_4:
        if ((songPosition & 0x1)==0) {
            if (computeStep) {
                currentFreq_ = PREENFM_FREQUENCY / BLOCK_SIZE / 8.0f * invTab[ticks_];
                ticks_ = 0;
            }
            phase_ = (songPosition & 0xE) * 0.0625f + *this->initPhase_;
        }
        break;
    case LFO_MIDICLOCK_MC_DIV_2:
        if ((songPosition & 0x1)==0) {
            if (computeStep) {
                currentFreq_ = PREENFM_FREQUENCY / BLOCK_SIZE / 4.0f * invTab[ticks_];
                ticks_ = 0;
            }
            // 0,2,4,6
            phase_ = (songPosition & 0x6) * .125f + *this->initPhase_;
        }
        break;
    case LFO_MIDICLOCK_MC:
        // Midi Clock
        if ((songPosition & 0x1) == 0) {
            if (computeStep) {
                currentFreq_ = PREENFM_FREQUENCY / BLOCK_SIZE / 2.0f * invTab[ticks_];
                ticks_ = 0;
            }
            // 0 or 2 -> 0 ou .5
            phase_ = (songPosition & 0x2) * .25f + *this->initPhase_;
        }
        break;
    case LFO_MIDICLOCK_MC_TIME_2:
        if ((songPosition & 0x1)==0) {
            if (computeStep) {
                currentFreq_ = PREENFM_FREQUENCY / BLOCK_SIZE * invTab[ticks_];
                ticks_ = 0;
            }
            phase_ =  *this->initPhase_;
        }
        break;
    case LFO_MIDICLOCK_MC_TIME_3:
        if ((songPosition & 0x3)==0) {
            if (computeStep) {
                currentFreq_ = PREENFM_FREQUENCY / BLOCK_SIZE * invTab[ticks_] * 3.0;
                ticks_ = 0;
            }
            phase_ = *this->initPhase_;
        }
        break;
    case LFO_MIDICLOCK_MC_TIME_4:
        if ((songPosition & 0x1)==0) {
            if (computeStep) {
                currentFreq_ = PREENFM_FREQUENCY / BLOCK_SIZE * invTab[ticks_] * 2.0f;
                ticks_ = 0;
            }
            phase_ = *this->initPhase_;
        }
        break;
    case LFO_MIDICLOCK_MC_TIME_8:
        if ((songPosition & 0x1)==0) {
            if (computeStep) {
                currentFreq_ = PREENFM_FREQUENCY / BLOCK_SIZE * invTab[ticks_] * 4.0f;
                ticks_ = 0;
            }
            phase_ = *this->initPhase_;
        }
        break;
    }
}


void LfoOsc::nextValueInMatrix() {
    float lfoValue;

    ticks_ ++;

    if (this->isNotMidiSynchronized_) {
        currentFreq_ = lfo_->freq + this->matrix_->getDestination(destination_);
    }
    phase_ += currentFreq_ * PREENFM_FREQUENCY_INVERSED_LFO;

    switch ((int)lfo_->shape) {
    case LFO_TRIANGLE:
    {
        if (unlikely(phase_ >= 1.0f)) {
            phase_ -= 1;
        }
        if (phase_ < .5f) {
            lfoValue = phase_ * 4.0f -1.0f ;
        } else {
            lfoValue = 1.0f - (phase_ - .5f ) * 4.0f;
        }
        break;
    }
    case LFO_SAW:
        if (unlikely(phase_ >= 1.0f)) {
            phase_ -= 1;
        }
        lfoValue = -1.0f + phase_ * 2.0f;
        break;
    case LFO_SIN:
    {
        if (unlikely(phase_ >= 1.0f)) {
            phase_ -= 1;
        }
        int sinIndex = (int) (phase_ * waveTables[0].max);
        sinIndex &= waveTables[0].max;
        lfoValue = sinTable[sinIndex];
        break;
    }
    case LFO_SQUARE:
        if (unlikely(phase_ >= 1.0f)) {
            phase_ -= 1;
        }
        if (phase_ < .5) {
            lfoValue = -1.0f;
        } else {
            lfoValue = 1.0f;
        }
        break;
    case LFO_RANDOM:
        if (unlikely(phase_ >= 1.0f)) {
            phase_ -= 1.0f;
            currentRandomValue_ = noise[0];
        }

        lfoValue = currentRandomValue_;
        break;
    case LFO_BROWNIAN:
        if (unlikely(phase_ >= 1.0f)) {
            phase_ -= 1.0f;
            noiseLp_ = noise[0] * 0.4f + noiseLp_ * 0.6f;
            currentRandomValue_ = noiseLp_;
        }

        lfoValue = currentRandomValue_;
        break;
    case LFO_WANDERING:
        if (unlikely(phase_ >= 1.0f)) {
            phase_ -= 1;
            currentRandomValue_ = nextRandomValue_;
            nextRandomValue_ = noise[0];
        }

        lfoValue = phase_ * (nextRandomValue_ - currentRandomValue_) + currentRandomValue_;
        break;
    case LFO_FLOW:
        if (unlikely(phase_ >= 1.0f)) {
            phase_ -= 1;
            noiseLp_ = noise[0] * 0.4f + noiseLp_ * 0.6f;
            currentRandomValue_ = nextRandomValue_;
            nextRandomValue_ = noiseLp_;
        }

        lfoValue = phase_ * (nextRandomValue_ - currentRandomValue_) + currentRandomValue_;
        break;
    }


    if (unlikely(currentRamp_ < ramp_)) {
        lfoValue = lfoValue * currentRamp_  * rampInv_ ;
        currentRamp_ += PREENFM_FREQUENCY_INVERSED_LFO;
    }

    lfoValue += lfo_->bias;

    matrix_->setSource((enum SourceEnum)source_, lfoValue);
}


void LfoOsc::noteOn() {
    if (ramp_ >= 0.0f) {
        currentRamp_ = 0.0f;
        if ((lfo_->freq * 10.0f) < LFO_MIDICLOCK_MC_DIV_16) {

            phase_ = *this->initPhase_;
        }
        // Retriger value if random...
        if (unlikely(lfo_->shape == LFO_RANDOM)) {
            currentRandomValue_ = noise[0];
        } else if (unlikely(lfo_->shape >= LFO_BROWNIAN)) {
            noiseLp_ = noise[0] * 0.4f + noiseLp_ * 0.6f;
            currentRandomValue_ = noiseLp_;
        }
    } else {
        // For KSyn Off
        currentRamp_ = 1; // greater than 0 :
    }
}

