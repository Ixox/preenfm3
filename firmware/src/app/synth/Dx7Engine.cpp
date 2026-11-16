/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * DX7 engine state and adapters.
 *
 * The DX7 headers are only included here so their Lfo/Env classes do not
 * collide with the PFM3 classes in the rest of the firmware.
 */

#include <new>

#include "Dx7Engine.h"
#include "Dx7Params.h"
#include "controllers.h"
#include "dx7note.h"
#include "fm_core.h"
#include "lfo.h"
#include "synth.h"
#include "tuning.h"

// DX7's Q24 output is quieter than PFM3's. Apply a linear gain while
// converting to float; the final mixer clamps before the DAC, so there is
// headroom here. Override with -DDX7_OUTPUT_GAIN=...
#ifndef DX7_OUTPUT_GAIN
#define DX7_OUTPUT_GAIN 2.0f
#endif

FmCore dx7Core;

static TuningState* dx7Tuning;

struct Dx7TimbreState {
    Controllers controllers;
    Lfo lfo;
    int32_t lfoValue;
    int32_t lfoDelay;
    uint8_t blockPhase;
};

struct Dx7VoiceState {
    Dx7Note noteObj;
    int32_t buffer[N];
    uint8_t bufferPos;
    bool active;
    bool released;
    uint8_t note;
    uint8_t velocity;

    Dx7VoiceState();
};

alignas(Dx7VoiceState) static uint8_t dx7VoiceStorage[MAX_NUMBER_OF_VOICES][sizeof(Dx7VoiceState)] __attribute__((section(".ram_d1")));
alignas(Dx7TimbreState) static uint8_t dx7TimbreStorage[NUMBER_OF_TIMBRES][sizeof(Dx7TimbreState)] __attribute__((section(".ram_d1")));

Dx7VoiceState* dx7VoiceStates = reinterpret_cast<Dx7VoiceState*>(dx7VoiceStorage);
Dx7TimbreState* dx7TimbreStates = reinterpret_cast<Dx7TimbreState*>(dx7TimbreStorage);

Dx7VoiceState* getDx7VoiceState(int index) {
    return dx7VoiceStates + index;
}

Dx7TimbreState* getDx7TimbreState(int index) {
    return dx7TimbreStates + index;
}

Dx7VoiceState::Dx7VoiceState() : noteObj(dx7Tuning, nullptr), bufferPos(N), active(false), released(false), note(0), velocity(0) {
    for (int i = 0; i < N; i++) {
        buffer[i] = 0;
    }
}

void dx7TimbreInit(Dx7TimbreState* state) {
    Controllers& controllers = state->controllers;

    controllers.pitch_cc = 0x2000;
    controllers.pitch_range_up = 3;
    controllers.pitch_range_dn = 3;
    controllers.pitch_step = 0;
    controllers.amp_mod = 0;
    controllers.pitch_mod = 0;
    controllers.eg_mod = 127;
    controllers.aftertouch_cc = 0;
    controllers.breath_cc = 0;
    controllers.foot_cc = 0;
    controllers.modwheel_cc = 0;
    controllers.portamento_enable_cc = false;
    controllers.portamento_cc = 0;
    controllers.portamento_gliss_cc = false;
    controllers.masterTune = 0;
    controllers.mpeEnabled = false;
    controllers.mpePitchBendRange = 24;
    controllers.core = &dx7Core;
    controllers.refresh();

    state->lfoValue = 1 << 23;
    state->lfoDelay = 1 << 24;
    state->blockPhase = 0;
}

void dx7TimbreKeyDown(Dx7TimbreState* state) {
    state->lfo.keydown();
}

void dx7TimbreNextBlock(Dx7TimbreState* state) {
    if (state->blockPhase == 0) {
        state->lfoValue = state->lfo.getsample();
        state->lfoDelay = state->lfo.getdelay();
    }
    state->blockPhase ^= 1;
}

void dx7TimbreLoadProgram(Dx7TimbreState* state, const uint8_t* program) {
    state->lfo.reset(program + 137);
}

void dx7TimbreSetController(Dx7TimbreState* state, enum SourceEnum source, float value) {
    Controllers& controllers = state->controllers;

    switch (source) {
    case MATRIX_SOURCE_PITCHBEND:
        controllers.pitch_cc = 8192 + (int)(value * 8192.0f);
        break;
    case MATRIX_SOURCE_MODWHEEL:
        controllers.modwheel_cc = (int)(value * 127.0f);
        break;
    case MATRIX_SOURCE_AFTERTOUCH:
        controllers.aftertouch_cc = (int)(value * 127.0f);
        break;
    case MATRIX_SOURCE_BREATH:
        controllers.breath_cc = (int)(value * 127.0f);
        break;
    default:
        return;
    }
    controllers.refresh();
}

void dx7VoiceStart(Dx7VoiceState* state, const uint8_t* program, uint8_t note, uint8_t velocity, uint8_t channel, Dx7TimbreState* ts,
                   bool voiceStolen) {
    state->noteObj.tuning_state_ = dx7Tuning;
    state->noteObj.init(program, note, velocity, channel, &ts->controllers);

    // Oscillator key sync (program byte 136). Do not sync a stolen voice:
    // restarting its phases would click. Dexed does the same.
    if (program[136] != 0 && !voiceStolen) {
        state->noteObj.oscSync();
    }

    for (int i = 0; i < N; i++) {
        state->buffer[i] = 0;
    }
    state->bufferPos = N;
    state->active = true;
    state->released = false;
    state->note = note;
    state->velocity = velocity;
}

void dx7VoiceKeyUp(Dx7VoiceState* state) {
    if (state == nullptr || !state->active) {
        return;
    }
    state->noteObj.keyup();
    state->released = true;
}

bool isDx7VoiceActive(Dx7VoiceState* state) {
    return state != nullptr && state->active;
}

bool dx7VoiceRender(Dx7VoiceState* state, Dx7TimbreState* ts, float* out) {
    if (state == nullptr || !state->active) {
        return false;
    }

    if (state->bufferPos >= N) {
        for (int i = 0; i < N; i++) {
            state->buffer[i] = 0;
        }
        state->noteObj.compute(state->buffer, ts->lfoValue, ts->lfoDelay, &ts->controllers);
        state->bufferPos = 0;
    }

    int32_t raw0 = state->buffer[state->bufferPos];
    for (int i = 0; i < BLOCK_SIZE; i++) {
        int32_t val = state->buffer[state->bufferPos + i] >> 4;
        int32_t clip = val < -(1 << 24) ? -32768 : (val >= (1 << 24) ? 32767 : (val >> 9));
        float f = (float)clip * (DX7_OUTPUT_GAIN / 32768.0f);
        *out++ = f;
        *out++ = f;
    }
    state->bufferPos += BLOCK_SIZE;

    if (!state->noteObj.isPlaying() || (state->released && state->noteObj.isFinished())) {
        state->active = false;
        return false;
    }
    return true;
}

void dx7VoiceUpdateProgram(Dx7VoiceState* state, const uint8_t* program) {
    if (state == nullptr || !state->active) {
        return;
    }
    state->noteObj.update(program, state->note, state->velocity, 1);
}

// preenFM3: fast live-parameter update. Dx7Note::update() is still used for
// note-on and full preset/engine/DX7 loads; this path avoids reloading all
// six operators when only one DX7 parameter changes.
void dx7VoiceUpdateParam(Dx7VoiceState* state, int paramId, int op) {
    if (state == nullptr || !state->active) {
        return;
    }
    if (paramId >= DX7_EG_RATE_1 && paramId <= DX7_DETUNE) {
        // Editor operator 0 is OP1; Dx7Note indexes patch blocks 0..5 with
        // 0 = OP6, so reverse the operator number.
        state->noteObj.updateOperator(5 - op, state->velocity);
    } else {
        state->noteObj.updateGlobal();
    }
}

void dx7VoiceReset(Dx7VoiceState* state) {
    if (state == nullptr) {
        return;
    }
    state->active = false;
    state->released = false;
    state->bufferPos = N;
    state->note = 0;
    state->velocity = 0;
    for (int i = 0; i < N; i++) {
        state->buffer[i] = 0;
    }
}

void getDx7EnvelopeDisplay(const uint8_t* program, uint8_t operatorNumber, Dx7EnvelopeDisplay* env) {
    const uint8_t* op = program + (5 - operatorNumber) * 21;

    // A DX7 rate is the time needed to sweep the full level range. Using that
    // fixed range for every stage keeps the segment widths independent of the
    // levels, so editing a level only moves its own point.
    const float fullRange = (float)(Env::scaleoutlevel(99) >> 1) * (64.0f / 256.0f);

    for (int i = 0; i < 4; i++) {
        int qrate = (op[i] * 41) >> 6;
        if (qrate > 63) {
            qrate = 63;
        }
        int64_t inc = (int64_t)(4 + (qrate & 3)) << (2 + LG_N + (qrate >> 2));

        // inc is normalised to 44.1 kHz, so the sample count divided by
        // 44100 is the real duration at any sample rate.
        int64_t samples = (int64_t)(fullRange * 16777216.0f) / inc;
        env->time[i] = (float)samples / 44100.0f;
        env->level[i] = (float)op[4 + i] / 99.0f;
    }
}

void dx7EngineInit() {
    dx7Tuning = createStandardTuning();

    for (int i = 0; i < MAX_NUMBER_OF_VOICES; i++) {
        new (&dx7VoiceStorage[i][0]) Dx7VoiceState();
    }
    for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
        new (&dx7TimbreStorage[i][0]) Dx7TimbreState();
        dx7TimbreInit(dx7TimbreStates + i);
    }
}
