/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef MIDI_CAPABILITIES_H_
#define MIDI_CAPABILITIES_H_

/*
 * Capability protocol exposed over NRPN:
 *   [127,126] feature bits
 *   [127,125] maximum LFO shape index
 *   [127,124] capability protocol version
 */
#define MIDI_CAPABILITY_PROTOCOL_VERSION 1

/* Number of NRPN messages returned by the engine capability block. */
#define MIDI_CAPABILITY_NRPN_COUNT 3

#define MIDI_FEATURE_OPERATOR_PHASE        (1u << 1)
#define MIDI_FEATURE_OPERATOR_WARP         (1u << 2)
#define MIDI_FEATURE_ENGINE_DECIMATION     (1u << 3)
#define MIDI_FEATURE_VOICE_PRIORITY        (1u << 4)
#define MIDI_FEATURE_STEP_SEQ_MODES        (1u << 5)
#define MIDI_FEATURE_EXTENDED_ARP          (1u << 6)
#define MIDI_FEATURE_EXTENDED_MATRIX_DESTS (1u << 7)
#define MIDI_FEATURE_LFO_ENV_CURVES        (1u << 8)

/*
 * Features currently implemented by this firmware.
 * All other capability bits remain cleared.
 */
#define MIDI_SUPPORTED_FEATURES 0

/* Current firmware implements the first 8 LFO shapes (0..7). */
#define MIDI_MAX_LFO_SHAPE 7

#endif