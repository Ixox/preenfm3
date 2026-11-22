/*
 * Copyright 2026 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
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

#include "EditorPfm3.h"
#include "EditorPfm3Algo.h"
#include "Pfm3ParameterRows.h"
#include "Pfm3Preset.h"
#include "ScreenEditor.h"
#include "ScreenTypes.h"
#include "SynthState.h"
#include <FirmwareTftDisplay.h>

EditorPfm3::EditorPfm3() {
    for (int i = 0; i < NUMBER_OF_ENCODERS; i++) {
        imChangedCounter_[i] = 0;
        mixChangedCounter_[i] = 0;
        mixChangeOperatorNumber_[i] = 0;
    }
    multipleEdition_ = false;
    bufferPageNumber_ = -1;
    bufferEditNumber_ = 0;
    for (int i = 0; i < 6; i++) {
        bufferParam_[i] = 0.0f;
    }
}

static const char* nullNames[] = {};
static const unsigned char* nullNamesOrder = 0;

const char* algoNames[] = {"alg1", "alg2", "alg3", "alg4", "alg5", "alg6", "alg7", "alg8", "alg9", "al10", "al11",
                           "al12", "al13", "al14", "al15", "al16", "al17", "al18", "al19", "al20", "al21", "al22",
                           "al23", "al24", "al25", "al26", "al27", "al28", "al29", "al30", "al31", "al32"};

const char* polyMonoNames[] = {"Mono", "Poly", "Unis"};

const char* glideTypeNames[] = {
    "Off    ",
    "Overlap",
    "Always ",
};

const char* syncNames[] = {"Int", "Ext"};

const char* clockName[] = {"Off ", "Int ", "Ext "};
const char* dirName[] = {"Up  ", "Down", "U&D ", "Play", "Rand", "Chrd", "RtUp", "RtDn", "RtUD", "ShUp", "ShDn", "ShUD"};

//   192, 144, 96, 72, 64, 48, 36, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1

const char* divNames[] = {"2/1 ", "3/2 ", "1/1 ", "3/4 ", "2/3 ", "1/2 ", "3/8 ", "1/3 ", "1/4 ",
                          "1/6 ", "1/8 ", "1/12", "1/16", "1/24", "1/32", "1/48", "1/96"};
const char* activeName[] = {"Off ", "On  "};

const char* patternName[] = {"1   ", "2   ", "3   ", "4   ", "5   ", "6   ", "7   ",  "8   ", "9   ", "10   ", "11  ", "12  ", "13  ",
                             "14  ", "15  ", "16  ", "17  ", "18  ", "19  ", "20   ", "21  ", "22  ", "Usr1",  "Usr2", "Usr3", "Usr4"};

const char* envCurveNames[] = {"Exp ", "Lin ", "Log ", "Usr1", "Usr2", "Usr3", "Usr4"};

/* FILTER ORDER

 0   Off , -0-
 1   Mix , -1-
 2   LP  , -2-
 3   HP  , -3-
 4   Bass, -4-
 5   BP  , -5-
 6   Crsh, -6-
 7   Oryx, -38-
 8   Orx2, -39-
 9   Orx3, -40-
 10  h3o+, -46-
 11  Svh3, -47-
 12  Pann, -22-
 13  LP2 , -7-
 14  HP2 , -8-
 15  Lp3 , -10-
 16  Hp3 , -11-
 17  Bp3 , -12-
 18  Peak, -13-
 19  Notc, -14-
 20  Bell, -15-
 21  LowS, -16-
 22  HigS, -17-
 23  LpHp, -18-
 24  BpDs, -19-
 25  LPws, -20-
 26  Tilt, -21-
 27  Sat , -23-
 28  Sigm, -24-
 29  Fold, -25-
 30  Wrap, -26-
 31  LpSn, -32-
 32  HpSn, -33-
 33  Not4, -34-
 34  Ap4 , -35-
 35  Ap4b, -36-
 36  Ap4D, -37-
 37  18db, -41-
 38  La+d, -42-
 39  BP2 , -9-
 40  Lad+, -43-
 41  Diod, -44-
 42  L+d+, -45-
 43  Xor , -27-
 44  Txr1, -28-
 45  Txr2, -29-
 46  LPx1, -30-
 47  LPx2, -31-
 48  Alkx, -48-
 49  Flng, -49-
 50  Chor, -50-
 51  Dim , -51-
 52  Dblr, -52-
 53  Harm, -53-
 54  Bode, -54-
 55  Wide, -55-
 56  DelC, -56-
 57  Ping, -57-
 58  Diff, -58-
 */

const unsigned char filtersOrder[] = {0,  1,  2,  3,  4,  5,  6,  38, 39, 40, 46, 47, 22, 7,  8,  10, 11, 12, 13, 14,
                                      15, 16, 17, 18, 19, 20, 21, 23, 24, 25, 26, 32, 33, 34, 35, 36, 37, 41, 42, 9,
                                      43, 44, 45, 27, 28, 29, 30, 31, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58};

const unsigned char filtersPosition[] = {0,  1,  2,  3,  4,  5,  6,  13, 14, 39, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                                         25, 26, 12, 27, 28, 29, 30, 43, 44, 45, 46, 47, 31, 32, 33, 34, 35, 36, 7,  8,
                                         9,  37, 38, 40, 41, 42, 10, 11, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58};

const char* fxName[] = {
    "Off ", /*  0   */
    "Mix ", /*  1   */
    "LP  ", /*  2   */
    "HP  ", /*  3   */
    "Bass", /*  4   */
    "BP  ", /*  5   */
    "Crsh", /*  6   */
    "LP2 ", /*  7   */
    "HP2 ", /*  8   */
    "BP2 ", /*  9   */
    "Lp3 ", /*  10  */
    "Hp3 ", /*  11  */
    "Bp3 ", /*  12  */
    "Peak", /*  13  */
    "Notc", /*  14  */
    "Bell", /*  15  */
    "LowS", /*  16  */
    "HigS", /*  17  */
    "LpHp", /*  18  */
    "BpDs", /*  19  */
    "LPws", /*  20  */
    "Tilt", /*  21  */
    "Pann", /*  22  */
    "Sat ", /*  23  */
    "Sigm", /*  24  */
    "Fold", /*  25  */
    "Wrap", /*  26  */
    "Xor ", /*  27  */
    "Txr1", /*  28  */
    "Txr2", /*  29  */
    "LPx1", /*  30  */
    "LPx2", /*  31  */
    "LpSn", /*  32  */
    "HpSn", /*  33  */
    "Not4", /*  34  */
    "Ap4 ", /*  35  */
    "Ap4b", /*  36  */
    "Ap4D", /*  37  */
    "Oryx", /*  38  */
    "Orx2", /*  39  */
    "Orx3", /*  40  */
    "18db", /*  41  */
    "La+d", /*  42  */
    "Lad+", /*  43  */
    "Diod", /*  44  */
    "L+d+", /*  45  */
    "h3o+", /*  46  */
    "Svh3", /*  47  */
    "Alkx", /*  48  */
};

const char* fx2Name[] = {
    "Off ", /* 0 */
    "Flng", /* 1  */
    "Dim ", /* 2  */
    "Chor", /* 3  */
    "Wide", /* 4  */
    "Dblr", /* 5  */
    "3Voi", /* 6  */
    "Bode", /* 7  */
    "Delc", /* 8  */
    "Ping", /* 9  */
    "Diff", /* 10 */
    "Gra1", /* 11 */
    "Gra2", /* 12 */
};

const char* oscShapeNames[] = {"sin ", "saw ", "squa", "s^2 ", "szer", "spos", "rand",
                               "off ", "Usr1", "Usr2", "Usr3", "Usr4", "Usr5", "Usr6"};
const unsigned char oscShapeNamesOrder[] = {7, 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13};
//                                                 { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
const unsigned char oscShapeNamesPosition[] = {1, 2, 3, 4, 5, 6, 7, 0, 8, 9, 10, 11, 12, 13};

const char* oscTypeNames[] = {"keyb", "fixe", "kehz"};
const unsigned char oscTypeNamesOrder[] = {0, 2, 1};
const unsigned char oscTypeNamesPosition[] = {0, 2, 1};

const char* lofEnv2Loop[] = {"No  ", "Sile", "Attk"};

const char* matrixSourceNames[MATRIX_SOURCE_MAX] = {/* 0 */ "none  ",  "Lfo 1 ", "Lfo 2 ", "Lfo 3 ", "Env 1 ", "Env 2 ", "Seq 1 ",
                                                    "Seq 2 ",
                                                    /* 8 */ "ModWel",  "PitBen", "AftTou", "Veloc ", "Note1 ", "Perf1 ", "Perf2 ",
                                                    /* 15 */ "Perf3 ", "Perf4 ", "Note2 ", "Breath", "74 MPE", "RndKey", "PolyAT",
                                                    /* 22 */ "UsrCC1", "UsrCC2", "UsrCC3", "UsrCC4",
                                                    /* 26 */ "PB MPE", "AT MPE"};

const unsigned char matrixSourceOrder[MATRIX_SOURCE_MAX] = {0,  1,  2,  3,  4,  5,  6,  7,  12, 17, 8,  9,  10, 21,
                                                            11, 18, 13, 14, 15, 16, 20, 26, 27, 19, 22, 23, 24, 25};
unsigned char matrixSourcePosition[MATRIX_SOURCE_MAX];

const char* matrixDestNames[DESTINATION_MAX] = {
    "None ",
    "Gate ",
    "IM 1 ",
    "IM 2 ",
    "IM 3 ",
    "IM 4 ",
    "IMAll",
    "Mix 1",
    "Pan 1",
    "Mix 2",
    "Pan 2",
    "Mix 3",
    "Pan 3",
    "Mix 4",
    "Pan 4",
    "Mix *",
    "Pan *",
    "o1 Fq",
    "o2 Fq",
    "o3 Fq",
    "o4 Fq",
    "o5 Fq",
    "o6 Fq",
    "o* Fq",
    /*24*/ "Atk 1",
    "Atk 2",
    "Atk 3",
    "Atk 4",
    "Atk 5",
    "Atk 6",
    /*30*/ "Atk C",
    "Rel C",
    /*32*/ "mtx 1",
    "mtx 2",
    "mtx 3",
    "mtx 4",
    /*36*/ "lfo1F",
    "lfo2F",
    "lfo3F",
    "env2S",
    "stp1G",
    "stp2G",
    /*42*/ "Fx1 1",
    /*43*/ "o* Fh",
    "Dec C",
    /*45*/ "Atk M",
    "Dec M",
    "Rel M",
    /* 48 pfm3 feedback */ "FdBck",
    /*filter param 2*/ "Fx1 2",
    /*filter amp 2*/ "fx1Am",
    /*filter2 param 1*/ "Fx2 1",
    /*filter2 param 1*/ "Fx2 2",
    /*filter2 amp*/ "Fx2Am",
};
const unsigned char matrixTargetOrder[DESTINATION_MAX] = {0,  1,  2,  3,  4,  5,  48, 6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                                          17, 18, 19, 20, 21, 22, 23, 43, 24, 25, 26, 27, 28, 29, 30, 45, 44, 46,
                                                          31, 47, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 49, 50, 51, 52, 53};

unsigned char matrixTargetPosition[DESTINATION_MAX];

const char* lfoOscMidiClock[] = {"C/16", "Ck/8", "Ck/4", "Ck/2", "Ck  ", "Ck*2", "Ck*3", "Ck*4", "Ck*8"};
const char* lfoShapeNames[] = {"Sin ", "Saw ", "Tria", "Squa", "Rand", "Brwn", "Wndr", "Flow"};

const char* lfoSeqMidiClock[] = {"Ck/4", "Ck/2", "Ck  ", "Ck*2", "Ck*4"};

const char* midiNoteCurves[] = {"Flat", "+Lin", "+Ln8", "+Exp", "-Lin", "-Ln8", "-Exp"};

// =================================================
// constant for pfm3 editor
// =================================================

// clang-format off
struct ParameterRowDisplay engine1ParameterRow = {
    "Engine",
    {
        "Algo",
        "Velo",
        "Mode",
        "Speed" },
    {
        {
            ALGO1,
            ALGO_END - 1,
            ALGO_END,
            DISPLAY_TYPE_STRINGS,
            algoNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            17,
            DISPLAY_TYPE_INT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            2,
            3,
            DISPLAY_TYPE_STRINGS,
            polyMonoNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            12,
            13,
            DISPLAY_TYPE_INT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay engine2ParameterRow = {
    "NewPfm3",
    {
        "Glide",
        "Spread",
        "Detune",
        "" },
    {
        {
            0,
            2,
            3,
            DISPLAY_TYPE_STRINGS,
            glideTypeNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay lfoSyncParameterRow = {
    "",
    {
        "Sync",
        "",
        "",
        "" },
    {
        {
            0,
            1,
            2,
            DISPLAY_TYPE_STRINGS,
            syncNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay engineArp1ParameterRow = {
    "Arpeggiator",
    {
        "Clock ",
        "BPM ",
        "Dire",
        "Octav" },
    {
        {
            0,
            2,
            3,
            DISPLAY_TYPE_STRINGS,
            clockName,
            nullNamesOrder,
            nullNamesOrder },
        {
            10,
            240,
            231,
            DISPLAY_TYPE_INT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            11,
            12,
            DISPLAY_TYPE_STRINGS,
            dirName,
            nullNamesOrder,
            nullNamesOrder },
        {
            1,
            3,
            3,
            DISPLAY_TYPE_INT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay engineArp2ParameterRow = {
    "Arpeggiator",
    {
        "Ptrn",
        "Divi",
        "Dura",
        "Latc" },
    {
        {
            0,
            ARPEGGIATOR_PATTERN_COUNT - 1,
            ARPEGGIATOR_PATTERN_COUNT,
            DISPLAY_TYPE_STRINGS,
            patternName,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            17,
            DISPLAY_TYPE_STRINGS,
            divNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            17,
            DISPLAY_TYPE_STRINGS,
            divNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            2,
            DISPLAY_TYPE_STRINGS,
            activeName,
            nullNamesOrder,
            nullNamesOrder }, } };

struct ParameterRowDisplay engineArpPatternRow = {
    "Pattern ",
    {
        "    ",
        "    ",
        "    ",
        "    " },
    {
        {
            0,
            0,
            0,
            DISPLAY_TYPE_ARP_PATTERN,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            0,
            0,
            DISPLAY_TYPE_ARP_PATTERN,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            0,
            0,
            DISPLAY_TYPE_ARP_PATTERN,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            0,
            0,
            DISPLAY_TYPE_ARP_PATTERN,
            nullNames,
            nullNamesOrder,
            nullNamesOrder }, } };

struct ParameterRowDisplay engineIM1ParameterRow = {
    "Modulation",
    {
        "IM1 ",
        "IM1v",
        "IM2 ",
        "IM2v" },
    {
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay engineIM2ParameterRow = {
    "Modulation",
    {
        "IM3 ",
        "IM3v",
        "IM4 ",
        "IM4v" },
    {
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay engineIM3ParameterRow = {
    "Modulation",
    {
        "IM5 ",
        "IM5v",
        "Fbk ",
        "Fdkv" },
    {
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay engineMix1ParameterRow = {
    "Mixer",
    {
        "Mix1",
        "Pan1",
        "Mix2",
        "Pan2" },
    {
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder }, } };

struct ParameterRowDisplay engineMix2ParameterRow = {
    "Mixer",
    {
        "Mix3",
        "Pan3",
        "Mix4",
        "Pan4" },
    {
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay engineMix3ParameterRow = {
    "Mixer",
    {
        "Mix5",
        "Pan5",
        "Mix6",
        "Pan6" },
    {
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay engineCurveParameterRow = {
    "Curve",
    {
        "Attk",
        "Dec ",
        "Sust",
        "Rel " },
    {
        {
            CURVE_TYPE_EXP,
            CURVE_TYPE_MAX - 1,
            CURVE_TYPE_MAX,
            DISPLAY_TYPE_STRINGS,
			envCurveNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            CURVE_TYPE_EXP,
            CURVE_TYPE_MAX - 1,
            CURVE_TYPE_MAX,
            DISPLAY_TYPE_STRINGS,
			envCurveNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            CURVE_TYPE_EXP,
            CURVE_TYPE_MAX - 1,
            CURVE_TYPE_MAX,
            DISPLAY_TYPE_STRINGS,
			envCurveNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            CURVE_TYPE_EXP,
            CURVE_TYPE_MAX - 1,
            CURVE_TYPE_MAX,
            DISPLAY_TYPE_STRINGS,
			envCurveNames,
            nullNamesOrder,
            nullNamesOrder }
    } };

struct ParameterRowDisplay fx1ParameterRow = {
    "FX1",
    {
        "Type",
        "    ",
        "    ",
        "Gain" },
    {
        {
            0,
            FILTER_LAST - 1,
            FILTER_LAST,
            DISPLAY_TYPE_STRINGS,
            fxName,
            filtersOrder,
            filtersPosition },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            2,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct FilterRowDisplay fx1RowDisplay[FILTER_LAST] = {
    {
        NULL,
        NULL,
        "Gain" },
    {
        "Pan ",
        NULL,
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "LoFr",
        "Boos",
        "Gain" },
    {
        "Freq",
        "Q   ",
        "Gain" },
    {
        "Samp",
        "Bits",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Q   ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Amp ",
        "Gain" },
    {
        "Freq",
        "Amp ",
        "Gain" },
    {
        "Freq",
        "Amp ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Mix ",
        "Gain" },
    {
        "Freq",
        "Mod ",
        "Gain" },
    {
        "Pos ",
        "Sprd",
        "Gain" },
    {
        "Thrs",
        "Tone",
        "Gain" },
    {
        "Driv",
        "Tone",
        "Gain" },
    {
        "Driv",
        "Tone",
        "Gain" },
    {
        "Driv",
        "Tone",
        "Gain" },
    {
        "Thrs",
        "Tone",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Fold",
        "Gain" },
    {
        "Freq",
        "Fold",
        "Gain" },
    {
        "Pos ",
        "Freq",
        "Gain" },
    {
        "Pos ",
        "Freq",
        "Gain" },
    {
        "Freq",
        "Sprd",
        "Gain" },
    {
        "Freq",
        "Sprd",
        "Gain" },
    {
        "Freq",
        "Sprd",
        "Gain" },
    {
        "Freq",
        "Sprd",
        "Gain" },
    {
        "Vowl",
        "Tone",
        "Gain" },
    {
        "Vowl",
        "Tone",
        "Gain" },
    {
        "Vowl",
        "Tone",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Freq",
        "Res ",
        "Gain" },
    {
        "Smp1",
        "Smp2",
        "Gain"
    }

};

struct ParameterRowDisplay fx2ParameterRow = {
    "FX2",
    {
        "Type",
        "    ",
        "    ",
        "Gain" },
    {
        {
            0,
            FILTER2_LAST - 1,
            FILTER2_LAST,
            DISPLAY_TYPE_STRINGS,
            fx2Name,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            2,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct FilterRowDisplay fx2RowDisplay[FILTER2_LAST] = {
    {
        NULL,
        NULL,
        "Gain" },
    {
        "Rate",
        "Feed",
        "Mix " },
    {
        "Rate",
        "M/S ",
        "Mix " },
    {
        "Rate",
        "Vibr",
        "Mix " },
    {
        "Detu",
        "Freq",
        "Mix " },
    {
        "Ptch",
        "Feed",
        "Mix " },
    {
        "Ptc1",
        "Ptc2",
        "Mix " },
    {
        "Freq",
        "Feed",
        "Mix " },
    {
        "Time",
        "Feed",
        "Mix " },
    {
        "Time",
        "Feed",
        "Mix " },
    {
        "Time",
        "Feed",
        "Mix " },
    {
        "Size",
        "Sprd",
        "Mix " },
    {
        "Tune",
        "Sprd",
        "Mix " }
};

struct ParameterRowDisplay oscParameterRow = {
    "Oscillator",
    {
        "Shap",
        "FTyp",
        "Freq",
        "FTun" },
    {
        {
            OSC_SHAPE_SIN,
            OSC_SHAPE_LAST - 1,
            OSC_SHAPE_LAST,
            DISPLAY_TYPE_STRINGS,
            oscShapeNames,
            oscShapeNamesOrder,
            oscShapeNamesPosition },
        {
            OSC_FT_KEYBOARD,
            OSC_FT_KEYHZ,
            3,
            DISPLAY_TYPE_STRINGS,
            oscTypeNames,
            oscTypeNamesOrder,
            oscTypeNamesPosition },
        {
            0,
            16,
            193,
            DISPLAY_TYPE_FLOAT_OSC_FREQUENCY,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -9,
            9,
            1801,
            DISPLAY_TYPE_FLOAT_OSC_FREQUENCY,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay envParameterTime = {
    "Env T",
    {
        "Attk",
        "Deca",
        "Sust",
        "Rel" },
    {
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay envParameterLevel = {
    "Env L",
    {
        "Attk",
        "Deca",
        "Sust",
        "Rel" },
    {
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay lfoEnvParameterRow = {
    "Free Env",
    {
        "Attk",
        "Deca",
        "Sust",
        "Rele" },
    {
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay lfoEnv2ParameterRow = {
    "Free Env",
    {
        "Sile",
        "Attk",
        "Deca",
        "Loop" },
    {
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            16,
            1601,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            LFO_ENV2_NOLOOP,
            LFO_ENV2_LOOP_ATTACK,
            3,
            DISPLAY_TYPE_STRINGS,
            lofEnv2Loop,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay matrixParameterRow = {
    "Matrix",
    {
        "Srce",
        "Mult",
        "Dest1",
        "Dest2" },
    {
        {
            MATRIX_SOURCE_NONE,
            MATRIX_SOURCE_MAX - 1,
            MATRIX_SOURCE_MAX,
            DISPLAY_TYPE_STRINGS,
            matrixSourceNames,
            matrixSourceOrder,
            matrixSourcePosition },
        {
            -10,
            24,
            3401,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            DESTINATION_NONE,
            DESTINATION_MAX - 1,
            DESTINATION_MAX,
            DISPLAY_TYPE_STRINGS,
            matrixDestNames,
            matrixTargetOrder,
            matrixTargetPosition },
        {
            DESTINATION_NONE,
            DESTINATION_MAX - 1,
            DESTINATION_MAX,
            DISPLAY_TYPE_STRINGS,
            matrixDestNames,
            matrixTargetOrder,
            matrixTargetPosition } } };

struct ParameterRowDisplay lfoParameterRow = {
    "LFO",
    {
        "Shap",
        "Freq",
        "Bias",
        "KSyn" },
    {
        {
            LFO_SIN,
            LFO_TYPE_MAX - 1,
            LFO_TYPE_MAX,
            DISPLAY_TYPE_STRINGS,
            lfoShapeNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            LFO_FREQ_MAX + .9f,
            LFO_FREQ_MAX_TIMES_10 + 1 + 9,
            DISPLAY_TYPE_FLOAT_LFO_FREQUENCY,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -0.01f,
            16.0f,
            1602,
            DISPLAY_TYPE_LFO_KSYN,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay lfoStepParameterRow = {
    "Step Seq",
    {
        "Bpm ",
        "Gate",
        "Steps",
        "" },
    {
        {
            10,
            245,
            236,
            DISPLAY_TYPE_STEP_SEQ_BPM,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            0,
            0,
            DISPLAY_TYPE_STEP_SEQ1,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            0,
            0,
            DISPLAY_TYPE_STEP_SEQ2,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay performanceParameterRow1 = {
    "   -Performance-",
    {
        " p1 ",
        " p2 ",
        " p3 ",
        " p4 " },
    {
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            -1,
            1,
            201,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay lfoPhaseParameterRow = {
    "LFO Phase",
    {
        "Phase",
        "Phase",
        "Phase",
        "    " },
    {
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            0,
            0,
            DISPLAY_TYPE_NONE,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay midiNote1ParameterRow = {
    "Note1 Midi Scaling",
    {
        "Befo",
        "Brk ",
        "Afte",
        "    " },
    {
        {
            MIDI_NOTE_CURVE_FLAT,
            MIDI_NOTE_CURVE_MAX - 1,
            MIDI_NOTE_CURVE_MAX,
            DISPLAY_TYPE_STRINGS,
            midiNoteCurves,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            127,
            128,
            DISPLAY_TYPE_INT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            MIDI_NOTE_CURVE_FLAT,
            MIDI_NOTE_CURVE_MAX - 1,
            MIDI_NOTE_CURVE_MAX,
            DISPLAY_TYPE_STRINGS,
            midiNoteCurves,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            0,
            0,
            DISPLAY_TYPE_NONE,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay midiNote2ParameterRow = {
    "Note2 Midi Scaling",
    {
        "Befo",
        "Brk ",
        "Afte",
        "    " },
    {
        {
            MIDI_NOTE_CURVE_FLAT,
            MIDI_NOTE_CURVE_MAX - 1,
            MIDI_NOTE_CURVE_MAX,
            DISPLAY_TYPE_STRINGS,
            midiNoteCurves,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            127,
            128,
            DISPLAY_TYPE_INT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            MIDI_NOTE_CURVE_FLAT,
            MIDI_NOTE_CURVE_MAX - 1,
            MIDI_NOTE_CURVE_MAX,
            DISPLAY_TYPE_STRINGS,
            midiNoteCurves,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            0,
            0,
            DISPLAY_TYPE_NONE,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay dummyParameterRow = {
    "",
    {
        "",
        "",
        "",
        "" },
    {
        {
            0,
            0,
            0,
            DISPLAY_TYPE_NONE,
            nullNames,
            nullNamesOrder,
            nullNamesOrder  },
        {
            0,
            0,
            0,
            DISPLAY_TYPE_NONE,
            nullNames,
            nullNamesOrder,
            nullNamesOrder  },
        {
            0,
            0,
            0,
            DISPLAY_TYPE_NONE,
            nullNames,
            nullNamesOrder,
            nullNamesOrder  },
        {
            0,
            0,
            0,
            DISPLAY_TYPE_NONE,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

const struct EditorOneButtonState pfm3ButtonEngineState = {
    "Engine",
    {
        {
            ROW_ENGINE,
            ENCODER_ENGINE_ALGO },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_ENGINE,
            ENCODER_ENGINE_VELOCITY },
        {
            ROW_ENGINE,
            ENCODER_ENGINE_PLAY_MODE },
        {
            ROW_NONE,
            ENCODER_NONE },
} };

const struct EditorOneButtonState pfm3ButtonMonoState = {
    "Mono",
    {
        {
            ROW_ENGINE2,
            ENCODER_ENGINE2_GLIDE_TYPE },
        {
            ROW_ENGINE,
            ENCODER_ENGINE_GLIDE_SPEED },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_ENGINE2,
            ENCODER_ENGINE2_UNISON_SPREAD },
        {
            ROW_ENGINE2,
            ENCODER_ENGINE2_UNISON_DETUNE },
        {
            ROW_NONE,
            ENCODER_NONE }
} };


const struct EditorOneButton pfm3ButtonEngine = {
    "Engine",
    BUTTONID_EDIT_ENGINE,
    2,
    {
        &pfm3ButtonEngineState,
        &pfm3ButtonMonoState } };

const struct EditorOneButtonState pfm3ButtonIMState = {
    "Modulation Index",
    {
        {
            ROW_MODULATION1,
            ENCODER_ENGINE_IM1 },
        {
            ROW_MODULATION1,
            ENCODER_ENGINE_IM2 },
        {
            ROW_MODULATION2,
            ENCODER_ENGINE_IM3 },
        {
            ROW_MODULATION2,
            ENCODER_ENGINE_IM4 },
        {
            ROW_MODULATION3,
            ENCODER_ENGINE_IM5 },
        {
            ROW_MODULATION3,
            ENCODER_ENGINE_IM6 } } };

const struct EditorOneButtonState pfm3ButtonIMVelocityState = {
    "Velocity",
    {
        {
            ROW_MODULATION1,
            ENCODER_ENGINE_IM1_VELOCITY },
        {
            ROW_MODULATION1,
            ENCODER_ENGINE_IM2_VELOCITY },
        {
            ROW_MODULATION2,
            ENCODER_ENGINE_IM3_VELOCITY },
        {
            ROW_MODULATION2,
            ENCODER_ENGINE_IM4_VELOCITY },
        {
            ROW_MODULATION3,
            ENCODER_ENGINE_IM5_VELOCITY },
        {
            ROW_MODULATION3,
            ENCODER_ENGINE_IM6_VELOCITY } } };

const struct EditorOneButton pfm3ButtonIM = {
    "MI",
    BUTTONID_EDIT_IM,
    2,
    {
        &pfm3ButtonIMState,
        &pfm3ButtonIMVelocityState } };

const struct EditorOneButtonState pfm3ButtonMixer123State = {
    "Op Mixer",
    {
        {
            ROW_OSC_MIX1,
            ENCODER_ENGINE_MIX1 },
        {
            ROW_OSC_MIX1,
            ENCODER_ENGINE_MIX2 },
        {
            ROW_OSC_MIX2,
            ENCODER_ENGINE_MIX3 },
        {
            ROW_OSC_MIX1,
            ENCODER_ENGINE_PAN1 },
        {
            ROW_OSC_MIX1,
            ENCODER_ENGINE_PAN2 },
        {
            ROW_OSC_MIX2,
            ENCODER_ENGINE_PAN3 } } };

const struct EditorOneButtonState pfm3ButtonMixer456State = {
    "Op Mixer",
    {
        {
            ROW_OSC_MIX2,
            ENCODER_ENGINE_MIX4 },
        {
            ROW_OSC_MIX3,
            ENCODER_ENGINE_MIX5 },
        {
            ROW_OSC_MIX3,
            ENCODER_ENGINE_MIX6 },
        {
            ROW_OSC_MIX2,
            ENCODER_ENGINE_PAN4 },
        {
            ROW_OSC_MIX3,
            ENCODER_ENGINE_PAN5 },
        {
            ROW_OSC_MIX3,
            ENCODER_ENGINE_PAN6 } } };

const struct EditorOneButton pfm3ButtonMixer = {
    "Mix",
    BUTTONID_EDIT_FILTER,
    2,
    {
        &pfm3ButtonMixer123State,
        &pfm3ButtonMixer456State } };

const struct EditorOneButtonState pfm3ButtonOPEnvLevel = {
    "Env Level",
    {
        {
            ROW_ENV1_LEVEL,
            ENCODER_ENV_A },
        {
            ROW_ENV1_LEVEL,
            ENCODER_ENV_D },
        {
            ROW_ENV1_LEVEL,
            ENCODER_ENV_S },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_ENV1_LEVEL,
            ENCODER_ENV_R }
    } };

const struct EditorOneButton pfm3ButtonOPEnv2 = {
    "Level",
    BUTTONID_ONLY_ONE_STATE,
    1,
    {
        &pfm3ButtonOPEnvLevel } };


const struct EditorOneButtonState pfm3ButtonOPEnvTime = {
    "Env Time",
    {
        {
            ROW_ENV1_TIME,
            ENCODER_ENV_A },
        {
            ROW_ENV1_TIME,
            ENCODER_ENV_D },
        {
            ROW_ENV1_TIME,
            ENCODER_ENV_S },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_ENV1_TIME,
            ENCODER_ENV_R }
    } };

const struct EditorOneButtonState pfm3ButtonEnvCurve = {
    "Env Curve",
    {
        {
            ROW_ENV1_CURVE,
            ENCODER_ENV_A_CURVE },
        {
            ROW_ENV1_CURVE,
            ENCODER_ENV_D_CURVE },
        {
            ROW_ENV1_CURVE,
            ENCODER_ENV_S_CURVE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_ENV1_CURVE,
            ENCODER_ENV_R_CURVE }
    } };

const struct EditorOneButton pfm3ButtonOPEnv1 = {
    "Env",
    BUTTONID_ENV_1,
    2,
    {
        &pfm3ButtonOPEnvTime,
        &pfm3ButtonEnvCurve } };

const struct EditorOneButtonState pfm3ButtonOPShapeState = {
    "Oscillator",
    {
        {
            ROW_OSC1,
            ENCODER_OSC_SHAP },
        {
            ROW_OSC1,
            ENCODER_OSC_FTYPE },
        {
            ROW_OSC1,
            ENCODER_OSC_FREQ },
        {
            ROW_OSC1,
            ENCODER_OSC_FTUNE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE } } };

const struct EditorOneButton pfm3ButtonOPShape = {
    "Osc",
    BUTTONID_ONLY_ONE_STATE,
    1,
    {
        &pfm3ButtonOPShapeState } };

const struct EditorOneButtonState pfm3ButtonNoPage = {
    "",
    {
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE } } };



const struct EditorOneButton pfm3ButtonOPMinus = {
    "1-2",
    BUTTONID_OPERATOR_1_2,
    2,
    {
        &pfm3ButtonNoPage } };

const struct EditorOneButton pfm3ButtonOPNothing = {
    "3-4",
    BUTTONID_OPERATOR_3_4,
    2,
    {
        &pfm3ButtonNoPage } };

const struct EditorOneButton pfm3ButtonOPPlus = {
    "5-6",
    BUTTONID_OPERATOR_5_6,
    2,
    {
        &pfm3ButtonNoPage } };

const struct EditorOneButtonState pfm3ButtonMatrix1State = {
    "Matrix 1",
    {
        {
            ROW_MATRIX1,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX1,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX1,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX1,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButtonState pfm3ButtonMatrix2State = {
    "Matrix 2",
    {
        {
            ROW_MATRIX2,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX2,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX2,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX2,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButtonState pfm3ButtonMatrix3State = {
    "Matrix 3",
    {
        {
            ROW_MATRIX3,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX3,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX3,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX3,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButtonState pfm3ButtonMatrix4State = {
    "Matrix 4",
    {
        {
            ROW_MATRIX4,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX4,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX4,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX4,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButtonState pfm3ButtonMatrix5State = {
    "Matrix 5",
    {
        {
            ROW_MATRIX5,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX5,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX5,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX5,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButtonState pfm3ButtonMatrix6State = {
    "Matrix 6",
    {
        {
            ROW_MATRIX6,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX6,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX6,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX6,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButtonState pfm3ButtonMatrix7State = {
    "Matrix 7",
    {
        {
            ROW_MATRIX7,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX7,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX7,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX7,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButtonState pfm3ButtonMatrix8State = {
    "Matrix 8",
    {
        {
            ROW_MATRIX8,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX8,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX8,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX8,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButtonState pfm3ButtonMatrix9State = {
    "Matrix 9",
    {
        {
            ROW_MATRIX9,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX9,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX9,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX9,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButtonState pfm3ButtonMatrix10State = {
    "Matrix 10",
    {
        {
            ROW_MATRIX10,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX10,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX10,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX10,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButtonState pfm3ButtonMatrix11State = {
    "Matrix 11",
    {
        {
            ROW_MATRIX11,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX11,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX11,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX11,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButtonState pfm3ButtonMatrix12State = {
    "Matrix 12",
    {
        {
            ROW_MATRIX12,
            ENCODER_MATRIX_SOURCE },
        {
            ROW_MATRIX12,
            ENCODER_MATRIX_MUL },
        {
            ROW_MATRIX12,
            ENCODER_MATRIX_DEST1 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_MATRIX12,
            ENCODER_MATRIX_DEST2 } } };

const struct EditorOneButton pfm3ButtonMatrix1 = {
    "1-2",
    BUTTONID_MATRIX_1_2,
    2,
    {
        &pfm3ButtonMatrix1State,
        &pfm3ButtonMatrix2State, } };
const struct EditorOneButton pfm3ButtonMatrix2 = {
    "3-4",
    BUTTONID_MATRIX_3_4,
    2,
    {
        &pfm3ButtonMatrix3State,
        &pfm3ButtonMatrix4State, } };
const struct EditorOneButton pfm3ButtonMatrix3 = {
    "5-6",
    BUTTONID_MATRIX_5_6,
    2,
    {
        &pfm3ButtonMatrix5State,
        &pfm3ButtonMatrix6State, } };
const struct EditorOneButton pfm3ButtonMatrix4 = {
    "7-8",
    BUTTONID_MATRIX_7_8,
    2,
    {
        &pfm3ButtonMatrix7State,
        &pfm3ButtonMatrix8State, } };
const struct EditorOneButton pfm3ButtonMatrix5 = {
    "9-10",
    BUTTONID_MATRIX_9_10,
    2,
    {
        &pfm3ButtonMatrix9State,
        &pfm3ButtonMatrix10State, } };
const struct EditorOneButton pfm3ButtonMatrix6 = {
    "11-12",
    BUTTONID_MATRIX_11_12,
    2,
    {
        &pfm3ButtonMatrix11State,
        &pfm3ButtonMatrix12State, } };

const struct EditorOneButtonState pfm3ButtonLfoState1 = {
    "LFO 1",
    {
        {
            ROW_LFOOSC1,
            ENCODER_LFO_SHAPE },
        {
            ROW_NONE,
            ENCODER_MODULATOR_SYNC_LFO },
        {
            ROW_LFOOSC1,
            ENCODER_LFO_FREQ },
        {
            ROW_LFOOSC1,
            ENCODER_LFO_KSYNC },
        {
            ROW_LFOOSC1,
            ENCODER_LFO_BIAS },
        {
            ROW_LFOPHASES,
            ENCODER_LFO_PHASE1 }
    }
};

const struct EditorOneButtonState pfm3ButtonLfoState2 = {
    "LFO 2",
    {
        {
            ROW_LFOOSC2,
            ENCODER_LFO_SHAPE },
        {
            ROW_NONE,
            ENCODER_MODULATOR_SYNC_LFO },
        {
            ROW_LFOOSC2,
            ENCODER_LFO_FREQ },
        {
            ROW_LFOOSC2,
            ENCODER_LFO_KSYNC },
        {
            ROW_LFOOSC2,
            ENCODER_LFO_BIAS },
        {
            ROW_LFOPHASES,
            ENCODER_LFO_PHASE2 } } };
const struct EditorOneButtonState pfm3ButtonLfoState3 = {
    "LFO 3",
    {
        {
            ROW_LFOOSC3,
            ENCODER_LFO_SHAPE },
        {
            ROW_NONE,
            ENCODER_MODULATOR_SYNC_LFO },
        {
            ROW_LFOOSC3,
            ENCODER_LFO_FREQ },
        {
            ROW_LFOOSC3,
            ENCODER_LFO_KSYNC },
        {
            ROW_LFOOSC3,
            ENCODER_LFO_BIAS },
        {
            ROW_LFOPHASES,
            ENCODER_LFO_PHASE3 } } };

const struct EditorOneButton pfm3ButtonLfos = {
    "Lfos",
    BUTTONID_EDIT_LFOS,
    3,
    {
        &pfm3ButtonLfoState1,
        &pfm3ButtonLfoState2,
        &pfm3ButtonLfoState3 } };

const struct EditorOneButtonState pfm3ButtonEnvState1 = {
    "Free Env 1",
    {
        {
            ROW_LFOENV1,
            0 },
        {
            ROW_LFOENV1,
            1 },
        {
            ROW_LFOENV1,
            2 },
        {
            ROW_LFOENV1,
            3 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE } } };

const struct EditorOneButtonState pfm3ButtonEnvState2 = {
    "Free Env 2",
    {
        {
            ROW_LFOENV2,
            0 },
        {
            ROW_LFOENV2,
            1 },
        {
            ROW_LFOENV2,
            2 },
        {
            ROW_LFOENV2,
            3 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE } } };

const struct EditorOneButton pfm3ButtonEnvs = {
    "Envs",
    BUTTONID_EDIT_ENVS,
    2,
    {
        &pfm3ButtonEnvState1,
        &pfm3ButtonEnvState2 } };

const struct EditorOneButtonState pfm3ButtonStepSeqStep1 = {
    "Step Sequencer 1",
    {
        {
            ROW_NONE,
            ENCODER_MODULATOR_SYNC_STEPS },
        {
            ROW_LFOSEQ1,
            ENCODER_STEPSEQ_BPM },
        {
            ROW_LFOSEQ1,
            ENCODER_STEPSEQ_GATE },
        {
            ROW_LFOSEQ1,
            2 },
        {
            ROW_LFOSEQ1,
            3 },
        {
            ROW_NONE,
            ENCODER_INVISIBLE } } };

const struct EditorOneButtonState pfm3ButtonStepSeqStep2 = {
    "Step Sequencer 2",
    {
        {
            ROW_NONE,
            ENCODER_MODULATOR_SYNC_STEPS },
        {
            ROW_LFOSEQ2,
            ENCODER_STEPSEQ_BPM },
        {
            ROW_LFOSEQ2,
            ENCODER_STEPSEQ_GATE },
        {
            ROW_LFOSEQ2,
            2 },
        {
            ROW_LFOSEQ2,
            3 },
        {
            ROW_NONE,
            ENCODER_INVISIBLE } } };

const struct EditorOneButton pfm3ButtonSteps = {
    "Steps",
    BUTTONID_EDIT_STEPS,
    2,
    {
        &pfm3ButtonStepSeqStep1,
        &pfm3ButtonStepSeqStep2 } };

const struct EditorOneButtonState pfm3ButtonMidiState1 = {
    "Midi note 1",
    {
        {
            ROW_MIDINOTE1CURVE,
            ENCODER_NOTECURVE_BEFORE },
        {
            ROW_MIDINOTE1CURVE,
            ENCODER_NOTECURVE_BREAK_NOTE },
        {
            ROW_MIDINOTE1CURVE,
            ENCODER_NOTECURVE_AFTER },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE } } };

const struct EditorOneButtonState pfm3ButtonMidiState2 = {
    "Midi note 2",
    {
        {
            ROW_MIDINOTE2CURVE,
            ENCODER_NOTECURVE_BEFORE },
        {
            ROW_MIDINOTE2CURVE,
            ENCODER_NOTECURVE_BREAK_NOTE },
        {
            ROW_MIDINOTE2CURVE,
            ENCODER_NOTECURVE_AFTER },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE } } };

const struct EditorOneButton pfm3ButtonMidiNotes = {
    "Midi",
    BUTTONID_EDIT_NOTES,
    2,
    {
        &pfm3ButtonMidiState1,
        &pfm3ButtonMidiState2 } };

const struct EditorEditMenu operatorEditMenu = {
    "Oper",
    6,
    {
        &pfm3ButtonOPShape,
        &pfm3ButtonOPEnv1,
        &pfm3ButtonOPEnv2,
        &pfm3ButtonOPMinus,
        &pfm3ButtonOPNothing,
        &pfm3ButtonOPPlus } };

const struct EditorEditMenu modulatorEditorMenu = {
    "Modul",
    4,
    {
        &pfm3ButtonLfos,
        &pfm3ButtonEnvs,
        &pfm3ButtonSteps,
        &pfm3ButtonMidiNotes } };

const struct EditorEditMenu pfm3EngineMenu = {
    "FM",
    3,
    {
        &pfm3ButtonEngine,
        &pfm3ButtonIM,
        &pfm3ButtonMixer,
 } };

const struct EditorEditMenu pfm3MatrixMenu = {
    "Matrix",
    6,
    {
        &pfm3ButtonMatrix1,
        &pfm3ButtonMatrix2,
        &pfm3ButtonMatrix3,
        &pfm3ButtonMatrix4,
        &pfm3ButtonMatrix5,
        &pfm3ButtonMatrix6, } };

const struct EditorOneButtonState perfEditorState = {
    "Performance",
    {
        {
            ROW_PERFORMANCE1,
            ENCODER_PERFORMANCE_CC1 },
        {
            ROW_PERFORMANCE1,
            ENCODER_PERFORMANCE_CC2 },
        {
            ROW_PERFORMANCE1,
            ENCODER_PERFORMANCE_CC3 },
        {
            ROW_PERFORMANCE1,
            ENCODER_PERFORMANCE_CC4 },
        {
            ROW_EFFECT1,
            ENCODER_EFFECT_PARAM1 },
        {
            ROW_EFFECT1,
            ENCODER_EFFECT_PARAM2 } } };

const struct EditorOneButton perfEditorPage = {
    "Perf",
    BUTTONID_PERFORMANCE,
    1,
    {
        &perfEditorState } };

static const struct EditorEditMenu pfm3PerfMenu = {
    "Perf",
    1,
    {
        &perfEditorPage } };

static const struct EditorMainMenu mainMenu = {
    &pfm3EngineMenu,
    &operatorEditMenu,
    &pfm3MatrixMenu,
    &modulatorEditorMenu,
    nullptr,
    &pfm3PerfMenu,
};
// clang-format on

void EditorPfm3::init(ScreenEditor* parent, SynthState* synthState, FirmwareTftDisplay* tft) {
    parent_ = parent;
    synthState_ = synthState;
    tft_ = tft;
    parent->insertEditor(this);

    int position = 0;
    for (int i = 0; i < DESTINATION_MAX; i++) {
        matrixTargetPosition[matrixTargetOrder[i]] = position++;
    }
    position = 0;
    for (int i = 0; i < MATRIX_SOURCE_MAX; i++) {
        matrixSourcePosition[matrixSourceOrder[i]] = position++;
    }

    EditorPfm3Algo::init();
}

SynthEngineType EditorPfm3::getEngineType() const {
    return SYNTH_ENGINE_PFM3;
}

const char* EditorPfm3::getEngineName() const {
    return "preenfm3";
}

const struct EditorEditMenu* EditorPfm3::getEngineMenu(int page) const {
    if (page >= 0 && page < 6) {
        return mainMenu.editMenu[page];
    }
    return nullptr;
}

int EditorPfm3::getCommonPage(int page) const {
    if (page == 4) {
        return COMMON_PAGE_ARP_FX;
    }
    return COMMON_PAGE_NONE;
}

void EditorPfm3::newPfm3ParamValue(int& refreshStatus, int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue,
                                   float newValue) {

    const struct EditorOneButton* page = currentPage();
    if (page == nullptr) {
        return;
    }
    uint8_t buttonState = synthState_->fullState_.buttonState[page->buttonId];

    int encoder6 = -1;
    for (int enc = 0; (enc < 6) && (encoder6 == -1); enc++) {
        const struct RowEncoder rowEncoder = page->states[buttonState]->rowEncoder[enc];
        int rowToTest = currentRow;
        if (unlikely(synthState_->fullState_.mainPage == 1)) {
            switch (currentRow) {
            case ROW_OSC2:
            case ROW_OSC3:
            case ROW_OSC4:
            case ROW_OSC5:
            case ROW_OSC6:
                rowToTest = ROW_OSC1;
                break;
            case ROW_ENV1_TIME:
            case ROW_ENV2_TIME:
            case ROW_ENV3_TIME:
            case ROW_ENV4_TIME:
            case ROW_ENV5_TIME:
            case ROW_ENV6_TIME: {
                int op = currentRow - ROW_ENV1_TIME;
                // This mean we used MENU + encoder, so some new value must not be displayed
                if (op != synthState_->fullState_.operatorNumber) {
                    return;
                }
                refreshOscillatorOperatorEnvelope();
                rowToTest = ROW_ENV1_TIME;
                break;
            }
            case ROW_ENV1_LEVEL:
            case ROW_ENV2_LEVEL:
            case ROW_ENV3_LEVEL:
            case ROW_ENV4_LEVEL:
            case ROW_ENV5_LEVEL:
            case ROW_ENV6_LEVEL: {
                int op = currentRow - ROW_ENV1_LEVEL;
                // This mean we used MENU + encoder, so some new value must not be displayed
                if (op != synthState_->fullState_.operatorNumber) {
                    return;
                }
                refreshOscillatorOperatorEnvelope();
                rowToTest = ROW_ENV1_LEVEL;
                break;
            }
            case ROW_ENV1_CURVE:
            case ROW_ENV2_CURVE:
            case ROW_ENV3_CURVE:
            case ROW_ENV4_CURVE:
            case ROW_ENV5_CURVE:
            case ROW_ENV6_CURVE: {
                int op = currentRow - ROW_ENV1_CURVE;
                if (op != synthState_->fullState_.operatorNumber) {
                    return;
                }
                refreshOscillatorOperatorEnvelope();
                rowToTest = ROW_ENV1_CURVE;
            }
            }
        }
        if (rowToTest == rowEncoder.row && encoder == rowEncoder.encoder) {
            encoder6 = enc;
        }
    }
    // Can be called when new param value is received on Midi
    // So encoder6 can often be -1
    if (encoder6 != -1) {
        uint8_t x = getEncoderX(encoder6);

        tft_->setCursor(x, encoder6 > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);

        struct ParameterDisplay* param = &allParameterRows.row[currentRow]->params[encoder];

        // Special cases
        switch (currentRow) {
        case ROW_ENGINE:
            if (encoder == ENCODER_ENGINE_ALGO) {
                checkOperatorNumber();
                tft_->drawAlgoTable(EditorPfm3Algo::table(newValue));
            }
            break;
        case ROW_LFOSEQ1:
        case ROW_LFOSEQ2:
            if (encoder > ENCODER_STEPSEQ_GATE) {
                parent_->updateStepSequencer(currentRow, encoder, oldValue, newValue);
                return;
            }
            break;
        case ROW_ARPEGGIATOR3:
            // special case arpegiattor user pattern
            parent_->updateArpPattern(currentRow, encoder, oldValue, newValue);
            return;
        case ROW_ARPEGGIATOR2:
            if (encoder == ENCODER_ARPEGGIATOR_PATTERN) {
                // Refresh pattern
                refreshStatus = 3;
            }
            break;
        case ROW_EFFECT1:
        case ROW_EFFECT2:
            if (encoder == ENCODER_EFFECT_TYPE) {
                resetHideParams();
                refreshStatus = 11;
            }
            break;
        case ROW_MODULATION1:
        case ROW_MODULATION3:
        case ROW_MODULATION2: {
            if (hideParam_[encoder6]) {
                return;
            }
            struct ModulationIndex im = EditorPfm3Algo::modulationIndex[(int)pfm3Preset()->engine1.algo][encoder6];
            if (im.source != im.destination) {
                tft_->highlightIM(encoder6, im.source, im.destination);
            } else {
                tft_->highlightFeedback(encoder6, im.source, im.destination);
            }
            imChangedCounter_[encoder6] = 4;
            break;
        }
        case ROW_OSC_MIX1:
        case ROW_OSC_MIX3:
        case ROW_OSC_MIX2: {
            if (hideParam_[encoder6]) {
                return;
            }
            int mixNumber = (currentRow - ROW_OSC_MIX1) * 2 + ((encoder & 0x2) >> 1);
            int currentAlgo = (int)pfm3Preset()->engine1.algo;
            int operatorNumber = EditorPfm3Algo::table(currentAlgo)[mixNumber * 2 + 1] - 1;

            tft_->highlightOperator(operatorNumber);
            mixChangedCounter_[encoder6] = 4;
            mixChangeOperatorNumber_[encoder6] = operatorNumber;
            break;
        }
        case ROW_OSC1:
        case ROW_OSC2:
        case ROW_OSC3:
        case ROW_OSC4:
        case ROW_OSC5:
        case ROW_OSC6: {
            switch (encoder) {
            case ENCODER_OSC_SHAP:
                refreshOscillatorOperatorShape();
                break;
            case ENCODER_OSC_FTYPE:
                // Refresh Freq and Fine Tune
                refreshStatus = 10;
                break;
            case ENCODER_OSC_FREQ: {
                // Refresh Fine Tune
                struct OscillatorParams* opParams = &pfm3Preset()->osc1;
                if (opParams[synthState_->fullState_.operatorNumber].frequencyType == OSC_FT_FIXE) {
                    refreshStatus = 9;
                }
                break;
            }
            case ENCODER_OSC_FTUNE: {
                // Refresh Fine Tune
                struct OscillatorParams* opParams = &pfm3Preset()->osc1;
                if (opParams[synthState_->fullState_.operatorNumber].frequencyType == OSC_FT_FIXE) {
                    refreshStatus = 10;
                }
                break;
            }
            }
            break;
        }
        case ROW_LFOPHASES:
        case ROW_LFOOSC1:
        case ROW_LFOOSC2:
        case ROW_LFOOSC3: {
            refreshLfoOscillator();
            break;
        }
        case ROW_LFOENV1:
        case ROW_LFOENV2:
            refreshLfoEnv();
            break;
        }

        tft_->setCharBackgroundColor(COLOR_BLACK);
        if (!multipleEdition_) {
            tft_->setCharColor(COLOR_YELLOW);
        } else {
            tft_->setCharColor(COLOR_GREEN);
        }
        valueChangedCounter_[encoder6] = 4;
        updateEncoderValueWithoutCursor(currentRow, encoder, param, oldValue, newValue);
    }
}

void EditorPfm3::displayParamValue(int encoder, TFT_COLOR color) {
    const struct EditorEditMenu* editMenu = currentEditMenu();
    if (editMenu == nullptr) {
        return;
    }
    const struct EditorOneButton* page = currentPage();
    const struct RowEncoder rowEncoder = page->states[synthState_->fullState_.buttonState[page->buttonId]]->rowEncoder[encoder];

    if (!hideParam_[encoder] && rowEncoder.encoder == ENCODER_MODULATOR_SYNC_LFO) {
        tft_->setCursor(getEncoderX(encoder), encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(color);
        struct LfoParams* lfoParams = &pfm3Preset()->lfoOsc1;
        int lfoButtonState = synthState_->fullState_.buttonState[BUTTONID_EDIT_LFOS];
        tft_->print(lfoSyncParameterRow.params[0].valueName[lfoParams[lfoButtonState].freq < 100.0f ? 0 : 1]);
        return;
    }
    if (!hideParam_[encoder] && rowEncoder.encoder == ENCODER_MODULATOR_SYNC_STEPS) {
        tft_->setCursor(getEncoderX(encoder), encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(color);
        struct StepSequencerParams* stepParams = &pfm3Preset()->lfoSeq1;
        int stepButtonState = synthState_->fullState_.buttonState[BUTTONID_EDIT_STEPS];
        tft_->print(lfoSyncParameterRow.params[0].valueName[stepParams[stepButtonState].bpm < 240.5f ? 0 : 1]);
        return;
    }
    EditorParamPage::displayParamValue(encoder, color);
}

void EditorPfm3::drawAlgorithm() {
    tft_->drawAlgoTable(EditorPfm3Algo::table(pfm3Preset()->engine1.algo));
}

void EditorPfm3::drawPageHeader() {
    if (synthState_->fullState_.mainPage == 1) {
        tft_->print("Op ");
        tft_->print(synthState_->fullState_.operatorNumber + 1);
        tft_->print(" ");
    }
}

void EditorPfm3::drawEditorButton(int button, const struct EditorEditMenu* editMenu, const struct EditorOneButton* page) {
    if (unlikely(isInOperatorPage(synthState_->fullState_.mainPage) && button >= 3 && button <= 5)) {
        tft_->drawButtonBiState(270, 29, button, synthState_->fullState_.operatorNumber,
                                algoInformation[(int)pfm3Preset()->engine1.algo].osc - 1,
                                synthState_->fullState_.buttonState[BUTTONID_OPERATOR_1_2 + button - 3], (button - 3) * 2);
    } else if (isInMatrixPage(synthState_->fullState_.mainPage)) {
        uint8_t currentSelection = synthState_->fullState_.editPage * 2 + synthState_->fullState_.buttonState[page->buttonId];
        tft_->drawButtonBiState(270, 29, button, currentSelection, 12, synthState_->fullState_.buttonState[BUTTONID_MATRIX_1_2 + button],
                                button * 2);
    } else {
        EditorParamPage::drawEditorButton(button, editMenu, page);
    }
}

void EditorPfm3::drawParameterName(int button, const struct RowEncoder& rowEncoder, const struct EditorEditMenu* editMenu,
                                   const struct EditorOneButton* page) {
    uint8_t x = getEncoderX(button);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_MEDIUM_GRAY);
    tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);

    if (rowEncoder.row != ROW_NONE) {
        const struct ParameterRowDisplay* paramRow = allParameterRows.row[rowEncoder.row];
        switch (rowEncoder.row) {
        case ROW_MODULATION1:
        case ROW_MODULATION2:
        case ROW_MODULATION3: {
            struct ModulationIndex im = EditorPfm3Algo::modulationIndex[(int)pfm3Preset()->engine1.algo][button];
            if (im.source == 0) {
                hideParam_[button] = true;
            } else {
                if (button < 5) {
                    tft_->print((int)im.source);
                    tft_->print('-');
                    tft_->print((int)im.destination);
                } else {
                    tft_->print("Fdbk");
                }
            }
            return;
        }
        case ROW_OSC_MIX1:
        case ROW_OSC_MIX2:
        case ROW_OSC_MIX3: {
            int mixNumber = (rowEncoder.row - ROW_OSC_MIX1) * 2 + ((rowEncoder.encoder & 0x2) >> 1);
            int currentAlgo = (int)pfm3Preset()->engine1.algo;
            int numberOfMix = algoInformation[currentAlgo].mix;

            if (mixNumber < numberOfMix) {
                tft_->print(paramRow->paramName[rowEncoder.encoder]);

                tft_->printSmallChar('(');
                tft_->printSmallChar(EditorPfm3Algo::table(currentAlgo)[mixNumber * 2 + 1]);
                tft_->printSmallChar(')');
            } else {
                tft_->setCharColor(COLOR_DARK_GRAY);
                tft_->print(paramRow->paramName[rowEncoder.encoder]);
                hideParam_[button] = true;
            }
            return;
        }
        case ROW_ENGINE2:
            if ((pfm3Preset()->engine1.playMode == PLAY_MODE_POLY && rowEncoder.encoder == ENCODER_ENGINE2_GLIDE_TYPE) ||
                (pfm3Preset()->engine1.playMode != PLAY_MODE_UNISON && rowEncoder.encoder >= ENCODER_ENGINE2_UNISON_SPREAD)) {
                hideParam_[button] = true;
                tft_->setCharColor(COLOR_DARK_GRAY);
                tft_->print(paramRow->paramName[rowEncoder.encoder]);
                return;
            }
            break;
        default:
            break;
        }
    } else {
        // Special case
        if (rowEncoder.encoder >= ENCODER_MODULATOR_SYNC_LFO) {
            tft_->print(lfoSyncParameterRow.paramName[0]);
            return;
        }
    }

    EditorParamPage::drawParameterName(button, rowEncoder, editMenu, page);
}

void EditorPfm3::preparePage() {
    if (synthState_->fullState_.mainPage == 1) {
        tft_->clearAlgoFG();
        tft_->highlightOperator(synthState_->fullState_.operatorNumber);
    }
    EditorParamPage::preparePage();
}

void EditorPfm3::tempoClick() {
    for (int e = 0; e < NUMBER_OF_ENCODERS; e++) {
        if (imChangedCounter_[e] > 0) {
            imChangedCounter_[e]--;
            if (imChangedCounter_[e] == 0) {
                struct ModulationIndex im = EditorPfm3Algo::modulationIndex[(int)pfm3Preset()->engine1.algo][e];
                if (im.source != im.destination) {
                    tft_->eraseHighlightIM(e, im.source, im.destination);
                } else {
                    tft_->eraseHighlightFeedback(e, im.source, im.destination);
                }
            }
        }
        if (mixChangedCounter_[e] > 0) {
            mixChangedCounter_[e]--;
            if (mixChangedCounter_[e] == 0) {
                tft_->eraseHighlightOperator(mixChangeOperatorNumber_[e]);
            }
        }
    }
    EditorParamPage::tempoClick();
}

void EditorPfm3::encoderTurnedWhileButtonPressed(int encoder6, int ticks, int button) {

    // SYNTH_MODE_EDIT
    switch (button) {
    case BUTTON_PREVIOUS_INSTRUMENT: {
        // Convert to legacy preenfm2 encoder and row
        const struct EditorOneButton* page = currentPage();
        if (page == nullptr) {
            return;
        }
        struct RowEncoder rowEncoder = page->states[synthState_->fullState_.buttonState[page->buttonId]]->rowEncoder[encoder6];

        // Operator is a special case for row
        if (unlikely(synthState_->fullState_.mainPage == 1)) {
            rowEncoder.row += synthState_->fullState_.operatorNumber;
        }

        struct ParameterDisplay* param = &(allParameterRows.row[rowEncoder.row]->params[rowEncoder.encoder]);
        const struct Pfm3Preset* defaultParams = &defaultPreset;
        int num = rowEncoder.encoder + rowEncoder.row * NUMBER_OF_ENCODERS_PFM2;

        float* value = &((float*)pfm3Preset())[num];
        float oldValue = *value;
        float newValue = ((float*)defaultParams)[num];
        (*value) = newValue;

        synthState_->propagateNewPfm3ParamValue(synthState_->getCurrentTimbre(), rowEncoder.row, rowEncoder.encoder, param, oldValue,
                                                newValue);
        break;
    }
    case BUTTON_PFM3_MENU: {

        // All update must be in green
        multipleEdition_ = true;

        // Convert to legacy preenfm2 encoder and row
        const struct EditorOneButton* page = currentPage();
        if (page == nullptr) {
            return;
        }

        struct RowEncoder rowEncoder = page->states[synthState_->fullState_.buttonState[page->buttonId]]->rowEncoder[encoder6];

        // Operator is a special case for row
        if (unlikely(synthState_->fullState_.mainPage == 1)) {
            rowEncoder.row += synthState_->fullState_.operatorNumber;
        }

        int row = rowEncoder.row;
        int encoder4 = rowEncoder.encoder;

        // Modify param of all operator of same types.
        if (row >= ROW_ENV_FIRST && row <= ROW_ENV_LAST) {
            int firstRow = ROW_ENV1_TIME;
            if (row >= ROW_ENV1_LEVEL) {
                firstRow = ROW_ENV1_LEVEL;
            }
            int currentAlgo = (int)pfm3Preset()->engine1.algo;
            int currentOpType = algoOpInformation[currentAlgo][(row - firstRow)];

            for (int op = 0; op < NUMBER_OF_OPERATORS; op++) {
                if (currentOpType == algoOpInformation[currentAlgo][op]) {
                    // Same operator type : carrier or modulation
                    encoderTurnedPfm2(firstRow + op, encoder4, ticks, false);
                }
            }
        } else if (row >= ROW_MODULATION1 && row <= ROW_MODULATION3) {
            int imMax = algoInformation[(int)pfm3Preset()->engine1.algo].im;
            // Im or v ?
            if ((encoder4 & 0x1) == 0) {
                for (int m = 0; m < imMax; m++) {
                    encoderTurnedPfm2(ROW_MODULATION1 + (m >> 1), ENCODER_ENGINE_IM1 + (m & 0x1) * 2, ticks);
                }
            } else {
                for (int m = 0; m < imMax; m++) {
                    encoderTurnedPfm2(ROW_MODULATION1 + (m >> 1), ENCODER_ENGINE_IM1_VELOCITY + (m & 0x1) * 2, ticks);
                }
            }
        } else if (row >= ROW_OSC_MIX1 && row <= ROW_OSC_MIX3) {
            // Mix or Pan ?
            int algoNumber = (int)pfm3Preset()->engine1.algo;
            if ((encoder4 & 0x1) == 0) {
                for (int m = 0; m < NUMBER_OF_OPERATORS; m++) {
                    if (algoOpInformation[algoNumber][m] == 1) {
                        encoderTurnedPfm2(ROW_OSC_MIX1 + (m >> 1), ENCODER_ENGINE_MIX1 + (m & 0x1) * 2, ticks);
                    }
                }
            } else {
                for (int m = 0; m < NUMBER_OF_OPERATORS; m++) {
                    if (algoOpInformation[algoNumber][m] == 1) {
                        encoderTurnedPfm2(ROW_OSC_MIX1 + (m >> 1), ENCODER_ENGINE_PAN1 + (m & 0x1) * 2, ticks);
                    }
                }
            }
        }

        // End of multiple edition
        multipleEdition_ = false;

        break;
    }
    }
}

void EditorPfm3::buttonPressed(int button) {
    if (button >= BUTTON_PFM3_4 && button <= BUTTON_PFM3_6 && unlikely(isInOperatorPage(synthState_->fullState_.mainPage))) {
        // Operator page : button 4 to 6 change the current operator
        const struct EditorEditMenu* editMenu = currentEditMenu();
        if (editMenu == nullptr || button >= editMenu->numberOfButtons) {
            return;
        }
        uint8_t buttonSelected = (button - BUTTON_PFM3_4);
        uint8_t currentButton = synthState_->fullState_.operatorNumber >> 1;
        int buttonId = BUTTONID_OPERATOR_1_2 + buttonSelected;

        uint8_t newButtonState = synthState_->fullState_.buttonState[buttonId];
        if (buttonSelected == currentButton) {
            newButtonState = (newButtonState + 1) & 0x1;
        }
        uint8_t numberOfOsc = algoInformation[(int)pfm3Preset()->engine1.algo].osc - 1;
        uint8_t newOperatorSelect = buttonSelected * 2 + newButtonState;
        if (newOperatorSelect <= numberOfOsc) {
            synthState_->fullState_.operatorNumber = newOperatorSelect;
            synthState_->fullState_.buttonState[buttonId] = newButtonState;
        }
        return;
    }

    EditorParamPage::buttonPressed(button);
}

void EditorPfm3::buttonLongPressed(int instrument, int button) {
    if (button == BUTTON_PREVIOUS_INSTRUMENT) {
        // Can be copied ?
        bool canBeCopied = isInMatrixPage(synthState_->fullState_.mainPage) || isInOperatorPage(synthState_->fullState_.mainPage) ||
                           (isInModulatorPage(synthState_->fullState_.mainPage) && synthState_->fullState_.editPage == 0);

        if (canBeCopied) {
            bufferPageNumber_ = synthState_->fullState_.mainPage;
            bufferEditNumber_ = synthState_->fullState_.editPage;

            const struct EditorOneButton* page = currentPage();
            if (page == nullptr) {
                return;
            }
            if (isInOperatorPage(synthState_->fullState_.mainPage) && synthState_->fullState_.editPage != 0) {
                // Env : we only store the op number.
                // We'll copy time, level and shape
                bufferParam_[0] = synthState_->fullState_.operatorNumber;
                parent_->displayPopup(COLOR_GREEN, "Copy Env", 8);
            } else {
                for (int e = 0; e < 6; e++) {
                    const struct RowEncoder rowEncoder = page->states[synthState_->fullState_.buttonState[page->buttonId]]->rowEncoder[e];
                    int row = rowEncoder.row;
                    if (row != ROW_NONE) {
                        int encoder4 = rowEncoder.encoder;
                        if (isInOperatorPage(synthState_->fullState_.mainPage)) {
                            row += synthState_->fullState_.operatorNumber;
                        }
                        int num = encoder4 + row * NUMBER_OF_ENCODERS_PFM2;
                        bufferParam_[e] = ((float*)pfm3Preset())[num];
                    }
                }
                if (isInMatrixPage(synthState_->fullState_.mainPage)) {
                    parent_->displayPopup(COLOR_GREEN, "Copy Mtx", 8);
                } else if (isInOperatorPage(synthState_->fullState_.mainPage)) {
                    parent_->displayPopup(COLOR_GREEN, "Copy Osc", 8);
                } else {
                    // LFO
                    parent_->displayPopup(COLOR_GREEN, "Copy LFO", 8);
                }
            }
        }
    } else if (button == BUTTON_NEXT_INSTRUMENT) {
        // Can be pasted ?
        bool canBePasted = isInMatrixPage(synthState_->fullState_.mainPage) && isInMatrixPage(bufferPageNumber_);
        // Operator Osc
        canBePasted |= isInOperatorPage(synthState_->fullState_.mainPage) && isInOperatorPage(bufferPageNumber_) &&
                       bufferEditNumber_ == 0 && synthState_->fullState_.editPage == 0;
        // Operator Env
        canBePasted |= isInOperatorPage(synthState_->fullState_.mainPage) && isInOperatorPage(bufferPageNumber_) &&
                       bufferEditNumber_ != 0 && synthState_->fullState_.editPage != 0;
        // LFO
        canBePasted |= isInModulatorPage(synthState_->fullState_.mainPage) && isInModulatorPage(bufferPageNumber_) &&
                       synthState_->fullState_.editPage == 0;

        if (canBePasted) {
            const struct EditorOneButton* page = currentPage();
            if (page == nullptr) {
                return;
            }
            if (isInOperatorPage(synthState_->fullState_.mainPage) && synthState_->fullState_.editPage != 0) {
                if (bufferParam_[0] != synthState_->fullState_.operatorNumber) {
                    uint8_t opNum = bufferParam_[0];
                    float* timeSource = (float*)(((EnvelopeTimeMemory*)&pfm3Preset()->env1Time) + opNum);
                    float* timeDest =
                        (float*)(((EnvelopeTimeMemory*)&pfm3Preset()->env1Time) + synthState_->fullState_.operatorNumber);
                    for (int i = 0; i < 4; i++) {
                        timeDest[i] = timeSource[i];
                    }
                    float* levelSource = (float*)(((EnvelopeLevelMemory*)&pfm3Preset()->env1Level) + opNum);
                    float* levelDest =
                        (float*)(((EnvelopeLevelMemory*)&pfm3Preset()->env1Level) + synthState_->fullState_.operatorNumber);
                    for (int i = 0; i < 4; i++) {
                        levelDest[i] = levelSource[i];
                    }
                    float* curveSource = (float*)(((EnvelopeCurveParams*)&pfm3Preset()->env1Curve) + opNum);
                    float* curveDest =
                        (float*)(((EnvelopeCurveParams*)&pfm3Preset()->env1Curve) + synthState_->fullState_.operatorNumber);
                    for (int i = 0; i < 4; i++) {
                        curveDest[i] = curveSource[i];
                    }
                }
                parent_->displayPopup(COLOR_GREEN, "Paste Env", 9);
            } else {
                for (int e = 0; e < 6; e++) {
                    const struct RowEncoder rowEncoder = page->states[synthState_->fullState_.buttonState[page->buttonId]]->rowEncoder[e];
                    int row = rowEncoder.row;
                    if (row != ROW_NONE) {
                        int encoder4 = rowEncoder.encoder;
                        if (isInOperatorPage(synthState_->fullState_.mainPage)) {
                            row += synthState_->fullState_.operatorNumber;
                        }
                        int num = encoder4 + row * NUMBER_OF_ENCODERS_PFM2;
                        ((float*)pfm3Preset())[num] = bufferParam_[e];
                    }
                }
                if (isInMatrixPage(synthState_->fullState_.mainPage)) {
                    parent_->displayPopup(COLOR_GREEN, "Paste Mtx", 9);
                } else if (isInOperatorPage(synthState_->fullState_.mainPage)) {
                    parent_->displayPopup(COLOR_GREEN, "Paste Osc", 9);
                } else {
                    // LFO
                    parent_->displayPopup(COLOR_GREEN, "Paste LFO", 9);
                }
            }
        } else {
            parent_->displayPopup(COLOR_RED, "Cannot", 6);
        }
    }
}

bool EditorPfm3::isInOperatorPage(uint8_t pageNumber) {
    return pageNumber == 1;
}

bool EditorPfm3::isInMatrixPage(uint8_t pageNumber) {
    // Main page 2 is the shared Arp/Fx page, not the PFM3 matrix.
    return getEngineType() == parent_->currentEngineType() && pageNumber == 2;
}

bool EditorPfm3::isInModulatorPage(uint8_t pageNumber) {
    return pageNumber == 3;
}

void EditorPfm3::refreshOscilloBG() {
    if (synthState_->fullState_.mainPage == 1) {
        if (synthState_->fullState_.editPage == 0) {
            refreshOscillatorOperatorShape();
        } else {
            refreshOscillatorOperatorEnvelope();
        }
    } else if (synthState_->fullState_.mainPage == 3) {
        if (synthState_->fullState_.editPage == 0) {
            refreshLfoOscillator();
        } else if (synthState_->fullState_.editPage == 1) {
            refreshLfoEnv();
        } else {
            tft_->oscilloBgActionClear();
        }
    } else {
        tft_->oscilloBgActionClear();
    }
}

void EditorPfm3::refreshOscillatorOperatorShape() {
    int op = synthState_->fullState_.operatorNumber;
    OscillatorParams* oscillatorParams = &pfm3Preset()->osc1;

    tft_->oscilloBgActionOperatorShape((int)oscillatorParams[op].shape);
}

void EditorPfm3::refreshLfoOscillator() {
    const struct EditorOneButton* page = currentPage();
    if (page == nullptr) {
        return;
    }

    if (page != &pfm3ButtonLfos) {
        return;
    }
    uint8_t lfoNumber = synthState_->fullState_.buttonState[page->buttonId];

    LfoParams* lfoParam = &pfm3Preset()->lfoOsc1;
    float* lfoPhase = (float*)&pfm3Preset()->lfoPhases;

    tft_->oscilloBgSetLfo(lfoParam[lfoNumber].shape, lfoParam[lfoNumber].freq, lfoParam[lfoNumber].keybRamp, lfoParam[lfoNumber].bias,
                          lfoPhase[lfoNumber]);
    tft_->oscilloBgActionLfo();
}

void EditorPfm3::refreshLfoEnv() {
    const struct EditorOneButton* page = currentPage();
    if (page == nullptr) {
        return;
    }

    if (page != &pfm3ButtonEnvs) {
        return;
    }
    uint8_t envNumber = synthState_->fullState_.buttonState[page->buttonId];

    if (envNumber == 0) {
        EnvelopeLfoParams* envParam = &pfm3Preset()->lfoEnv1;
        tft_->oscilloBgSetLfoEnvelope(envParam->attack, envParam->decay, 1.0f, envParam->release, 1.0f, envParam->sustain,
                                      envParam->sustain, 0.0f);
        tft_->oscilloBgActionEnvelope();
    } else if (envNumber == 1) {
        Envelope2LfoParams* envParam = &pfm3Preset()->lfoEnv2;
        tft_->oscilloBgSetLfoEnvelope(envParam->silence, envParam->attack, envParam->decay, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        tft_->oscilloBgActionEnvelope();
    }
}

void EditorPfm3::refreshOscillatorOperatorEnvelope() {
    int op = synthState_->fullState_.operatorNumber;
    EnvelopeTimeMemory* envTime = &pfm3Preset()->env1Time;
    EnvelopeLevelMemory* envLevel = &pfm3Preset()->env1Level;
    EnvelopeCurveParams* envCurve = &pfm3Preset()->env1Curve;
    tft_->oscilloBgSetEnvelope(envTime[op].attackTime, envTime[op].decayTime, envTime[op].sustainTime, envTime[op].releaseTime,
                               envLevel[op].attackLevel, envLevel[op].decayLevel, envLevel[op].sustainLevel, envLevel[op].releaseLevel,
                               envCurve[op].attackCurve, envCurve[op].decayCurve, envCurve[op].sustainCurve, envCurve[op].releaseCurve);
    tft_->oscilloBgActionEnvelope();
}

void EditorPfm3::newTimbre() {
    if (getEngineType() == parent_->currentEngineType()) {
        checkOperatorNumber();
    }
}

void EditorPfm3::checkOperatorNumber() {
    int numberOfOsc = algoInformation[(int)pfm3Preset()->engine1.algo].osc;

    if (unlikely(numberOfOsc == 3)) {
        // Special case for 3 operators
        // We don't want button 4 to be selected
        synthState_->fullState_.buttonState[BUTTONID_OPERATOR_3_4] = 0;
    }

    // If lower number of Osc, adjust current selected one so that it shows up on screen
    // when going to operator page
    if (synthState_->fullState_.operatorNumber > (numberOfOsc - 1)) {
        synthState_->fullState_.operatorNumber = (numberOfOsc - 1);
    }
}
