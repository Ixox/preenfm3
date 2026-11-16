/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PFM3_PARAMETER_ROWS_H_
#define PFM3_PARAMETER_ROWS_H_

struct AllParameterRowsDisplay;

/*
 * PFM3 parameter display metadata, indexed by PFM3 row number.
 * Used by the engine, the MIDI map and the editor.
 */
extern struct AllParameterRowsDisplay allParameterRows;

#endif
