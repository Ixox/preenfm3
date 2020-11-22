/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <dot> hosxe (at) g m a i l <dot> com)
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


#include "FMDisplayEditor.h"
#include "FMDisplay.h"
#include "SynthState.h"
#include "TftDisplay.h"
#include "preenfm3.h"

extern bool carrierOperator[28][6];

static const char *nullNames[] = { };
static const unsigned char *nullNamesOrder = 0;

const char *algoNames[] = {
    "alg1",
    "alg2",
    "alg3",
    "alg4",
    "alg5",
    "alg6",
    "alg7",
    "alg8",
    "alg9",
    "al10",
    "al11",
    "al12",
    "al13",
    "al14",
    "al15",
    "al16",
    "al17",
    "al18",
    "al19",
    "al20",
    "al21",
    "al22",
    "al23",
    "al24",
    "al25",
    "al26",
    "al27",
    "al28" };

const char *polyMonoNames[] = {
    "Mono",
    "Poly" };

struct ParameterRowDisplay engine1ParameterRow = {
    "Engine",
    {
        "Algo",
        "Velo",
        "Voice",
        "Porta" },
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
            1,
            2,
            DISPLAY_TYPE_STRINGS,
            polyMonoNames,
            nullNamesOrder,
            nullNamesOrder },
        {
            0,
            10,
            11,
            DISPLAY_TYPE_INT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

const char *clockName[] = {
    "Off ",
    "Int ",
    "Ext " };
const char *dirName[] = {
    "Up  ",
    "Down",
    "U&D ",
    "Play",
    "Rand",
    "Chrd",
    "RtUp",
    "RtDn",
    "RtUD",
    "ShUp",
    "ShDn",
    "ShUD" };

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

//   192, 144, 96, 72, 64, 48, 36, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1

const char *divNames[] = {
    "2/1 ",
    "3/2 ",
    "1/1 ",
    "3/4 ",
    "2/3 ",
    "1/2 ",
    "3/8 ",
    "1/3 ",
    "1/4 ",
    "1/6 ",
    "1/8 ",
    "1/12",
    "1/16",
    "1/24",
    "1/32",
    "1/48",
    "1/96" };
const char *activeName[] = {
    "Off ",
    "On  " };

const char *patternName[] = {
    "1   ",
    "2   ",
    "3   ",
    "4   ",
    "5   ",
    "6   ",
    "7   ",
    "8   ",
    "9   ",
    "10   ",
    "11  ",
    "12  ",
    "13  ",
    "14  ",
    "15  ",
    "16  ",
    "17  ",
    "18  ",
    "19  ",
    "20   ",
    "21  ",
    "22  ",
    "Usr1",
    "Usr2",
    "Usr3",
    "Usr4" };

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
 */

const unsigned char filtersOrder[] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    38,
    39,
    40,
    46,
    47,
    22,
    7,
    8,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    23,
    24,
    25,
    26,
    32,
    33,
    34,
    35,
    36,
    37,
    41,
    42,
    9,
    43,
    44,
    45,
    27,
    28,
    29,
    30,
    31,
    48 };

const unsigned char filtersPosition[] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    13,
    14,
    39,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    12,
    27,
    28,
    29,
    30,
    43,
    44,
    45,
    46,
    47,
    31,
    32,
    33,
    34,
    35,
    36,
    7,
    8,
    9,
    37,
    38,
    40,
    41,
    42,
    10,
    11,
    48 };

const char *fxName[] = {
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

struct ParameterRowDisplay effectParameterRow = {
    "Filter",
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

struct FilterRowDisplay filterRowDisplay[FILTER_LAST] = {
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
        "Gain" } };

const char *oscShapeNames[] = {
    "sin ",
    "saw ",
    "squa",
    "s^2 ",
    "szer",
    "spos",
    "rand",
    "off ",
    "Usr1",
    "Usr2",
    "Usr3",
    "Usr4",
    "Usr5",
    "Usr6" };
const unsigned char oscShapeNamesOrder[] = {
    7,
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    8,
    9,
    10,
    11,
    12,
    13 };
//                                                 { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
const unsigned char oscShapeNamesPosition[] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    0,
    8,
    9,
    10,
    11,
    12,
    13 };

const char *oscTypeNames[] = {
    "keyb",
    "fixe",
    "kehz" };
const unsigned char oscTypeNamesOrder[] = {
    0,
    2,
    1 };
const unsigned char oscTypeNamesPosition[] = {
    0,
    2,
    1 };

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

struct ParameterRowDisplay envParameterRow1 = {
    "Env A",
    {
        "Attk",
        "lv  ",
        "Deca",
        "lv  " },
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
            nullNamesOrder },
        {
            0,
            1,
            101,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder } } };

struct ParameterRowDisplay envParameterRow2 = {
    "Env B",
    {
        "Sust",
        "lv  ",
        "Rele",
        "lv  " },
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

const char *lofEnv2Loop[] = {
    "No  ",
    "Sile",
    "Attk" };
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

const char *matrixSourceNames[] = {
    "None",
    "lfo1",
    "lfo2",
    "lfo3",
    "env1",
    "env2",
    "seq1",
    "seq2",
    "ModW",
    "PitB",
    "AftT",
    "Velo",
    "Not1",
    "p1  ",
    "p2  ",
    "p3  ",
    "p4  ",
    "Not2",
    "Brth" };

const unsigned char matrixSourceOrder[] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    17,
    18,
    13,
    14,
    15,
    16 };
const unsigned char matrixSourcePosition[] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    15,
    16,
    17,
    18,
    13,
    14 };

const char *matrixDestNames[] = {
    "None",
    "Gate",
    "IM1 ",
    "IM2 ",
    "IM3 ",
    "IM4 ",
    "IM* ",
    "Mix1",
    "Pan1",
    "Mix2",
    "Pan2",
    "Mix3",
    "Pan3",
    "Mix4",
    "Pan4",
    "Mix*",
    "Pan*",
    "o1Fq",
    "o2Fq",
    "o3Fq",
    "o4Fq",
    "o5Fq",
    "o6Fq",
    "o*Fq",
    /*24*/"Att1",
    "Att2",
    "Att3",
    "Att4",
    "Att5",
    "Att6",
    /*30*/"AttC",
    "RelC",
    /*32*/"mx01",
    "mx02",
    "mx03",
    "mx04",
    /*36*/"l1Fq",
    "l2Fq",
    "l3Fq",
    "e2si",
    "s1ga",
    "s2ga",
    /*42*/"Fltr",
    /*43*/"o*Fh",
    "DecC",
    /*45*/"AttM",
    "DecM",
    "RelM"

};
const unsigned char matrixTargetOrder[] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    43,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    45,
    44,
    46,
    31,
    47,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42 };
//   Order                                        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44 };
const unsigned char matrixTargetPosition[] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    35,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    24,
    33,
    32,
    34,
    36 };

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
            10,
            2001,
            DISPLAY_TYPE_FLOAT,
            nullNames,
            nullNamesOrder,
            nullNamesOrder },
// We removed 8 destination target in firmware 2.0
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

const char *lfoOscMidiClock[] = {
    "M/16",
    "MC/8",
    "MC/4",
    "MC/2",
    "MClk",
    "MC*2",
    "MC*3",
    "MC*4",
    "MC*8" };
const char *lfoShapeNames[] = {
    "Sin ",
    "Saw ",
    "Tria",
    "Squa",
    "Rand" };

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

const char *lfoSeqMidiClock[] = {
    "MC/4",
    "MC/2",
    "MC  ",
    "MC*2",
    "MC*4" };

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

const char *midiNoteCurves[] = {
    "Flat",
    "+Lin",
    "+Ln8",
    "+Exp",
    "-Lin",
    "-Ln8",
    "-Exp" };

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

struct AllParameterRowsDisplay allParameterRows = {
    {
        &engine1ParameterRow,
        &engineIM1ParameterRow,
        &engineIM2ParameterRow,
        &engineIM3ParameterRow,
        &engineMix1ParameterRow,
        &engineMix2ParameterRow,
        &engineMix3ParameterRow,
        &engineArp1ParameterRow,
        &engineArp2ParameterRow,
        &engineArpPatternRow,
        &effectParameterRow,
        &oscParameterRow,
        &oscParameterRow,
        &oscParameterRow,
        &oscParameterRow,
        &oscParameterRow,
        &oscParameterRow,
        &envParameterRow1,
        &envParameterRow2,
        &envParameterRow1,
        &envParameterRow2,
        &envParameterRow1,
        &envParameterRow2,
        &envParameterRow1,
        &envParameterRow2,
        &envParameterRow1,
        &envParameterRow2,
        &envParameterRow1,
        &envParameterRow2,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &matrixParameterRow,
        &performanceParameterRow1,
        &lfoParameterRow,
        &lfoParameterRow,
        &lfoParameterRow,
        &lfoPhaseParameterRow,
        &lfoEnvParameterRow,
        &lfoEnv2ParameterRow,
        &lfoStepParameterRow,
        &lfoStepParameterRow,
        &midiNote1ParameterRow,
        &midiNote2ParameterRow } };

// constant for pfm3 editor

const struct Pfm3OneButtonState pfm3ButtonEngineState = {
    "Engine",
    {
        {
            ROW_ENGINE,
            ENCODER_ENGINE_ALGO },
        {
            ROW_ENGINE,
            ENCODER_ENGINE_MONOPOLY },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_ENGINE,
            ENCODER_ENGINE_GLIDE },
        {
            ROW_ENGINE,
            ENCODER_ENGINE_VELOCITY },
        {
            ROW_NONE,
            ENCODER_NONE } } };

const struct Pfm3OneButton pfm3ButtonEngine = {
    "Engine",
    BUTTONID_EDIT_ENGINE,
    1,
    {
        &pfm3ButtonEngineState } };

const struct Pfm3OneButtonState pfm3ButtonIMState = {
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

const struct Pfm3OneButtonState pfm3ButtonIMVelocityState = {
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

const struct Pfm3OneButton pfm3ButtonIM = {
    "MI",
    BUTTONID_EDIT_IM,
    2,
    {
        &pfm3ButtonIMState,
        &pfm3ButtonIMVelocityState } };

const struct Pfm3OneButtonState pfm3ButtonMixer123State = {
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

const struct Pfm3OneButtonState pfm3ButtonMixer456State = {
    "Op Mixer",
    {
        {
            ROW_OSC_MIX2,
            ENCODER_ENGINE_MIX2 },
        {
            ROW_OSC_MIX3,
            ENCODER_ENGINE_MIX1 },
        {
            ROW_OSC_MIX3,
            ENCODER_ENGINE_MIX2 },
        {
            ROW_OSC_MIX2,
            ENCODER_ENGINE_PAN2 },
        {
            ROW_OSC_MIX3,
            ENCODER_ENGINE_PAN1 },
        {
            ROW_OSC_MIX3,
            ENCODER_ENGINE_PAN2 } } };

const struct Pfm3OneButton pfm3ButtonMixer = {
    "Mix",
    BUTTONID_EDIT_FILTER,
    2,
    {
        &pfm3ButtonMixer123State,
        &pfm3ButtonMixer456State } };

const struct Pfm3OneButtonState pfm3ButtonArpeggiator1State = {
    "Arpeggiator",
    {
        {
            ROW_ARPEGGIATOR1,
            ENCODER_ARPEGGIATOR_CLOCK },
        {
            ROW_ARPEGGIATOR1,
            ENCODER_ARPEGGIATOR_BPM },
        {
            ROW_ARPEGGIATOR1,
            ENCODER_ARPEGGIATOR_DIRECTION },
        {
            ROW_ARPEGGIATOR1,
            ENCODER_ARPEGGIATOR_OCTAVE },
        {
            ROW_ARPEGGIATOR2,
            ENCODER_ARPEGGIATOR_DIVISION },
        {
            ROW_ARPEGGIATOR2,
            ENCODER_ARPEGGIATOR_DURATION } } };

const struct Pfm3OneButtonState pfm3ButtonArpeggiator2State = {
    "Arpeggiator",
    {
        {
            ROW_ARPEGGIATOR2,
            ENCODER_ARPEGGIATOR_PATTERN },
        {
            ROW_ARPEGGIATOR2,
            ENCODER_ARPEGGIATOR_LATCH },
        {
            ROW_NONE,
            ROW_NONE },
        {
            ROW_ARPEGGIATOR3,
            0 },
        {
            ROW_ARPEGGIATOR3,
            1 },
        {
            ROW_ARPEGGIATOR3,
            2 } } };

const struct Pfm3OneButton pfm3ButtonArpeggiator = {
    "Arp",
    BUTTONID_EDIT_ARPEGIATOR,
    2,
    {
        &pfm3ButtonArpeggiator1State,
        &pfm3ButtonArpeggiator2State } };

const struct Pfm3OneButtonState pfm3ButtonFilterState = {
    "Voice Filter",
    {
        {
            ROW_EFFECT,
            ENCODER_EFFECT_TYPE },
        {
            ROW_EFFECT,
            ENCODER_EFFECT_PARAM1 },
        {
            ROW_EFFECT,
            ENCODER_EFFECT_PARAM2 },
        {
            ROW_EFFECT,
            ENCODER_EFFECT_PARAM3 },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE } } };

const struct Pfm3OneButton pfm3ButtonFilter = {
    "Filter",
    BUTTONID_ONLY_ONE_STATE,
    1,
    {
        &pfm3ButtonFilterState } };

// ======================= OPERATOR ====================================
//      { &pfm3ButtonOPMinus, &pfm3ButtonOPNothing, &pfm3ButtonOPPlus,
//        &pfm3ButtonOPShape, &pfm3ButtonOPEnv1, &pfm3ButtonOPEnv2}

const struct Pfm3OneButtonState pfm3ButtonOPEnv2State = {
    "Enveloppe",
    {
        {
            ROW_ENV1b,
            ENCODER_ENV_R },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_ENV1b,
            ENCODER_ENV_R_LVL },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_NONE,
            ENCODER_NONE } } };

const struct Pfm3OneButton pfm3ButtonOPEnv2 = {
    "R",
    BUTTONID_ONLY_ONE_STATE,
    1,
    {
        &pfm3ButtonOPEnv2State } };

const struct Pfm3OneButtonState pfm3ButtonOPEnv1State = {
    "Enveloppe",
    {
        {
            ROW_ENV1a,
            ENCODER_ENV_A },
        {
            ROW_ENV1a,
            ENCODER_ENV_D },
        {
            ROW_ENV1b,
            ENCODER_ENV_S },
        {
            ROW_ENV1a,
            ENCODER_ENV_A_LVL },
        {
            ROW_ENV1a,
            ENCODER_ENV_D_LVL },
        {
            ROW_ENV1b,
            ENCODER_ENV_S_LVL } } };

const struct Pfm3OneButton pfm3ButtonOPEnv1 = {
    "ADS",
    BUTTONID_ONLY_ONE_STATE,
    1,
    {
        &pfm3ButtonOPEnv1State } };

const struct Pfm3OneButtonState pfm3ButtonOPShapeState = {
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

const struct Pfm3OneButton pfm3ButtonOPShape = {
    "Osc",
    BUTTONID_ONLY_ONE_STATE,
    1,
    {
        &pfm3ButtonOPShapeState } };

const struct Pfm3OneButtonState pfm3ButtonNoPage = {
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

const struct Pfm3OneButton pfm3ButtonOPMinus = {
    "<<",
    BUTTONID_ONLY_ONE_STATE,
    0,
    {
        &pfm3ButtonNoPage } };

const struct Pfm3OneButton pfm3ButtonOPNothing = {
    "",
    BUTTONID_ONLY_ONE_STATE,
    0,
    {
        &pfm3ButtonNoPage } };

const struct Pfm3OneButton pfm3ButtonOPPlus = {
    ">>",
    BUTTONID_ONLY_ONE_STATE,
    0,
    {
        &pfm3ButtonNoPage } };

// ======================= MATRIX ====================================

const struct Pfm3OneButtonState pfm3ButtonMatrix1State = {
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

const struct Pfm3OneButtonState pfm3ButtonMatrix2State = {
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

const struct Pfm3OneButtonState pfm3ButtonMatrix3State = {
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

const struct Pfm3OneButtonState pfm3ButtonMatrix4State = {
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

const struct Pfm3OneButtonState pfm3ButtonMatrix5State = {
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

const struct Pfm3OneButtonState pfm3ButtonMatrix6State = {
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

const struct Pfm3OneButtonState pfm3ButtonMatrix7State = {
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

const struct Pfm3OneButtonState pfm3ButtonMatrix8State = {
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

const struct Pfm3OneButtonState pfm3ButtonMatrix9State = {
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

const struct Pfm3OneButtonState pfm3ButtonMatrix10State = {
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

const struct Pfm3OneButtonState pfm3ButtonMatrix11State = {
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

const struct Pfm3OneButtonState pfm3ButtonMatrix12State = {
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

const struct Pfm3OneButton pfm3ButtonMatrix1 = {
    "1-2",
    BUTTONID_MATRIX_1_2,
    2,
    {
        &pfm3ButtonMatrix1State,
        &pfm3ButtonMatrix2State, } };
const struct Pfm3OneButton pfm3ButtonMatrix2 = {
    "3-4",
    BUTTONID_MATRIX_3_4,
    2,
    {
        &pfm3ButtonMatrix3State,
        &pfm3ButtonMatrix4State, } };
const struct Pfm3OneButton pfm3ButtonMatrix3 = {
    "5-6",
    BUTTONID_MATRIX_5_6,
    2,
    {
        &pfm3ButtonMatrix5State,
        &pfm3ButtonMatrix6State, } };
const struct Pfm3OneButton pfm3ButtonMatrix4 = {
    "7-8",
    BUTTONID_MATRIX_7_8,
    2,
    {
        &pfm3ButtonMatrix7State,
        &pfm3ButtonMatrix8State, } };
const struct Pfm3OneButton pfm3ButtonMatrix5 = {
    "9-10",
    BUTTONID_MATRIX_9_10,
    2,
    {
        &pfm3ButtonMatrix9State,
        &pfm3ButtonMatrix10State, } };
const struct Pfm3OneButton pfm3ButtonMatrix6 = {
    "11-12",
    BUTTONID_MATRIX_11_12,
    2,
    {
        &pfm3ButtonMatrix11State,
        &pfm3ButtonMatrix12State, } };

// =================== MODULATOR ========================

//{ , &pfm3ButtonLfo2, &pfm3ButtonLfo3, &pfm3ButtonEnvs, &pfm3ButtonStepSeq1, &pfm3ButtonStepSeq2}
const struct Pfm3OneButtonState pfm3ButtonLfoState1 = {
    "LFO 1",
    {
        {
            ROW_LFOOSC1,
            ENCODER_LFO_SHAPE },
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
            ENCODER_LFO_PHASE1 },
        {
            ROW_NONE,
            ENCODER_NONE } } };

const struct Pfm3OneButtonState pfm3ButtonLfoState2 = {
    "LFO 2",
    {
        {
            ROW_LFOOSC2,
            ENCODER_LFO_SHAPE },
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
            ENCODER_LFO_PHASE2 },
        {
            ROW_NONE,
            ENCODER_NONE } } };
const struct Pfm3OneButtonState pfm3ButtonLfoState3 = {
    "LFO 3",
    {
        {
            ROW_LFOOSC3,
            ENCODER_LFO_SHAPE },
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
            ENCODER_LFO_PHASE3 },
        {
            ROW_NONE,
            ENCODER_NONE } } };

const struct Pfm3OneButton pfm3ButtonLfos = {
    "Lfos",
    BUTTONID_EDIT_LFOS,
    3,
    {
        &pfm3ButtonLfoState1,
        &pfm3ButtonLfoState2,
        &pfm3ButtonLfoState3 } };

const struct Pfm3OneButtonState pfm3ButtonEnvState1 = {
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

const struct Pfm3OneButtonState pfm3ButtonEnvState2 = {
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

const struct Pfm3OneButton pfm3ButtonEnvs = {
    "Envs",
    BUTTONID_EDIT_ENVS,
    2,
    {
        &pfm3ButtonEnvState1,
        &pfm3ButtonEnvState2 } };

const struct Pfm3OneButtonState pfm3ButtonStepSeqStep1 = {
    "Step Sequencer 1",
    {
        {
            ROW_LFOSEQ1,
            ENCODER_STEPSEQ_BPM },
        {
            ROW_LFOSEQ1,
            ENCODER_STEPSEQ_GATE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_LFOSEQ1,
            2 },
        {
            ROW_LFOSEQ1,
            3 },
        {
            ROW_NONE,
            ENCODER_INVISIBLE } } };

const struct Pfm3OneButtonState pfm3ButtonStepSeqStep2 = {
    "Step Sequencer 2",
    {
        {
            ROW_LFOSEQ2,
            ENCODER_STEPSEQ_BPM },
        {
            ROW_LFOSEQ2,
            ENCODER_STEPSEQ_GATE },
        {
            ROW_NONE,
            ENCODER_NONE },
        {
            ROW_LFOSEQ2,
            2 },
        {
            ROW_LFOSEQ2,
            3 },
        {
            ROW_NONE,
            ENCODER_INVISIBLE } } };

const struct Pfm3OneButton pfm3ButtonSteps = {
    "Steps",
    BUTTONID_EDIT_STEPS,
    2,
    {
        &pfm3ButtonStepSeqStep1,
        &pfm3ButtonStepSeqStep2 } };

const struct Pfm3OneButtonState pfm3ButtonMidiState1 = {
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

const struct Pfm3OneButtonState pfm3ButtonMidiState2 = {
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

const struct Pfm3OneButton pfm3ButtonMidiNotes = {
    "Midi",
    BUTTONID_EDIT_NOTES,
    2,
    {
        &pfm3ButtonMidiState1,
        &pfm3ButtonMidiState2 } };

//{ ROW_NONE, ENCODER_NONE }

const struct Pfm3EditMenu operatorEditMenu = {
    "Oper",
    6,
    {
        &pfm3ButtonOPShape,
        &pfm3ButtonOPEnv1,
        &pfm3ButtonOPEnv2,
        &pfm3ButtonOPMinus,
        &pfm3ButtonOPNothing,
        &pfm3ButtonOPPlus } };

const struct Pfm3EditMenu modulatorEditorMenu = {
    "Modul",
    4,
    {
        &pfm3ButtonLfos,
        &pfm3ButtonEnvs,
        &pfm3ButtonSteps,
        &pfm3ButtonMidiNotes } };

const struct Pfm3EditMenu pfm3EngineMenu = {
    "FM",
    3,
    {
        &pfm3ButtonEngine,
        &pfm3ButtonIM,
        &pfm3ButtonMixer,
 } };

const struct Pfm3EditMenu pfm3MatrixMenu = {
    "Matrix",
    6,
    {
        &pfm3ButtonMatrix1,
        &pfm3ButtonMatrix2,
        &pfm3ButtonMatrix3,
        &pfm3ButtonMatrix4,
        &pfm3ButtonMatrix5,
        &pfm3ButtonMatrix6, } };

// Performance

const struct Pfm3OneButtonState perfEditorState = {
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
            ROW_EFFECT,
            ENCODER_EFFECT_PARAM1 },
        {
            ROW_EFFECT,
            ENCODER_EFFECT_PARAM2 } } };

const struct Pfm3OneButton perfEditorPage = {
    "Perf",
    BUTTONID_PERFORMANCE,
    1,
    {
        &perfEditorState } };

const struct Pfm3EditMenu pfm3PerfMenu = {
    "Perf",
    1,
    {
        &perfEditorPage } };

const struct Pfm3EditMenu pfm3ArpFilterMenu = {
    "Arp/Fx",
    2,
    {
        &pfm3ButtonArpeggiator,
        &pfm3ButtonFilter
    } };


struct PfmMainMenu mainMenu = {
    &pfm3EngineMenu,
    &operatorEditMenu,
    &pfm3MatrixMenu,
    &modulatorEditorMenu,
    &pfm3ArpFilterMenu,
    &pfm3PerfMenu,
};

FMDisplayEditor::FMDisplayEditor() {
    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        valueChangedCounter_[k] = 0;
        presetModifed_[k] = false;
    }
    multipleEdition_ = false;
}

void FMDisplayEditor::newTimbre(int timbre) {
    currentTimbre_ = timbre;
}

uint8_t FMDisplayEditor::getX(int encoder) {
    uint8_t x = (encoder) * 8;
    if (x >= 24) {
        x -= 24;
    }
    return x;
}

void FMDisplayEditor::newParamValue(int &refreshStatus, int timbre, int currentRow, int encoder,
    ParameterDisplay *param, float oldValue, float newValue) {

    const struct Pfm3EditMenu *editMenu = mainMenu.editMenu[synthState_->fullState.mainPage];
    const struct Pfm3OneButton *page = editMenu->pages[synthState_->fullState.editPage];
    uint8_t buttonState = synthState_->fullState.buttonState[page->buttonId];

    int encoder6 = -1;
    for (int enc = 0; (enc < 6) && (encoder6 == -1); enc++) {
        const struct RowEncoder rowEncoder = page->states[buttonState]->rowEncoder[enc];
        int rowToTest = currentRow;
        if (unlikely(synthState_->fullState.mainPage == 1)) {
            switch (currentRow) {
            case ROW_OSC2:
            case ROW_OSC3:
            case ROW_OSC4:
            case ROW_OSC5:
            case ROW_OSC6:
                rowToTest = ROW_OSC1;
                break;
            case ROW_ENV2a:
            case ROW_ENV3a:
            case ROW_ENV4a:
            case ROW_ENV5a:
            case ROW_ENV6a:
                rowToTest = ROW_ENV1a;
                break;
            case ROW_ENV2b:
            case ROW_ENV3b:
            case ROW_ENV4b:
            case ROW_ENV5b:
            case ROW_ENV6b:
                rowToTest = ROW_ENV1b;
                break;
            }
        }
        if (rowToTest == rowEncoder.row && encoder == rowEncoder.encoder) {
            encoder6 = enc;
        }
    }
    // Can be called when new param value is received on Midi
    // So encoder6 can often be -1
    if (encoder6 != -1) {
        uint8_t x = getX(encoder6);

        tft_->setCursor(x, encoder6 > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);

        struct ParameterDisplay *param = &allParameterRows.row[currentRow]->params[encoder];

        // Special cases
        switch (currentRow) {
        case ROW_ENGINE:
            if (encoder == ENCODER_ENGINE_ALGO) {
                int numberOfOsc = algoInformation[(int)synthState_->params->engine1.algo].osc - 1;

                // If lower number of Osc, adjust current selected one so that it shows up on screen
                // when going to operator page
                if (synthState_->fullState.operatorNumber > numberOfOsc) {
                    synthState_->fullState.operatorNumber = numberOfOsc;
                }
                tft_->drawAlgo(newValue);
            }
            break;
        case ROW_LFOSEQ1:
        case ROW_LFOSEQ2:
            if (encoder > ENCODER_STEPSEQ_GATE) {
                updateStepSequencer(currentRow, encoder, oldValue, newValue);
                return;
            }
            break;
        case ROW_ARPEGGIATOR3:
            // special case arpegiattor user pattern
            updateArpPattern(currentRow, encoder, oldValue, newValue);
            return;
        case ROW_ARPEGGIATOR2:
            if (encoder == ENCODER_ARPEGGIATOR_PATTERN) {
                // Refresh pattern
                refreshStatus = 3;
            }
            break;
        case ROW_EFFECT:
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
            struct ModulationIndex im = modulationIndex[(int) synthState_->params->engine1.algo][encoder6];
            tft_->highlightIM(encoder6, im.source, im.destination);
            imChangedCounter_[encoder6] = 4;
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
                struct OscillatorParams *opParams = &synthState_->params->osc1;
                if (opParams[synthState_->fullState.operatorNumber].frequencyType == OSC_FT_FIXE) {
                    refreshStatus = 9;
                }
                break;
            }
            case ENCODER_OSC_FTUNE: {
                // Refresh Fine Tune
                struct OscillatorParams *opParams = &synthState_->params->osc1;
                if (opParams[synthState_->fullState.operatorNumber].frequencyType == OSC_FT_FIXE) {
                    refreshStatus = 10;
                }
                break;
            }
            }
            break;
        }
        case ROW_ENV1a:
        case ROW_ENV1b:
        case ROW_ENV2a:
        case ROW_ENV2b:
        case ROW_ENV3a:
        case ROW_ENV3b:
        case ROW_ENV4a:
        case ROW_ENV4b:
        case ROW_ENV5a:
        case ROW_ENV5b:
        case ROW_ENV6a:
        case ROW_ENV6b:
            refreshOscillatorOperatorEnvelope();
            break;
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
        updateEncoderValueWithoutCursor(currentRow, encoder, param, newValue);
    }

}

void FMDisplayEditor::displayParamValue(int encoder, TFT_COLOR color) {
    const struct Pfm3EditMenu *editMenu = mainMenu.editMenu[synthState_->fullState.mainPage];
    const struct Pfm3OneButton *page = editMenu->pages[synthState_->fullState.editPage];
    const struct RowEncoder rowEncoder =
        page->states[synthState_->fullState.buttonState[page->buttonId]]->rowEncoder[encoder];

    uint8_t x = getX(encoder);

    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);

    if (rowEncoder.row != ROW_NONE && !hideParam_[encoder]) {
        int row = rowEncoder.row;
        // Operator page
        if (synthState_->fullState.mainPage == 1) {
            if (synthState_->fullState.editPage == 0) {
                row += synthState_->fullState.operatorNumber;
            } else {
                row += synthState_->fullState.operatorNumber * 2;
            }
        }

        struct ParameterDisplay *param = &allParameterRows.row[row]->params[rowEncoder.encoder];
        float newValue = ((float*) synthState_->params)[row * NUMBER_OF_ENCODERS + rowEncoder.encoder];

        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(color);
        updateEncoderValueWithoutCursor(row, rowEncoder.encoder, param, newValue);
    } else {
        if (rowEncoder.encoder != ENCODER_INVISIBLE) {
            tft_->print("     ");
        }
    }
}

void FMDisplayEditor::refreshEditorByStep(int &refreshStatus, int &endRefreshStatus) {

    if (synthState_->fullState.mainPage == -1) {
        switch (refreshStatus) {
        case 20:
            tft_->pauseRefresh();
            tft_->clear();
            displayPreset();
            tft_->oscilloForceNextDisplay();
            break;
        case 19:
        case 18:
        case 17:
        case 16:
        case 15:
        case 14: {
            int button = 19 - refreshStatus;
            tft_->drawButton(mainMenu.editMenu[button]->mainMenuName, 270, 29, button, 0, 1);
            break;
        }
        case 13:
            tft_->setCursorInPixel(54, 60);
            tft_->print("E d i t o r", COLOR_DARK_BLUE, COLOR_BLACK);
            break;

        case 11:
//          tft->setCursorInPixel(5, 100);
//          tft->print("Choose button bellow", COLOR_DARK_BLUE, COLOR_BLACK);
            break;
        case 10:
            tft_->drawAlgo(synthState_->params->engine1.algo);
            break;
        }
    } else {
        const struct Pfm3EditMenu *editMenu = mainMenu.editMenu[synthState_->fullState.mainPage];
        const struct Pfm3OneButton *page = editMenu->pages[synthState_->fullState.editPage];

        switch (refreshStatus) {
        case 21:
            // 21 only when comming from newSynthMode...
            tft_->pauseRefresh();
            tft_->clear();
            displayPreset();
            tft_->oscilloForceNextDisplay();
            tft_->drawAlgo(synthState_->params->engine1.algo);
            break;
        case 20:
            tft_->pauseRefresh();
            tft_->fillArea(0, 40, 240, 120, COLOR_BLACK);
            tft_->fillArea(0, 270, 240, 30, COLOR_BLACK);
            tft_->setCursor(0, 2);
            tft_->setCharBackgroundColor(COLOR_BLACK);
            tft_->setCharColor(COLOR_DARK_BLUE);
            // OPerator page
            if (synthState_->fullState.mainPage == 1) {
                tft_->print("Op ");
                tft_->print(synthState_->fullState.operatorNumber + 1);
                tft_->print(" ");
                // Highlight operator in the algo schema
                tft_->highlightOperator(synthState_->params->engine1.algo,
                    synthState_->fullState.operatorNumber);
            }
            tft_->print(page->states[synthState_->fullState.buttonState[page->buttonId]]->stateLabel);
            resetHideParams();
            break;
        case 18:
        case 17:
        case 16:
        case 15:
        case 14:
        case 13: {

            int button = 18 - refreshStatus;

            if (button < editMenu->numberOfButtons) {
                const struct Pfm3OneButton *buttonPage = editMenu->pages[button];
                tft_->drawButton(editMenu->pages[button]->buttonLabel, 270, 29, button, buttonPage->numberOfStates,
                    synthState_->fullState.editPage == button ?
                        synthState_->fullState.buttonState[page->buttonId] + 1 : 0);
            } else {
                tft_->drawButton("", 270, 29, button, 0, 0);
            }
            break;
        }
        case 12:
        case 11:
        case 10:
        case 9:
        case 8:
        case 7: {
            int button = 12 - refreshStatus;
            const struct RowEncoder rowEncoder =
                page->states[synthState_->fullState.buttonState[page->buttonId]]->rowEncoder[button];
            uint8_t x = getX(button);

            tft_->setCharColor(COLOR_MEDIUM_GRAY);
            tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);

            if (rowEncoder.row != ROW_NONE) {
                const struct ParameterRowDisplay *paramRow = allParameterRows.row[rowEncoder.row];
                if (rowEncoder.row >= ROW_MODULATION1 && rowEncoder.row <= ROW_MODULATION3) {
                    struct ModulationIndex im = modulationIndex[(int) synthState_->params->engine1.algo][button];
                    if (im.source == 0) {
                        hideParam_[button] = true;
                    } else {
                        if (button < 5) {
                            tft_->print((int) im.source);
                            tft_->print("-");
                            tft_->print((int) im.destination);
                        } else {
                            tft_->print("Fdbk");
                        }
                    }
                } else if (rowEncoder.row >= ROW_OSC_MIX1 && rowEncoder.row <= ROW_OSC_MIX3) {
                    // Add 1 if encoder >= 2
                    int mixNumber = (rowEncoder.row - ROW_OSC_MIX1) * 2 + ((rowEncoder.encoder & 0x2) >> 1);

                    // Let's hide modulator operator in the MIX page
                    if (!carrierOperator[(int) synthState_->params->engine1.algo][mixNumber]) {
                        tft_->setCharColor(COLOR_DARK_GRAY);
                        hideParam_[button] = true;
                    }
                    tft_->print(paramRow->paramName[rowEncoder.encoder]);

                } else if (rowEncoder.row == ROW_EFFECT && rowEncoder.encoder > 0) {
                    int effect = synthState_->params->effect.type;
                    if (filterRowDisplay[effect].paramName[rowEncoder.encoder - 1] != NULL) {
                        tft_->print(filterRowDisplay[effect].paramName[rowEncoder.encoder - 1]);
                    } else {
                        hideParam_[button] = true;
                        tft_->print("    ");
                    }
                } else {
                    tft_->print(paramRow->paramName[rowEncoder.encoder]);
                }
            }
            break;
        }
        case 6:
        case 5:
        case 4:
        case 3:
        case 2:
        case 1: {
            displayParamValue(6 - refreshStatus, COLOR_WHITE);
            break;
        }
        }
    }
    if (refreshStatus == endRefreshStatus) {
        endRefreshStatus = 0;
        refreshStatus = 0;
    }

}

void FMDisplayEditor::resetHideParams() {
    for (int h = 0; h < 6; h++) {
        hideParam_[h] = false;
    }
}

void FMDisplayEditor::tempoClick() {
    static float lastVolume, lastGainReduction;

    for (int e = 0; e < NUMBER_OF_ENCODERS_PFM3; e++) {
        if (valueChangedCounter_[e] > 0) {
            valueChangedCounter_[e]--;
            if (valueChangedCounter_[e] == 0) {
                if (synthState_->fullState.mainPage != -1) {
                    tft_->setCharBackgroundColor(COLOR_BLACK);
                    displayParamValue(e, COLOR_WHITE);
                }
            }
        }
        if (imChangedCounter_[e] > 0) {
            imChangedCounter_[e]--;
            if (imChangedCounter_[e] == 0) {
                struct ModulationIndex im = modulationIndex[(int) synthState_->params->engine1.algo][e];
                tft_->eraseHighlightIM(e, im.source, im.destination);
            }
        }
    }

    if (synthState_->mixerState.levelMeterWhere_ == 2) {
        float volume = getCompInstrumentVolume(currentTimbre_);
        float gr = getCompInstrumentGainReduction(currentTimbre_);
        // gr tend to slower reach 0
        // Let'as accelerate when we don't want to draw the metter anymore
        if (gr > -.1f) {
            gr = 0;
        }
        if (volume != lastVolume || gr != lastGainReduction) {
            lastVolume = volume;
            lastGainReduction = gr;

            bool isCompressed = synthState_->mixerState.instrumentState_[currentTimbre_].compressorType > 0;
            tft_->drawLevelMetter(5, 150, 230, 3, 5, volume, isCompressed, gr);
        }
    }
}

void FMDisplayEditor::displayPreset() {

    tft_->fillArea(0, 0, 240, 21, COLOR_DARK_BLUE);

    tft_->setCharBackgroundColor(COLOR_DARK_BLUE);
    tft_->setCharColor(COLOR_YELLOW);
    tft_->setCursorInPixel(2, 2);
    if (synthState_->getIsPlayingNote()) {
        tft_->print('~');
    } else {
        tft_->print('I');
    }
    tft_->print((char) ('0' + currentTimbre_ + 1));
    tft_->print(' ');
    tft_->setCharColor(COLOR_WHITE);
    tft_->print(synthState_->params->presetName);
    tft_->setCharColor(COLOR_YELLOW);
    if (presetModifed_[currentTimbre_]) {
        tft_->print('*');
    }
    tft_->setCharBackgroundColor(COLOR_BLACK);
}

void FMDisplayEditor::resetAllPresetModified() {
    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        presetModifed_[k] = false;
    }
}

void FMDisplayEditor::setPresetModified(int timbre, bool state) {
    presetModifed_[timbre] = state;
}

void FMDisplayEditor::updateArpPattern(int currentRow, int encoder, int oldValue, int newValue) {

    const int index = (int) synthState_->params->engineArp2.pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT;
    const arp_pattern_t user_pattern = synthState_->params->engineArpUserPatterns.patterns[index];

    if (synthState_->params->engineArp2.pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT) {
        return;
    }

    if (encoder == 1) {
        // new value to display
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(synthState_->patternSelect, 6);
        tft_->print((char) (127 + ((newValue >> (synthState_->patternSelect)) & 0x1) * 15));
    } else if (encoder == 0) {
        if ((oldValue % 4) == 0) {
            tft_->setCharColor(COLOR_WHITE);
        } else {
            tft_->setCharColor(COLOR_GRAY);
        }
        tft_->setCursor(oldValue, 6);
        tft_->print((char) (127 + ((user_pattern >> (oldValue)) & 0x1) * 15));
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(newValue, 6);
        tft_->print((char) (127 + ((user_pattern >> (newValue)) & 0x1) * 15));
    }
}

void FMDisplayEditor::updateStepSequencer(int currentRow, int encoder, int oldValue, int newValue) {
    int whichStepSeq = currentRow - ROW_LFOSEQ1;

    StepSequencerSteps *seqSteps = &((StepSequencerSteps*) (&synthState_->params->lfoSteps1))[whichStepSeq];

    tft_->setCharBackgroundColor(COLOR_BLACK);

    if (encoder == 3) {
        // new value to display
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(synthState_->stepSelect[whichStepSeq], 6);
        tft_->print((char) (127 + newValue));
    } else if (encoder == 2) {
        if ((oldValue % 4) == 0) {
            tft_->setCharColor(COLOR_WHITE);
        } else {
            tft_->setCharColor(COLOR_GRAY);
        }
        tft_->setCursor(oldValue, 6);
        tft_->print((char) (127 + seqSteps->steps[oldValue]));
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(newValue, 6);
        tft_->print((char) (127 + seqSteps->steps[newValue]));
    }
}

void FMDisplayEditor::updateEncoderValueWithoutCursor(int row, int encoder, ParameterDisplay *param,
    float newFloatValue) {

    int newValue = (int) newFloatValue;

    switch (param->displayType) {
    case DISPLAY_TYPE_STRINGS:
        tft_->print(param->valueName[newValue]);
        if (unlikely(param->valueName == polyMonoNames)) {
            int numberOfVoices = synthState_->mixerState.instrumentState_[currentTimbre_].numberOfVoices;
            if (numberOfVoices == 0 || (newValue == 1 && numberOfVoices == 1)
                || (newValue == 0 && numberOfVoices > 1)) {
                tft_->setCharColor(COLOR_RED);
            } else {
                tft_->setCharColor(COLOR_GRAY);
            }
            tft_->print("(");
            tft_->print(numberOfVoices);
            tft_->print(')');
        }
        break;
    case DISPLAY_TYPE_FLOAT_LFO_FREQUENCY:
        if (newFloatValue * 10.0f > LFO_FREQ_MAX_TIMES_10) {
            int stringIndex = newFloatValue * 10.0f + .005f;
#ifdef DEBUG
            if (likely(stringIndex <= LFO_MIDICLOCK_MC_TIME_8)) {
#endif

                tft_->print(' ');
                tft_->print(lfoOscMidiClock[stringIndex - LFO_FREQ_MAX_TIMES_10 - 1]);
#ifdef DEBUG
            } else {
                tft_->print("#ER#");
            }
#endif
        } else {
            tft_->printFloatWithSpace(newFloatValue);
        }
        break;
    case DISPLAY_TYPE_STEP_SEQ_BPM:
#ifdef DEBUG
        if (unlikely(newValue > LFO_SEQ_MIDICLOCK_TIME_4)) {
            tft_->print("#ER#");
            break;
        }
#endif
        if (newValue > 240) {
            tft_->print(lfoSeqMidiClock[newValue - 241]);
            break;
        }
        tft_->printValueWithSpace(newValue);
        break;
    case DISPLAY_TYPE_LFO_KSYN:
        if (unlikely(newFloatValue < 0.0f)) {
            tft_->print(" Off ");
            break;
        }
        tft_->printFloatWithSpace(newFloatValue);
        break;
    case DISPLAY_TYPE_FLOAT:
        tft_->printFloatWithSpace(newFloatValue);
        break;
        // else what follows
    case DISPLAY_TYPE_INT:
        tft_->printValueWithSpace(newValue);
        break;
    case DISPLAY_TYPE_FLOAT_OSC_FREQUENCY: {
        // Hack... to deal with the special case of the fixed frequency.....
        int oRow = row - ROW_OSC_FIRST;
        OscillatorParams *oParam = (OscillatorParams*) &synthState_->params->osc1;
        OscFrequencyType ft = (OscFrequencyType) oParam[oRow].frequencyType;

        if (ft == OSC_FT_FIXE) {
            newValue = oParam[oRow].frequencyMul * 1000.0 + oParam[oRow].detune * 100;
            if (newValue < 0) {
                newValue = 0;
            }
            tft_->print(newValue);
            if (newValue < 10000) {
                tft_->print(' ');
            }
            if (newValue < 1000) {
                tft_->print(' ');
            }
            if (newValue < 100) {
                tft_->print(' ');
            }
            if (newValue < 10) {
                tft_->print(' ');
            }
        } else {
            tft_->printFloatWithSpace(newFloatValue);
        }
        break;
    }
    case DISPLAY_TYPE_NONE:
        tft_->print("    ");
        break;
    case DISPLAY_TYPE_STEP_SEQ2:
    case DISPLAY_TYPE_STEP_SEQ1: {
        // Display the steps only once
        if (encoder == 2) {
            int whichStepSeq = row - ROW_LFOSEQ1;
            StepSequencerSteps *seqSteps = &((StepSequencerSteps*) (&synthState_->params->lfoSteps1))[whichStepSeq];

            tft_->setCharBackgroundColor(COLOR_BLACK);
            for (int k = 0; k < 16; k++) {
                if (synthState_->stepSelect[whichStepSeq] == k) {
                    tft_->setCharColor(COLOR_YELLOW);
                } else if ((k % 4) == 0) {
                    tft_->setCharColor(COLOR_WHITE);
                } else {
                    tft_->setCharColor(COLOR_GRAY);
                }
                tft_->print((char) (127 + seqSteps->steps[k]));
            }
        }
    }
        break;
    case DISPLAY_TYPE_ARP_PATTERN: {
        // Display the steps only once
        if (encoder == 0) {

            const int index = (int) synthState_->params->engineArp2.pattern;
            arp_pattern_t user_pattern;
            bool editable = false;

            if (synthState_->params->engineArp2.pattern >= ARPEGGIATOR_PRESET_PATTERN_COUNT) {
                user_pattern = synthState_->params->engineArpUserPatterns.patterns[index
                    - ARPEGGIATOR_PRESET_PATTERN_COUNT];
                editable = true;
            } else {
                user_pattern = lut_res_arpeggiator_patterns[index];
            }

            tft_->setCharBackgroundColor(COLOR_BLACK);
            for (int k = 0; k < 16; k++) {
                bool stepOn = ((user_pattern >> (k)) & 0x1) > 0;
                if (synthState_->patternSelect == k && editable) {
                    tft_->setCharColor(COLOR_YELLOW);
                } else if ((k % 4) == 0) {
                    tft_->setCharColor(COLOR_WHITE);
                } else {
                    tft_->setCharColor(COLOR_GRAY);
                }
                tft_->print((char) (127 + (stepOn ? 15 : 0)));
            }

        }
    }
        break;
    }

    tft_->setCharColor(COLOR_WHITE);

}

void FMDisplayEditor::encoderTurnedPfm3(int encoder6, int ticks) {
    struct FullState *fullState = &(synthState_->fullState);

    if (unlikely(fullState->mainPage == -1)) {
        return;
    }

    // Convert to legacy preenfm2 encoder and row
    const struct Pfm3EditMenu *editMenu = mainMenu.editMenu[synthState_->fullState.mainPage];
    const struct Pfm3OneButton *page = editMenu->pages[synthState_->fullState.editPage];
    const struct RowEncoder rowEncoder =
        page->states[synthState_->fullState.buttonState[page->buttonId]]->rowEncoder[encoder6];

    int encoder4 = rowEncoder.encoder;
    int row = rowEncoder.row;

    // Step sequencer special case
    if (unlikely(row == ROW_LFOSEQ1 || row == ROW_LFOSEQ2)) {
        if (encoder4 >= 2) {
            synthState_->encoderTurnedForStepSequencer(row, encoder4, encoder6, ticks);
            return;
        }
    };

    encoderTurnedPfm2(row, encoder4, ticks);
}


/*
 * In some case we want to shortcut the Operator row calculation case
 * so we need  specialOpCase = false
 */
void FMDisplayEditor::encoderTurnedPfm2(int row, int encoder4, int ticks, bool specialOpCase) {

    if (row == ROW_NONE) {
        return;
    }
    if (unlikely(row == ROW_ARPEGGIATOR3)) {
        synthState_->encoderTurnedForArpPattern(row, encoder4, ticks);
        return;
    }

    //
    int num;
    struct ParameterDisplay *param;
    if (unlikely(synthState_->fullState.mainPage == 1) && specialOpCase) {
        int multiplier = synthState_->fullState.editPage == 0 ? 1 : 2;
        // operator is a bit different with PFM3
        num = encoder4 + (row + synthState_->fullState.operatorNumber * multiplier) * NUMBER_OF_ENCODERS;
        param = &(allParameterRows.row[row + synthState_->fullState.operatorNumber * multiplier]->params[encoder4]);
        row += synthState_->fullState.operatorNumber * multiplier;
    } else {
        num = encoder4 + row * NUMBER_OF_ENCODERS;
        param = &(allParameterRows.row[row]->params[encoder4]);
    }
    float newValue;
    float oldValue;

    if (param->displayType == DISPLAY_TYPE_STRINGS) {
        // Do not use encoder acceleration
        ticks = ticks > 0 ? 1 : -1;
    }

    if (param->valueNameOrder == NULL) {
        // Not string parameters

        // floating point test to be sure numberOfValues is different from 1.
        // for voices when number of voices forced to 0
        if (param->numberOfValues < 1.5) {
            return;
        }

        float &value = ((float*) synthState_->params)[num];
        oldValue = value;

        float inc = param->incValue;

        // Slow down LFO frequency
        if (unlikely(param->displayType == DISPLAY_TYPE_FLOAT_LFO_FREQUENCY)) {
            if (oldValue < 1.0f) {
                inc = inc * .1f;
            }
        }

        int tickIndex = (value - param->minValue) / inc + .0005f + ticks;
        newValue = param->minValue + tickIndex * inc;

        if (newValue > param->maxValue) {
            newValue = param->maxValue;
        }
        if (newValue < param->minValue) {
            newValue = param->minValue;
        }
        value = newValue;
    } else {
        float *value = &((float*) synthState_->params)[num];
        newValue = oldValue = (*value);

        // Must use newValue (int) so that the minValue comparaison works
        // Is there any other order than the default one
        int pos = param->valueNameOrderReversed[(int) (*value)];

        // Special Case for filter - hide not so good filter
        if (unlikely(param->valueNameOrderReversed == filtersPosition)) {
            if (ticks > 0 && oldValue != FILTER_LADDER) {
                newValue = param->valueNameOrder[pos + 1];
            }
            if (ticks < 0 && pos > param->minValue) {
                newValue = param->valueNameOrder[pos - 1];
            }
        } else {
            if (ticks > 0 && pos < param->maxValue) {
                newValue = param->valueNameOrder[pos + 1];
            }
            if (ticks < 0 && pos > param->minValue) {
                newValue = param->valueNameOrder[pos - 1];
            }
        }

        (*value) = (float) newValue;
    }
    if (newValue != oldValue) {
        synthState_->propagateNewParamValue(currentTimbre_, row, encoder4, param, oldValue, newValue);
    }

}

void FMDisplayEditor::encoderTurnedWhileButtonPressed(int encoder6, int ticks, int button) {

    // SYNTH_MODE_EDIT
    switch (button) {
    case BUTTON_PREVIOUS_INSTRUMENT: {
        // Convert to legacy preenfm2 encoder and row
        const struct Pfm3EditMenu *editMenu = mainMenu.editMenu[synthState_->fullState.mainPage];
        const struct Pfm3OneButton *page = editMenu->pages[synthState_->fullState.editPage];
        struct RowEncoder rowEncoder =
            page->states[synthState_->fullState.buttonState[page->buttonId]]->rowEncoder[encoder6];

        // Operator is a special case for row
        if (unlikely(synthState_->fullState.mainPage == 1)) {
            int multiplier = synthState_->fullState.editPage == 0 ? 1 : 2;
            rowEncoder.row += synthState_->fullState.operatorNumber * multiplier;
        }

        struct ParameterDisplay *param = &(allParameterRows.row[rowEncoder.row]->params[rowEncoder.encoder]);
        const struct OneSynthParams *defaultParams = &defaultPreset;
        int num = rowEncoder.encoder + rowEncoder.row * NUMBER_OF_ENCODERS;

        float *value = &((float*) synthState_->params)[num];
        float oldValue = *value;
        float newValue = ((float*) defaultParams)[num];
        (*value) = newValue;

        synthState_->propagateNewParamValue(currentTimbre_, rowEncoder.row, rowEncoder.encoder, param, oldValue,
            newValue);
        break;
    }
    case BUTTON_PFM3_EDIT:
    {

        // All update must be in green
        multipleEdition_ = true;

        // Convert to legacy preenfm2 encoder and row
        const struct Pfm3EditMenu *editMenu = mainMenu.editMenu[synthState_->fullState.mainPage];
        const struct Pfm3OneButton *page = editMenu->pages[synthState_->fullState.editPage];

        struct RowEncoder rowEncoder =
            page->states[synthState_->fullState.buttonState[page->buttonId]]->rowEncoder[encoder6];

        // Operator is a special case for row
        if (unlikely(synthState_->fullState.mainPage == 1)) {
            int multiplier = synthState_->fullState.editPage == 0 ? 1 : 2;
            rowEncoder.row += synthState_->fullState.operatorNumber * multiplier;
        }

        int row = rowEncoder.row;
        int encoder4 = rowEncoder.encoder;

        // Modify param of all operator of same types.
        if (row >= ROW_ENV1a && row <= ROW_ENV6b) {
            int firstRow = ROW_ENV1a;
            int envBBit = ROW_ENV1b & 0x1;
            if ((row & 0x1) == envBBit) {
                firstRow = ROW_ENV1b;
            }
            int currentAlgo = (int)synthState_->params->engine1.algo;
            int currentOpType = algoOpInformation[currentAlgo][(row - ROW_ENV1a) >> 1];

            for (int op = 0; op < NUMBER_OF_OPERATORS; op++) {
                if (currentOpType == algoOpInformation[currentAlgo][op]) {
                    // Same operator type : carrier or modulation
                    encoderTurnedPfm2(firstRow + op*2, encoder4, ticks, false);
                }
            }
        } else if (row >= ROW_MODULATION1 && row <= ROW_MODULATION3) {
            int imMax = algoInformation[(int)synthState_->params->engine1.algo].im;
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
            int algoNumber = (int)synthState_->params->engine1.algo;
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

void FMDisplayEditor::buttonPressed(int button) {
    struct FullState *fullState = &synthState_->fullState;

    SynthEditMode oldSynthMode = synthState_->fullState.synthMode;
    MenuState oldMenuState = synthState_->fullState.currentMenuItem->menuState;

    if (synthState_->fullState.mainPage == -1) {
        if (button >= BUTTON_PFM3_1 && button <= BUTTON_PFM3_6) {
            synthState_->fullState.mainPage = button;
            synthState_->fullState.editPage = 0;
        }
    } else {
        switch (button) {
        case BUTTON_PFM3_EDIT:
        case BUTTON_PFM3_MENU:
            synthState_->fullState.mainPage = -1;
            break;
        case BUTTON_PFM3_1:
        case BUTTON_PFM3_2:
        case BUTTON_PFM3_3:
        case BUTTON_PFM3_4:
        case BUTTON_PFM3_5:
        case BUTTON_PFM3_6:
            const struct Pfm3EditMenu *editMenu = mainMenu.editMenu[synthState_->fullState.mainPage];
            const struct Pfm3OneButton *page = editMenu->pages[synthState_->fullState.editPage];
            if (button >= editMenu->numberOfButtons) {
                break;
            }
            if (synthState_->fullState.editPage == button) {
                if (page->numberOfStates > 0) {
                    synthState_->fullState.buttonState[page->buttonId] =
                        (synthState_->fullState.buttonState[page->buttonId] + 1) % page->numberOfStates;
                }
            } else {
                // If new page has no state.. then we don't activate it
                if (editMenu->pages[button]->numberOfStates > 0) {
                    synthState_->fullState.editPage = button;
                } else {
                    if (synthState_->fullState.mainPage == 1) {
                        int numberOfOsc = algoInformation[(int)synthState_->params->engine1.algo].osc - 1;
                        if (button ==  BUTTON_PFM3_4) {
                            if (synthState_->fullState.operatorNumber > 0) {
                                synthState_->fullState.operatorNumber--;
                            } else {
                                synthState_->fullState.operatorNumber = numberOfOsc;
                            }
                        } else if (button == BUTTON_PFM3_6) {
                            if (synthState_->fullState.operatorNumber < numberOfOsc) {
                                synthState_->fullState.operatorNumber++;
                            } else {
                                synthState_->fullState.operatorNumber = 0;
                            }
                        }
                    }
                }
            }
            break;
        }

    }

    synthState_->propagateNewPfm3Page();
}

void FMDisplayEditor::refreshOscillatorOperatorShape() {
    int op = synthState_->fullState.operatorNumber;
    OscillatorParams *oscillatorParams = &synthState_->params->osc1;

    tft_->oscilloBgActionOperatorShape((int) oscillatorParams[op].shape);
}

void FMDisplayEditor::refreshOscillatorOperatorEnvelope() {
    int op = synthState_->fullState.operatorNumber;
    EnvelopeParamsA *envParamA = &synthState_->params->env1a;
    EnvelopeParamsB *envParamB = &synthState_->params->env1b;
    tft_->oscilloBgSetEnvelope(envParamA[op * 2].attackTime, envParamA[op * 2].decayTime, envParamB[op * 2].sustainTime,
        envParamB[op * 2].releaseTime, envParamA[op * 2].attackLevel, envParamA[op * 2].decayLevel,
        envParamB[op * 2].sustainLevel, envParamB[op * 2].releaseLevel);
    tft_->oscilloBgActionEnvelope();
}

void FMDisplayEditor::refreshLfoOscillator() {
    const struct Pfm3EditMenu *editMenu = mainMenu.editMenu[synthState_->fullState.mainPage];
    const struct Pfm3OneButton *page = editMenu->pages[synthState_->fullState.editPage];

    if (page != &pfm3ButtonLfos) {
        return;
    }
    uint8_t lfoNumber = synthState_->fullState.buttonState[page->buttonId];

    LfoParams *lfoParam = &synthState_->params->lfoOsc1;
    float *lfoPhase = (float*) &synthState_->params->lfoPhases;

    tft_->oscilloBgSetLfo(lfoParam[lfoNumber].shape, lfoParam[lfoNumber].freq, lfoParam[lfoNumber].keybRamp,
        lfoParam[lfoNumber].bias, lfoPhase[lfoNumber]);
    tft_->oscilloBgActionLfo();
}

void FMDisplayEditor::refreshLfoEnv() {
    const struct Pfm3EditMenu *editMenu = mainMenu.editMenu[synthState_->fullState.mainPage];
    const struct Pfm3OneButton *page = editMenu->pages[synthState_->fullState.editPage];

    if (page != &pfm3ButtonEnvs) {
        return;
    }
    uint8_t envNumber = synthState_->fullState.buttonState[page->buttonId];

    if (envNumber == 0) {
        EnvelopeParams *envParam = &synthState_->params->lfoEnv1;
        tft_->oscilloBgSetLfoEnvelope(envParam->attack, envParam->decay, 1.0f, envParam->release, 1.0f,
            envParam->sustain, envParam->sustain, 0.0f);
        tft_->oscilloBgActionEnvelope();
    } else if (envNumber == 1) {
        Envelope2Params *envParam = &synthState_->params->lfoEnv2;
        tft_->oscilloBgSetLfoEnvelope(envParam->silence, envParam->attack, envParam->decay, 0.0f, 0.0f, 1.0f, 0.0f,
            0.0f);
        tft_->oscilloBgActionEnvelope();
    }

}

