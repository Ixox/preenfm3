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
 * NRPN v2 capability values (doc/nrpn_spec_v2.md):
 *   [127,67..71] CAP_PROTOCOL_VERSION, feature bits lo/hi, max voices, max timbres
 */
#define MIDI_CAPABILITY_PROTOCOL_VERSION 1

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
#define MIDI_SUPPORTED_FEATURES MIDI_FEATURE_EXTENDED_ARP

#endif