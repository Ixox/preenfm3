/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "MidiControlChange.h"
#include "Pfm3ControlChange.h"
#include "Pfm3Nrpn.h"
#include "Pfm3Param.h"
#include "Pfm3Preset.h"
#include "PreenFMFileType.h"
#include "Preenfm3Engine.h"
#include "SynthState.h"
#include "Timbre.h"
#include "Voice.h"
#include "VoicePool.h"

extern float midiNoteScale[2][NUMBER_OF_TIMBRES][128];

// ======================================
// MIDI input / parameter application
// ======================================

// Write one flat preset value, clamp it, and propagate it as an external change.
void Preenfm3Engine::setPfm3ValueFromMidi(int index, float newValue) {
    const ParamDesc* d;
    const ParamUiDesc* ui;
    Pfm3Param engineParamId = pfm3FlatToParam((uint16_t)index);
    if ((uint16_t)engineParamId != PFM3_PARAM_NONE) {
        d = &pfm3Desc(engineParamId);
        ui = &pfm3FullDesc(engineParamId).ui;
    } else {
        CommonParamId cid = commonFlatToParam((uint16_t)index);
        if (cid == COMMON_PARAM_NONE) {
            return;
        }
        d = &commonDesc(cid);
        ui = &commonFullDesc(cid).ui;
    }
    float oldValue = ((float*)getPfm3Preset())[index];

    // Special case for Clock frequency values in LFO osc Frequency, we need to accept values > max value
    float maxValue = d->max;
    if ((uint16_t)engineParamId != PFM3_PARAM_NONE && pfm3ParamBlockOf(engineParamId) == (uint8_t)Pfm3Block::LfoOsc &&
        pfm3ParamFieldOf(engineParamId) == 1) {
        maxValue = LFO_FREQ_MAX + 0.9f;
    }

    if (newValue > maxValue) {
        // in v2, matrix target were removed so some values are > to max value but we need to accept it
        bool mustConstraint = true;
        if (ui->valueOrder != NULL) {
            for (int v = 0; v < (int)ui->values; v++) {
                if ((int)ui->valueOrder[v] == (int)(newValue + .01)) {
                    mustConstraint = false;
                }
            }
        }
        if (mustConstraint) {
            newValue = maxValue;
        }
    } else if (newValue < d->min) {
        newValue = d->min;
    }
    ((float*)getPfm3Preset())[index] = newValue;

    if (oldValue != newValue) {
        Pfm3Param pid = pfm3FlatToParam((uint16_t)index);
        if ((uint16_t)pid != PFM3_PARAM_NONE) {
            synthState_->propagateNewEngineParamValueFromExternal(timbreNumber_, (int)pid, 0, oldValue, newValue);
        } else {
            CommonParamId cid = commonFlatToParam((uint16_t)index);
            if (cid != COMMON_PARAM_NONE) {
                synthState_->propagateNewEngineParamValueFromExternal(timbreNumber_, (int)cid, 1, oldValue, newValue);
            }
        }
    }
}

// Dispatch one MIDI event: matrix sources, CC table, NRPN, aftertouch, bend and MPE.
bool Preenfm3Engine::midiData(int timbre, const MidiData& data) {
    (void)timbre;
    switch (data.type) {
    case MIDI_DATA_CC:
        switch (data.cc) {
        case CC_MODWHEEL:
            setMatrixSource(MATRIX_SOURCE_MODWHEEL, INV127 * data.ccValue);
            return true;
        case CC_BREATH:
            setMatrixSource(MATRIX_SOURCE_BREATH, INV127 * data.ccValue);
            return true;
        case CC_MPE_SLIDE_CC74:
            if (!(synthState_->mixerState_.MPE_inst1_ > 0) || timbreNumber_ != 0) {
                setMatrixSource(MATRIX_SOURCE_MPESLIDE, INV127 * data.ccValue);
            }
            return true;
        default:
            return midiCcData(data.cc, data.ccValue);
        }
    case MIDI_DATA_USER_CC:
        for (int i = 0; i < 4; i++) {
            if (synthState_->mixerState_.userCC_[i] == data.cc) {
                setMatrixSource((SourceEnum)(MATRIX_SOURCE_USER_CC1 + i), INV127 * data.ccValue);
                return true;
            }
        }
        return false;
    case MIDI_DATA_NRPN:
        return midiNrpnData(data.nrpnParam, data.nrpnValue);
    case MIDI_DATA_AFTERTOUCH:
        setMatrixSource(MATRIX_SOURCE_AFTERTOUCH, data.nrpnValue * INV127);
        return true;
    case MIDI_DATA_PITCHBEND:
        setMatrixSource(MATRIX_SOURCE_PITCHBEND, ((int)data.nrpnValue - 8192) * (1.0f / 8192.0f));
        return true;
    case MIDI_DATA_MPE_AFTERTOUCH:
        setMatrixSourceMPE(data.channel, MATRIX_SOURCE_AFTERTOUCH_MPE, data.nrpnValue * INV127);
        return true;
    case MIDI_DATA_MPE_PITCHBEND:
        setMatrixSourceMPE(data.channel, MATRIX_SOURCE_PITCHBEND_MPE, ((int)data.nrpnValue - 8192) * (1.0f / 8192.0f));
        return true;
    case MIDI_DATA_MPE_SLIDE:
        setMatrixSourceMPE(data.channel, MATRIX_SOURCE_MPESLIDE, data.nrpnValue * INV127);
        return true;
    }
    return false;
}

// Apply one CC through the Pfm3 CC table, including the all-envelope shortcuts.
bool Preenfm3Engine::midiCcData(uint8_t cc, uint8_t value) {
    const CcEntry* entry = pfm3CcEntry(cc);
    if (entry == nullptr) {
        return false;
    }
    const float transformed = pfm3ApplyMidiTransform(entry->transform, value);

    switch (entry->kind) {
    case CcKind::Engine: {
        Pfm3Param id = (Pfm3Param)entry->id;
        uint16_t flat = pfm3FlatIndex(id);
        setPfm3ValueFromMidi(flat, transformed);
        return true;
    }
    case CcKind::Shared: {
        CommonParamId id = (CommonParamId)entry->id;
        uint16_t flat = commonFlatIndex(id);
        if (flat != 0xFFFF) {
            setPfm3ValueFromMidi(flat, transformed);
            return true;
        }
        return false;
    }
    case CcKind::EnvAllAttackModulator:
    case CcKind::EnvAllAttackCarrier:
    case CcKind::EnvAllReleaseModulator:
    case CcKind::EnvAllReleaseCarrier: {
        int currentAlgo = (int)getPfm3Preset()->engine1.algo;
        int type = (entry->kind == CcKind::EnvAllAttackCarrier || entry->kind == CcKind::EnvAllReleaseCarrier) ? OPERATOR_CARRIER
                                                                                                               : OPERATOR_MODULATOR;
        bool isAttack = entry->kind == CcKind::EnvAllAttackModulator || entry->kind == CcKind::EnvAllAttackCarrier;
        for (int e = 0; e < NUMBER_OF_OPERATORS; e++) {
            if (algoOpInformation[currentAlgo][e] == type) {
                setPfm3ValueFromMidi(pfm3FlatIndex(pfm3Param(Pfm3Block::EnvTime, e, isAttack ? 0 : 3)), transformed);
            }
        }
        return true;
    }
    }
    return false;
}

// Write one step-sequencer step (0..15) and propagate it.
void Preenfm3Engine::setStepValueFromMidi(int whichStepSeq, int step, int newValue) {
    if (!timbre_->supportsStepSequencer()) {
        return;
    }
    if (whichStepSeq < 0 || whichStepSeq > 1 || step < 0 || step > 15 || newValue < 0 || newValue > 15) {
        return;
    }

    int oldValue = getSeqStepValue(whichStepSeq, step);
    if (oldValue != newValue) {
        int oldStep = stepSelect_[whichStepSeq];
        stepSelect_[whichStepSeq] = step;
        if (oldStep != step) {
            synthState_->propagateNewEngineParamValueFromExternal(timbreNumber_, (int)pfm3Param(Pfm3Block::StepSeq, whichStepSeq, 0), 0,
                                                                  oldStep, step);
        }
        setSeqStepValue(whichStepSeq, step, newValue);
        int newNewValue = getSeqStepValue(whichStepSeq, step);
        if (oldValue != newNewValue) {
            synthState_->propagateNewEngineParamValueFromExternal(timbreNumber_, (int)pfm3Param(Pfm3Block::StepSeq, whichStepSeq, 0), 0,
                                                                  oldValue, newValue);
        }
    }
}

// Apply an engine parameter: StepSeq direct, otherwise through the flat preset.
bool Preenfm3Engine::applyEngineParam(Pfm3Param id, float value) {
    if (pfm3ParamBlockOf(id) == (uint8_t)Pfm3Block::StepSeq) {
        setStepValueFromMidi(pfm3ParamElementOf(id), pfm3ParamFieldOf(id), (int)value);
        return true;
    }
    uint16_t flat = pfm3FlatIndex(id);
    if (flat == 0xFFFF) {
        return false;
    }
    setPfm3ValueFromMidi(flat, value);
    return true;
}

// Apply a common parameter: flat preset when available, else the live shared field.
bool Preenfm3Engine::applyCommonParam(CommonParamId id, float value) {
    uint16_t flat = commonFlatIndex(id);
    if (flat != 0xFFFF) {
        setPfm3ValueFromMidi(flat, value);
        return true;
    }
    // ArpPattern has no flat mirror: write the live shared field directly.
    TimbreSharedParams& shared = timbre_->sharedParams();
    float oldValue = commonGetParam(shared, id);
    commonSetParam(shared, id, value);
    float newValue = commonGetParam(shared, id);
    if (oldValue != newValue) {
        syncSharedParams(shared);
        synthState_->propagateNewEngineParamValueFromExternal(timbreNumber_, (int)id, 1, oldValue, newValue);
    }
    return true;
}

// Write one preset-name character; index 11 also propagates the completed name.
bool Preenfm3Engine::applyPresetNameChar(uint8_t index, float value) {
    if (index >= Pfm3Nrpn::PRESET_NAME_LENGTH) {
        return false;
    }
    int character = (int)(value + 0.5f);
    if (character < 32) {
        character = 32;
    }
    if (character > 126) {
        character = 126;
    }
    timbre_->setPresetNameChar(index, (char)character);
    if (index == Pfm3Nrpn::PRESET_NAME_LENGTH - 1) {
        synthState_->propagateNewPresetName(timbreNumber_);
    }
    return true;
}

// ======================================
// Mixer / global
// ======================================

// Anonymous namespace: these mixer/global NRPN tables and helpers are file-local
// (internal linkage), so they cannot collide with other translation units.
namespace {

struct NrpnFieldDesc {
    ParamAccess access;
    float min;
    float max;
    float step;
};

const NrpnFieldDesc mixerNrpnFields[Pfm3Nrpn::MIXER_FIELD_COUNT] = {
    {ParamAccess::U8, 0, 8, 0},                                   // out
    {ParamAccess::U8, 0, 6, 0},                                   // compressorType
    {ParamAccess::U8, 0, 16, 0},                                  // midiChannel
    {ParamAccess::U8, 0, 127, 0},                                 // firstNote
    {ParamAccess::U8, 0, 127, 0},                                 // lastNote
    {ParamAccess::Float, -63, 64, 1},                             // shiftNote
    {ParamAccess::U8, 0, MAX_NUMBER_OF_VOICES, 0},                // numberOfVoices
    {ParamAccess::U8, 0, 1, 0},                                   // scalaEnable
    {ParamAccess::U8, 0, SCALA_MAPPING_NUMBER_OF_OPTIONS - 1, 0}, // scalaMapping
    {ParamAccess::U8, 0, NUMBEROFSCALASCALEFILES - 1, 0},         // scaleScaleNumber
    {ParamAccess::Float, 0, 4, 0.01f},                            // volume
    {ParamAccess::Float, -63, 63, 1},                             // pan
    {ParamAccess::Float, 0, 1, 0.01f},                            // send
};

const NrpnFieldDesc globalNrpnFields[Pfm3Nrpn::GLOBAL_FIELD_COUNT] = {
    {ParamAccess::U8, 0, 16, 0},          // currentChannel
    {ParamAccess::U8, 0, 16, 0},          // globalChannel
    {ParamAccess::U8, 0, 1, 0},           // midiThru
    {ParamAccess::Float, 420, 460, 0.1f}, // tuning
    {ParamAccess::U8, 0, 2, 0},           // levelMeterWhere
    {ParamAccess::U8, 0, 18, 0},          // reverbPreset
    {ParamAccess::U8, 0, 2, 0},           // reverbOutput
    {ParamAccess::Float, 0, 1, 0.01f},    // reverbLevel
    {ParamAccess::U8, 0, 127, 0},         // userCC1
    {ParamAccess::U8, 0, 127, 0},         // userCC2
    {ParamAccess::U8, 0, 127, 0},         // userCC3
    {ParamAccess::U8, 0, 127, 0},         // userCC4
    {ParamAccess::U8, 0, 5, 0},           // MPE_inst1
};

// Convert a mixer/global NRPN field descriptor to a ParamDesc.
ParamDesc asParamDesc(const NrpnFieldDesc& f) {
    ParamDesc desc;
    desc.access = f.access;
    desc.fieldOffset = 0;
    desc.min = f.min;
    desc.max = f.max;
    desc.step = f.step;
    return desc;
}

// Map a mixer NRPN field to its MIXER_VALUE_* type for propagation.
uint8_t mixerValueType(uint8_t field) {
    switch (field) {
    case 0:
        return MIXER_VALUE_OUT;
    case 1:
        return MIXER_VALUE_COMPRESSOR;
    case 2:
        return MIXER_VALUE_MIDI_CHANNEL;
    case 3:
        return MIXER_VALUE_MIDI_FIRST_NOTE;
    case 4:
        return MIXER_VALUE_MIDI_LAST_NOTE;
    case 5:
        return MIXER_VALUE_MIDI_SHIFT_NOTE;
    case 6:
        return MIXER_VALUE_NUMBER_OF_VOICES;
    case 7:
        return MIXER_VALUE_SCALA_ENABLE;
    case 8:
        return MIXER_VALUE_SCALA_MAPPING;
    case 9:
        return MIXER_VALUE_SCALA_SCALE;
    case 10:
        return MIXER_VALUE_VOLUME;
    case 11:
        return MIXER_VALUE_PAN;
    default:
        return MIXER_VALUE_SEND;
    }
}

} // namespace

// Current mixer value for the addressed timbre, indexed by NRPN field.
float Preenfm3Engine::mixerFieldValue(uint8_t field) const {
    const MixerInstrumentState& inst = synthState_->mixerState_.instrumentState_[timbreNumber_];
    switch (field) {
    case 0:
        return (float)inst.out;
    case 1:
        return (float)inst.compressorType;
    case 2:
        return (float)inst.midiChannel;
    case 3:
        return (float)inst.firstNote;
    case 4:
        return (float)inst.lastNote;
    case 5:
        return (float)inst.shiftNote;
    case 6:
        return (float)inst.numberOfVoices;
    case 7:
        return (float)inst.scalaEnable;
    case 8:
        return (float)inst.scalaMapping;
    case 9:
        return (float)inst.scaleScaleNumber;
    case 10:
        return inst.volume;
    case 11:
        return (float)inst.pan;
    default:
        return inst.send;
    }
}

// Current global value, indexed by NRPN field.
float Preenfm3Engine::globalFieldValue(uint8_t field) const {
    const MixerState& mixer = synthState_->mixerState_;
    switch (field) {
    case 0:
        return (float)mixer.currentChannel_;
    case 1:
        return (float)mixer.globalChannel_;
    case 2:
        return (float)mixer.midiThru_;
    case 3:
        return mixer.tuning_;
    case 4:
        return (float)mixer.levelMeterWhere_;
    case 5:
        return (float)mixer.reverbPreset_;
    case 6:
        return (float)mixer.reverbOutput_;
    case 7:
        return mixer.reverbLevel_;
    case 8:
        return (float)mixer.userCC_[0];
    case 9:
        return (float)mixer.userCC_[1];
    case 10:
        return (float)mixer.userCC_[2];
    case 11:
        return (float)mixer.userCC_[3];
    default:
        return (float)mixer.MPE_inst1_;
    }
}

// Decode and write a mixer field, then propagate it to Synth and the UI.
bool Preenfm3Engine::applyMixerField(uint8_t field, float rawValue) {
    if (field >= Pfm3Nrpn::MIXER_FIELD_COUNT) {
        return false;
    }
    float value = Pfm3Nrpn::decode(asParamDesc(mixerNrpnFields[field]), (uint16_t)(rawValue + 0.5f));
    MixerInstrumentState& inst = synthState_->mixerState_.instrumentState_[timbreNumber_];
    float oldValue = mixerFieldValue(field);

    switch (field) {
    case 0:
        inst.out = (uint8_t)value;
        break;
    case 1:
        inst.compressorType = (uint8_t)value;
        break;
    case 2:
        inst.midiChannel = (uint8_t)value;
        break;
    case 3:
        inst.firstNote = (uint8_t)value;
        break;
    case 4:
        inst.lastNote = (uint8_t)value;
        break;
    case 5:
        inst.shiftNote = (int8_t)value;
        break;
    case 6:
        inst.numberOfVoices = (uint8_t)value;
        break;
    case 7:
        inst.scalaEnable = (uint8_t)value;
        break;
    case 8:
        inst.scalaMapping = (uint8_t)value;
        break;
    case 9:
        inst.scaleScaleNumber = (uint16_t)value;
        break;
    case 10:
        inst.volume = value;
        break;
    case 11:
        inst.pan = (int8_t)value;
        break;
    default:
        inst.send = value;
        break;
    }

    if (field == 6) {
        const int maxVoices = timbre_->getEngineMaxVoices();
        if (inst.numberOfVoices > maxVoices) {
            inst.numberOfVoices = (uint8_t)maxVoices;
        }
    }

    float newValue = mixerFieldValue(field);
    if (oldValue != newValue) {
        synthState_->propagateNewMixerValueFromExternal(timbreNumber_, mixerValueType(field), oldValue, newValue);
    }
    return true;
}

// Decode and write a global field, then propagate it with its mixer sub-index.
bool Preenfm3Engine::applyGlobalField(uint8_t field, float rawValue) {
    if (field >= Pfm3Nrpn::GLOBAL_FIELD_COUNT) {
        return false;
    }
    float value = Pfm3Nrpn::decode(asParamDesc(globalNrpnFields[field]), (uint16_t)(rawValue + 0.5f));
    MixerState& mixer = synthState_->mixerState_;
    float oldValue = globalFieldValue(field);
    uint8_t valueType = MIXER_VALUE_GLOBAL_SETTINGS_1;
    uint8_t subIndex = 0;

    switch (field) {
    case 0:
        mixer.currentChannel_ = (uint8_t)value;
        subIndex = 2;
        break;
    case 1:
        mixer.globalChannel_ = (uint8_t)value;
        subIndex = 1;
        break;
    case 2:
        mixer.midiThru_ = (uint8_t)value;
        subIndex = 3;
        break;
    case 3:
        mixer.tuning_ = value;
        subIndex = 4;
        break;
    case 4:
        mixer.levelMeterWhere_ = (uint8_t)value;
        subIndex = 5;
        break;
    case 5:
        mixer.reverbPreset_ = (uint8_t)value;
        valueType = MIXER_VALUE_GLOBAL_SETTINGS_3;
        subIndex = 0;
        break;
    case 6:
        mixer.reverbOutput_ = (uint8_t)value;
        valueType = MIXER_VALUE_GLOBAL_SETTINGS_3;
        subIndex = 2;
        break;
    case 7:
        mixer.reverbLevel_ = value;
        valueType = MIXER_VALUE_GLOBAL_SETTINGS_3;
        subIndex = 1;
        break;
    case 8:
        mixer.userCC_[0] = (uint8_t)value;
        valueType = MIXER_VALUE_GLOBAL_SETTINGS_2;
        break;
    case 9:
        mixer.userCC_[1] = (uint8_t)value;
        valueType = MIXER_VALUE_GLOBAL_SETTINGS_2;
        subIndex = 1;
        break;
    case 10:
        mixer.userCC_[2] = (uint8_t)value;
        valueType = MIXER_VALUE_GLOBAL_SETTINGS_2;
        subIndex = 2;
        break;
    case 11:
        mixer.userCC_[3] = (uint8_t)value;
        valueType = MIXER_VALUE_GLOBAL_SETTINGS_2;
        subIndex = 3;
        break;
    default:
        mixer.MPE_inst1_ = (uint8_t)value;
        subIndex = 0;
        break;
    }

    float newValue = globalFieldValue(field);
    if (oldValue != newValue) {
        synthState_->propagateNewMixerValueFromExternal(subIndex, valueType, oldValue, newValue);
    }
    return true;
}

bool Preenfm3Engine::midiMixerFeedback(int timbre, int valueType, float newValue, struct MidiParamMessage& out) const {
    uint8_t field;
    bool global = false;
    switch (valueType) {
    case MIXER_VALUE_SEND:
        field = 12;
        break;
    case MIXER_VALUE_COMPRESSOR:
        field = 1;
        break;
    case MIXER_VALUE_GLOBAL_SETTINGS_3:
        global = true;
        field = (timbre == 1) ? 7 : 5; // reverbLevel : reverbPreset
        break;
    default:
        return false;
    }

    out.isNrpn = true;
    out.cc = 0;
    out.ccValue = 0;
    if (global) {
        out.nrpn = Pfm3Nrpn::nrpnAddress(Pfm3Nrpn::MSB_GLOBAL, field);
        out.nrpnValue = Pfm3Nrpn::encode(asParamDesc(globalNrpnFields[field]), newValue);
    } else {
        out.nrpn = Pfm3Nrpn::nrpnAddress(Pfm3Nrpn::MSB_MIXER, field);
        out.nrpnValue = Pfm3Nrpn::encode(asParamDesc(mixerNrpnFields[field]), newValue);
    }
    return true;
}

// ======================================
// Dump
// ======================================

/*
 * Preset/block dump. The decoder drains getNextNrpn() from the main loop.
 */
static const uint8_t DUMP_BLOCKS[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 99, 100, 101,
};
static const uint8_t DUMP_BLOCK_COUNT = (uint8_t)(sizeof(DUMP_BLOCKS) / sizeof(DUMP_BLOCKS[0]));

// True for blocks a v2 dump can stream.
bool Preenfm3Engine::isDumpBlockSupported(uint8_t block) {
    return block <= 21 || block == 99 || block == 100 || block == 101;
}

// Number of LSB values (parameters) in one dump block.
uint16_t Preenfm3Engine::dumpBlockSize(uint8_t block) {
    if (block <= 16) {
        const ParamBlock& b = pfm3Block(pfm3Param((Pfm3Block)block, 0, 0));
        return (uint16_t)b.count * b.fieldCount;
    }
    if (block <= 21) {
        const ParamBlock& b = commonBlock(commonParam((CommonBlock)(block - Pfm3Nrpn::MSB_COMMON_FIRST), 0, 0));
        return (uint16_t)b.count * b.fieldCount;
    }
    if (block == Pfm3Nrpn::MSB_PRESET_NAME) {
        return Pfm3Nrpn::PRESET_NAME_LENGTH;
    }
    if (block == Pfm3Nrpn::MSB_MIXER) {
        return Pfm3Nrpn::MIXER_FIELD_COUNT;
    }
    if (block == Pfm3Nrpn::MSB_GLOBAL) {
        return Pfm3Nrpn::GLOBAL_FIELD_COUNT;
    }
    return 0;
}

// Block id at a dump cursor: the single requested block or the preset order.
uint8_t Preenfm3Engine::dumpBlockAt(uint8_t cursor) const {
    if (!nrpnDumpMultiBlock_) {
        return nrpnDumpSingleBlock_;
    }
    return cursor < DUMP_BLOCK_COUNT ? DUMP_BLOCKS[cursor] : 0xFF;
}

// Read and 14-bit encode one parameter for the dump.
uint16_t Preenfm3Engine::dumpParamValue(uint8_t block, uint8_t lsb) {
    if (block <= 16) {
        Pfm3Block pb = (Pfm3Block)block;
        const ParamBlock& b = pfm3Block(pfm3Param(pb, 0, 0));
        uint8_t element = (uint8_t)(lsb / b.fieldCount);
        uint8_t field = (uint8_t)(lsb % b.fieldCount);
        Pfm3Param id = pfm3Param(pb, element, field);
        float value;
        if (pb == Pfm3Block::StepSeq) {
            value = (float)getSeqStepValue(element, field);
        } else {
            value = pfm3GetParam(pfm3Params(), id);
        }
        return Pfm3Nrpn::encode(pfm3Desc(id), value);
    }
    if (block <= 21) {
        CommonBlock cb = (CommonBlock)(block - Pfm3Nrpn::MSB_COMMON_FIRST);
        const ParamBlock& b = commonBlock(commonParam(cb, 0, 0));
        uint8_t element = (uint8_t)(lsb / b.fieldCount);
        uint8_t field = (uint8_t)(lsb % b.fieldCount);
        CommonParamId id = commonParam(cb, element, field);
        return Pfm3Nrpn::encode(commonDesc(id), commonGetParam(timbre_->sharedParams(), id));
    }
    if (block == Pfm3Nrpn::MSB_PRESET_NAME) {
        return (uint16_t)(unsigned char)timbre_->sharedParams().presetName[lsb];
    }
    if (block == Pfm3Nrpn::MSB_MIXER) {
        return Pfm3Nrpn::encode(asParamDesc(mixerNrpnFields[lsb]), mixerFieldValue(lsb));
    }
    if (block == Pfm3Nrpn::MSB_GLOBAL) {
        return Pfm3Nrpn::encode(asParamDesc(globalNrpnFields[lsb]), globalFieldValue(lsb));
    }
    return 0;
}

// Arm the iterator; getNextNrpn() emits DUMP_END once it is exhausted.
void Preenfm3Engine::startDump(bool multiBlock, uint8_t block, uint8_t blockCount) {
    nrpnDumpActive_ = true;
    nrpnDumpEndPending_ = true;
    nrpnDumpMultiBlock_ = multiBlock;
    nrpnDumpSingleBlock_ = block;
    nrpnDumpBlockCursor_ = 0;
    nrpnDumpBlockCount_ = blockCount;
    nrpnDumpLsb_ = 0;
    nrpnDumpParamCount_ = 0;
}

// Emit one parameter per call: block by block, LSB by LSB.
bool Preenfm3Engine::nextDumpMessage(MidiParamMessage& out) {
    while (nrpnDumpActive_) {
        if (nrpnDumpBlockCursor_ >= nrpnDumpBlockCount_) {
            nrpnDumpActive_ = false;
            return false;
        }
        uint8_t block = dumpBlockAt(nrpnDumpBlockCursor_);
        uint16_t size = dumpBlockSize(block);
        if (nrpnDumpLsb_ < size) {
            uint8_t lsb = (uint8_t)nrpnDumpLsb_++;
            out.isNrpn = true;
            out.cc = 0;
            out.ccValue = 0;
            out.nrpn = Pfm3Nrpn::nrpnAddress(block, lsb);
            out.nrpnValue = dumpParamValue(block, lsb);
            nrpnDumpParamCount_++;
            return true;
        }
        nrpnDumpLsb_ = 0;
        nrpnDumpBlockCursor_++;
    }
    return false;
}

// ======================================
// NRPN data and responses
// ======================================

/*
 * Apply one incoming NRPN v2 data write (doc/nrpn_spec_v2.md).
 * param is the 14-bit address (msb << 7 | lsb), value the 14-bit data.
 */
bool Preenfm3Engine::midiNrpnData(uint16_t param, uint16_t value) {
    lastNrpnError_ = 0xFF;
    if (Pfm3Nrpn::nrpnMsb(param) == Pfm3Nrpn::MSB_CONTROL) {
        // Command-domain NRPNs are handled by startNrpnResponse() from the
        // main loop; never report them as an unsupported data block.
        return true;
    }
    Pfm3Nrpn::Address address = Pfm3Nrpn::resolve(Pfm3Nrpn::nrpnMsb(param), Pfm3Nrpn::nrpnLsb(param));
    if (!address.ok()) {
        lastNrpnError_ = address.error;
        return false;
    }
    switch (address.target) {
    case Pfm3Nrpn::Target::Engine: {
        Pfm3Param id = (Pfm3Param)address.id;
        if (!applyEngineParam(id, Pfm3Nrpn::decode(pfm3Desc(id), value))) {
            lastNrpnError_ = Pfm3Nrpn::ERROR_UNKNOWN_PARAMETER;
            return false;
        }
        return true;
    }
    case Pfm3Nrpn::Target::Common: {
        CommonParamId id = (CommonParamId)address.id;
        if (!applyCommonParam(id, Pfm3Nrpn::decode(commonDesc(id), value))) {
            lastNrpnError_ = Pfm3Nrpn::ERROR_UNKNOWN_PARAMETER;
            return false;
        }
        return true;
    }
    case Pfm3Nrpn::Target::PresetName:
        return applyPresetNameChar(address.field, (float)value);
    case Pfm3Nrpn::Target::Mixer:
        return applyMixerField(address.field, (float)value);
    case Pfm3Nrpn::Target::Global:
        return applyGlobalField(address.field, (float)value);
    default:
        lastNrpnError_ = Pfm3Nrpn::ERROR_UNSUPPORTED_BLOCK;
        return false;
    }
}

// Error code of the last NRPN data write, or 0xFF when it succeeded.
uint8_t Preenfm3Engine::nrpnError() const {
    return lastNrpnError_;
}

/*
 * Called once per command-domain NRPN ([127, command]) from
 * MidiDecoder::processAsyncActions() in the main loop (SEND_NRPN_RESPONSE
 * async action). Fills nrpnQueue_ and/or arms the dump iterator; the decoder
 * then streams everything through getNextNrpn().
 *
 * Commands:
 *  - REQUEST_PRESET_DUMP (0): timbre channel + scope 0 only; queues DUMP_BEGIN
 *    and starts the full preset dump.
 *  - REQUEST_BLOCK_DUMP (1): starts a dump of one block (no DUMP_BEGIN).
 *  - REQUEST_PARAM (2): queues one [block, lsb] write with the current value.
 *  - REQUEST_CAPABILITIES (3): queues the five CAP_* responses.
 *  - anything else: ERROR (unknown command).
 *
 * Invalid channel/scope/block or unknown parameter queues the matching ERROR.
 */
void Preenfm3Engine::startNrpnResponse(uint8_t channel, uint16_t address, uint16_t value) {
    nrpnDumpActive_ = false;
    nrpnDumpEndPending_ = false;
    nrpnQueueCount_ = 0;
    nrpnQueueIndex_ = 0;

    auto push = [this](uint16_t nrpn, uint16_t nrpnValue) {
        if (nrpnQueueCount_ < (uint8_t)(sizeof(nrpnQueue_) / sizeof(nrpnQueue_[0]))) {
            nrpnQueue_[nrpnQueueCount_].isNrpn = true;
            nrpnQueue_[nrpnQueueCount_].cc = 0;
            nrpnQueue_[nrpnQueueCount_].ccValue = 0;
            nrpnQueue_[nrpnQueueCount_].nrpn = nrpn;
            nrpnQueue_[nrpnQueueCount_].nrpnValue = nrpnValue;
            nrpnQueueCount_++;
        }
    };
    auto pushError = [&push](uint8_t error) { push(Pfm3Nrpn::nrpnAddress(Pfm3Nrpn::MSB_CONTROL, Pfm3Nrpn::RESPONSE_ERROR), error); };

    if (Pfm3Nrpn::nrpnMsb(address) != Pfm3Nrpn::MSB_CONTROL) {
        pushError(Pfm3Nrpn::ERROR_UNKNOWN_PARAMETER);
        return;
    }

    switch (Pfm3Nrpn::nrpnLsb(address)) {
    case Pfm3Nrpn::REQUEST_PRESET_DUMP: {
        if (channel == (uint8_t)(synthState_->mixerState_.globalChannel_ - 1)) {
            pushError(Pfm3Nrpn::ERROR_UNKNOWN_COMMAND);
            return;
        }
        if (value != Pfm3Nrpn::DUMP_SCOPE_FULL) {
            pushError(Pfm3Nrpn::ERROR_VALUE_OUT_OF_RANGE);
            return;
        }
        // Queue DUMP_BEGIN; parameters and DUMP_END come from getNextNrpn().
        push(Pfm3Nrpn::nrpnAddress(Pfm3Nrpn::MSB_CONTROL, Pfm3Nrpn::RESPONSE_DUMP_BEGIN), Pfm3Nrpn::PROTOCOL_VERSION);
        startDump(true, 0, DUMP_BLOCK_COUNT);
        return;
    }
    case Pfm3Nrpn::REQUEST_BLOCK_DUMP: {
        if (value > 127) {
            pushError(Pfm3Nrpn::ERROR_VALUE_OUT_OF_RANGE);
            return;
        }
        uint8_t block = (uint8_t)value;
        if (!isDumpBlockSupported(block)) {
            pushError(Pfm3Nrpn::ERROR_UNSUPPORTED_BLOCK);
            return;
        }
        // No DUMP_BEGIN for a block dump (spec 8.3).
        startDump(false, block, 1);
        return;
    }
    case Pfm3Nrpn::REQUEST_PARAM: {
        Pfm3Nrpn::Address a = Pfm3Nrpn::resolve(Pfm3Nrpn::nrpnMsb(value), Pfm3Nrpn::nrpnLsb(value));
        if (!a.ok()) {
            pushError(a.error);
            return;
        }
        if (a.target == Pfm3Nrpn::Target::Engine) {
            Pfm3Param id = (Pfm3Param)a.id;
            float current;
            if (pfm3ParamBlockOf(id) == (uint8_t)Pfm3Block::StepSeq) {
                current = (float)getSeqStepValue(pfm3ParamElementOf(id), pfm3ParamFieldOf(id));
            } else {
                current = pfm3GetParam(pfm3Params(), id);
            }
            push(value, Pfm3Nrpn::encode(pfm3Desc(id), current));
            return;
        }
        if (a.target == Pfm3Nrpn::Target::Common) {
            CommonParamId id = (CommonParamId)a.id;
            push(value, Pfm3Nrpn::encode(commonDesc(id), commonGetParam(timbre_->sharedParams(), id)));
            return;
        }
        if (a.target == Pfm3Nrpn::Target::PresetName) {
            char character = timbre_->sharedParams().presetName[a.field];
            push(value, (uint16_t)(unsigned char)character);
            return;
        }
        if (a.target == Pfm3Nrpn::Target::Mixer) {
            push(value, Pfm3Nrpn::encode(asParamDesc(mixerNrpnFields[a.field]), mixerFieldValue(a.field)));
            return;
        }
        if (a.target == Pfm3Nrpn::Target::Global) {
            push(value, Pfm3Nrpn::encode(asParamDesc(globalNrpnFields[a.field]), globalFieldValue(a.field)));
            return;
        }
        pushError(Pfm3Nrpn::ERROR_UNSUPPORTED_BLOCK);
        return;
    }
    case Pfm3Nrpn::REQUEST_CAPABILITIES:
        push(Pfm3Nrpn::nrpnAddress(Pfm3Nrpn::MSB_CONTROL, Pfm3Nrpn::RESPONSE_CAP_PROTOCOL_VERSION), Pfm3Nrpn::PROTOCOL_VERSION);
        push(Pfm3Nrpn::nrpnAddress(Pfm3Nrpn::MSB_CONTROL, Pfm3Nrpn::RESPONSE_CAP_FEATURE_BITS_LO),
             (uint16_t)(Pfm3Nrpn::SUPPORTED_FEATURES & 0x3FFF));
        push(Pfm3Nrpn::nrpnAddress(Pfm3Nrpn::MSB_CONTROL, Pfm3Nrpn::RESPONSE_CAP_FEATURE_BITS_HI),
             (uint16_t)((Pfm3Nrpn::SUPPORTED_FEATURES >> 14) & 0x3FFF));
        push(Pfm3Nrpn::nrpnAddress(Pfm3Nrpn::MSB_CONTROL, Pfm3Nrpn::RESPONSE_CAP_MAX_VOICES), Pfm3Nrpn::CAP_MAX_VOICES);
        push(Pfm3Nrpn::nrpnAddress(Pfm3Nrpn::MSB_CONTROL, Pfm3Nrpn::RESPONSE_CAP_MAX_TIMBRES), Pfm3Nrpn::CAP_MAX_TIMBRES);
        return;
    default:
        pushError(Pfm3Nrpn::ERROR_UNKNOWN_COMMAND);
        return;
    }
}

/*
 * One message per call, in order: queued response (DUMP_BEGIN/errors/
 * capabilities), then dump parameters, then DUMP_END with the count, then
 * nullptr.
 */
const MidiParamMessage* Preenfm3Engine::getNextNrpn() {
    if (nrpnQueueIndex_ < nrpnQueueCount_) {
        return &nrpnQueue_[nrpnQueueIndex_++];
    }
    if (nrpnDumpActive_) {
        if (nextDumpMessage(nrpnDumpMessage_)) {
            return &nrpnDumpMessage_;
        }
    }
    if (nrpnDumpEndPending_) {
        // End of stream: DUMP_END carries the number of parameters sent.
        nrpnDumpEndPending_ = false;
        nrpnDumpMessage_.isNrpn = true;
        nrpnDumpMessage_.cc = 0;
        nrpnDumpMessage_.ccValue = 0;
        nrpnDumpMessage_.nrpn = Pfm3Nrpn::nrpnAddress(Pfm3Nrpn::MSB_CONTROL, Pfm3Nrpn::RESPONSE_DUMP_END);
        nrpnDumpMessage_.nrpnValue = nrpnDumpParamCount_;
        return &nrpnDumpMessage_;
    }
    return nullptr;
}

// ======================================
// Parameter feedback
// ======================================

// Build the CC/NRPN feedback for a parameter change, when feedback is enabled.
bool Preenfm3Engine::midiParamFeedback(int paramId, int context, float newValue, struct MidiParamMessage& out) const {
    // Menu "Sends": 0 = none, 1 = CC feedback, 2 = NRPN feedback.
    int sendCCOrNRPN = synthState_->fullState_.midiConfigValue[MIDICONFIG_SENDS];

    // 2 = NRPN feedback enabled in the menu: reply with the v2 [block, lsb] address.
    if (sendCCOrNRPN == 2) {
        out.isNrpn = true;
        // CC feedback: context 0 = engine parameter, 1 = common/shared parameter.
        if (context == 0) {
            Pfm3Param id = (Pfm3Param)paramId;
            if (pfm3ParamBlockOf(id) == (uint8_t)Pfm3Block::StepSeq) {
                return false;
            }
            const ParamBlock& block = pfm3Block(id);
            out.nrpn =
                Pfm3Nrpn::nrpnAddress(pfm3ParamBlockOf(id), (uint8_t)(pfm3ParamElementOf(id) * block.fieldCount + pfm3ParamFieldOf(id)));
            out.nrpnValue = Pfm3Nrpn::encode(pfm3Desc(id), newValue);
            return true;
        }
        CommonParamId id = (CommonParamId)paramId;
        if (commonFlatIndex(id) == 0xFFFF) {
            return false;
        }
        const ParamBlock& block = commonBlock(id);
        out.nrpn = Pfm3Nrpn::nrpnAddress((uint8_t)(Pfm3Nrpn::MSB_COMMON_FIRST + commonBlockOf(id)),
                                         (uint8_t)(commonElementOf(id) * block.fieldCount + commonFieldOf(id)));
        out.nrpnValue = Pfm3Nrpn::encode(commonDesc(id), newValue);
        return true;
    }

    // Not CC feedback (0 = none, 2 = NRPN handled above): nothing more to send.
    if (sendCCOrNRPN != 1) {
        return false;
    }

    // CC feedback enabled in the menu: build the CC number/value mapping.
    out.isNrpn = false;
    out.cc = 0;
    out.ccValue = 0;

    // context 0 = engine parameter
    if (context == 0) {
        Pfm3Param id = (Pfm3Param)paramId;
        uint8_t block = pfm3ParamBlockOf(id);
        uint8_t element = pfm3ParamElementOf(id);
        uint8_t field = pfm3ParamFieldOf(id);

        switch ((Pfm3Block)block) {
        case Pfm3Block::Engine1:
            if (field == 0) { // algo
                out.ccValue = newValue;
                out.cc = CC_ALGO;
            } else if (field == 2) { // play mode
                out.cc = CC_PLAY_MODE;
                out.ccValue = newValue * (127.0f / 2.0f) + .1f;
            } else if (field == 3) { // glide speed
                out.cc = CC_GLIDE;
                out.ccValue = newValue * (127.0f / 12.0f) + .1f;
            }
            break;
        case Pfm3Block::Mod:
            if (field == 0) { // modulation element
                out.ccValue = newValue * 10 + .001f;
                out.cc = CC_IM1 + element;
            }
            break;
        case Pfm3Block::OscMix:
            if (element < 4) {
                out.cc = CC_MIX1 + element * 2 + field;
                if (field == 0) { // mix
                    out.ccValue = newValue * 100.0f + .1f;
                } else {
                    out.ccValue = (newValue + 1.0f) * 50.0f + .1f;
                }
            } else if (element == 4) {
                out.cc = (field == 0) ? CC_MIX5 : CC_PAN5;
                out.ccValue = (field == 0) ? (newValue * 100.0f + .1f) : ((newValue + 1.0f) * 50.0f + .1f);
            } else if (element == 5) {
                out.cc = (field == 0) ? CC_MIX6 : CC_PAN6;
                out.ccValue = (field == 0) ? (newValue * 100.0f + .1f) : ((newValue + 1.0f) * 50.0f + .1f);
            }
            break;
        case Pfm3Block::Osc:
            if (field == 2) { // frequency
                out.cc = CC_OSC1_FREQ + element;
                out.ccValue = newValue * 12.0f + .1f;
            }
            break;
        case Pfm3Block::Matrix:
            if (element < 4 && field == 1) { // multiplier
                out.cc = CC_MATRIXROW1_MUL + element;
                out.ccValue = newValue * 5.0f + 50.1f;
            }
            break;
        case Pfm3Block::LfoOsc:
            switch (field) {
            case 0: // shape
                out.cc = CC_LFO1_SHAPE + element;
                out.ccValue = newValue + .1f;
                break;
            case 1: // frequency
                out.cc = CC_LFO1_FREQ + element;
                out.ccValue = newValue * 5.0f + .1f;
                break;
            case 2: // bias
                out.cc = CC_LFO1_BIAS + element;
                out.ccValue = newValue * 100.0f + .1f;
                break;
            }
            break;
        case Pfm3Block::LfoPhase:
            // LfoPhase uses element 0, field = LFO number.
            out.cc = CC_LFO1_PHASE + field;
            out.ccValue = newValue * 100.0f + .1f;
            break;
        case Pfm3Block::LfoEnv2:
            if (field == 0) { // silence
                out.cc = CC_LFO_ENV2_SILENCE;
                out.ccValue = newValue * 8.0f + .1f;
            }
            break;
        case Pfm3Block::LfoSeq:
            if (field == 1) { // gate
                out.cc = CC_STEPSEQ5_GATE + element;
                out.ccValue = newValue * 100.0f + .1f;
            }
            break;
        case Pfm3Block::EnvTime:
            if (field == 0) { // attack
                out.cc = (element == 0) ? CC_ENV_ATK_OP1 : (CC_ENV_ATK_OP2 + element - 1);
                out.ccValue = newValue * 64.0f + .1f;
            } else if (field == 3) { // release
                out.cc = (element == 0) ? CC_ENV_REL_OP1 : (CC_ENV_REL_OP2 + element - 1);
                out.ccValue = newValue * 32.0f + .1f;
            }
            break;
        default:
            break;
        }
    } else {
        CommonBlock block = (CommonBlock)commonBlockOf((CommonParamId)paramId);
        uint8_t field = commonFieldOf((CommonParamId)paramId);

        switch (block) {
        case CommonBlock::Arp1:
            switch (field) {
            case 0:
                out.cc = CC_ARP_CLOCK;
                break;
            case 2:
                out.cc = CC_ARP_DIRECTION;
                break;
            case 3:
                out.cc = CC_ARP_OCTAVE;
                break;
            default:
                break;
            }
            if (out.cc != 0) {
                out.ccValue = newValue + .1f;
            }
            break;
        case CommonBlock::Arp2:
            switch (field) {
            case 0:
                out.cc = CC_ARP_PATTERN;
                break;
            case 1:
                out.cc = CC_ARP_DIVISION;
                break;
            case 2:
                out.cc = CC_ARP_DURATION;
                break;
            default:
                break;
            }
            if (out.cc != 0) {
                out.ccValue = newValue + .1f;
            }
            break;
        case CommonBlock::Effect1:
        case CommonBlock::Effect2:
            if (field == 0) { // type
                out.cc = (block == CommonBlock::Effect1) ? CC_FILTER_TYPE : CC_FILTER2_TYPE;
                out.ccValue = newValue + .1f;
            } else {
                out.cc = (block == CommonBlock::Effect1) ? (CC_FILTER_PARAM1 + field - 1) : (CC_FILTER2_PARAM1 + field - 1);
                if (field == 3) { // gain
                    out.ccValue = newValue * 100.0f + .1f;
                } else {
                    out.ccValue = newValue * 128.0f + .1f;
                }
            }
            break;
        default:
            break;
        }
    }

    if (out.cc == 0) {
        return false;
    }
    // CC limited to 127
    if (out.ccValue > 127) {
        out.ccValue = 127;
    }
    return true;
}

// ======================================
// MIDI clock
// ======================================

// Send a MIDI song-position step to the LFOs used by one voice.
void Preenfm3Engine::midiClockSongPositionStepOnVoice(Voice* voice, int songPosition, bool recomputeNext) {
    Pfm3VoiceState& state = pfm3State(voice);
    if (likely(timbre_->isLfoUsed(0)))
        state.lfoOsc[0].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(1)))
        state.lfoOsc[1].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(2)))
        state.lfoOsc[2].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(3)))
        state.lfoEnv[0].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(4)))
        state.lfoEnv2[0].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(5)))
        state.lfoStepSeq[0].midiClock(songPosition, recomputeNext);
    if (likely(timbre_->isLfoUsed(6)))
        state.lfoStepSeq[1].midiClock(songPosition, recomputeNext);
}

// Continue the LFOs used by one voice at the given song position.
void Preenfm3Engine::midiClockContinueOnVoice(Voice* voice, int songPosition) {
    Pfm3VoiceState& state = pfm3State(voice);
    if (likely(timbre_->isLfoUsed(0)))
        state.lfoOsc[0].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(1)))
        state.lfoOsc[1].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(2)))
        state.lfoOsc[2].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(3)))
        state.lfoEnv[0].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(4)))
        state.lfoEnv2[0].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(5)))
        state.lfoStepSeq[0].midiClock(songPosition, false);
    if (likely(timbre_->isLfoUsed(6)))
        state.lfoStepSeq[1].midiClock(songPosition, false);
}

// Restart the LFOs used by one voice on MIDI start.
void Preenfm3Engine::midiClockStartOnVoice(Voice* voice) {
    Pfm3VoiceState& state = pfm3State(voice);
    if (likely(timbre_->isLfoUsed(0)))
        state.lfoOsc[0].midiContinue();
    if (likely(timbre_->isLfoUsed(1)))
        state.lfoOsc[1].midiContinue();
    if (likely(timbre_->isLfoUsed(2)))
        state.lfoOsc[2].midiContinue();
    if (likely(timbre_->isLfoUsed(3)))
        state.lfoEnv[0].midiContinue();
    if (likely(timbre_->isLfoUsed(4)))
        state.lfoEnv2[0].midiContinue();
    if (likely(timbre_->isLfoUsed(5)))
        state.lfoStepSeq[0].midiContinue();
    if (likely(timbre_->isLfoUsed(6)))
        state.lfoStepSeq[1].midiContinue();
}

// Apply MIDI continue to all voices of this timbre.
void Preenfm3Engine::midiClockContinue(int songPosition) {
    for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
        midiClockContinueOnVoice(timbre_->voices_[timbre_->voiceNumber_[k]], songPosition);
    }

    recomputeNext_ = ((songPosition & 0x1) == 0);
    timbre_->OnMidiContinue();
}

// Apply MIDI start to all voices of this timbre.
void Preenfm3Engine::midiClockStart() {
    for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
        midiClockStartOnVoice(timbre_->voices_[timbre_->voiceNumber_[k]]);
    }

    recomputeNext_ = true;
    timbre_->OnMidiStart();
}

// Apply a MIDI song-position step to all voices of this timbre.
void Preenfm3Engine::midiClockSongPositionStep(int songPosition) {
    for (int k = 0; k < (int)timbre_->numberOfVoices_; k++) {
        midiClockSongPositionStepOnVoice(timbre_->voices_[timbre_->voiceNumber_[k]], songPosition, recomputeNext_);
    }

    if ((songPosition & 0x1) == 0) {
        recomputeNext_ = true;
    }
}

// ======================================
// MIDI note scale
// ======================================

// Rebuild midiNoteScale[scale] from the MIDI note curve parameters.
void Preenfm3Engine::updateMidiNoteScale(int scale) {
    int intBreakNote;
    int curveBefore;
    int curveAfter;
    if (scale == 0) {
        intBreakNote = pfm3Params().midiNote1Curve.breakNote;
        curveBefore = pfm3Params().midiNote1Curve.curveBefore;
        curveAfter = pfm3Params().midiNote1Curve.curveAfter;
    } else {
        intBreakNote = pfm3Params().midiNote2Curve.breakNote;
        curveBefore = pfm3Params().midiNote2Curve.curveBefore;
        curveAfter = pfm3Params().midiNote2Curve.curveAfter;
    }
    float floatBreakNote = intBreakNote;
    float multiplier = 1.0f;

    switch (curveBefore) {
    case MIDI_NOTE_CURVE_FLAT:
        for (int n = 0; n < intBreakNote; n++) {
            midiNoteScale[scale][timbre_->timbreNumber_][n] = 0;
        }
        break;
    case MIDI_NOTE_CURVE_M_LINEAR:
        multiplier = -1.0f;
        goto linearBefore;
    case MIDI_NOTE_CURVE_M_LINEAR2:
        multiplier = -8.0f;
        goto linearBefore;
    case MIDI_NOTE_CURVE_LINEAR2:
        multiplier = 8.0f;
        goto linearBefore;
    case MIDI_NOTE_CURVE_LINEAR:
    linearBefore:
        for (int n = 0; n < intBreakNote; n++) {
            float fn = (floatBreakNote - n);
            midiNoteScale[scale][timbre_->timbreNumber_][n] = fn * INV127 * multiplier;
        }
        break;
    case MIDI_NOTE_CURVE_M_EXP:
        multiplier = -1.0f;
    case MIDI_NOTE_CURVE_EXP:
        if (floatBreakNote == 0) {
            floatBreakNote = 1;
        }
        for (int n = 0; n < intBreakNote; n++) {
            float fn = (floatBreakNote - n);
            fn = fn * fn / floatBreakNote;
            midiNoteScale[scale][timbre_->timbreNumber_][n] = fn * INV16 * multiplier;
        }
        break;
    }

    midiNoteScale[scale][timbre_->timbreNumber_][intBreakNote] = 0;

    switch (curveAfter) {
    case MIDI_NOTE_CURVE_FLAT:
        for (int n = intBreakNote + 1; n < 128; n++) {
            midiNoteScale[scale][timbre_->timbreNumber_][n] = 0;
        }
        break;
    case MIDI_NOTE_CURVE_M_LINEAR:
        multiplier = -1.0f;
        goto linearAfter;
    case MIDI_NOTE_CURVE_M_LINEAR2:
        multiplier = -8.0f;
        goto linearAfter;
    case MIDI_NOTE_CURVE_LINEAR2:
        multiplier = 8.0f;
        goto linearAfter;
    case MIDI_NOTE_CURVE_LINEAR:
    linearAfter:
        for (int n = intBreakNote + 1; n < 128; n++) {
            float fn = n - floatBreakNote;
            midiNoteScale[scale][timbre_->timbreNumber_][n] = fn * INV127 * multiplier;
        }
        break;
    case MIDI_NOTE_CURVE_M_EXP:
        multiplier = -1.0f;
    case MIDI_NOTE_CURVE_EXP:
        if (floatBreakNote == 0) {
            floatBreakNote = 1;
        }
        for (int n = intBreakNote + 1; n < 128; n++) {
            float fn = n - floatBreakNote;
            fn = fn * fn / floatBreakNote;
            midiNoteScale[scale][timbre_->timbreNumber_][n] = fn * INV16 * multiplier;
        }
        break;
    }
}
