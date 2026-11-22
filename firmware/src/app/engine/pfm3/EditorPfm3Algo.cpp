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

#include "EditorPfm3Algo.h"

// clang-format off
const uint8_t algo1[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 3, 2,
    FEEDBACK, 6, 3, 3,
    END,
};

const uint8_t algo2[] = {
    MIX, 1,
    MIX, 2,
    OPERATOR, 1, 10,
    OPERATOR, 2, 12,
    OPERATOR, 3, 5,
    IM, 1, 3, 1,
    IM, 2, 3, 2,
    FEEDBACK, 6, 3, 3,
    END,
};


const uint8_t algo3[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 2,
    OPERATOR, 4, 6,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 4, 1,
    IM, 4, 3, 4,
    FEEDBACK, 6, 3, 3,
    END,
};

const uint8_t algo4[] = {
    MIX, 1,
    MIX, 2,
    OPERATOR, 1, 10,
    OPERATOR, 2, 12,
    OPERATOR, 3, 4,
    OPERATOR, 4, 3,
    IM, 1, 3, 1,
    IM, 2, 4, 2,
    IM, 3, 3, 2,
    IM, 4, 4, 3,
    FEEDBACK, 6, 4, 4,
    END,
};

const uint8_t algo5[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 8,
    OPERATOR, 3, 4,
    OPERATOR, 4, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 2,
    IM, 3, 4, 3,
    IM, 4, 4, 2,
    IM, 4, 4, 2,
    FEEDBACK, 6, 4, 4,
    END,
};

const uint8_t algo6[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 11,
    OPERATOR, 3, 12,
    OPERATOR, 4, 5,
    IM, 1, 4, 1,
    IM, 2, 4, 2,
    IM, 3, 4, 3,
    FEEDBACK, 6, 4, 4,
    END,
};

const uint8_t algo7[] = {
    MIX, 1,
    MIX, 3,
    MIX, 5,
    OPERATOR, 1, 10,
    OPERATOR, 3, 11,
    OPERATOR, 5, 12,
    OPERATOR, 2, 4,
    OPERATOR, 4, 2,
    OPERATOR, 6, 6,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 6, 5,
    IM, 4, 4, 6,
    FEEDBACK, 6, 4, 4,
    END,
};

const uint8_t algo8[] = {
    MIX, 1,
    MIX, 5,
    OPERATOR, 1, 10,
    OPERATOR, 2, 1,
    OPERATOR, 3, 2,
    OPERATOR, 4, 3,
    OPERATOR, 5, 12,
    OPERATOR, 6, 9,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 4, 1,
    IM, 4, 6, 5,
    FEEDBACK, 6, 4, 4,
    END,
};

const uint8_t algo9[] = {
    MIX, 1,
    MIX, 4,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 5,
    OPERATOR, 4, 12,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 5, 4,
    IM, 4, 6, 5,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t algo10[] = {
    MIX, 1,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 7,
    OPERATOR, 3, 12,
    OPERATOR, 4, 9,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 5, 4,
    IM, 4, 6, 5,
    FEEDBACK, 6, 2, 2,
    END,
};

const uint8_t algo11[] = {
    MIX, 1,
    MIX, 4,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 1,
    OPERATOR, 4, 12,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 2,
    IM, 3, 5, 4,
    IM, 4, 6, 5,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t algo12[] = {
    MIX, 1,
    MIX, 3,
    MIX, 5,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 12,
    OPERATOR, 6, 6,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 6, 5,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t algo13[] = {
    MIX, 1,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 5, 3,
    IM, 4, 6, 5,
    FEEDBACK, 6, 4, 4,
    END,
};

const uint8_t algo14[] = {
    MIX, 1,
    MIX, 4,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 1,
    OPERATOR, 4, 11,
    OPERATOR, 5, 5,
    OPERATOR, 6, 6,
    IM, 1, 2, 1,
    IM, 2, 3, 2,
    IM, 3, 5, 4,
    IM, 4, 6, 4,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t algo15[] = {
    MIX, 1,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 7,
    OPERATOR, 3, 12,
    OPERATOR, 4, 1,
    OPERATOR, 5, 2,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 5, 3,
    IM, 4, 6, 3,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t algo16[] = {
    MIX, 1,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 12,
    OPERATOR, 4, 9,
    OPERATOR, 5, 2,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 5, 4,
    IM, 4, 6, 4,
    FEEDBACK, 6, 2, 2,
    END,
};

const uint8_t algo17[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 5,
    OPERATOR, 4, 2,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 4, 3,
    IM, 4, 5, 1,
    IM, 5, 6, 5,
    FEEDBACK, 6, 2, 2,
    END,
};

const uint8_t algo18[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 7,
    OPERATOR, 3, 8,
    OPERATOR, 4, 9,
    OPERATOR, 5, 6,
    OPERATOR, 6, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 4, 1,
    IM, 4, 5, 4,
    IM, 5, 6, 5,
    FEEDBACK, 6, 3, 3,
    END,
};

const uint8_t algo19[] = {
    MIX, 1,
    MIX, 4,
    MIX, 5,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 1,
    OPERATOR, 4, 11,
    OPERATOR, 5, 12,
    OPERATOR, 6, 5,
    IM, 1, 2, 1,
    IM, 2, 3, 2,
    IM, 3, 6, 4,
    IM, 4, 6, 5,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t algo20[] = {
    MIX, 1,
    MIX, 2,
    MIX, 4,
    OPERATOR, 1, 10,
    OPERATOR, 2, 11,
    OPERATOR, 3, 4,
    OPERATOR, 4, 12,
    OPERATOR, 5, 5,
    OPERATOR, 6, 6,
    IM, 1, 3, 1,
    IM, 2, 3, 2,
    IM, 3, 5, 4,
    IM, 4, 6, 4,
    FEEDBACK, 6, 3, 3,
    END,
};

const uint8_t algo21[] = {
    MIX, 1,
    MIX, 2,
    MIX, 4,
    MIX, 5,
    OPERATOR, 1, 4,
    OPERATOR, 2, 6,
    OPERATOR, 3, 2,
    OPERATOR, 4, 10,
    OPERATOR, 5, 12,
    OPERATOR, 6, 8,
    IM, 1, 3, 1,
    IM, 2, 3, 2,
    IM, 3, 6, 4,
    IM, 4, 6, 5,
    FEEDBACK, 6, 3, 3,
    END,
};

const uint8_t algo22[] = {
    MIX, 1,
    MIX, 3,
    MIX, 4,
    MIX, 5,
    OPERATOR, 1, 4,
    OPERATOR, 2, 1,
    OPERATOR, 3, 10,
    OPERATOR, 4, 11,
    OPERATOR, 5, 12,
    OPERATOR, 6, 6,
    IM, 1, 2, 1,
    IM, 2, 6, 3,
    IM, 3, 6, 4,
    IM, 4, 6, 5,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t algo23[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    MIX, 4,
    MIX, 5,
    OPERATOR, 1, 1,
    OPERATOR, 2, 3,
    OPERATOR, 3, 10,
    OPERATOR, 4, 11,
    OPERATOR, 5, 12,
    OPERATOR, 6, 5,
    IM, 1, 6, 3,
    IM, 2, 6, 4,
    IM, 3, 6, 5,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t algo24[] = {
    MIX, 1,
    MIX, 3,
    MIX, 6,
    OPERATOR, 1, 10,
    OPERATOR, 2, 4,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 2,
    OPERATOR, 6, 12,
    IM, 1, 2, 1,
    IM, 2, 4, 3,
    IM, 3, 5, 4,
    FEEDBACK, 6, 5, 5,
    END,
};

const uint8_t algo25[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    MIX, 5,
    OPERATOR, 1, 1,
    OPERATOR, 2, 10,
    OPERATOR, 3, 11,
    OPERATOR, 4, 5,
    OPERATOR, 5, 12,
    OPERATOR, 6, 6,
    IM, 1, 4, 3,
    IM, 2, 6, 5,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t algo26[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    MIX, 6,
    OPERATOR, 1, 1,
    OPERATOR, 2, 10,
    OPERATOR, 3, 11,
    OPERATOR, 4, 8,
    OPERATOR, 5, 5,
    OPERATOR, 6, 12,
    IM, 1, 4, 3,
    IM, 2, 5, 4,
    FEEDBACK, 6, 5, 5,
    END,
};

const uint8_t algo27[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    MIX, 4,
    MIX, 5,
    MIX, 6,
    OPERATOR, 1, 4,
    OPERATOR, 2, 5,
    OPERATOR, 3, 6,
    OPERATOR, 4, 10,
    OPERATOR, 5, 11,
    OPERATOR, 6, 12,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t algo28[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    MIX, 4,
    MIX, 5,
    OPERATOR, 1, 4,
    OPERATOR, 2, 5,
    OPERATOR, 3, 6,
    OPERATOR, 4, 10,
    OPERATOR, 5, 11,
    OPERATOR, 6, 8,
    IM, 1, 6, 5,
    FEEDBACK, 6, 6, 6,
    END,
};

const uint8_t algo29[] = {
    MIX, 1,
    MIX, 2,
    OPERATOR, 1, 10,
    OPERATOR, 2, 12,
    OPERATORSYNC, 3, 4,
    OPERATOR, 4, 3,
    IMSYNC, 1, 3, 1,
    IM, 2, 4, 2,
    IM, 3, 4, 3,
    FEEDBACK, 6, 4, 4,
    END,
};

const uint8_t algo30[] = {
    MIX, 1,
    MIX, 2,
    OPERATOR, 1, 10,
    OPERATOR, 2, 12,
    OPERATORSYNC, 3, 4,
    OPERATOR, 4, 3,
    IMSYNC, 1, 3, 1,
    IMSYNC, 2, 3, 2,
    IM, 3, 4, 1,
    IM, 4, 4, 2,
    FEEDBACK, 6, 4, 4,
    END,
};

const uint8_t algo31[] = {
    MIX, 1,
    MIX, 2,
    MIX, 3,
    OPERATOR, 1, 10,
    OPERATOR, 2, 11,
    OPERATOR, 3, 12,
    OPERATORSYNC, 4, 5,
    IMSYNC, 1, 4, 1,
    IMSYNC, 2, 4, 2,
    IMSYNC, 3, 4, 3,
    END,
};

const uint8_t algo32[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 2,
    OPERATORSYNC, 4, 6,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IMSYNC, 3, 4, 1,
    IM, 4, 3, 4,
    FEEDBACK, 6, 3, 3,
    END,
};

const uint8_t* allAlgos[ALGO_END] = { algo1, algo2, algo3 , algo4, algo5, algo6, algo7, algo8, algo9, algo10, algo11,
        algo12, algo13, algo14, algo15, algo16, algo17, algo18, algo19, algo20, algo21, algo22, algo23, algo24, algo25,
        algo26, algo27, algo28, algo29, algo30, algo31, algo32 };
// clang-format on

struct ModulationIndex EditorPfm3Algo::modulationIndex[ALGO_END][6];

void EditorPfm3Algo::init() {
    for (int a = 0; a < ALGO_END; a++) {
        const uint8_t* algoInfo = allAlgos[a];
        int idx = 0;
        for (int o = 0; o < 6; o++) {
            modulationIndex[a][o].source = 0;
            modulationIndex[a][o].destination = 0;
        }
        while (algoInfo[idx] != END) {
            switch (algoInfo[idx++]) {
            case OPERATORSYNC:
            case OPERATOR:
                idx += 2;
                break;
            case IM:
            case IMSYNC:
            case FEEDBACK:
                modulationIndex[a][algoInfo[idx] - 1].source = algoInfo[idx + 1];
                modulationIndex[a][algoInfo[idx] - 1].destination = algoInfo[idx + 2];
                idx += 3;
                break;
            case MIX:
                idx++;
                break;
            }
        }
    }
}

const uint8_t* EditorPfm3Algo::table(int algo) {
    return allAlgos[algo % ALGO_END];
}
