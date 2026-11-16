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
#include "MsfaDebug.h"

uint8_t MsfaSynthEngine::packedPatch_[NUMBER_OF_TIMBRES][MSFA_PACKED_PATCH_SIZE] __attribute__((section(".ram_d1")));
uint8_t MsfaSynthEngine::program_[NUMBER_OF_TIMBRES][MSFA_PROGRAM_SIZE] __attribute__((section(".ram_d1")));

void MsfaSynthEngine::init(Timbre *timbre, SynthState *synthState) {
    (void) synthState;
    timbre_ = timbre;
    timbreNumber_ = timbre->getTimbreNumber();
    msfaState_ = msfaGetTimbreState(timbreNumber_);

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

void MsfaSynthEngine::afterNewParamsLoad() {
    uint8_t *program = program_[timbreNumber_];

    for (int k = 0; k < (int) timbre_->numberOfVoices_; k++) {
        Voice *voice = timbre_->voices_[timbre_->voiceNumber_[k]];
        // Reset the matrix used by the FX2/gate path; MSFA does not fill it.
        voice->resetMatrix();
        voice->msfaUpdateProgram(program);
    }
    msfaTimbreLoadProgram(msfaState_, program);
}

void MsfaSynthEngine::bindVoice(Voice *voice) {
    voice->bindMsfaTimbre(timbre_);
}

void MsfaSynthEngine::noteOn(char note, char velocity) {
    float scaleFrequency = timbre_->mixerState_->instrumentState_[timbre_->timbreNumber_].scaleFrequencies[(int) note];

    if (unlikely(timbre_->maxVoices_ == 0 || scaleFrequency == 0.0f)) {
        return;
    }

    note &= 0x7f;

    msfaTimbreKeyDown(msfaState_);

    int voiceToUse = timbre_->allocateVoice(note);
    if (voiceToUse == -1) {
        return;
    }
    timbre_->clearMpeMapping(voiceToUse);

    timbre_->voices_[voiceToUse]->msfaNoteOn(program_[timbreNumber_], note, velocity, timbre_->voiceIndex_++, msfaState_);

    timbre_->lastPlayedNote_ = voiceToUse;
    if (note <= timbre_->lowerNote_ || timbre_->lowerNoteReleased_) {
        timbre_->lowerNote_ = note;
        timbre_->lowerNoteReleased_ = false;
        timbre_->lowerNoteFrequency = scaleFrequency;
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
                timbre_->voices_[n]->msfaNoteOff();
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
        voice->msfaNextBlock(msfaState_);
        if (wasPlaying) {
            voice->fxAfterBlock();
        }
        if (voice->isPlaying()) {
            playing++;
        }
    }
    return playing;
}

void MsfaSynthEngine::loadMsfaPackedPatch(const uint8_t *packedPatch) {
    uint8_t *packed = packedPatch_[timbreNumber_];
    uint8_t *program = program_[timbreNumber_];
    memcpy(packed, packedPatch, MSFA_PACKED_PATCH_SIZE);
    Hexter::patchUnpack(packed, program);
    // Dexed expects a 156th byte holding the operator on/off bitmask.
    program[155] = 0x3F;
    afterNewParamsLoad();
}

void MsfaSynthEngine::loadMsfaProgram(const uint8_t *program) {
    memcpy(program_[timbreNumber_], program, MSFA_PROGRAM_SIZE);
    afterNewParamsLoad();
}

void MsfaSynthEngine::setController(SourceEnum source, float value) {
    msfaTimbreSetController(msfaState_, source, value);
}

void MsfaSynthEngine::setMatrixSource(SourceEnum source, float value) {
    msfaTimbreSetController(msfaState_, source, value);
}
