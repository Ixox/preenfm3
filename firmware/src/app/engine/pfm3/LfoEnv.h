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

#ifndef LFOENV_H_
#define LFOENV_H_

#include "Env.h"
#include "Lfo.h"

class LfoEnv : public Pfm3Lfo {
public:
    LfoEnv();

    void init(struct EnvelopeLfoParams* envParams, Matrix* matrix, SourceEnum source, DestinationEnum dest);

    void valueChanged(int encoder) {
        switch (encoder) {
        case 0:
            stateInc_[ENV_STATE_ON_A] = 0.0008f / (envParams_->attack + 0.0001);
            break;
        case 1:
            stateInc_[ENV_STATE_ON_D] = 0.0008f / (envParams_->decay + 0.0001);
            break;
        case 2:
            stateTarget_[ENV_STATE_ON_D] = envParams_->sustain;
            stateTarget_[ENV_STATE_ON_S] = envParams_->sustain;
            break;
        case 3:
            stateInc_[ENV_STATE_ON_R] = 0.0008f / (envParams_->release + 0.0001);
            break;
        }
    }

    void newState() {

        if (env_.envState == ENV_STATE_DEAD) {
            env_.currentValue = 0;
        }
        env_.previousStateValue = env_.currentValue;
        env_.nextStateValue = stateTarget_[env_.envState];
        env_.currentPhase = 0;
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
        env_.currentValue = 0;
        env_.envState = ENV_STATE_ON_A;
        newState();
    }

    void noteOff() {
        env_.envState = ENV_STATE_ON_R;
        newState();
    }

    void midiClock(int songPosition, bool computeStep) {
    }

private:
    // target values of ADSR
    float stateTarget_[ENV_NUMBER_OF_STATES];
    // float
    float stateInc_[ENV_NUMBER_OF_STATES];

    EnvelopeLfoParams* envParams_;
    EnvData env_;
};

#endif /* LFOENV_H_ */
