/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "MsfaSynthEngine.h"
#include "Timbre.h"
#include "Voice.h"

void MsfaSynthEngine::init(Timbre *timbre) {
    timbre_ = timbre;
}

void MsfaSynthEngine::afterNewParamsLoad() {
    timbre_->msfaAfterNewParamsLoad();
}

void MsfaSynthEngine::bindVoice(Voice *voice) {
    voice->bindMsfaTimbre(timbre_);
}

void MsfaSynthEngine::noteOn(char note, char velocity) {
    timbre_->msfaNoteOn(note, velocity);
}

void MsfaSynthEngine::noteOff(char note) {
    timbre_->msfaNoteOff(note);
}

uint8_t MsfaSynthEngine::voicesNextBlock() {
    return timbre_->msfaVoicesNextBlock();
}
