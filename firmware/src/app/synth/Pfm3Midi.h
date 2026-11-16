/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PFM3_MIDI_H_
#define PFM3_MIDI_H_

namespace Pfm3Midi {

/*
 * Legacy PFM3 NRPN index <-> flat PFM3 memory index translation.
 * Keep these mapping tables unchanged: they define the external wire format.
 */
int getMemoryIndexFromMidi(int index);
int getMidiIndexFromMemory(int index);

} // namespace Pfm3Midi

#endif
