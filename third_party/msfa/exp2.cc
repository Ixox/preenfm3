/*
 * Copyright 2013 Google Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _USE_MATH_DEFINES
#include <math.h>

#include "synth.h"
#include "exp2.h"

#include <stdio.h>

#ifdef _MSC_VER
#define exp2(arg) pow(2.0, arg)
#endif



// preenFM3: keep the exponential table in ITCM (0-wait, no cache).
int32_t exp2tab[EXP2_N_SAMPLES << 1] __attribute__((section(".instruction_ram")));

void Exp2::init() {
  double inc = exp2(1.0 / EXP2_N_SAMPLES);
  double y = 1 << 30;
  for (int i = 0; i < EXP2_N_SAMPLES; i++) {
    exp2tab[(i << 1) + 1] = (int32_t)floor(y + 0.5);
    y *= inc;
  }
  for (int i = 0; i < EXP2_N_SAMPLES - 1; i++) {
    exp2tab[i << 1] = exp2tab[(i << 1) + 3] - exp2tab[(i << 1) + 1];
  }
  exp2tab[(EXP2_N_SAMPLES << 1) - 2] = (1U << 31) - exp2tab[(EXP2_N_SAMPLES << 1) - 1];
}


