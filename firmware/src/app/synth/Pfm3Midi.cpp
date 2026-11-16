/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Pfm3Midi.h"

#include "Common.h"

namespace Pfm3Midi {

/*
 * Allow to conserve same MIDI NRPN even when parameter rows are reordered.
 */
int getMidiIndexFromMemory(int index) {
    int paramRow = index >> 2;
    int encoder = index % 4;

    switch (paramRow) {
    case ROW_LFOPHASES:
        // Move after ROW_LFOSEQ2
        paramRow += 4;
        break;
    case ROW_LFOENV1:
    case ROW_LFOENV2:
    case ROW_LFOSEQ1:
    case ROW_LFOSEQ2:
        // Move before ROW_LFOPHASE
        paramRow -= 1;
        break;
    // TIME and LEVEL have been grouped in december 2022
    case ROW_ENV1_TIME:
    case ROW_ENV2_TIME:
    case ROW_ENV3_TIME:
    case ROW_ENV4_TIME:
    case ROW_ENV5_TIME:
    case ROW_ENV6_TIME:
        paramRow = ROW_ENV1_TIME + (paramRow - ROW_ENV1_TIME) * 2;
        encoder *= 2;
        break;
    case ROW_ENV1_LEVEL:
    case ROW_ENV2_LEVEL:
    case ROW_ENV3_LEVEL:
    case ROW_ENV4_LEVEL:
    case ROW_ENV5_LEVEL:
    case ROW_ENV6_LEVEL:
        paramRow = ROW_ENV1_TIME + (paramRow - ROW_ENV1_LEVEL) * 2;
        encoder = encoder * 2 + 1;
        break;
    }

    return paramRow * NUMBER_OF_ENCODERS_PFM2 + encoder;
}

int getMemoryIndexFromMidi(int index) {
    int row = index >> 2;
    int encoder = index % 4;
    // Move back row
    switch (row) {
    case ROW_LFOPHASES + 4:
        row = ROW_LFOPHASES;
        break;
    case ROW_LFOENV1 - 1:
    case ROW_LFOENV2 - 1:
    case ROW_LFOSEQ1 - 1:
    case ROW_LFOSEQ2 - 1:
        row++;
        break;
    case ROW_ENV1_TIME:
    case ROW_ENV2_TIME:
    case ROW_ENV3_TIME:
    case ROW_ENV4_TIME:
    case ROW_ENV5_TIME:
    case ROW_ENV6_TIME:
    case ROW_ENV1_LEVEL:
    case ROW_ENV2_LEVEL:
    case ROW_ENV3_LEVEL:
    case ROW_ENV4_LEVEL:
    case ROW_ENV5_LEVEL:
    case ROW_ENV6_LEVEL: {
        bool isLevel = encoder & 0x1;
        if (isLevel)
            encoder--;
        // num is the number of the value.. More than 4 => next operator
        int num = (row - ROW_ENV1_TIME) * 2 + (encoder >> 1);
        // Convert to new row/encoder
        row = (isLevel ? ROW_ENV1_LEVEL : ROW_ENV1_TIME) + (num >> 2);
        encoder = num % 4;
        break;
    }
    }
    return row * 4 + encoder;
}

} // namespace Pfm3Midi
