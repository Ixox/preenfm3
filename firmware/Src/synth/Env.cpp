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

#include "Env.h"

// ram_d1
float Env::incTab[1601] __attribute__((section(".instruction_ram")));

int Env::initTab = 0;


float envLinear[] = { 0.0f, 1.0f};

float envExponential[] = {
		0.000000,0.062148,0.120174,0.174684,0.225892,0.273997,0.319188,0.361640,
		0.401521,0.438985,0.474180,0.507242,0.538301,0.567478,0.594887,0.620636,
		0.644825,0.667548,0.688895,0.708948,0.727786,0.745483,0.762108,0.777725,
		0.792397,0.806179,0.819126,0.831289,0.842715,0.853449,0.863532,0.873005,
		0.881903,0.890263,0.898115,0.905493,0.912423,0.918933,0.925049,0.930794,
		0.936192,0.941262,0.946025,0.950499,0.954703,0.958651,0.962361,0.965846,
		0.969119,0.972195,0.975083,0.977797,0.980347,0.982742,0.984992,0.987105,
		0.989091,0.990956,0.992708,0.994354,0.995901,0.997353,0.998718,1.000000
};
//cube root :
float envExponential2[] = {
		0.000000,0.251316,0.316638,0.362460,0.398939,0.429744,0.456671,0.480750,
		0.502632,0.522758,0.541444,0.558921,0.575370,0.590928,0.605707,0.619798,
		0.633276,0.646204,0.658634,0.670612,0.682176,0.693361,0.704197,0.714709,
		0.724920,0.734852,0.744522,0.753947,0.763143,0.772122,0.780897,0.789479,
		0.797878,0.806104,0.814166,0.822071,0.829827,0.837440,0.844918,0.852265,
		0.859488,0.866592,0.873580,0.880459,0.887232,0.893904,0.900477,0.906955,
		0.913342,0.919641,0.925855,0.931987,0.938039,0.944014,0.949914,0.955742,
		0.961500,0.967189,0.972813,0.978372,0.983868,0.989304,0.994681,1.000000
};
// pow 2.5
float envLog[] = {
		0.000000,0.000031,0.000173,0.000476,0.000977,0.001706,0.002691,0.003956,
		0.005524,0.007416,0.009651,0.012247,0.015223,0.018596,0.022381,0.026594,
		0.031250,0.036364,0.041950,0.048021,0.054592,0.061673,0.069280,0.077423,
		0.086115,0.095367,0.105192,0.115600,0.126603,0.138212,0.150436,0.163288,
		0.176777,0.190913,0.205706,0.221167,0.237305,0.254129,0.271650,0.289876,
		0.308816,0.328481,0.348878,0.370017,0.391906,0.414554,0.437970,0.462163,
		0.487139,0.512909,0.539480,0.566860,0.595057,0.624079,0.653935,0.684631,
		0.716177,0.748578,0.781844,0.815981,0.850997,0.886900,0.923696,0.961394
};
// pow 3.5
float envLog2[] = {
		0.000000,0.000000,0.000005,0.000022,0.000061,0.000133,0.000252,0.000433,
		0.000691,0.001043,0.001508,0.002105,0.002854,0.003777,0.004896,0.006233,
		0.007813,0.009659,0.011798,0.014256,0.017060,0.020237,0.023815,0.027824,
		0.032293,0.037253,0.042734,0.048769,0.055389,0.062627,0.070517,0.079093,
		0.088388,0.098439,0.109281,0.120951,0.133484,0.146918,0.161292,0.176643,
		0.193010,0.210433,0.228951,0.248605,0.269435,0.291483,0.314791,0.339401,
		0.365354,0.392696,0.421468,0.451716,0.483484,0.516816,0.551758,0.588355,
		0.626655,0.666703,0.708546,0.752233,0.797810,0.845327,0.894831,0.946372
};

void Env::init(struct EnvelopeParamsA *envParamsA, struct EnvelopeParamsB *envParamsB, uint8_t envNumber, float* algoNumber, struct EnvCurveParams *envCurve) {
	this->envParamsA = envParamsA;
	this->envParamsB = envParamsB;
	this->envNumber = envNumber;
	this->algoNumber = algoNumber;
	this->isLoop = checkIsLoop();
	this->envCurve = envCurve;

    if (initTab == 0) {
        initTab = 1;
        for (float k=1.0f; k<1601; k += 1.0f) {
            incTab[(int)(k + .005f)] = BLOCK_SIZE / PREENFM_FREQUENCY / (k / 100.0f);
        }
        // 1.0 to recognize it...
        incTab[0] = 1.0f;
    }

    // Init All ADSR
	for (int k=0; k<4; k++) {
		reloadADSR(k);
		reloadADSR(k + 4);
	}

	applyCurves();
}
