/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * MSFA engine state.
 *
 * The MSFA (Dexed) headers define Lfo/Env, which collide with the PFM3
 * classes of the same name. Keep those headers out of the firmware headers
 * and expose an opaque state plus free functions instead.
 *
 * The state lives in dedicated RAM_D1 arrays instead of inside Voice/Timbre
 * (which are in DTCMRAM).
 */

#ifndef MSFA_ENGINE_H_
#define MSFA_ENGINE_H_

#include "Common.h"

struct MsfaVoiceState;
struct MsfaTimbreState;

/*
 * Approximated operator envelope for the oscilloscope background.
 * time[] is in seconds, level[] is normalised to the highest stage (0..1).
 */
struct MsfaEnvelopeDisplay {
    float time[4];
    float level[4];
};

extern MsfaVoiceState *msfaVoiceStates;
extern MsfaTimbreState *msfaTimbreStates;

MsfaVoiceState *getMsfaVoiceState(int index);
MsfaTimbreState *getMsfaTimbreState(int index);

void msfaEngineInit();

void msfaTimbreInit(MsfaTimbreState *state);
void msfaTimbreKeyDown(MsfaTimbreState *state);
void msfaTimbreNextBlock(MsfaTimbreState *state);
void msfaTimbreLoadProgram(MsfaTimbreState *state, const uint8_t *program);
void msfaTimbreSetController(MsfaTimbreState *state, enum SourceEnum source, float value);

void msfaVoiceStart(MsfaVoiceState *state, const uint8_t *program, uint8_t note,
                    uint8_t velocity, uint8_t channel, MsfaTimbreState *ts);
void msfaVoiceKeyUp(MsfaVoiceState *state);
bool isMsfaVoiceActive(MsfaVoiceState *state);
bool msfaVoiceRender(MsfaVoiceState *state, MsfaTimbreState *ts, float *out);
void msfaVoiceUpdateProgram(MsfaVoiceState *state, const uint8_t *program);
void msfaVoiceUpdateParam(MsfaVoiceState *state, int paramId, int op);
void msfaVoiceReset(MsfaVoiceState *state);

void getMsfaEnvelopeDisplay(const uint8_t *program, uint8_t operatorNumber, MsfaEnvelopeDisplay *env);

#endif
