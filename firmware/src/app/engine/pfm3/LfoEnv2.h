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

/*
 * Eveloppe with 3 states
 * S = silence
 * A = attack
 * R = release
 * After release go to S or finished
 */

#ifndef LFOENV2_H_
#define LFOENV2_H_

#include "Env.h"
#include "Lfo.h"

enum Env2State { ENV2_STATE_ON_S = 0, ENV2_STATE_ON_A, ENV2_STATE_ON_D, ENV2_STATE_DEAD, ENV2_NUMBER_OF_STATES };

class LfoEnv2 : public Pfm3Lfo {
public:
    LfoEnv2();

    void init(struct Envelope2LfoParams* envParams, Matrix* matrix, SourceEnum source, DestinationEnum dest);

    void valueChanged(int encoder) {
        switch (encoder) {
        case 0:
            stateInc_[ENV2_STATE_ON_S] = 0.0008f / (envParams_->silence + 0.0001);
            break;
        case 1:
            stateInc_[ENV2_STATE_ON_A] = 0.0008f / (envParams_->attack + 0.0001);
            break;
        case 2:
            stateInc_[ENV2_STATE_ON_D] = 0.0008f / (envParams_->decay + 0.0001);
            break;
        }
    }

    void newState() {
        if (env_.envState == ENV2_STATE_DEAD) {
            env_.currentValue = 0;
            if (envParams_->loop > 1) {
                env_.envState = ENV2_STATE_ON_A;
            } else if (envParams_->loop > 0) {
                env_.envState = ENV2_STATE_ON_S;
            }
        }
        env_.previousStateValue = env_.currentValue;
        env_.nextStateValue = stateTarget_[env_.envState];
        env_.currentPhase = 0;
        if (env_.envState == ENV2_STATE_ON_S) {
            if (this->matrix_->getDestination((enum DestinationEnum)this->destination_) != 0) {
                float silencePlusMatrix = envParams_->silence + this->matrix_->getDestination((enum DestinationEnum)this->destination_);
                if (silencePlusMatrix <= 0) {
                    stateInc_[ENV2_STATE_ON_S] = 1.0;
                } else if (silencePlusMatrix > 8) {
                    stateInc_[ENV2_STATE_ON_S] = 0.0001f;
                } else {
                    stateInc_[ENV2_STATE_ON_S] = 0.0008f / silencePlusMatrix;
                }
            }
        }
    }

    void nextValueInMatrix() {
        env_.currentPhase += stateInc_[env_.envState];

        if (unlikely(env_.currentPhase >= 1.0f)) {
            env_.currentValue = env_.nextStateValue;
            env_.envState++;
            newState();
        } else if (likely(stateInc_[env_.envState] > 0)) {
            env_.currentValue = env_.previousStateValue * (1 - env_.currentPhase) + env_.nextStateValue * env_.currentPhase;
        }

        matrix_->setSource((enum SourceEnum)source_, env_.currentValue);
    }

    void noteOn() {
        // index is decremented in the first call...
        env_.currentValue = 0;
        env_.envState = ENV2_STATE_ON_S;
        newState();
    }

    void noteOff() {
    }

    void midiClock(int songPosition, bool computeStep) {
    }

private:
    // target values of ADSR
    float stateTarget_[ENV2_NUMBER_OF_STATES];
    // float
    float stateInc_[ENV2_NUMBER_OF_STATES];

    Envelope2LfoParams* envParams_;
    EnvData env_;
};

#endif /* LfoEnv2_H_ */
