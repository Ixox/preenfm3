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

extern MsfaVoiceState *msfaVoiceStates;
extern MsfaTimbreState *msfaTimbreStates;

MsfaVoiceState *msfaGetVoiceState(int index);
MsfaTimbreState *msfaGetTimbreState(int index);

void msfaEngineInit();

void msfaTimbreInit(MsfaTimbreState *state);
void msfaTimbreKeyDown(MsfaTimbreState *state);
void msfaTimbreNextBlock(MsfaTimbreState *state);
void msfaTimbreLoadProgram(MsfaTimbreState *state, const uint8_t *program);
void msfaTimbreSetController(MsfaTimbreState *state, enum SourceEnum source, float value);

void msfaVoiceStart(MsfaVoiceState *state, const uint8_t *program, uint8_t note,
                    uint8_t velocity, uint8_t channel, MsfaTimbreState *ts);
void msfaVoiceKeyUp(MsfaVoiceState *state);
bool msfaVoiceIsActive(MsfaVoiceState *state);
bool msfaVoiceRender(MsfaVoiceState *state, MsfaTimbreState *ts, float *out);
void msfaVoiceUpdateProgram(MsfaVoiceState *state, const uint8_t *program);
void msfaVoiceReset(MsfaVoiceState *state);

#endif
