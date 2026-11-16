/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Pfm3ParameterRows.h"
#include "SynthState.h"

extern struct ParameterRowDisplay engine1ParameterRow;
extern struct ParameterRowDisplay engine2ParameterRow;
extern struct ParameterRowDisplay engineIM1ParameterRow;
extern struct ParameterRowDisplay engineIM2ParameterRow;
extern struct ParameterRowDisplay engineIM3ParameterRow;
extern struct ParameterRowDisplay engineMix1ParameterRow;
extern struct ParameterRowDisplay engineMix2ParameterRow;
extern struct ParameterRowDisplay engineMix3ParameterRow;
extern struct ParameterRowDisplay engineArp1ParameterRow;
extern struct ParameterRowDisplay engineArp2ParameterRow;
extern struct ParameterRowDisplay engineArpPatternRow;
extern struct ParameterRowDisplay engineCurveParameterRow;
extern struct ParameterRowDisplay fx1ParameterRow;
extern struct ParameterRowDisplay fx2ParameterRow;
extern struct ParameterRowDisplay oscParameterRow;
extern struct ParameterRowDisplay envParameterTime;
extern struct ParameterRowDisplay envParameterLevel;
extern struct ParameterRowDisplay matrixParameterRow;
extern struct ParameterRowDisplay performanceParameterRow1;
extern struct ParameterRowDisplay lfoParameterRow;
extern struct ParameterRowDisplay lfoPhaseParameterRow;
extern struct ParameterRowDisplay lfoEnvParameterRow;
extern struct ParameterRowDisplay lfoEnv2ParameterRow;
extern struct ParameterRowDisplay lfoStepParameterRow;
extern struct ParameterRowDisplay midiNote1ParameterRow;
extern struct ParameterRowDisplay midiNote2ParameterRow;
extern struct ParameterRowDisplay dummyParameterRow;

struct AllParameterRowsDisplay allParameterRows = {
    {
        &engine1ParameterRow,
        &engineIM1ParameterRow,
        &engineIM2ParameterRow,
        &engineIM3ParameterRow,
        &engineMix1ParameterRow,
        &engineMix2ParameterRow,
        &engineMix3ParameterRow,
        &engineArp1ParameterRow,
        &engineArp2ParameterRow,
        &engineArpPatternRow,
        &fx1ParameterRow,
        &oscParameterRow,
        &oscParameterRow,
        &oscParameterRow,
        &oscParameterRow,
        &oscParameterRow,
        &oscParameterRow,
        &envParameterTime,
        &envParameterTime,
        &envParameterTime,
        &envParameterTime,
        &envParameterTime,
        &envParameterTime,
        &envParameterLevel,
        &envParameterLevel,
        &envParameterLevel,
        &envParameterLevel,
        &envParameterLevel,
        &envParameterLevel,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &performanceParameterRow1,
        &lfoParameterRow,
        &lfoParameterRow,
        &lfoParameterRow,
        &lfoPhaseParameterRow,
        &lfoEnvParameterRow,
        &lfoEnv2ParameterRow,
        &lfoStepParameterRow,
        &lfoStepParameterRow,
        &midiNote1ParameterRow,
        &midiNote2ParameterRow,
        &dummyParameterRow,
        &dummyParameterRow,
        &engine2ParameterRow,
        &engineCurveParameterRow,
        &engineCurveParameterRow,
        &engineCurveParameterRow,
        &engineCurveParameterRow,
        &engineCurveParameterRow,
        &engineCurveParameterRow,
        &fx2ParameterRow

} };
