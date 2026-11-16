/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <string.h>

#include "MsfaSynthEngine.h"
#include "Timbre.h"
#include "Voice.h"
#include "Hexter.h"
#include "MsfaEngine.h"
#include "MsfaParams.h"
#include "MsfaDebug.h"
#include "waves.h"

uint8_t MsfaSynthEngine::packedPatch_[NUMBER_OF_TIMBRES][MSFA_PACKED_PATCH_SIZE] __attribute__((section(".ram_d1")));
uint8_t MsfaSynthEngine::program_[NUMBER_OF_TIMBRES][MSFA_PROGRAM_SIZE] __attribute__((section(".ram_d1")));

void MsfaSynthEngine::init(Timbre *timbre, SynthState *synthState) {
    (void) synthState;
    timbre_ = timbre;
    timbreNumber_ = timbre->getTimbreNumber();
    msfaState_ = getMsfaTimbreState(timbreNumber_);

    uint8_t *program = program_[timbreNumber_];
    uint8_t *packed = packedPatch_[timbreNumber_];

    for (int i = 0; i < MSFA_PACKED_PATCH_SIZE; i++) {
        packed[i] = 0;
    }
    for (int i = 0; i < MSFA_PROGRAM_SIZE; i++) {
        program[i] = 0;
    }
    // Dexed expects a 156th byte holding the operator on/off bitmask.
    program[155] = 0x3F;

    msfaTimbreInit(msfaState_);
    msfaTimbreLoadProgram(msfaState_, program);
}

void MsfaSynthEngine::engineParamChanged(int paramId, int context, float oldValue, float newValue) {
    uint8_t *program = program_[timbreNumber_];
    setMsfaParamValue(program, paramId, context, (uint8_t) newValue);

    if (paramId == MSFA_LFO_SPEED || paramId == MSFA_LFO_DELAY
        || paramId == MSFA_LFO_KEY_SYNC || paramId == MSFA_LFO_WAVEFORM) {
        msfaTimbreLoadProgram(msfaState_, program);
        return;
    }

    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        Voice *voice = timbre_->voices_[timbre_->voiceNumber_[k]];
        msfaVoiceUpdateParam(getMsfaVoiceState(voice->slot()), paramId, context);
    }
}

void MsfaSynthEngine::afterNewParamsLoad() {
    uint8_t *program = program_[timbreNumber_];

    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        Voice *voice = timbre_->voices_[timbre_->voiceNumber_[k]];
        updateVoiceProgram(voice, program);
    }
    msfaTimbreLoadProgram(msfaState_, program);
}

void MsfaSynthEngine::killVoice(Voice *voice) {
    voice->resetVoiceState();
    msfaVoiceReset(getMsfaVoiceState(voice->slot()));
}

void MsfaSynthEngine::noteOffVoice(Voice *voice) {
    keyUp(voice);
}

void MsfaSynthEngine::noteOffVoiceQuick(Voice *voice) {
    killVoice(voice);
}

void MsfaSynthEngine::startVoice(Voice *voice, const uint8_t *program, uint8_t note, uint8_t velocity, uint32_t index) {
    voice->resetVoiceState();
    MsfaVoiceState *state = getMsfaVoiceState(voice->slot());
    msfaVoiceReset(state);

    voice->note = note;
    voice->midiVelocity = velocity;
    voice->index = index;
    voice->playing = true;
    voice->released = false;
    voice->isFullOfZero = false;
    voice->holdedByPedal = false;

    msfaVoiceStart(state, program, note, velocity, 1, msfaState_);

    msfaDebugLog("V note=%d state=%08x active=%d\n", note, (unsigned) state,
        isMsfaVoiceActive(state));
}

void MsfaSynthEngine::updateVoiceProgram(Voice *voice, const uint8_t *program) {
    msfaVoiceUpdateProgram(getMsfaVoiceState(voice->slot()), program);
}

void MsfaSynthEngine::keyUp(Voice *voice) {
    MsfaVoiceState *state = getMsfaVoiceState(voice->slot());
    if (!isMsfaVoiceActive(state)) {
        return;
    }
    msfaVoiceKeyUp(state);
    voice->released = true;
    voice->holdedByPedal = false;
    msfaDebugLog("O note=%d state=%08x\n", voice->note, (unsigned) state);
}

void MsfaSynthEngine::bindVoice(Voice *voice) {
    voice->bindTimbre(timbre_);
}

void MsfaSynthEngine::noteOn(char note, char velocity) {
    if (unlikely(timbre_->maxVoices_ == 0)) {
        return;
    }

    note &= 0x7f;

    msfaTimbreKeyDown(msfaState_);

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

void MsfaSynthEngine::noteOff(char note) {
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
                msfaDebugLog("N note=%d v=%d\n", note, n);
            }
        }
    }
}

uint8_t MsfaSynthEngine::voicesNextBlock() {
    msfaTimbreNextBlock(msfaState_);

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

void MsfaSynthEngine::renderVoice(Voice *voice) {
    if (!isMsfaVoiceActive(getMsfaVoiceState(voice->slot()))) {
        if (voice->playing) {
            msfaDebugLog("Z note=%d state=%08x\n", voice->note, (unsigned) getMsfaVoiceState(voice->slot()));
        }
        voice->emptyBuffer();
        return;
    }

    bool stillPlaying = msfaVoiceRender(getMsfaVoiceState(voice->slot()), msfaState_, voice->sampleBlock);
    voice->isFullOfZero = false;

    if (!stillPlaying) {
        voice->playing = false;
        voice->released = false;
        msfaDebugLog("E note=%d state=%08x\n", voice->note, (unsigned) getMsfaVoiceState(voice->slot()));
    }
}

void MsfaSynthEngine::loadDx7Patch(const uint8_t *packedPatch) {
    uint8_t *packed = packedPatch_[timbreNumber_];
    uint8_t *program = program_[timbreNumber_];
    memcpy(packed, packedPatch, MSFA_PACKED_PATCH_SIZE);
    Hexter::patchUnpack(packed, program);
    // Dexed expects a 156th byte holding the operator on/off bitmask.
    program[155] = 0x3F;
    afterNewParamsLoad();
}

void MsfaSynthEngine::setController(SourceEnum source, float value) {
    msfaTimbreSetController(msfaState_, source, value);
}

void MsfaSynthEngine::setMatrixSource(SourceEnum source, float value) {
    msfaTimbreSetController(msfaState_, source, value);
}

uint32_t MsfaSynthEngine::getStorageMagic() const {
    return PRESET_MSFA;
}

int MsfaSynthEngine::getStorageNameOffset(uint32_t magic) const {
    return 160;
}

const char* MsfaSynthEngine::getStorageDefaultName() const {
    return "MSFA";
}

uint32_t MsfaSynthEngine::getStorageBlob(uint8_t *out, uint32_t maxSize, const TimbreSharedParams &shared) {
    memcpy(out, program_[timbreNumber_], MSFA_PROGRAM_SIZE);
    memcpy(out + 160, shared.presetName, 13);
    memcpy(out + 176, &shared, sizeof(TimbreSharedParams));
    return 176 + sizeof(TimbreSharedParams);
}

bool MsfaSynthEngine::loadStorageBlob(const uint8_t *in, uint32_t size, uint32_t magic) {
    memcpy(program_[timbreNumber_], in, MSFA_PROGRAM_SIZE);

    TimbreSharedParams shared;
    memcpy(&shared, in + 176, sizeof(TimbreSharedParams));
    timbre_->setSharedParams(shared);
    return true;
}
