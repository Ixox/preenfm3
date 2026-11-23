/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PFM3_PARAM_UI_DATA_H_
#define PFM3_PARAM_UI_DATA_H_

#include "Pfm3Param.h"

/* Full table: engine/storage + editor + MIDI per field. */
extern const ParamFullDescription pfm3EngineParams[72];
extern const ParamFullDescription pfm3CommonParams[68];

/*
 * Editor name/order tables. They live in Pfm3ParamTable.cpp so the full
 * parameter table is compiled by the host tests.
 */
extern const char* algoNames[];
extern const char* polyMonoNames[];
extern const char* glideTypeNames[];
extern const char* clockName[];
extern const char* dirName[];
extern const char* divNames[];
extern const char* activeName[];
extern const char* patternName[];
extern const char* envCurveNames[];
extern const unsigned char filtersOrder[];
extern const unsigned char filtersPosition[];
extern const char* fxName[];
extern const char* fx2Name[];
extern const char* oscShapeNames[];
extern const unsigned char oscShapeNamesOrder[];
extern const unsigned char oscShapeNamesPosition[];
extern const char* oscTypeNames[];
extern const unsigned char oscTypeNamesOrder[];
extern const unsigned char oscTypeNamesPosition[];
extern const char* lofEnv2Loop[];
extern const char* matrixSourceNames[];
extern const unsigned char matrixSourceOrder[];
extern unsigned char matrixSourcePosition[];
extern const char* matrixDestNames[];
extern const unsigned char matrixTargetOrder[];
extern unsigned char matrixTargetPosition[];
extern const char* lfoShapeNames[];
extern const char* midiNoteCurves[];

#endif