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

#ifndef MATRIX_H_
#define MATRIX_H_

#define MATRIX_SIZE 12

#include "Common.h"
#include "Pfm3Params.h"

class Matrix {
    friend class Timbre;

public:
    Matrix();
    ~Matrix();

    void init(struct MatrixRowParams* matrixRows);

    void resetSources() {
        for (int k = 0; k < MATRIX_SOURCE_MAX; k++) {
            setSource((SourceEnum)k, 0);
        }
    }

    void resetAllDestination() {
        for (int k = 0; k < DESTINATION_MAX; k++) {
            destinations_[k] = 0;
        }
    }

    void resetDestination(int k) {
        destinations_[k] = 0;
    }

    void computeAllDestinations() {
        // Store values before erasing destinations
        float mul1 = destinations_[MTX1_MUL];
        float mul2 = destinations_[MTX2_MUL];
        float mul3 = destinations_[MTX3_MUL];
        float mul4 = destinations_[MTX4_MUL];

        // Let's erase destinations first
        int k = 0;
        // No need to erase first one (tiny optim)
        // destinations[(int)rows[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;
        destinations_[(int)rows_[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;
        destinations_[(int)rows_[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;
        destinations_[(int)rows_[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;
        destinations_[(int)rows_[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;
        destinations_[(int)rows_[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;
        destinations_[(int)rows_[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;
        destinations_[(int)rows_[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;
        destinations_[(int)rows_[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;
        destinations_[(int)rows_[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;
        destinations_[(int)rows_[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;
        destinations_[(int)rows_[k].dest1] = 0;
        destinations_[(int)rows_[k++].dest2] = 0;

        // For first row can be modified by destination
        if (likely(rows_[0].source != MATRIX_SOURCE_NONE)) {
            float sourceTimesMul = sources_[(int)rows_[0].source] * (rows_[0].mul + mul1);
            // First one was not set to 0 above (tiny optim)
            destinations_[(int)rows_[0].dest1] = sourceTimesMul;
            destinations_[(int)rows_[0].dest2] += sourceTimesMul;
        }
        if (likely(rows_[1].source != MATRIX_SOURCE_NONE)) {
            float sourceTimesMul = sources_[(int)rows_[1].source] * (rows_[1].mul + mul2);
            destinations_[(int)rows_[1].dest1] += sourceTimesMul;
            destinations_[(int)rows_[1].dest2] += sourceTimesMul;
        }
        if (likely(rows_[2].source != MATRIX_SOURCE_NONE)) {
            float sourceTimesMul = sources_[(int)rows_[2].source] * (rows_[2].mul + mul3);
            destinations_[(int)rows_[2].dest1] += sourceTimesMul;
            destinations_[(int)rows_[2].dest2] += sourceTimesMul;
        }
        if (likely(rows_[3].source != MATRIX_SOURCE_NONE)) {
            float sourceTimesMul = sources_[(int)rows_[3].source] * (rows_[3].mul + mul4);
            destinations_[(int)rows_[3].dest1] += sourceTimesMul;
            destinations_[(int)rows_[3].dest2] += sourceTimesMul;
        }

        // and compute other rows
        for (int r = 4; r < MATRIX_SIZE; r++) {
            if (likely(rows_[r].source != MATRIX_SOURCE_NONE && rows_[r].mul != 0.0f)) {
                float sourceTimesMul = sources_[(int)rows_[r].source] * rows_[r].mul;
                destinations_[(int)rows_[r].dest1] += sourceTimesMul;
                destinations_[(int)rows_[r].dest2] += sourceTimesMul;
            }
        }
    }

    void setSource(SourceEnum source, float value) __attribute__((always_inline)) {
        this->sources_[source] = value;
    }

    float getDestination(DestinationEnum destination) __attribute__((always_inline)) {
        return this->destinations_[destination];
    }

    // Usefull for MPE to retrieve PITCHBEND
    float getSource(SourceEnum source) {
        return this->sources_[source];
    }

private:
    float sources_[MATRIX_SOURCE_MAX];
    float destinations_[DESTINATION_MAX];
    struct MatrixRowParams* rows_;
};

#endif /* MATRIX_H_ */
