/*
 * Copyright 2021  - Patrice Vigouroux
 *
 * Author: Patrice Vigouroux (Toltekradiation)
 * Forum discussion : http://ixox.fr/forum/index.php?topic=69835.0
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

#ifndef FX_BUS_
#define FX_BUS_

#include "Common.h"

#define GLOBALFX_PREDELAYTIME_DEFAULT  0.54f
#define GLOBALFX_PREDELAYMIX_DEFAULT  0.35f
#define GLOBALFX_SIZE_DEFAULT  0.41f
#define GLOBALFX_DIFFUSION_DEFAULT  0.84f
#define GLOBALFX_DAMPING_DEFAULT  0.63f
#define GLOBALFX_DECAY_DEFAULT  0.74f
#define GLOBALFX_LFODEPTH_DEFAULT  0.28f
#define GLOBALFX_LFOSPEED_DEFAULT  0.69f
#define GLOBALFX_INPUTBASE_DEFAULT  0.36f
#define GLOBALFX_INPUTWIDTH_DEFAULT  0.46f
#define GLOBALFX_NOTCHBASE_DEFAULT  0.5f
#define GLOBALFX_NOTCHSPREAD_DEFAULT  0.69f
#define GLOBALFX_LOOPHP_DEFAULT  0.34f


// Must not be changed after VERSION6
enum MASTERFXPARAMS {
    GLOBALFX_PREDELAYTIME = 0,
    GLOBALFX_DECAY,
    GLOBALFX_PREDELAYMIX,
    GLOBALFX_SIZE,
    GLOBALFX_DIFFUSION,
    GLOBALFX_DAMPING,
    GLOBALFX_LFODEPTH,
    GLOBALFX_LFOSPEED,
    GLOBALFX_INPUTBASE,
    GLOBALFX_INPUTWIDTH,
    GLOBALFX_LOOPHP,
    GLOBALFX_NOTCHBASE,
    GLOBALFX_NOTCHSPREAD,
    GLOBALFX_PARAMS_SIZE
};


extern float panTable[];

class FxBus {
	friend class FMDisplayMixer;
    friend class MixerState;
public:
    FxBus();
    virtual ~FxBus() {}
    void init();
    void setDefaultValue();
    void mixSumInit();
    void presetChanged(int presetNum);
    void paramChanged();
    void mixAdd(float *inStereo, float send, float reverbLevel);
    void processBlock(int32_t *outBuff);
    float delayInterpolation(float readPos, float buffer[], int bufferLenM1);
    void lfoProcess(float *lfo, float *lfotri, float *lfoInc);
    float delayAllpassInterpolation(float readPos, float buffer[], int bufferLenM1, float prevVal);
    float* getSampleBlock() {
        return sampleBlock_;
    }
    const float* getSampleBlock() const {
        return sampleBlock_;
    }

protected:
    // Reverb params
    float masterfxConfig[GLOBALFX_PARAMS_SIZE];

    #define _dattorroSampleRateMod PREENFM_FREQUENCY / 29761.0f

    float headRoomMultiplier = 1;
    float sampleMultipler;

    float totalSent = 0;

    //lfo
    float lfo1 = 0;
    float lfo1tri;
    float lfo1Inc = 0.00037f;

    float lfo2 = 0;
    float lfo2tri;
    float lfo2Inc = lfo1Inc * 1.07f;

    float lfo3 = 1;
    float lfo3tri;
    float lfo3Inc = lfo1Inc;

    float lfo4 = 1;
    float lfo4tri;
    float lfo4Inc = lfo2Inc;


    float sampleBlock_[BLOCK_SIZE * 2];
    float *sample;

    float fxTime = 0.98, prevTime = -1, fxTimeLinear, prevFxTimeLinear = -1;
    float prevdecayVal = 0;
    float decayVal = 0.5, prevDecayVal = -1, decayFdbck = 0;
    const float decayMaxVal = 0.96f;
    float sizeParam, prevSizeParam;
    float inputDiffusion, prevInputDiffusion;
    float diffusion, prevDiffusion;
    float damping, prevDamping = -1;
    float predelayMixLevel = 0.5f;
    float predelayMixAttn = predelayMixLevel * 0.75;
    float lfoDepth =  0;
    float lfoSpeed = 0, lfoSpeedLinear = 0, prevLfoSpeedLinear = -1;
    float loopLpf, loopLpf2, prevFilterBase = -1;
    float damp_a, damp_b;
    float inputWidth = 0.5f, prevInputWidth = -1;
    float prevLoopHp = -1;
    float lfoLpCoef1 = 3999, lfoLpCoef2 = 0.00025f;
    float timeCvControl1 = 0, timeCvControl2 = 0, timeCvControl3 = 0, timeCvControl4 = 0;
    float timeCv = 0, prevTimeCv = 0, timeCvSpeed = 0, prevtimeCvSpeed = 0, cvDelta;

    float loopHpf = 0.14f;
    float inHpf = 0.1f;

    float inR, inL;

    float outL, outR;

    float int1 = 0, int2 = 0, int3 = 0, int4 = 0;

    static const int delay1BufferSize     = 4453 * _dattorroSampleRateMod;
    static const int delay1BufferSizeM1    = delay1BufferSize - 1;
    static float delay1Buffer[delay1BufferSize];
    int delay1WritePos     = 0;
    int delay1ReadPos     = 0;
    float delay1DelayLen     = 0, delay1ReadLen;
    float delay1FxTarget     = 0;

    static const int delay2BufferSize     = 3720 * _dattorroSampleRateMod;
    static const int delay2BufferSizeM1    = delay2BufferSize - 1;
    static float delay2Buffer[delay2BufferSize];
    int delay2WritePos     = 0;
    int delay2ReadPos;
    float delay2DelayLen     = 0, delay2ReadLen;
    float delay2FxTarget     = 0;

    static const int delay3BufferSize     = 4217 * _dattorroSampleRateMod;
    static const int delay3BufferSizeM1    = delay3BufferSize - 1;
    static float delay3Buffer[delay3BufferSize];
    int delay3WritePos     = 0;
    int delay3ReadPos     = 0;
    float delay3DelayLen     = 0, delay3ReadLen;
    float delay3FxTarget     = 0;

    static const int delay4BufferSize     = 3163 * _dattorroSampleRateMod;
    static const int delay4BufferSizeM1    = delay4BufferSize - 1;
    static float delay4Buffer[delay4BufferSize];
    int delay4WritePos     = 0;
    int delay4ReadPos;
    float delay4DelayLen     = 0, delay4ReadLen;
    float delay4FxTarget     = 0;

    const float dcBlockerCoef1 = 0.9973854301f;//~20 hz

    //pre delay

    static const int predelayBufferSize     = 16000;
    static const int predelayBufferSizeM1     = predelayBufferSize - 1;
    static float predelayBuffer[predelayBufferSize];
    int predelaySize             = predelayBufferSize;
    int predelayWritePos         = 0;
    float predelayReadPos         = 0;

    // input diffuser

    static const int inputBufferLen1 = 141 * _dattorroSampleRateMod;
    static const int inputBufferLen2 = 107 * _dattorroSampleRateMod;
    static const int inputBufferLen3 = 379 * _dattorroSampleRateMod;
    static const int inputBufferLen4 = 277 * _dattorroSampleRateMod;
    static float inputBuffer1[inputBufferLen1];
    static float inputBuffer2[inputBufferLen2];
    static float inputBuffer3[inputBufferLen3];
    static float inputBuffer4[inputBufferLen4];
    float inputBuffer1ReadLen = inputBufferLen1;
    float inputBuffer2ReadLen = inputBufferLen2;
    float inputBuffer3ReadLen = inputBufferLen3;
    float inputBuffer4ReadLen = inputBufferLen4;
    int inputWritePos1     = 0;
    int inputWritePos2     = 0;
    int inputWritePos3     = 0;
    int inputWritePos4     = 0;

    int inputReadPos1;
    int inputReadPos2;
    int inputReadPos3;
    int inputReadPos4;

    float inputCoef1 = 0.75f;
    float inputCoef2 = 0.625f;

    // diffuser decay

    static const int diffuserBufferLen1 = 672     * _dattorroSampleRateMod;
    static const int diffuserBufferLen2 = 1800     * _dattorroSampleRateMod;
    static const int diffuserBufferLen3 = 908     * _dattorroSampleRateMod;
    static const int diffuserBufferLen4 = 2656     * _dattorroSampleRateMod;
    static const int diffuserBufferLen1M1 = diffuserBufferLen1 - 1;
    static const int diffuserBufferLen2M1 = diffuserBufferLen2 - 1;
    static const int diffuserBufferLen3M1 = diffuserBufferLen3 - 1;
    static const int diffuserBufferLen4M1 = diffuserBufferLen4 - 1;
    float diffuserBuffer1ReadLen = diffuserBufferLen1M1;
    float diffuserBuffer2ReadLen = diffuserBufferLen2M1;
    float diffuserBuffer3ReadLen = diffuserBufferLen3M1;
    float diffuserBuffer4ReadLen = diffuserBufferLen4M1;

    float diffuserBuffer1ReadLen_b = diffuserBuffer1ReadLen * 0.1333f;//0.5f;
    float diffuserBuffer2ReadLen_b = diffuserBuffer2ReadLen * 0.3237f;//0.6180339887f;
    float diffuserBuffer3ReadLen_b = diffuserBuffer3ReadLen * 0.17f;//0.5f;
    float diffuserBuffer4ReadLen_b = diffuserBuffer4ReadLen * 0.5237f;//0.6180339887f;

    static float diffuserBuffer1[diffuserBufferLen1];
    static float diffuserBuffer2[diffuserBufferLen2];
    static float diffuserBuffer3[diffuserBufferLen3];
    static float diffuserBuffer4[diffuserBufferLen4];
    int diffuserWritePos1     = 0;
    int diffuserWritePos2     = 0;
    int diffuserWritePos3     = 0;
    int diffuserWritePos4     = 0;

    float diffuserReadPos1;
    float diffuserReadPos2;
    float diffuserReadPos3;
    float diffuserReadPos4;

    float diffuserCoef1 = 0.7f;
    float diffuserCoef2 = 0.5f;

    float monoIn, preDelayOut = 0, diff1Out, diff2Out, diff3Out, diff4Out;
    float ap1In, ap2In, ap3In, ap4In;
    float ap1Out, ap2Out, ap3Out, ap4Out;
    float feedbackInL = 0, feedbackInR = 0;

    // Filter
    float notchBase, prevNotchBase = -1;
    float notchSpread = 0.37f , prevNotchSpread = -1;

    float dcBlock1a, dcBlock1b, dcBlock2a, dcBlock2b;

    float lowL = 0, highL = 0, bandL = 0;
    float lowL2 = 0, highL2 = 0, bandL2 = 0;
    float lowL3 = 0, highL3 = 0, bandL3 = 0;
    float lowL4 = 0, highL4 = 0, bandL4 = 0;

    const float windowMin = 0.005f, windowMax = 0.99f;

    float f1L;
    float f2L;
    float f3L;
    float f4L;

    float coef1L;
    float coef2L;
    float coef3L;
    float coef4L;

    float tankLp1a = 0, tankLp1b = 0;
    float tankLp2a = 0, tankLp2b = 0;
    float inLpF;

    float hp1_x0 = 0;
    float hp1_y0 = 0;
    float hp1_y1 = 0;
    float hp1_x1 = 0;

    float hp2_x0 = 0;
    float hp2_y0 = 0;
    float hp2_y1 = 0;
    float hp2_x1 = 0;

    float _b1, _a0, _a1;

    float hp_in_x0 = 0;
    float hp_in_y0 = 0;
    float hp_in_y1 = 0;
    float hp_in_x1 = 0;

    float _in_lp_a, _in_lp_b;
    float _in_b1, _in_a0, _in_a1;

    const int kl1 = 266     * _dattorroSampleRateMod;
    const int kl2 = 2974     * _dattorroSampleRateMod;
    const int kl3 = 1713     * _dattorroSampleRateMod;
    const int kl4 = 1996     * _dattorroSampleRateMod;
    const int kl5 = 1990     * _dattorroSampleRateMod;
    const int kl6 = 187     * _dattorroSampleRateMod;
    const int kl7 = 1066     * _dattorroSampleRateMod;
    const int _kLeftTaps[7] = {kl1 , kl2, kl3, kl4, kl5, kl6, kl7};

    const int kr1 = 353     * _dattorroSampleRateMod;
    const int kr2 = 3627     * _dattorroSampleRateMod;
    const int kr3 = 1228     * _dattorroSampleRateMod;
    const int kr4 = 2673     * _dattorroSampleRateMod;
    const int kr5 = 2111     * _dattorroSampleRateMod;
    const int kr6 = 335     * _dattorroSampleRateMod;
    const int kr7 = 121     * _dattorroSampleRateMod;
    const int _kRightTaps[7] = {kr1, kr2, kr3, kr4, kr5, kr6, kr7};

};

#endif    // end FX_BUS_
