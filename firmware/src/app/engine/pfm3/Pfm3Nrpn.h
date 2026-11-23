/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PFM3_NRPN_H_
#define PFM3_NRPN_H_

#include <stdint.h>

#include "Common.h"
#include "MidiCapabilities.h"
#include "Pfm3Param.h"

/*
 * NRPN protocol v2 (see doc/nrpn_spec_v2.md).
 *
 * The NRPN MSB selects the block. The LSB is (element * fieldCount + field)
 * for the parameter blocks, the field index for Mixer/Global, and the
 * character index for the preset name. MSB 127 carries the control commands.
 */
namespace Pfm3Nrpn {

/* Block ids (the NRPN MSB). */
constexpr uint8_t MSB_ENGINE_FIRST = 0;
constexpr uint8_t MSB_ENGINE_LAST = (uint8_t)Pfm3Block::Count - 1; // 16

constexpr uint8_t MSB_COMMON_FIRST = 17;
// PresetName has its own MSB, so only the first common blocks are numbered.
constexpr uint8_t COMMON_BLOCK_COUNT = (uint8_t)CommonBlock::Count - 1;
constexpr uint8_t MSB_COMMON_LAST = MSB_COMMON_FIRST + COMMON_BLOCK_COUNT - 1; // 21

constexpr uint8_t MSB_PRESET_NAME = 99;
constexpr uint8_t MSB_MIXER = 100;
constexpr uint8_t MSB_GLOBAL = 101;

constexpr uint8_t MSB_RESERVED_FIRST = 22;
constexpr uint8_t MSB_RESERVED_LAST = 98;

constexpr uint8_t MSB_CONTROL = 127;

/* Command domain: commands 0..63, responses 64..127. */
constexpr uint8_t CONTROL_LAST_COMMAND = 63;
constexpr uint8_t CONTROL_FIRST_RESPONSE = 64;

enum Command : uint8_t { REQUEST_PRESET_DUMP = 0, REQUEST_BLOCK_DUMP = 1, REQUEST_PARAM = 2, REQUEST_CAPABILITIES = 3, COMMAND_COUNT = 4 };

enum Response : uint8_t {
    RESPONSE_DUMP_BEGIN = 64,
    RESPONSE_DUMP_END = 65,
    RESPONSE_ERROR = 66,
    RESPONSE_CAP_PROTOCOL_VERSION = 67,
    RESPONSE_CAP_FEATURE_BITS_LO = 68,
    RESPONSE_CAP_FEATURE_BITS_HI = 69,
    RESPONSE_CAP_MAX_VOICES = 70,
    RESPONSE_CAP_MAX_TIMBRES = 71,
    RESPONSE_COUNT = 72
};

enum Error : uint8_t { ERROR_UNKNOWN_PARAMETER = 0, ERROR_VALUE_OUT_OF_RANGE = 1, ERROR_UNKNOWN_COMMAND = 2, ERROR_UNSUPPORTED_BLOCK = 3 };

constexpr uint8_t PROTOCOL_VERSION = MIDI_CAPABILITY_PROTOCOL_VERSION;
constexpr uint8_t CAP_MAX_VOICES = (uint8_t)MAX_NUMBER_OF_VOICES;
constexpr uint8_t CAP_MAX_TIMBRES = (uint8_t)NUMBER_OF_TIMBRES;
constexpr uint32_t SUPPORTED_FEATURES = (uint32_t)MIDI_SUPPORTED_FEATURES;

constexpr uint16_t VALUE_MAX = 0x3FFF;

constexpr uint8_t PRESET_NAME_LENGTH = 12;
constexpr uint8_t MIXER_FIELD_COUNT = 13;
constexpr uint8_t GLOBAL_FIELD_COUNT = 13;

/* Scope 0 = preset + addressed mixer + global. */
constexpr uint16_t DUMP_SCOPE_FULL = 0;

constexpr uint16_t nrpnAddress(uint8_t msb, uint8_t lsb) {
    return (uint16_t)(((uint16_t)(msb & 0x7F) << 7) | (lsb & 0x7F));
}

constexpr uint8_t nrpnMsb(uint16_t address) {
    return (uint8_t)(address >> 7);
}
constexpr uint8_t nrpnLsb(uint16_t address) {
    return (uint8_t)(address & 0x7F);
}

constexpr bool isEngineBlock(uint8_t msb) {
    return msb <= MSB_ENGINE_LAST;
}
constexpr bool isCommonBlock(uint8_t msb) {
    return msb >= MSB_COMMON_FIRST && msb <= MSB_COMMON_LAST;
}
constexpr bool isControl(uint8_t msb) {
    return msb == MSB_CONTROL;
}

constexpr bool isRawAccess(ParamAccess access) {
    return access != ParamAccess::Float;
}

enum class Target : uint8_t { None, Engine, Common, PresetName, Mixer, Global };

struct Address {
    Target target;
    uint16_t id; // Pfm3Param / CommonParamId bits
    uint8_t element;
    uint8_t field;
    uint8_t error; // valid when target == Target::None
    bool ok() const {
        return target != Target::None;
    }
};

/* Map a (MSB, LSB) pair to a target parameter or an error code. */
inline Address resolve(uint8_t msb, uint8_t lsb) {
    Address a;
    a.target = Target::None;
    a.id = 0;
    a.element = 0;
    a.field = 0;
    a.error = ERROR_UNSUPPORTED_BLOCK;

    if (msb <= MSB_ENGINE_LAST) {
        Pfm3Block block = (Pfm3Block)msb;
        const ParamBlock& b = pfm3Block(pfm3Param(block, 0, 0));
        if (b.fieldCount == 0) {
            return a;
        }
        uint8_t element = (uint8_t)(lsb / b.fieldCount);
        if (element >= b.count) {
            a.error = ERROR_UNKNOWN_PARAMETER;
            return a;
        }
        a.target = Target::Engine;
        a.element = element;
        a.field = (uint8_t)(lsb % b.fieldCount);
        a.id = pfm3ParamMake(block, element, a.field);
        return a;
    }

    if (msb >= MSB_COMMON_FIRST && msb <= MSB_COMMON_LAST) {
        CommonBlock block = (CommonBlock)(msb - MSB_COMMON_FIRST);
        const ParamBlock& b = commonBlock(commonParam(block, 0, 0));
        if (b.fieldCount == 0) {
            return a;
        }
        uint8_t element = (uint8_t)(lsb / b.fieldCount);
        if (element >= b.count) {
            a.error = ERROR_UNKNOWN_PARAMETER;
            return a;
        }
        a.target = Target::Common;
        a.element = element;
        a.field = (uint8_t)(lsb % b.fieldCount);
        a.id = (uint16_t)commonParam(block, element, a.field);
        return a;
    }

    if (msb == MSB_PRESET_NAME) {
        if (lsb >= PRESET_NAME_LENGTH) {
            a.error = ERROR_UNKNOWN_PARAMETER;
            return a;
        }
        a.target = Target::PresetName;
        a.field = lsb;
        return a;
    }

    if (msb == MSB_MIXER) {
        if (lsb >= MIXER_FIELD_COUNT) {
            a.error = ERROR_UNKNOWN_PARAMETER;
            return a;
        }
        a.target = Target::Mixer;
        a.field = lsb;
        return a;
    }

    if (msb == MSB_GLOBAL) {
        if (lsb >= GLOBAL_FIELD_COUNT) {
            a.error = ERROR_UNKNOWN_PARAMETER;
            return a;
        }
        a.target = Target::Global;
        a.field = lsb;
        return a;
    }

    return a;
}

/*
 * Float parameters use the full 14-bit range linearly over [min, max] and are
 * snapped to `step` on decode. Other access kinds carry the raw value/index.
 */
inline float snapToStep(const ParamDesc& desc, float value) {
    if (desc.step > 0.0f) {
        float ticks = (value - desc.min) / desc.step;
        int index = (int)(ticks + (ticks >= 0.0f ? 0.5f : -0.5f));
        value = desc.min + (float)index * desc.step;
    }
    if (value < desc.min) {
        value = desc.min;
    }
    if (value > desc.max) {
        value = desc.max;
    }
    return value;
}

inline uint16_t encode(const ParamDesc& desc, float value) {
    if (desc.access == ParamAccess::Float) {
        float range = desc.max - desc.min;
        if (range <= 0.0f) {
            return 0;
        }
        float x = (value - desc.min) / range;
        if (x < 0.0f) {
            x = 0.0f;
        }
        if (x > 1.0f) {
            x = 1.0f;
        }
        return (uint16_t)(x * (float)VALUE_MAX + 0.5f);
    }
    if (value < desc.min) {
        value = desc.min;
    }
    if (value > desc.max) {
        value = desc.max;
    }
    if (value > (float)VALUE_MAX) {
        value = (float)VALUE_MAX;
    }
    return (uint16_t)(value + 0.5f);
}

inline float decode(const ParamDesc& desc, uint16_t raw) {
    if (raw > VALUE_MAX) {
        raw = VALUE_MAX;
    }
    if (desc.access == ParamAccess::Float) {
        return snapToStep(desc, desc.min + (desc.max - desc.min) * ((float)raw / (float)VALUE_MAX));
    }
    float value = (float)raw;
    if (value > desc.max) {
        value = desc.max;
    }
    return value;
}

} // namespace Pfm3Nrpn

#endif