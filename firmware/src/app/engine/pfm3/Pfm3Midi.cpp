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
#include "Pfm3Param.h"

namespace Pfm3Midi {

/*
 * Allow to conserve same MIDI NRPN even when parameter rows are reordered.
 */
namespace {
struct Rows {
    int lfoPhase;
    int lfoEnv1;
    int lfoEnv2;
    int lfoSeq1;
    int lfoSeq2;
    int envTime;
    int envLevel;
};

Rows rows() {
    return Rows{
        pfm3FlatIndex(pfm3Param(Pfm3Block::LfoPhase, 0, 0)) / 4,
        pfm3FlatIndex(pfm3Param(Pfm3Block::LfoEnv1, 0, 0)) / 4,
        pfm3FlatIndex(pfm3Param(Pfm3Block::LfoEnv2, 0, 0)) / 4,
        pfm3FlatIndex(pfm3Param(Pfm3Block::LfoSeq, 0, 0)) / 4,
        pfm3FlatIndex(pfm3Param(Pfm3Block::LfoSeq, 1, 0)) / 4,
        pfm3FlatIndex(pfm3Param(Pfm3Block::EnvTime, 0, 0)) / 4,
        pfm3FlatIndex(pfm3Param(Pfm3Block::EnvLevel, 0, 0)) / 4,
    };
}
} // namespace

/*
 * Allow to conserve same MIDI NRPN even when parameter rows are reordered.
 */
int getMidiIndexFromMemory(int index) {
    const Rows r = rows();
    int paramRow = index >> 2;
    int encoder = index % 4;

    if (paramRow == r.lfoPhase) {
        // Move after the step sequencers
        paramRow += 4;
    } else if (paramRow == r.lfoEnv1 || paramRow == r.lfoEnv2 || paramRow == r.lfoSeq1 || paramRow == r.lfoSeq2) {
        // Move before the LFO phase row
        paramRow -= 1;
    } else if (paramRow >= r.envTime && paramRow < r.envTime + NUMBER_OF_OPERATORS) {
        // TIME and LEVEL have been grouped in december 2022
        paramRow = r.envTime + (paramRow - r.envTime) * 2;
        encoder *= 2;
    } else if (paramRow >= r.envLevel && paramRow < r.envLevel + NUMBER_OF_OPERATORS) {
        paramRow = r.envTime + (paramRow - r.envLevel) * 2;
        encoder = encoder * 2 + 1;
    }

    return paramRow * PFM3_PARAMS_PER_ROW + encoder;
}

int getMemoryIndexFromMidi(int index) {
    const Rows r = rows();
    int row = index >> 2;
    int encoder = index % 4;

    if (row == r.lfoPhase + 4) {
        row = r.lfoPhase;
    } else if (row == r.lfoEnv1 - 1 || row == r.lfoEnv2 - 1 || row == r.lfoSeq1 - 1 || row == r.lfoSeq2 - 1) {
        row++;
    } else if (row >= r.envTime && row < r.envTime + NUMBER_OF_OPERATORS * 2) {
        bool isLevel = encoder & 0x1;
        if (isLevel) {
            encoder--;
        }
        int num = (row - r.envTime) * 2 + (encoder >> 1);
        row = (isLevel ? r.envLevel : r.envTime) + (num >> 2);
        encoder = num % 4;
    }

    return row * PFM3_PARAMS_PER_ROW + encoder;
}

} // namespace Pfm3Midi
