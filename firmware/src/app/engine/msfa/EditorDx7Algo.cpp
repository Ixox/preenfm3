/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <dot> hosxe (at) g m a i l <dot> com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "EditorDx7Algo.h"
#include "TftAlgo.h"

// clang-format off
const uint8_t dx7algo1[] = {
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 7,
    OPERATOR, 3, 12,
    OPERATOR, 4, 9,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 6, 5,
    IM, 2, 5, 4,
    IM, 3, 4, 3,
    IM, 4, 2, 1,
    FEEDBACK, 5, 6, 6,
    END,
};

const uint8_t dx7algo2[] = {
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 7,
    OPERATOR, 3, 12,
    OPERATOR, 4, 9,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 6, 5,
    IM, 2, 5, 4,
    IM, 3, 4, 3,
    IM, 4, 2, 1,
    FEEDBACK, 5, 2, 2,
    END,
};

const uint8_t dx7algo3[] = {
    MIX, 4,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 1,
    OPERATOR, 4, 12,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 6, 5,
    IM, 2, 5, 4,
    IM, 3, 3, 2,
    IM, 4, 2, 1,
    FEEDBACK, 5, 6, 6,
    END,
};

const uint8_t dx7algo4[] = {
    MIX, 4,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 1,
    OPERATOR, 4, 12,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 6, 5,
    IM, 2, 5, 4,
    IM, 3, 3, 2,
    IM, 4, 2, 1,
    FEEDBACK, 5, 4, 6,
    END,
};

const uint8_t dx7algo5[] = {
    MIX, 5,
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 12,
    OPERATOR, 6, 6,
    IM, 1, 6, 5,
    IM, 2, 4, 3,
    IM, 3, 2, 1,
    FEEDBACK, 4, 6, 6,
    END,
};

const uint8_t dx7algo6[] = {
    MIX, 5,
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 12,
    OPERATOR, 6, 6,
    IM, 1, 6, 5,
    IM, 2, 4, 3,
    IM, 3, 2, 1,
    FEEDBACK, 4, 5, 6,
    END,
};

const uint8_t dx7algo7[] = {
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 6, 5,
    IM, 2, 5, 3,
    IM, 3, 4, 3,
    IM, 4, 2, 1,
    FEEDBACK, 5, 6, 6,
    END,
};

const uint8_t dx7algo8[] = {
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 6, 5,
    IM, 2, 5, 3,
    IM, 3, 4, 3,
    IM, 4, 2, 1,
    FEEDBACK, 5, 4, 4,
    END,
};

const uint8_t dx7algo9[] = {
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 6, 5,
    IM, 2, 5, 3,
    IM, 3, 4, 3,
    IM, 4, 2, 1,
    FEEDBACK, 5, 2, 2,
    END,
};

const uint8_t dx7algo10[] = {
    MIX, 4,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 1,
    OPERATOR, 4, 11,
    OPERATOR, 5, 5,
    OPERATOR, 6, 6,
    IM, 1, 6, 4,
    IM, 2, 5, 4,
    IM, 3, 3, 2,
    IM, 4, 2, 1,
    FEEDBACK, 5, 3, 3,
    END,
};

const uint8_t dx7algo11[] = {
    MIX, 4,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 1,
    OPERATOR, 4, 11,
    OPERATOR, 5, 5,
    OPERATOR, 6, 6,
    IM, 1, 6, 4,
    IM, 2, 5, 4,
    IM, 3, 3, 2,
    IM, 4, 2, 1,
    FEEDBACK, 5, 6, 6,
    END,
};

const uint8_t dx7algo12[] = {
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 7,
    OPERATOR, 3, 12,
    OPERATOR, 4, 1,
    OPERATOR, 5, 2,
    OPERATOR, 6, 3,
    IM, 1, 6, 3,
    IM, 2, 5, 3,
    IM, 3, 4, 3,
    IM, 4, 2, 1,
    FEEDBACK, 5, 2, 2,
    END,
};

const uint8_t dx7algo13[] = {
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 7,
    OPERATOR, 3, 12,
    OPERATOR, 4, 1,
    OPERATOR, 5, 2,
    OPERATOR, 6, 3,
    IM, 1, 6, 3,
    IM, 2, 5, 3,
    IM, 3, 4, 3,
    IM, 4, 2, 1,
    FEEDBACK, 5, 6, 6,
    END,
};

const uint8_t dx7algo14[] = {
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 12,
    OPERATOR, 4, 9,
    OPERATOR, 5, 2,
    OPERATOR, 6, 3,
    IM, 1, 6, 4,
    IM, 2, 5, 4,
    IM, 3, 4, 3,
    IM, 4, 2, 1,
    FEEDBACK, 5, 6, 6,
    END,
};

const uint8_t dx7algo15[] = {
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 12,
    OPERATOR, 4, 9,
    OPERATOR, 5, 2,
    OPERATOR, 6, 3,
    IM, 1, 6, 4,
    IM, 2, 5, 4,
    IM, 3, 4, 3,
    IM, 4, 2, 1,
    FEEDBACK, 5, 2, 2,
    END,
};

const uint8_t dx7algo16[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 5,
    OPERATOR, 4, 2,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 6, 5,
    IM, 2, 4, 3,
    IM, 3, 5, 1,
    IM, 4, 3, 1,
    IM, 5, 2, 1,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t dx7algo17[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 5,
    OPERATOR, 4, 2,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 6, 5,
    IM, 2, 4, 3,
    IM, 3, 5, 1,
    IM, 4, 3, 1,
    IM, 5, 2, 1,
    FEEDBACK, 6, 2, 2,
    END,
};

const uint8_t dx7algo18[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 7,
    OPERATOR, 3, 8,
    OPERATOR, 4, 9,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 6, 5,
    IM, 2, 5, 4,
    IM, 3, 4, 1,
    IM, 4, 3, 1,
    IM, 5, 2, 1,
    FEEDBACK, 6, 3, 3,
    END,
};

const uint8_t dx7algo19[] = {
    MIX, 5,
    MIX, 4,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 1,
    OPERATOR, 4, 11,
    OPERATOR, 5, 12,
    OPERATOR, 6, 6,
    IM, 1, 6, 5,
    IM, 2, 6, 4,
    IM, 3, 3, 2,
    IM, 4, 2, 1,
    FEEDBACK, 5, 6, 6,
    END,
};

const uint8_t dx7algo20[] = {
    MIX, 4,
    MIX, 2,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 11,
    OPERATOR, 3, 4,
    OPERATOR, 4, 12,
    OPERATOR, 5, 5,
    OPERATOR, 6, 6,
    IM, 1, 6, 4,
    IM, 2, 5, 4,
    IM, 3, 3, 2,
    IM, 4, 3, 1,
    FEEDBACK, 5, 3, 3,
    END,
};

const uint8_t dx7algo21[] = {
    MIX, 5,
    MIX, 4,
    MIX, 2,
    MIX, 1,
    OPERATOR, 1, 4,
    OPERATOR, 2, 6,
    OPERATOR, 3, 2,
    OPERATOR, 4, 10,
    OPERATOR, 5, 12,
    OPERATOR, 6, 8,
    IM, 1, 6, 5,
    IM, 2, 6, 4,
    IM, 3, 3, 2,
    IM, 4, 3, 1,
    FEEDBACK, 5, 3, 3,
    END,
};

const uint8_t dx7algo22[] = {
    MIX, 5,
    MIX, 4,
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 4,
    OPERATOR, 2, 1,
    OPERATOR, 3, 10,
    OPERATOR, 4, 11,
    OPERATOR, 5, 12,
    OPERATOR, 6, 8,
    IM, 1, 6, 5,
    IM, 2, 6, 4,
    IM, 3, 6, 3,
    IM, 4, 2, 1,
    FEEDBACK, 5, 6, 6,
    END,
};

const uint8_t dx7algo23[] = {
    MIX, 5,
    MIX, 4,
    MIX, 2,
    MIX, 1,
    OPERATOR, 1, 4,
    OPERATOR, 2, 6,
    OPERATOR, 3, 3,
    OPERATOR, 4, 10,
    OPERATOR, 5, 12,
    OPERATOR, 6, 8,
    IM, 1, 6, 5,
    IM, 2, 6, 4,
    IM, 3, 3, 2,
    FEEDBACK, 4, 6, 6,
    END,
};

const uint8_t dx7algo24[] = {
    MIX, 5,
    MIX, 4,
    MIX, 3,
    MIX, 2,
    MIX, 1,
    OPERATOR, 1, 1,
    OPERATOR, 2, 2,
    OPERATOR, 3, 10,
    OPERATOR, 4, 11,
    OPERATOR, 5, 12,
    OPERATOR, 6, 8,
    IM, 1, 6, 5,
    IM, 2, 6, 4,
    IM, 3, 6, 3,
    FEEDBACK, 4, 6, 6,
    END,
};

const uint8_t dx7algo25[] = {
    MIX, 5,
    MIX, 4,
    MIX, 3,
    MIX, 2,
    MIX, 1,
    OPERATOR, 1, 1,
    OPERATOR, 2, 2,
    OPERATOR, 3, 3,
    OPERATOR, 4, 10,
    OPERATOR, 5, 12,
    OPERATOR, 6, 8,
    IM, 1, 6, 5,
    IM, 2, 6, 4,
    FEEDBACK, 3, 6, 6,
    END,
};

const uint8_t dx7algo26[] = {
    MIX, 4,
    MIX, 2,
    MIX, 1,
    OPERATOR, 1, 4,
    OPERATOR, 2, 6,
    OPERATOR, 3, 3,
    OPERATOR, 4, 11,
    OPERATOR, 5, 7,
    OPERATOR, 6, 9,
    IM, 1, 6, 4,
    IM, 2, 5, 4,
    IM, 3, 3, 2,
    FEEDBACK, 4, 6, 6,
    END,
};

const uint8_t dx7algo27[] = {
    MIX, 4,
    MIX, 2,
    MIX, 1,
    OPERATOR, 1, 4,
    OPERATOR, 2, 6,
    OPERATOR, 3, 3,
    OPERATOR, 4, 11,
    OPERATOR, 5, 7,
    OPERATOR, 6, 9,
    IM, 1, 6, 4,
    IM, 2, 5, 4,
    IM, 3, 3, 2,
    FEEDBACK, 4, 3, 3,
    END,
};

const uint8_t dx7algo28[] = {
    MIX, 6,
    MIX, 3,
    MIX, 1,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 2,
    OPERATOR, 6, 12,
    IM, 1, 5, 4,
    IM, 2, 4, 3,
    IM, 3, 2, 1,
    FEEDBACK, 4, 5, 5,
    END,
};

const uint8_t dx7algo29[] = {
    MIX, 5,
    MIX, 3,
    MIX, 2,
    MIX, 1,
    OPERATOR, 1, 1,
    OPERATOR, 2, 3,
    OPERATOR, 3, 10,
    OPERATOR, 4, 7,
    OPERATOR, 5, 12,
    OPERATOR, 6, 9,
    IM, 1, 6, 5,
    IM, 2, 4, 3,
    FEEDBACK, 3, 6, 6,
    END,
};

const uint8_t dx7algo30[] = {
    MIX, 6,
    MIX, 3,
    MIX, 2,
    MIX, 1,
    OPERATOR, 1, 1,
    OPERATOR, 2, 2,
    OPERATOR, 3, 11,
    OPERATOR, 4, 8,
    OPERATOR, 5, 5,
    OPERATOR, 6, 3,
    IM, 1, 5, 4,
    IM, 2, 4, 3,
    FEEDBACK, 3, 5, 5,
    END,
};

const uint8_t dx7algo31[] = {
    MIX, 5,
    MIX, 4,
    MIX, 3,
    MIX, 2,
    MIX, 1,
    OPERATOR, 1, 1,
    OPERATOR, 2, 2,
    OPERATOR, 3, 3,
    OPERATOR, 4, 10,
    OPERATOR, 5, 12,
    OPERATOR, 6, 9,
    IM, 1, 6, 5,
    FEEDBACK, 2, 6, 6,
    END,
};

const uint8_t dx7algo32[] = {
    MIX, 6,
    MIX, 5,
    MIX, 4,
    MIX, 3,
    MIX, 2,
    MIX, 1,
    OPERATOR, 1, 4,
    OPERATOR, 2, 5,
    OPERATOR, 3, 6,
    OPERATOR, 4, 10,
    OPERATOR, 5, 11,
    OPERATOR, 6, 12,
    FEEDBACK, 1, 6, 6,
    END,
};

#define NUMBER_OF_DX7_ALGOS 32
const uint8_t* dx7Algos[NUMBER_OF_DX7_ALGOS] = { dx7algo1, dx7algo2, dx7algo3, dx7algo4, dx7algo5, dx7algo6, dx7algo7, dx7algo8,
        dx7algo9, dx7algo10, dx7algo11, dx7algo12, dx7algo13, dx7algo14, dx7algo15, dx7algo16, dx7algo17, dx7algo18,
        dx7algo19, dx7algo20, dx7algo21, dx7algo22, dx7algo23, dx7algo24, dx7algo25, dx7algo26, dx7algo27, dx7algo28,
        dx7algo29, dx7algo30, dx7algo31, dx7algo32 };
// clang-format on

const uint8_t* EditorDx7Algo::table(int algo) {
    return dx7Algos[algo % NUMBER_OF_DX7_ALGOS];
}

bool EditorDx7Algo::getFeedback(int algo, uint8_t& opSource, uint8_t& opDest) {
    algo = algo % NUMBER_OF_DX7_ALGOS;
    const uint8_t* algoInfo = dx7Algos[algo];
    int idx = 0;
    while (algoInfo[idx] != END) {
        switch (algoInfo[idx++]) {
        case OPERATORSYNC:
        case OPERATOR:
            idx += 2;
            break;
        case IM:
        case IMSYNC:
            idx += 3;
            break;
        case FEEDBACK:
            opSource = algoInfo[idx + 1];
            opDest = algoInfo[idx + 2];
            return true;
        case MIX:
            idx++;
            break;
        }
    }
    return false;
}
