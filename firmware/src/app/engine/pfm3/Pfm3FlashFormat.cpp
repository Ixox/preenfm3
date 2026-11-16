/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Pfm3FlashFormat.h"

static inline void copyFloat(float* source, float* dest, int n) {
    for (int k = 0; k < n; k++) {
        dest[k] = source[k];
    }
}

/*******************************************************
 *  engine2.pfm3Version : Set the version we save.
 *  Used by convertFlashToParams to make sure what we're reading from the SD card
 *
 *  > version 1.0
 *    engine2.spread and engine2.detune available
 *    engine1.monoPoly has a new value : 2 for unison
 *
 *********************************************************/
void pfm3ParamsToFlash(const struct Pfm3Preset* params, struct FlashSynthParams* flashMemory, bool saveArp) {
    // Engines line
    copyFloat((float*)&params->engine1, (float*)&flashMemory->engine1, 4);
    copyFloat((float*)&params->engine2, (float*)&flashMemory->engine2, 4);

    // VERSION : 1.0
    flashMemory->engine2.pfm3Version = 1.0f;

    if (saveArp) {
        copyFloat((float*)&params->engineArp1, (float*)&flashMemory->engineArp1, 4 * 2);
        flashMemory->engineArpUserPatterns = params->engineArpUserPatterns;
    } else {
        flashMemory->engineArp1.clock = 0;
        flashMemory->engineArp1.BPM = 90;
        flashMemory->engineArp1.octave = 1;
        flashMemory->engineArp2.pattern = 2;
        flashMemory->engineArp2.division = 12;
        flashMemory->engineArp2.duration = 14;
        flashMemory->engineArp2.latche = 0;
        for (int p = 0; p < ARRAY_SIZE(flashMemory->engineArpUserPatterns.patterns); ++p)
            flashMemory->engineArpUserPatterns.patterns[p] = 0;
    }

    flashMemory->flashEngineIm1.modulationIndex1 = params->engineIm1.modulationIndex1;
    flashMemory->flashEngineIm1.modulationIndex2 = params->engineIm1.modulationIndex2;
    flashMemory->flashEngineIm1.modulationIndex3 = params->engineIm2.modulationIndex3;
    flashMemory->flashEngineIm1.modulationIndex4 = params->engineIm2.modulationIndex4;
    flashMemory->flashEngineIm2.modulationIndex5 = params->engineIm3.modulationIndex5;
    flashMemory->flashEngineIm2.modulationIndex6 = params->engineIm3.modulationIndex6;
    flashMemory->flashEngineIm2.notUsed1 = 0.0f;
    flashMemory->flashEngineIm2.notUsed2 = 0.0f;

    flashMemory->flashEngineVeloIm1.modulationIndexVelo1 = params->engineIm1.modulationIndexVelo1;
    flashMemory->flashEngineVeloIm1.modulationIndexVelo2 = params->engineIm1.modulationIndexVelo2;
    flashMemory->flashEngineVeloIm1.modulationIndexVelo3 = params->engineIm2.modulationIndexVelo3;
    flashMemory->flashEngineVeloIm1.modulationIndexVelo4 = params->engineIm2.modulationIndexVelo4;
    flashMemory->flashEngineVeloIm2.modulationIndexVelo5 = params->engineIm3.modulationIndexVelo5;
    flashMemory->flashEngineVeloIm2.modulationIndexVelo6 = params->engineIm3.modulationIndexVelo6;
    flashMemory->flashEngineVeloIm2.notUsed1 = 0.0f;
    flashMemory->flashEngineVeloIm2.notUsed1 = 0.0f;

    copyFloat((float*)&params->engineMix1, (float*)&flashMemory->engineMix1, 4 * 3);
    copyFloat((float*)&params->effect1, (float*)&flashMemory->effect, 4);
    copyFloat((float*)&params->effect2, (float*)&flashMemory->effect2, 4);
    copyFloat((float*)&params->osc1, (float*)&flashMemory->osc1, 4 * 6);

    // Copy env time and levels from new memory mapping (31/12/2022)
    const EnvelopeTimeMemory* envTime = &params->env1Time;
    const EnvelopeLevelMemory* envLevel = &params->env1Level;
    float* envFlashFloat = (float*)&flashMemory->env1a;
    for (int e = 0; e < 6; e++) {
        envFlashFloat[e * 8] = envTime[e].attackTime;
        envFlashFloat[e * 8 + 2] = envTime[e].decayTime;
        envFlashFloat[e * 8 + 4] = envTime[e].sustainTime;
        envFlashFloat[e * 8 + 6] = envTime[e].releaseTime;

        envFlashFloat[e * 8 + 1] = envLevel[e].attackLevel;
        envFlashFloat[e * 8 + 3] = envLevel[e].decayLevel;
        envFlashFloat[e * 8 + 5] = envLevel[e].sustainLevel;
        envFlashFloat[e * 8 + 7] = envLevel[e].releaseLevel;
    }

    // copy envCurve to int8_t flash memory
    const EnvelopeCurveParams* envCurveParam = &params->env1Curve;
    uint8_t* envFlashUint8 = (uint8_t*)&flashMemory->envCurves1To4;
    for (int e = 0; e < 6; e++) {
        envFlashUint8[e * 4 + 0] = (uint8_t)envCurveParam[e].attackCurve;
        envFlashUint8[e * 4 + 1] = (uint8_t)envCurveParam[e].decayCurve;
        envFlashUint8[e * 4 + 2] = (uint8_t)envCurveParam[e].sustainCurve;
        envFlashUint8[e * 4 + 3] = (uint8_t)envCurveParam[e].releaseCurve;
    }

    copyFloat((float*)&params->matrixRowState1, (float*)&flashMemory->matrixRowState1, 4 * 12);
    copyFloat((float*)&params->lfoOsc1, (float*)&flashMemory->lfoOsc1, 4 * 3);
    copyFloat((float*)&params->lfoEnv1, (float*)&flashMemory->lfoEnv1, 4);
    copyFloat((float*)&params->lfoEnv2, (float*)&flashMemory->lfoEnv2, 4);
    copyFloat((float*)&params->lfoSeq1, (float*)&flashMemory->lfoSeq1, 4 * 2);

    copyFloat((float*)&params->midiNote1Curve, (float*)&flashMemory->midiNote1Curve, 4);
    copyFloat((float*)&params->midiNote2Curve, (float*)&flashMemory->midiNote2Curve, 4);
    copyFloat((float*)&params->lfoPhases, (float*)&flashMemory->lfoPhases, 4);

    for (int s = 0; s < 16; s++) {
        flashMemory->lfoSteps1.steps[s] = params->lfoSteps1.steps[s];
        flashMemory->lfoSteps2.steps[s] = params->lfoSteps2.steps[s];
    }
    for (int s = 0; s < 13; s++) {
        flashMemory->presetName[s] = params->presetName[s];
    }
}

inline int iszero(void* ptr, int bytes) {
    char* bptr = (char*)ptr;
    while (bytes--)
        if (*bptr++)
            return 0;
    return 1;
}

void pfm3FlashToParams(const struct FlashSynthParams* flashMemory, struct Pfm3Preset* params, bool loadArp) {
    // First engine line
    copyFloat((float*)&flashMemory->engine1, (float*)&params->engine1, 4);
    copyFloat((float*)&flashMemory->engine2, (float*)&params->engine2, 4);

    if (loadArp) {
        copyFloat((float*)&flashMemory->engineArp1, (float*)&params->engineArp1, 4 * 2);
        params->engineArpUserPatterns = flashMemory->engineArpUserPatterns;
    }

    params->engineIm1.modulationIndex1 = flashMemory->flashEngineIm1.modulationIndex1;
    params->engineIm1.modulationIndex2 = flashMemory->flashEngineIm1.modulationIndex2;
    params->engineIm2.modulationIndex3 = flashMemory->flashEngineIm1.modulationIndex3;
    params->engineIm2.modulationIndex4 = flashMemory->flashEngineIm1.modulationIndex4;
    params->engineIm3.modulationIndex5 = flashMemory->flashEngineIm2.modulationIndex5;
    params->engineIm3.modulationIndex6 = flashMemory->flashEngineIm2.modulationIndex6;
    params->engineIm1.modulationIndexVelo1 = flashMemory->flashEngineVeloIm1.modulationIndexVelo1;
    params->engineIm1.modulationIndexVelo2 = flashMemory->flashEngineVeloIm1.modulationIndexVelo2;
    params->engineIm2.modulationIndexVelo3 = flashMemory->flashEngineVeloIm1.modulationIndexVelo3;
    params->engineIm2.modulationIndexVelo4 = flashMemory->flashEngineVeloIm1.modulationIndexVelo4;
    params->engineIm3.modulationIndexVelo5 = flashMemory->flashEngineVeloIm2.modulationIndexVelo5;
    params->engineIm3.modulationIndexVelo6 = flashMemory->flashEngineVeloIm2.modulationIndexVelo6;

    copyFloat((float*)&flashMemory->engineMix1, (float*)&params->engineMix1, 4 * 3);
    copyFloat((float*)&flashMemory->effect, (float*)&params->effect1, 4);
    copyFloat((float*)&flashMemory->effect2, (float*)&params->effect2, 4);
    copyFloat((float*)&flashMemory->osc1, (float*)&params->osc1, 4 * 6);

    // Copy env time and levels to new memory mapping (31/12/2022)
    EnvelopeTimeMemory* envTime = &params->env1Time;
    EnvelopeLevelMemory* envLevel = &params->env1Level;
    float* envFlashFloat = (float*)&flashMemory->env1a;
    for (int e = 0; e < 6; e++) {
        envTime[e].attackTime = envFlashFloat[e * 8];
        envTime[e].decayTime = envFlashFloat[e * 8 + 2];
        envTime[e].sustainTime = envFlashFloat[e * 8 + 4];
        envTime[e].releaseTime = envFlashFloat[e * 8 + 6];

        envLevel[e].attackLevel = envFlashFloat[e * 8 + 1];
        envLevel[e].decayLevel = envFlashFloat[e * 8 + 3];
        envLevel[e].sustainLevel = envFlashFloat[e * 8 + 5];
        envLevel[e].releaseLevel = envFlashFloat[e * 8 + 7];

        //  A bug created banks with level > 1
        if (envLevel[e].attackLevel > 1.0) {
            envLevel[e].attackLevel = 1.0;
        }
        if (envLevel[e].decayLevel > 1.0) {
            envLevel[e].decayLevel = 1.0;
        }
        if (envLevel[e].sustainLevel > 1.0) {
            envLevel[e].sustainLevel = 1.0;
        }
        if (envLevel[e].releaseLevel > 1.0) {
            envLevel[e].releaseLevel = 1.0;
        }
    }

    // copy envCurve from int8_t flash memory
    EnvelopeCurveParams* envCurveParam = &params->env1Curve;
    uint8_t* envFlashUint8 = (uint8_t*)&flashMemory->envCurves1To4;

    if (iszero(envFlashUint8, 24)) {
        // Set default env curve if not initialized
        for (int e = 0; e < 6; e++) {
            envCurveParam[e].attackCurve = 1.0f;
            envCurveParam[e].decayCurve = 0.0f;
            envCurveParam[e].sustainCurve = 1.0f;
            envCurveParam[e].releaseCurve = 0.0f;
        }
    } else {
        for (int e = 0; e < 6; e++) {
            envCurveParam[e].attackCurve = (float)envFlashUint8[e * 4 + 0];
            envCurveParam[e].decayCurve = (float)envFlashUint8[e * 4 + 1];
            envCurveParam[e].sustainCurve = (float)envFlashUint8[e * 4 + 2];
            envCurveParam[e].releaseCurve = (float)envFlashUint8[e * 4 + 3];
        }
    }

    copyFloat((float*)&flashMemory->matrixRowState1, (float*)&params->matrixRowState1, 4 * 12);
    copyFloat((float*)&flashMemory->lfoOsc1, (float*)&params->lfoOsc1, 4 * 3);
    copyFloat((float*)&flashMemory->lfoEnv1, (float*)&params->lfoEnv1, 4);
    copyFloat((float*)&flashMemory->lfoEnv2, (float*)&params->lfoEnv2, 4);
    copyFloat((float*)&flashMemory->lfoSeq1, (float*)&params->lfoSeq1, 4 * 2);

    copyFloat((float*)&flashMemory->lfoPhases, (float*)&params->lfoPhases, 4);
    copyFloat((float*)&flashMemory->midiNote1Curve, (float*)&params->midiNote1Curve, 4);
    copyFloat((float*)&flashMemory->midiNote2Curve, (float*)&params->midiNote2Curve, 4);

    for (int s = 0; s < 16; s++) {
        params->lfoSteps1.steps[s] = flashMemory->lfoSteps1.steps[s];
        params->lfoSteps2.steps[s] = flashMemory->lfoSteps2.steps[s];
    }
    for (int s = 0; s < 13; s++) {
        params->presetName[s] = flashMemory->presetName[s];
    }

    params->performance1.perf1 = 0.0f;
    params->performance1.perf2 = 0.0f;
    params->performance1.perf3 = 0.0f;
    params->performance1.perf4 = 0.0f;

    // Initialized not initialize params in flashMemory
    if (params->engineArp1.BPM < 10) {
        params->engineArp1.clock = 0;
        params->engineArp1.BPM = 90;
        params->engineArp1.octave = 1;
        params->engineArp2.pattern = 2;
        params->engineArp2.division = 12;
        params->engineArp2.duration = 14;
        params->engineArp2.latche = 0;
    }

    // No effect in preset, set default values
    if (params->effect1.type == 0.0f && params->effect1.param1 == 0.0f && params->effect1.param2 == 0.0f &&
        params->effect1.param3 == 0.0f) {
        params->effect1.param1 = 0.5f;
        params->effect1.param2 = 0.5f;
        params->effect1.param3 = 1.0f;
    }

    // No effect2 in preset, set default values
    if (params->effect2.type == 0.0f && params->effect2.param1 == 0.0f && params->effect2.param2 == 0.0f &&
        params->effect2.param3 == 0.0f) {
        params->effect2.param1 = 0.5f;
        params->effect2.param2 = 0.5f;
        params->effect2.param3 = 1.0f;
    }

    // Set default env curve if not initialized
    if (iszero(&params->env1Curve, 24)) {
        params->env1Curve = {1, 0, 1, 0};
        params->env2Curve = {1, 0, 1, 0};
        params->env3Curve = {1, 0, 1, 0};
        params->env4Curve = {1, 0, 1, 0};
        params->env5Curve = {1, 0, 1, 0};
        params->env6Curve = {1, 0, 1, 0};
    }

    if (params->midiNote1Curve.breakNote == 0.0f && params->midiNote1Curve.curveAfter == 0.0f &&
        params->midiNote1Curve.curveBefore == 0.0f) {
        // Default compatibility value
        // FLAT 0 +Lin
        params->midiNote1Curve.curveAfter = 1;
    }
    if (params->midiNote2Curve.breakNote == 0.0f && params->midiNote2Curve.curveAfter == 0.0f &&
        params->midiNote2Curve.curveBefore == 0.0f) {
        // Default compatibility value
        // FLAT 0 +Lin
        params->midiNote2Curve.curveBefore = 4;
        params->midiNote2Curve.curveAfter = 1;
        params->midiNote2Curve.breakNote = 60;
    }

    // Fixe poly Mono depending on pfm3Version :
    uint32_t version = (uint32_t)(params->engine2.pfm3Version + .1f);
    if (version == 0) {
        // map to new parameters
        // Compatible with pfm2 bank
        // in pfm2 bank playMode was the number of voice
        if (params->engine1.playMode > 2.0f ||
            (params->engine1.playMode == 2.0f && params->engine2.unisonSpread == 0.0f && params->engine2.unisonDetune == 0.0f)) {
            params->engine1.playMode = PLAY_MODE_POLY;
        }
        // Old preset, we set unisonSpread and unisonDetune different from 0
        if (params->engine2.unisonSpread == 0.0f && params->engine2.unisonDetune == 0.0f) {
            params->engine2.unisonSpread = 0.5f;
            params->engine2.unisonDetune = 0.12f;
        }
        // Map glide speed to new glideSpeed and glideType
        if (params->engine1.glideSpeed > 0.0f) {
            params->engine2.glideType = GLIDE_TYPE_OVERLAP;
        } else {
            params->engine2.glideType = GLIDE_TYPE_OFF;
        }
    }
}
