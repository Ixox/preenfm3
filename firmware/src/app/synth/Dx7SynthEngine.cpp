/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <string.h>

#include "Dx7SynthEngine.h"
#include "SynthState.h"
#include "Dx7ControlChange.h"
#include "Timbre.h"
#include "Voice.h"
#include "Hexter.h"
#include "Dx7Engine.h"
#include "Dx7Params.h"
#include "msfa_init.h"
#include "waves.h"

uint8_t Dx7SynthEngine::packedPatch_[NUMBER_OF_TIMBRES][DX7_PACKED_PATCH_SIZE] __attribute__((section(".ram_d1")));
uint8_t Dx7SynthEngine::program_[NUMBER_OF_TIMBRES][DX7_PROGRAM_SIZE] __attribute__((section(".ram_d1")));

void Dx7SynthEngine::initGlobal() {
    msfaInit(PREENFM_FREQUENCY);
    dx7EngineInit();
}

void Dx7SynthEngine::init(Timbre *timbre, SynthState *synthState) {
    timbre_ = timbre;
    synthState_ = synthState;
    timbreNumber_ = timbre->getTimbreNumber();
    dx7State_ = getDx7TimbreState(timbreNumber_);

    uint8_t *program = program_[timbreNumber_];
    uint8_t *packed = packedPatch_[timbreNumber_];

    for (int i = 0; i < DX7_PACKED_PATCH_SIZE; i++) {
        packed[i] = 0;
    }
    for (int i = 0; i < DX7_PROGRAM_SIZE; i++) {
        program[i] = 0;
    }
    // Dexed expects a 156th byte holding the operator on/off bitmask.
    program[155] = 0x3F;

    dx7TimbreInit(dx7State_);
    dx7TimbreLoadProgram(dx7State_, program);
}

bool Dx7SynthEngine::acceptsFlatParamFromMidi(int row) const {
    return row == ROW_ARPEGGIATOR1 || row == ROW_ARPEGGIATOR2
        || row == ROW_ARPEGGIATOR3 || row == ROW_EFFECT1 || row == ROW_EFFECT2;
}

void Dx7SynthEngine::engineParamChanged(int paramId, int context, float oldValue, float newValue) {
    uint8_t *program = program_[timbreNumber_];
    setDx7ParamValue(program, paramId, context, (uint8_t) newValue);

    if (paramId == DX7_LFO_SPEED || paramId == DX7_LFO_DELAY
        || paramId == DX7_LFO_KEY_SYNC || paramId == DX7_LFO_WAVEFORM) {
        dx7TimbreLoadProgram(dx7State_, program);
        return;
    }

    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        Voice *voice = timbre_->voices_[timbre_->voiceNumber_[k]];
        dx7VoiceUpdateParam(getDx7VoiceState(voice->slot()), paramId, context);
    }
}

bool Dx7SynthEngine::midiControlChange(int timbre, uint8_t cc, uint8_t value) {
    (void) timbre;

    int paramId;
    switch (cc) {
    case CC_DX7_ALGORITHM:
        paramId = DX7_ALGORITHM;
        break;
    case CC_DX7_FEEDBACK:
        paramId = DX7_FEEDBACK;
        break;
    case CC_DX7_LFO_SPEED:
        paramId = DX7_LFO_SPEED;
        break;
    case CC_DX7_LFO_DELAY:
        paramId = DX7_LFO_DELAY;
        break;
    case CC_DX7_LFO_PITCH_MOD_DEPTH:
        paramId = DX7_LFO_PITCH_MOD_DEPTH;
        break;
    case CC_DX7_LFO_AMP_MOD_DEPTH:
        paramId = DX7_LFO_AMP_MOD_DEPTH;
        break;
    case CC_DX7_LFO_KEY_SYNC:
        paramId = DX7_LFO_KEY_SYNC;
        break;
    case CC_DX7_LFO_WAVEFORM:
        paramId = DX7_LFO_WAVEFORM;
        break;
    case CC_DX7_LFO_PITCH_MOD_SENSITIVITY:
        paramId = DX7_LFO_PITCH_MOD_SENSITIVITY;
        break;
    default:
        return false;
    }

    const struct Dx7ParamDesc *desc = &dx7Params[paramId];
    int newValue = desc->minValue
        + (((int) value * (desc->maxValue - desc->minValue)) + 63) / 127;
    int oldValue = getDx7ParamValue(program_[timbreNumber_], paramId, 0);

    if (newValue != oldValue) {
        synthState_->propagateNewEngineParamValueFromExternal(timbreNumber_, paramId, 0, oldValue, newValue);
    }
    return true;
}

bool Dx7SynthEngine::midiParamFeedback(int paramId, int context, float newValue, struct MidiParamMessage &out) const {
    (void) context;

    uint8_t cc;
    switch (paramId) {
    case DX7_ALGORITHM:
        cc = CC_DX7_ALGORITHM;
        break;
    case DX7_FEEDBACK:
        cc = CC_DX7_FEEDBACK;
        break;
    case DX7_LFO_SPEED:
        cc = CC_DX7_LFO_SPEED;
        break;
    case DX7_LFO_DELAY:
        cc = CC_DX7_LFO_DELAY;
        break;
    case DX7_LFO_PITCH_MOD_DEPTH:
        cc = CC_DX7_LFO_PITCH_MOD_DEPTH;
        break;
    case DX7_LFO_AMP_MOD_DEPTH:
        cc = CC_DX7_LFO_AMP_MOD_DEPTH;
        break;
    case DX7_LFO_KEY_SYNC:
        cc = CC_DX7_LFO_KEY_SYNC;
        break;
    case DX7_LFO_WAVEFORM:
        cc = CC_DX7_LFO_WAVEFORM;
        break;
    case DX7_LFO_PITCH_MOD_SENSITIVITY:
        cc = CC_DX7_LFO_PITCH_MOD_SENSITIVITY;
        break;
    default:
        return false;
    }

    const struct Dx7ParamDesc *desc = &dx7Params[paramId];
    int range = desc->maxValue - desc->minValue;
    int value = (int) (newValue + .5f);
    if (value < desc->minValue) {
        value = desc->minValue;
    } else if (value > desc->maxValue) {
        value = desc->maxValue;
    }
    int ccValue = range == 0 ? 0 : ((value - desc->minValue) * 127 + range / 2) / range;

    out.isNrpn = false;
    out.cc = cc;
    out.ccValue = (uint8_t) ccValue;
    return true;
}

void Dx7SynthEngine::afterNewParamsLoad() {
    uint8_t *program = program_[timbreNumber_];

    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        Voice *voice = timbre_->voices_[timbre_->voiceNumber_[k]];
        updateVoiceProgram(voice, program);
    }
    dx7TimbreLoadProgram(dx7State_, program);
}

void Dx7SynthEngine::killVoice(Voice *voice) {
    voice->resetVoiceState();
    dx7VoiceReset(getDx7VoiceState(voice->slot()));
}

void Dx7SynthEngine::noteOffVoice(Voice *voice) {
    keyUp(voice);
}

void Dx7SynthEngine::noteOffVoiceQuick(Voice *voice) {
    killVoice(voice);
}

void Dx7SynthEngine::startVoice(Voice *voice, const uint8_t *program, uint8_t note, uint8_t velocity, uint32_t index) {
    voice->resetVoiceState();
    Dx7VoiceState *state = getDx7VoiceState(voice->slot());
    dx7VoiceReset(state);

    voice->note = note;
    voice->midiVelocity = velocity;
    voice->index = index;
    voice->playing = true;
    voice->released = false;
    voice->isFullOfZero = false;
    voice->holdedByPedal = false;

    dx7VoiceStart(state, program, note, velocity, 1, dx7State_);

}

void Dx7SynthEngine::updateVoiceProgram(Voice *voice, const uint8_t *program) {
    dx7VoiceUpdateProgram(getDx7VoiceState(voice->slot()), program);
}

void Dx7SynthEngine::keyUp(Voice *voice) {
    Dx7VoiceState *state = getDx7VoiceState(voice->slot());
    if (!isDx7VoiceActive(state)) {
        return;
    }
    dx7VoiceKeyUp(state);
    voice->released = true;
    voice->holdedByPedal = false;
}

void Dx7SynthEngine::bindVoice(Voice *voice) {
    voice->bindTimbre(timbre_);
}

void Dx7SynthEngine::noteOn(char note, char velocity) {
    if (unlikely(timbre_->maxVoices_ == 0)) {
        return;
    }

    note &= 0x7f;

    dx7TimbreKeyDown(dx7State_);

    int voiceToUse = timbre_->allocateVoice(note);
    if (voiceToUse == -1) {
        return;
    }
    timbre_->clearMpeMapping(voiceToUse);

    startVoice(timbre_->voices_[voiceToUse], program_[timbreNumber_], note, velocity, timbre_->voiceIndex_++);

    timbre_->lastPlayedNote_ = voiceToUse;
    if (note <= timbre_->lowerNote_ || timbre_->lowerNoteReleased_) {
        timbre_->lowerNote_ = note;
        timbre_->lowerNoteReleased_ = false;
        timbre_->lowerNoteFrequency = diatonicScaleFrequency[(int) note];
    }
}

void Dx7SynthEngine::noteOff(char note) {
    if (note == timbre_->lowerNote_) {
        timbre_->lowerNoteReleased_ = true;
    }

    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        int n = timbre_->voiceNumber_[k];

        if (!timbre_->voices_[n]->isPlaying()) {
            continue;
        }

        if (timbre_->voices_[n]->getNote() == note) {
            if (unlikely(timbre_->holdPedal_)) {
                timbre_->voices_[n]->setHoldedByPedal(true);
            } else {
                keyUp(timbre_->voices_[n]);
            }
        }
    }
}

uint8_t Dx7SynthEngine::voicesNextBlock() {
    dx7TimbreNextBlock(dx7State_);

    uint8_t playing = 0;
    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        Voice *voice = timbre_->voices_[timbre_->voiceNumber_[k]];
        bool wasPlaying = voice->isPlaying();
        renderVoice(voice);
        if (wasPlaying) {
            voice->fxAfterBlock(0.0f, 0.0f, 0.0f);
        }
        if (voice->isPlaying()) {
            playing++;
        }
    }
    return playing;
}

void Dx7SynthEngine::renderVoice(Voice *voice) {
    if (!isDx7VoiceActive(getDx7VoiceState(voice->slot()))) {
        if (voice->playing) {
        }
        voice->emptyBuffer();
        return;
    }

    bool stillPlaying = dx7VoiceRender(getDx7VoiceState(voice->slot()), dx7State_, voice->sampleBlock);
    voice->isFullOfZero = false;

    if (!stillPlaying) {
        voice->playing = false;
        voice->released = false;
    }
}

void Dx7SynthEngine::loadDx7Patch(const uint8_t *packedPatch) {
    uint8_t *packed = packedPatch_[timbreNumber_];
    uint8_t *program = program_[timbreNumber_];
    memcpy(packed, packedPatch, DX7_PACKED_PATCH_SIZE);
    Hexter::patchUnpack(packed, program);
    // Dexed expects a 156th byte holding the operator on/off bitmask.
    program[155] = 0x3F;
    afterNewParamsLoad();
}

void Dx7SynthEngine::setController(SourceEnum source, float value) {
    dx7TimbreSetController(dx7State_, source, value);
}

void Dx7SynthEngine::setMatrixSource(SourceEnum source, float value) {
    dx7TimbreSetController(dx7State_, source, value);
}

uint32_t Dx7SynthEngine::getStorageMagic() const {
    return PRESET_DX7;
}

int Dx7SynthEngine::getStorageNameOffset(uint32_t magic) const {
    return 160;
}

const char* Dx7SynthEngine::getStorageDefaultName() const {
    return "DX7";
}

uint32_t Dx7SynthEngine::getStorageBlob(uint8_t *out, uint32_t maxSize, const TimbreSharedParams &shared) {
    memcpy(out, program_[timbreNumber_], DX7_PROGRAM_SIZE);
    memcpy(out + 160, shared.presetName, 13);
    memcpy(out + 176, &shared, sizeof(TimbreSharedParams));
    return 176 + sizeof(TimbreSharedParams);
}

bool Dx7SynthEngine::loadStorageBlob(const uint8_t *in, uint32_t size, uint32_t magic) {
    memcpy(program_[timbreNumber_], in, DX7_PROGRAM_SIZE);

    TimbreSharedParams shared;
    memcpy(&shared, in + 176, sizeof(TimbreSharedParams));
    timbre_->setSharedParams(shared);
    return true;
}
