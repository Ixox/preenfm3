/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PFM3_PARAM_H_
#define PFM3_PARAM_H_

#include <stdint.h>

struct Pfm3TimbreParams;
struct TimbreSharedParams;

/*
 * Stable PFM3 parameter identity.
 * 16 bits: 5 bits block, 5 bits element, 5 bits field.
 * Bit 15 stays free, so 0xFFFF means "no parameter".
 */
enum class Pfm3Block : uint8_t {
    Engine1 = 0,
    Engine2,
    Mod,
    OscMix,
    Osc,
    EnvTime,
    EnvLevel,
    EnvCurve,
    Matrix,
    Performance,
    LfoOsc,
    LfoPhase,
    LfoEnv1,
    LfoEnv2,
    LfoSeq,
    MidiCurve,
    StepSeq,
    Count
};

// Opaque, type-safe 16-bit parameter id (no enumerators; see pfm3Param()).
enum class Pfm3Param : uint16_t {};

constexpr uint16_t PFM3_PARAM_NONE = 0xFFFF;

// element: operator, matrix row, LFO, step... within the block.
// field: parameter within that element (shape, freq, attack...).
constexpr uint16_t pfm3ParamMake(Pfm3Block block, uint8_t element, uint8_t field) {
    return (uint16_t)(((uint16_t)block << 10) | ((element & 0x1F) << 5) | (field & 0x1F));
}
constexpr Pfm3Param pfm3Param(Pfm3Block block, uint8_t element, uint8_t field) {
    return (Pfm3Param)pfm3ParamMake(block, element, field);
}
constexpr uint8_t pfm3ParamBlockOf(Pfm3Param id) {
    return (uint8_t)(((uint16_t)id >> 10) & 0x1F);
}
constexpr uint8_t pfm3ParamElementOf(Pfm3Param id) {
    return (uint8_t)(((uint16_t)id >> 5) & 0x1F);
}
constexpr uint8_t pfm3ParamFieldOf(Pfm3Param id) {
    return (uint8_t)((uint16_t)id & 0x1F);
}
constexpr Pfm3Param pfm3ParamWithBlock(Pfm3Param id, Pfm3Block block) {
    return (Pfm3Param)(((uint16_t)id & ~0x7C00u) | (((uint16_t)block & 0x1Fu) << 10));
}
constexpr Pfm3Param pfm3ParamWithElement(Pfm3Param id, uint8_t element) {
    return (Pfm3Param)(((uint16_t)id & ~0x03E0u) | ((element & 0x1Fu) << 5));
}
constexpr Pfm3Param pfm3ParamWithField(Pfm3Param id, uint8_t field) {
    return (Pfm3Param)(((uint16_t)id & ~0x001Fu) | (field & 0x1Fu));
}

/*
 * Shared (engine-neutral) parameters: Arp/Fx and preset name.
 * A separate namespace so DX7/BYOS common pages never depend on Pfm3Param.
 */
enum class CommonBlock : uint8_t { Arp1 = 0, Arp2, ArpPattern, Effect1, Effect2, PresetName, Count };

enum class CommonParamId : uint16_t {};

constexpr CommonParamId COMMON_PARAM_NONE = (CommonParamId)PFM3_PARAM_NONE;

constexpr CommonParamId commonParam(CommonBlock block, uint8_t element, uint8_t field) {
    return (CommonParamId)pfm3ParamMake((Pfm3Block)block, element, field);
}
constexpr uint8_t commonBlockOf(CommonParamId id) {
    return pfm3ParamBlockOf((Pfm3Param)id);
}
constexpr uint8_t commonElementOf(CommonParamId id) {
    return pfm3ParamElementOf((Pfm3Param)id);
}
constexpr uint8_t commonFieldOf(CommonParamId id) {
    return pfm3ParamFieldOf((Pfm3Param)id);
}

enum ParameterDisplayType {
    DISPLAY_TYPE_NONE = 0,
    DISPLAY_TYPE_FLOAT,
    DISPLAY_TYPE_FLOAT_OSC_FREQUENCY,
    DISPLAY_TYPE_FLOAT_LFO_FREQUENCY,
    DISPLAY_TYPE_INT,
    DISPLAY_TYPE_STRINGS,
    DISPLAY_TYPE_STEP_SEQ1,
    DISPLAY_TYPE_STEP_SEQ2,
    DISPLAY_TYPE_STEP_SEQ_BPM,
    DISPLAY_TYPE_LFO_KSYN,
    DISPLAY_TYPE_ARP_PATTERN,
    DISPLAY_TYPE_SCALA_SCALE
};

enum class ParamAccess : uint8_t { Float, U8, U16, Bitfield, Char, Custom };

enum class MidiTransform : uint8_t {
    Raw,
    Float127,
    Bipolar,
    ImTimes10,
    OscFrequency,
    MatrixMul,
    LfoFrequency,
    EnvAttack,
    EnvRelease,
    Percent,
    Linear8,
    Bipolar64,
    UnisonDetune,
    PlayMode,
    GlideSpeed
};

enum class CcKind : uint8_t { Engine, Shared, EnvAllAttackModulator, EnvAllAttackCarrier, EnvAllReleaseModulator, EnvAllReleaseCarrier };

struct CcEntry {
    uint8_t cc;
    CcKind kind;
    uint16_t id;
    MidiTransform transform;
};

enum class ParamStore : uint8_t { Engine = 0, Shared = 1 };

enum class ParamSource : uint8_t { Ui, Midi, PresetLoad, Internal };

struct ParamDesc {
    ParamAccess access;
    uint16_t fieldOffset;
    float min;
    float max;
    float step; // 0 = raw (char / bitfield / step index), no quantization
};

struct ParamUiDesc {
    const char* name;
    ParameterDisplayType displayType;
    float values;
    const char* const* valueNames;
    const unsigned char* valueOrder;
    const unsigned char* valueOrderReversed;
};

struct ParamFullDescription {
    ParamDesc desc;
    ParamUiDesc ui;
};

struct ParamBlock {
    uint16_t descBase;  // first descriptor for element 0, field 0
    uint16_t liveBase;  // byte offset of element 0 in the live store
    uint16_t flatBase;  // flat Pfm3Preset index for defaults, 0xFFFF if N/A
    uint8_t count;      // number of elements
    uint8_t fieldCount; // fields per element
    uint8_t liveStride; // bytes per element in the live store
    uint8_t flatStride; // flat indices per element
    uint8_t store;      // ParamStore
};

/* Engine table */
const ParamBlock& pfm3Block(Pfm3Param id);
const ParamDesc& pfm3Desc(Pfm3Param id);
const ParamFullDescription& pfm3FullDesc(Pfm3Param id);
uint16_t pfm3DescIndex(Pfm3Param id);
uint16_t pfm3Offset(Pfm3Param id);
uint16_t pfm3FlatIndex(Pfm3Param id);

float pfm3GetParam(const Pfm3TimbreParams& params, Pfm3Param id);
void pfm3SetParam(Pfm3TimbreParams& params, Pfm3Param id, float value);

/* Shared table */
const ParamBlock& commonBlock(CommonParamId id);
const ParamDesc& commonDesc(CommonParamId id);
const ParamFullDescription& commonFullDesc(CommonParamId id);
uint16_t commonDescIndex(CommonParamId id);
uint16_t commonOffset(CommonParamId id);
uint16_t commonFlatIndex(CommonParamId id);

float commonGetParam(const TimbreSharedParams& shared, CommonParamId id);
void commonSetParam(TimbreSharedParams& shared, CommonParamId id, float value);

/* Tables and validation */
Pfm3Param pfm3FlatToParam(uint16_t flat);
CommonParamId commonFlatToParam(uint16_t flat);
const CcEntry* pfm3CcEntry(uint8_t cc);
float pfm3ApplyMidiTransform(MidiTransform transform, uint8_t value);
void pfm3InitTables();
bool pfm3ValidateTables();

bool pfm3ValidateDefaults(const Pfm3TimbreParams& params, const TimbreSharedParams& shared);

#endif