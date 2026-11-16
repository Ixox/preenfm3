/*
 * One-time initialization for the MSFA lookup tables.
 *
 * Call this once during firmware startup, before the SAI DMA callbacks start.
 * Do not reinitialize from the audio interrupt or from a patch load.
 */

#include "msfa_init.h"

#include "env.h"
#include "exp2.h"
#include "freqlut.h"
#include "lfo.h"
#include "pitchenv.h"
#include "porta.h"
#include "sin.h"

void msfaInit(float sampleRate) {
  Freqlut::init(sampleRate);
  Lfo::init(sampleRate);
  PitchEnv::init(sampleRate);
  Env::init_sr(sampleRate);
  Porta::init_sr(sampleRate);
  Sin::init();
  Exp2::init();
  Tanh::init();
}
