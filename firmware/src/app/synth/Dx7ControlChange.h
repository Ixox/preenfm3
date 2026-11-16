/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef DX7_CONTROL_CHANGE_H_
#define DX7_CONTROL_CHANGE_H_

/*
 * CCs that map to DX7 global parameters. 38 is skipped because it is the NRPN
 * data LSB.
 */
enum Dx7ControlChange {
    CC_DX7_ALGORITHM = 33,
    CC_DX7_FEEDBACK = 34,
    CC_DX7_LFO_SPEED = 35,
    CC_DX7_LFO_DELAY = 36,
    CC_DX7_LFO_PITCH_MOD_DEPTH = 37,
    CC_DX7_LFO_AMP_MOD_DEPTH = 39,
    CC_DX7_LFO_KEY_SYNC = 111,
    CC_DX7_LFO_WAVEFORM = 112,
    CC_DX7_LFO_PITCH_MOD_SENSITIVITY = 113
};

#endif
