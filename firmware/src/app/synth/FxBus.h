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
	friend class ScreenMixer;
    friend class MixerState;
public:
    FxBus();
    virtual ~FxBus() {}
    void init();
    void setDefaultValue();
    void mixSumInit();
    void presetChanged(int presetNum);
    void paramChanged();
    void slowParamChange();
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

    // Reverb params
    float masterfxConfig_[GLOBALFX_PARAMS_SIZE];

    bool somethingChanged_ = false;
    int waitCountBeforeChange_ = 0;
    int nextPresetNum_ = 0;

protected:

    #define _dattorroSampleRateMod PREENFM_FREQUENCY / 29761.0f

    int currentPresetNum_ = 0;

    float headRoomMultiplier_ = 1;
    float sampleMultipler_;

    float totalSent_ = 0;

    //lfo
    float lfo1_ = 0;
    float lfo1tri_;
    float lfo1Inc_ = 0.00037f;

    float lfo2_ = 0;
    float lfo2tri_;
    float lfo2Inc_ = lfo1Inc_ * 1.07f;

    float lfo3_ = 1;
    float lfo3tri_;
    float lfo3Inc_ = lfo1Inc_;

    float lfo4_ = 1;
    float lfo4tri_;
    float lfo4Inc_ = lfo2Inc_;


    float sampleBlock_[BLOCK_SIZE * 2];
    float *sample_;

    float fxTime_ = 0.98, prevTime_ = -1, fxTimeLinear_, prevFxTimeLinear_ = -1;
    float prevdecayVal_ = 0;
    float decayVal_ = 0.5, prevDecayVal_ = -1, decayFdbck_ = 0;
    const float decayMaxVal_ = 0.96f;
    float sizeParam_, prevSizeParam_;
    float inputDiffusion_, prevInputDiffusion_;
    float diffusion_, prevDiffusion_;
    float damping_, prevDamping_ = -1;
    float predelayMixLevel_ = 0.5f;
    float predelayMixAttn_ = predelayMixLevel_ * 0.75f;
    float lfoDepth_ =  0;
    float lfoSpeed_ = 0, lfoSpeedLinear_ = 0, prevLfoSpeedLinear_ = -1;
    float loopLpf_, loopLpf2_, prevFilterBase_ = -1;
    float damp_a_, damp_b_;
    float inputWidth_ = 0.5f, prevInputWidth_ = -1;
    float prevLoopHp_ = -1;
    float lfoLpCoef1_ = 3999, lfoLpCoef2_ = 0.00025f;
    float timeCvControl1_ = 0, timeCvControl2_ = 0, timeCvControl3_ = 0, timeCvControl4_ = 0;
    float timeCv_ = 0, prevTimeCv_ = 0, timeCvSpeed_ = 0, prevtimeCvSpeed_ = 0, cvDelta_;

    float loopHpf_ = 0.14f;
    float inHpf_ = 0.1f;

    float inR_, inL_;

    float outL_, outR_;

    float int1_ = 0, int2_ = 0, int3_ = 0, int4_ = 0;

    static const int delay1BufferSize     = 4453 * _dattorroSampleRateMod;
    static const int delay1BufferSizeM1    = delay1BufferSize - 1;
    static float delay1Buffer[delay1BufferSize];
    int delay1WritePos_     = 0;
    int delay1ReadPos_     = 0;
    float delay1DelayLen_     = 0, delay1ReadLen_;
    float delay1FxTarget_     = 0;

    static const int delay2BufferSize     = 3720 * _dattorroSampleRateMod;
    static const int delay2BufferSizeM1    = delay2BufferSize - 1;
    static float delay2Buffer[delay2BufferSize];
    int delay2WritePos_     = 0;
    int delay2ReadPos_;
    float delay2DelayLen_     = 0, delay2ReadLen_;
    float delay2FxTarget_     = 0;

    static const int delay3BufferSize     = 4217 * _dattorroSampleRateMod;
    static const int delay3BufferSizeM1    = delay3BufferSize - 1;
    static float delay3Buffer[delay3BufferSize];
    int delay3WritePos_     = 0;
    int delay3ReadPos_     = 0;
    float delay3DelayLen_     = 0, delay3ReadLen_;
    float delay3FxTarget_     = 0;

    static const int delay4BufferSize     = 3163 * _dattorroSampleRateMod;
    static const int delay4BufferSizeM1    = delay4BufferSize - 1;
    static float delay4Buffer[delay4BufferSize];
    int delay4WritePos_     = 0;
    int delay4ReadPos_;
    float delay4DelayLen_     = 0, delay4ReadLen_;
    float delay4FxTarget_     = 0;

    const float dcBlockerCoef1_ = 0.9973854301f;//~20 hz

    //pre delay

    static const int predelayBufferSize     = 8000;
    static const int predelayBufferSizeM1     = predelayBufferSize - 1;
    static float predelayBuffer[predelayBufferSize];
    int predelaySize_             = predelayBufferSize;
    int predelayWritePos_         = 0;
    float predelayReadPos_         = 0;
    const int inputRateDivider_ = 2;
    int inputIncCount_ = 0;

    // input diffuser

    static const int inputBufferLen1 = 141 * _dattorroSampleRateMod;
    static const int inputBufferLen2 = 107 * _dattorroSampleRateMod;
    static const int inputBufferLen3 = 379 * _dattorroSampleRateMod;
    static const int inputBufferLen4 = 277 * _dattorroSampleRateMod;
    static float inputBuffer1[inputBufferLen1];
    static float inputBuffer2[inputBufferLen2];
    static float inputBuffer3[inputBufferLen3];
    static float inputBuffer4[inputBufferLen4];
    float inputBuffer1ReadLen_ = inputBufferLen1;
    float inputBuffer2ReadLen_ = inputBufferLen2;
    float inputBuffer3ReadLen_ = inputBufferLen3;
    float inputBuffer4ReadLen_ = inputBufferLen4;
    int inputWritePos1_     = 0;
    int inputWritePos2_     = 0;
    int inputWritePos3_     = 0;
    int inputWritePos4_     = 0;

    int inputReadPos1_;
    int inputReadPos2_;
    int inputReadPos3_;
    int inputReadPos4_;

    float inputCoef1_ = 0.75f;
    float inputCoef2_ = 0.625f;

    // diffuser decay

    static const int diffuserBufferLen1 = 672     * _dattorroSampleRateMod;
    static const int diffuserBufferLen2 = 1800     * _dattorroSampleRateMod;
    static const int diffuserBufferLen3 = 908     * _dattorroSampleRateMod;
    static const int diffuserBufferLen4 = 2656     * _dattorroSampleRateMod;
    static const int diffuserBufferLen1M1 = diffuserBufferLen1 - 1;
    static const int diffuserBufferLen2M1 = diffuserBufferLen2 - 1;
    static const int diffuserBufferLen3M1 = diffuserBufferLen3 - 1;
    static const int diffuserBufferLen4M1 = diffuserBufferLen4 - 1;
    float diffuserBuffer1ReadLen_ = diffuserBufferLen1M1;
    float diffuserBuffer2ReadLen_ = diffuserBufferLen2M1;
    float diffuserBuffer3ReadLen_ = diffuserBufferLen3M1;
    float diffuserBuffer4ReadLen_ = diffuserBufferLen4M1;

    float diffuserBuffer1ReadLen_b_ = diffuserBuffer1ReadLen_ * 0.1333f;//0.5f;
    float diffuserBuffer2ReadLen_b_ = diffuserBuffer2ReadLen_ * 0.3237f;//0.6180339887f;
    float diffuserBuffer3ReadLen_b_ = diffuserBuffer3ReadLen_ * 0.17f;//0.5f;
    float diffuserBuffer4ReadLen_b_ = diffuserBuffer4ReadLen_ * 0.5237f;//0.6180339887f;

    static float diffuserBuffer1[diffuserBufferLen1];
    static float diffuserBuffer2[diffuserBufferLen2];
    static float diffuserBuffer3[diffuserBufferLen3];
    static float diffuserBuffer4[diffuserBufferLen4];
    int diffuserWritePos1_     = 0;
    int diffuserWritePos2_     = 0;
    int diffuserWritePos3_     = 0;
    int diffuserWritePos4_     = 0;

    float diffuserReadPos1_;
    float diffuserReadPos2_;
    float diffuserReadPos3_;
    float diffuserReadPos4_;

    float diffuserCoef1_ = 0.7f;
    float diffuserCoef2_ = 0.5f;

    float monoIn_, preDelayOut_ = 0, diff1Out_, diff2Out_, diff3Out_, diff4Out_;
    float ap1In_, ap2In_, ap3In_, ap4In_;
    float ap1Out_, ap2Out_, ap3Out_, ap4Out_;
    float feedbackInL_ = 0, feedbackInR_ = 0;

    // Filter
    float notchBase_, prevNotchBase_ = -1;
    float notchSpread_ = 0.37f , prevNotchSpread_ = -1;

    float dcBlock1a_, dcBlock1b_, dcBlock2a_, dcBlock2b_;

    float lowL_ = 0, highL_ = 0, bandL_ = 0;
    float lowL2_ = 0, highL2_ = 0, bandL2_ = 0;
    float lowL3_ = 0, highL3_ = 0, bandL3_ = 0;
    float lowL4_ = 0, highL4_ = 0, bandL4_ = 0;

    const float windowMin_ = 0.005f, windowMax_ = 0.99f;

    float f1L_;
    float f2L_;
    float f3L_;
    float f4L_;

    float coef1L_;
    float coef2L_;
    float coef3L_;
    float coef4L_;

    float tankLp1a_ = 0, tankLp1b_ = 0;
    float tankLp2a_ = 0, tankLp2b_ = 0;
    float inLpF_;

    float hp1_x0_ = 0;
    float hp1_y0_ = 0;
    float hp1_y1_ = 0;
    float hp1_x1_ = 0;

    float hp2_x0_ = 0;
    float hp2_y0_ = 0;
    float hp2_y1_ = 0;
    float hp2_x1_ = 0;

    float _b1_, _a0_, _a1_;

    float hp_in_x0_ = 0;
    float hp_in_y0_ = 0;
    float hp_in_y1_ = 0;
    float hp_in_x1_ = 0;

    float _in_lp_a_, _in_lp_b_;
    float _in_b1_, _in_a0_, _in_a1_;

    const int kl1_ = 266     * _dattorroSampleRateMod;
    const int kl2_ = 2974     * _dattorroSampleRateMod;
    const int kl3_ = 1713     * _dattorroSampleRateMod;
    const int kl4_ = 1996     * _dattorroSampleRateMod;
    const int kl5_ = 1990     * _dattorroSampleRateMod;
    const int kl6_ = 187     * _dattorroSampleRateMod;
    const int kl7_ = 1066     * _dattorroSampleRateMod;
    const int _kLeftTaps_[7] = {kl1_ , kl2_, kl3_, kl4_, kl5_, kl6_, kl7_};

    const int kr1_ = 353     * _dattorroSampleRateMod;
    const int kr2_ = 3627     * _dattorroSampleRateMod;
    const int kr3_ = 1228     * _dattorroSampleRateMod;
    const int kr4_ = 2673     * _dattorroSampleRateMod;
    const int kr5_ = 2111     * _dattorroSampleRateMod;
    const int kr6_ = 335     * _dattorroSampleRateMod;
    const int kr7_ = 121     * _dattorroSampleRateMod;
    const int _kRightTaps_[7] = {kr1_, kr2_, kr3_, kr4_, kr5_, kr6_, kr7_};

};

#endif    // end FX_BUS_
