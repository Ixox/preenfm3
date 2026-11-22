/*
 * Copyright 2013 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>

#include "SynthState.h"
#include "Timbre.h"
#include "Voice.h"
#include "waves.h"

Voice::Voice(void) {

    index_ = 0;

    // Init FX variables
    v0L_ = v1L_ = v2L_ = v3L_ = v4L_ = v5L_ = v6L_ = v7L_ = v8L_ = v0R_ = v1R_ = v2R_ = v3R_ = v4R_ = v5R_ = v6R_ = v7R_ = v8R_ = v8R_ =
        0.0f;
    fxParam1PlusMatrix_ = -1.0;
    fxNoteTrigger_ = 0;
}

Voice::~Voice(void) {
}

void Voice::init(int slot) {
    this->slot_ = slot;
    this->currentTimbre_ = 0;
    this->pendingTimbre_ = nullptr;
    this->transferReady_ = false;
    this->playing_ = false;
    this->isFullOfZero_ = false;
    this->note_ = 0;
    this->midiVelocity_ = 0;
    this->fxNoteTrigger_ = 0;
    this->holdedByPedal_ = false;
}

void Voice::resetVoiceState() {
    pendingTimbre_ = nullptr;
    transferReady_ = false;
    playing_ = false;
    released_ = false;
}

// Called when loading new preset

void Voice::emptyBuffer() {
    if (!isFullOfZero_) {
        isFullOfZero_ = true;
        float* sample = sampleBlock_;

        for (int k = 0; k < BLOCK_SIZE; k++) {
            *sample++ = 0.0f;
            *sample++ = 0.0f;
        }
    }
}
