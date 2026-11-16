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

#include "Common.h"
#include "Matrix.h"
#include "SynthState.h"

enum EnvState {
    ENV_STATE_ON_A = 0,
    ENV_STATE_ON_D,
    ENV_STATE_ON_S,
    ENV_STATE_ON_REAL_S,
    ENV_STATE_ON_R,
    ENV_STATE_DEAD,
    ENV_STATE_ON_QUICK_R,
    ENV_NUMBER_OF_STATES
};

struct EnvData {
    // State of the env
    uint8_t envState;
    // Current sample
    float currentValue;
    // previous state value and next state value
    float previousStateValue, nextStateValue;
    // currentPhase
    float currentPhase;
    // Inc attack and Inc Release is per voice since firmware 2.0
    float stateIncAttack;
    float stateIncRelease;
    // Inc dacay per voice since firmware 2.05
    float stateIncDecay;
};

struct table {
    float* table;
    uint8_t size;
};

extern float envExponential[];
extern float envLinear[];
extern float envLog[];
extern float userEnvCurves[4][64];

extern struct table allLfoTables[CURVE_TYPE_MAX];

class Env {
public:
    Env() {

        nextState_[ENV_STATE_ON_A] = ENV_STATE_ON_D;
        nextState_[ENV_STATE_ON_D] = ENV_STATE_ON_S;
        nextState_[ENV_STATE_ON_S] = ENV_STATE_ON_REAL_S;
        nextState_[ENV_STATE_ON_REAL_S] = ENV_STATE_ON_REAL_S;
        nextState_[ENV_STATE_ON_R] = ENV_STATE_DEAD;
        nextState_[ENV_STATE_ON_QUICK_R] = ENV_STATE_DEAD;

        tables_[ENV_STATE_ON_REAL_S].table = envLinear;
        tables_[ENV_STATE_ON_REAL_S].size = 1;
        tables_[ENV_STATE_ON_QUICK_R].table = envLinear;
        tables_[ENV_STATE_ON_QUICK_R].size = 1;

        // Quick release reaches 0 in 6 blocks
        stateTarget_[ENV_STATE_ON_QUICK_R] = 0.0f;
        stateInc_[ENV_STATE_ON_QUICK_R] = 0.166667f;

        stateTarget_[ENV_STATE_DEAD] = 0.0f;
    }

    virtual ~Env(void) {
    }

    void init(struct EnvelopeTimeMemory* envTime, struct EnvelopeLevelMemory* envLevel, uint8_t envNumber, float* algoNumber,
              struct EnvelopeCurveParams* envCurve);

    void applyCurves() {
        applyCurveToSegment((int)envCurve_->attackCurve, ENV_STATE_ON_A);
        applyCurveToSegment((int)envCurve_->decayCurve, ENV_STATE_ON_D);
        applyCurveToSegment((int)envCurve_->sustainCurve, ENV_STATE_ON_S);
        applyCurveToSegment((int)envCurve_->releaseCurve, ENV_STATE_ON_R);
    }

    void applyCurveToSegment(int segment, int segmentPos) {
        tables_[segmentPos].table = allLfoTables[segment].table;
        tables_[segmentPos].size = allLfoTables[segment].size;
    }

    void reloadADSR(int encoder) {
        // 0 Attack
        // 1 Decay
        // 2 Sustain
        // 3 Release
        switch (encoder) {
        case 0:
            stateTarget_[ENV_STATE_ON_A] = envLevel_->attackLevel;
            // Not necessary... recalculated in noteOn....
            // stateInc[ENV_STATE_ON_A] = incTab[(int)(envParamsA->attackTime * 100.0f)];
            break;
        case 1:
            stateTarget_[ENV_STATE_ON_D] = envLevel_->decayLevel;
            stateInc_[ENV_STATE_ON_D] = incTab[(int)(envTime_->decayTime * 100.0f)];
            break;
        case 2:
            stateTarget_[ENV_STATE_ON_S] = envLevel_->sustainLevel;
            stateTarget_[ENV_STATE_ON_REAL_S] = envLevel_->sustainLevel;
            stateInc_[ENV_STATE_ON_S] = incTab[(int)(envTime_->sustainTime * 100.0f)];
            stateInc_[ENV_STATE_ON_REAL_S] = 0.0f;
            break;
        case 3:
            stateTarget_[ENV_STATE_ON_R] = envLevel_->releaseLevel;
            isLoop_ = checkIsLoop();
            // Not necessary... recalculated in noteOff....
            // stateInc[ENV_STATE_ON_R] = incTab[(int)(envParamsB->releaseTime * 100.0f)];
            break;
        }
    }

    void initLoopState() {
        isLoop_ = false;
    }

    inline bool checkIsLoop() {
        // loopable only for modulators
        bool isModulator = algoOpInformation[(int)*this->algoNumber_][this->envNumber_] == OPERATOR_MODULATOR;
        // loop trick : modulator enveloppe loop if release = 1:0
        return isModulator && (envLevel_->releaseLevel == 1.0f) && (envTime_->releaseTime == 0.0f);
    }

    void newState(struct EnvData* env) {
        if (unlikely(isLoop_ && (env->envState > ENV_STATE_ON_S))) {
            // loop env
            env->envState = ENV_STATE_ON_A;
        }
        env->previousStateValue = env->currentValue;
        env->nextStateValue = stateTarget_[env->envState];
        env->currentPhase = 0;
    }

    // Large block-rate routine inlined at ~170 call sites in Preenfm3Engine::renderVoice().
    // Keep it out of line to limit code size / I-cache pressure; the call
    // overhead is negligible because it runs once per block per operator.
    __attribute__((noinline)) float getNextAmpExp(struct EnvData* env) {
        float incPhase;

        switch (env->envState) {
        case ENV_STATE_ON_A:
            incPhase = env->stateIncAttack;
            break;
        case ENV_STATE_ON_R:
            incPhase = env->stateIncRelease;
            break;
        case ENV_STATE_ON_D:
            incPhase = env->stateIncDecay;
            break;
        default:
            incPhase = stateInc_[env->envState];
            break;
        }
        env->currentPhase += incPhase;

        if (unlikely(env->currentPhase >= 1.0f)) {
            env->currentValue = env->nextStateValue;
            env->envState = nextState_[env->envState];
            newState(env);

            return env->currentValue;
        }

        if (likely(incPhase > 0.0f)) {
            float fIndex = env->currentPhase * tables_[env->envState].size;
            int index = (int)fIndex;

            float floatingPart = fIndex - index;
            float from = tables_[env->envState].table[index];
            float to = tables_[env->envState].table[index + 1];
            float tmpValue = from * (1.0f - floatingPart) + to * floatingPart;
            // When table value is 0 => next value
            // When table value is 1 => previous value
            // see table
            env->currentValue = tmpValue * (env->nextStateValue - env->previousStateValue) + env->previousStateValue;
            return env->currentValue;
        }

        return env->currentValue;
    }

    float noteOnAfterMatrixCompute(struct EnvData* env, Matrix* matrix) {

        float attack = envTime_->attackTime + matrix->getDestination((enum DestinationEnum)(ENV1_ATTACK + envNumber_));
        float decay = envTime_->decayTime;

        if (unlikely(algoOpInformation[(int)*this->algoNumber_][this->envNumber_]) == OPERATOR_CARRIER) {
            attack += matrix->getDestination(ALL_ENV_ATTACK);
            decay += matrix->getDestination(ALL_ENV_DECAY);
        }
        if (unlikely(algoOpInformation[(int)*this->algoNumber_][this->envNumber_]) == OPERATOR_MODULATOR) {
            attack += matrix->getDestination(ALL_ENV_ATTACK_MODULATOR);
            decay += matrix->getDestination(ALL_ENV_DECAY_MODULATOR);
        }

        if (unlikely(decay < 0.0f)) {
            decay = 0.0f;
        }
        if (unlikely(attack < 0.0f)) {
            attack = 0.0f;
        }
        // stateInc[ENV_STATE_ON_A] = incTab[(int)(attack * 100.0f)];
        env->stateIncAttack = incTab[(int)(attack * 100.0f)];
        env->stateIncDecay = incTab[(int)(decay * 100.0f)];

        // For Later / XH 2 Jan 2020
        //        if (attack != 0.0f) {
        //            env->currentValue = 0.0f;
        //        } else {
        //            // Attack 0s, so set currentValue and start in Decay state
        //            env->currentValue = envParamsA->attackLevel;;
        //        }
        //        env->envState = ENV_STATE_ON_A;
        //        newState(env);
        //
        //        return env->currentValue;

        // But in the meantime
        env->currentValue = 0.0f;
        env->envState = ENV_STATE_ON_A;
        newState(env);
        return 0.0f;
    }

    void noteOffQuick(struct EnvData* env) {
        env->envState = ENV_STATE_ON_QUICK_R;
        newState(env);
    }

    void noteOff(struct EnvData* env, Matrix* matrix) {

        float release = envTime_->releaseTime;
        if (unlikely(algoOpInformation[(int)*this->algoNumber_][this->envNumber_]) == OPERATOR_CARRIER) {
            release += matrix->getDestination(ALL_ENV_RELEASE);
        }
        if (unlikely(algoOpInformation[(int)*this->algoNumber_][this->envNumber_]) == OPERATOR_MODULATOR) {
            release += matrix->getDestination(ALL_ENV_RELEASE_MODULATOR);
        }
        if (release < 0.0f) {
            release = 0.0f;
        }
        // The lower we are the shorter the release is...
        if (env->currentValue > stateTarget_[ENV_STATE_ON_R]) {
            release *= (1.0f + (env->currentValue - stateTarget_[ENV_STATE_ON_R])) / 2.0f;
        } else {
            release *= (1.0f + (stateTarget_[ENV_STATE_ON_R] - env->currentValue)) / 2.0f;
        }
        env->stateIncRelease = incTab[(int)(release * 100.0f)];
        env->envState = ENV_STATE_ON_R;
        newState(env);
    }

    bool isDead(struct EnvData* env) __attribute__((always_inline)) {
        return env->envState == ENV_STATE_DEAD;
    }

private:
    // target values of ADSR
    float stateTarget_[ENV_NUMBER_OF_STATES];
    // float

    float stateInc_[ENV_NUMBER_OF_STATES];
    uint8_t nextState_[ENV_NUMBER_OF_STATES];
    struct table tables_[ENV_NUMBER_OF_STATES];

    EnvelopeTimeMemory* envTime_;
    EnvelopeLevelMemory* envLevel_;
    EnvelopeCurveParams* envCurve_;

    uint8_t envNumber_;
    float* algoNumber_;

    // loopable envelope
    bool isLoop_;

    static int initTab;
    static float incTab[1601];
};
