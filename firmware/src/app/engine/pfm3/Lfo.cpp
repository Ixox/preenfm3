/*
 * Copyright 2013 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Lfo.h"

// Engine state in RAM_D1.
float Pfm3Lfo::invTab[2048] __attribute__((section(".ram_d1")));
int Pfm3Lfo::initTab = 0;

Pfm3Lfo::Pfm3Lfo() {
}

void Pfm3Lfo::init(Matrix* matrix, SourceEnum source, DestinationEnum dest) {
    this->destination_ = (uint8_t)dest;
    this->source_ = (uint8_t)source;
    this->matrix_ = matrix;

    if (initTab == 0) {
        initTab = 1;
        for (int k = 1; k < 2048; k++) {
            invTab[k] = 1.0f / ((float)k);
        }
        invTab[0] = 1.0;
    }
}
