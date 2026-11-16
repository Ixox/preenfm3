/*
 * One-time MSFA initialization.
 *
 * Keep this header free of MSFA internal headers so the firmware can call it
 * without pulling synth.h macros (N, LG_N, min/max) into unrelated code.
 */

#ifndef MSFA_INIT_H
#define MSFA_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

void msfaInit(float sampleRate);

#ifdef __cplusplus
}
#endif

#endif  // MSFA_INIT_H
