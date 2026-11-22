/*
 * Compile-time checks for the on-flash preset layout and for the size of the
 * large objects that live in DTCM. Changing one of these constants requires a
 * deliberate change of the storage format or of the memory budget.
 */

#include "Common.h"
#include "SynthState.h"
#include "Timbre.h"
#include "engine/pfm3/Pfm3FlashFormat.h"
#include "engine/pfm3/Pfm3Preset.h"

#include <stddef.h>

// PFM3 preset compatibility: these values are the on-flash contract.
static_assert(sizeof(Pfm3Preset) == 1008, "Pfm3Preset layout changed");
static_assert(sizeof(FlashSynthParams) == 936, "FlashSynthParams layout changed");
static_assert(sizeof(EnvelopeCurveFlashMemory1) == 16, "envelope curve block 1 changed");
static_assert(sizeof(EnvelopeCurveFlashMemory2) == 8, "envelope curve block 2 changed");
static_assert(offsetof(FlashSynthParams, presetName) == 720, "presetName offset changed");
static_assert(offsetof(FlashSynthParams, envCurves1To4) == 896, "envCurves1To4 offset changed");
static_assert(offsetof(FlashSynthParams, envCurves5To6) == 912, "envCurves5To6 offset changed");

// The voice count is stored in uint8_t fields in several places.
static_assert(MAX_NUMBER_OF_VOICES <= 127, "MAX_NUMBER_OF_VOICES must fit in int8_t");

// DTCM-resident objects must not grow by embedding large arrays inline.
static_assert(sizeof(Timbre) <= 4096, "Timbre grew too large for DTCM");
static_assert(sizeof(SynthState) <= 2048, "SynthState grew too large for DTCM");