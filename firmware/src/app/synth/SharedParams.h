/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHARED_PARAMS_H_
#define SHARED_PARAMS_H_

/*
 * Timbre-wide parameters that are valid for every engine (Arp and FX1/FX2).
 * Engines with their own flat parameter model map these internally.
 */
enum SharedParamId {
    SHARED_PARAM_ARP_CLOCK = 0,
    SHARED_PARAM_ARP_DIRECTION,
    SHARED_PARAM_ARP_OCTAVE,
    SHARED_PARAM_ARP_PATTERN,
    SHARED_PARAM_ARP_DIVISION,
    SHARED_PARAM_ARP_DURATION,
    SHARED_PARAM_EFFECT1_TYPE,
    SHARED_PARAM_EFFECT1_PARAM1,
    SHARED_PARAM_EFFECT1_PARAM2,
    SHARED_PARAM_EFFECT1_PARAM3,
    SHARED_PARAM_EFFECT2_TYPE,
    SHARED_PARAM_EFFECT2_PARAM1,
    SHARED_PARAM_EFFECT2_PARAM2,
    SHARED_PARAM_EFFECT2_PARAM3,
    SHARED_PARAM_COUNT
};

#endif
