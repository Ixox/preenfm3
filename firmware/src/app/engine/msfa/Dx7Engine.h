/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * DX7 engine state.
 *
 * Keep the vendored DX7 (Dexed/MSFA) headers out of the firmware headers and
 * expose an opaque state plus free functions instead, so those headers (and
 * their std dependencies) are not pulled into most translation units.
 *
 * The state lives in dedicated RAM_D1 arrays instead of inside Voice/Timbre
 * (which are in DTCMRAM).
 */

#ifndef DX7_ENGINE_H_
#define DX7_ENGINE_H_

#include "Common.h"

struct Dx7VoiceState;
struct Dx7TimbreState;

/*
 * Approximated operator envelope for the oscilloscope background.
 * time[] is in seconds, level[] is normalised to the highest stage (0..1).
 */
struct Dx7EnvelopeDisplay {
    float time[4];
    float level[4];
};

Dx7VoiceState* getDx7VoiceState(int index);
Dx7TimbreState* getDx7TimbreState(int index);

void dx7EngineInit();

void dx7TimbreInit(Dx7TimbreState* state);
void dx7TimbreKeyDown(Dx7TimbreState* state);
void dx7TimbreNextBlock(Dx7TimbreState* state);
void dx7TimbreLoadProgram(Dx7TimbreState* state, const uint8_t* program);
void dx7TimbreSetController(Dx7TimbreState* state, enum SourceEnum source, float value);

void dx7VoiceStart(Dx7VoiceState* state, const uint8_t* program, uint8_t note, uint8_t velocity, uint8_t channel, Dx7TimbreState* ts,
                   bool voiceStolen);
void dx7VoiceKeyUp(Dx7VoiceState* state);
bool isDx7VoiceActive(Dx7VoiceState* state);
bool dx7VoiceRender(Dx7VoiceState* state, Dx7TimbreState* ts, float* out);
void dx7VoiceUpdateProgram(Dx7VoiceState* state, const uint8_t* program);
void dx7VoiceUpdateParam(Dx7VoiceState* state, int paramId, int op);
void dx7VoiceReset(Dx7VoiceState* state);

void getDx7EnvelopeDisplay(const uint8_t* program, uint8_t operatorNumber, Dx7EnvelopeDisplay* env);

#endif
