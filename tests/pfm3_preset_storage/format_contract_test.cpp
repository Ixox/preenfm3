#include "doctest.h"

#include <stddef.h>

#include "PersistenceMagic.h"
#include "Pfm3FlashFormat.h"
#include "Pfm3Preset.h"

TEST_CASE("PFM3 preset storage contract") {
    CHECK(PFM3_PATCH_VERSION == 2.0f);

    CHECK(sizeof(Pfm3Preset) == 1048);
    CHECK(sizeof(FlashSynthParams) == 976);
    CHECK(sizeof(ArpAdvancedParams) == 40);

    CHECK(PFM3_LEGACY_PRESET_SIZE == 1008);
    CHECK(offsetof(Pfm3Preset, arpAdvanced) == 1008);

    // Old V1 payload prefix must not move.
    CHECK(offsetof(FlashSynthParams, presetName) == 720);
    CHECK(offsetof(FlashSynthParams, envCurves1To4) == 896);
    CHECK(offsetof(FlashSynthParams, envCurves5To6) == 912);
    CHECK(offsetof(FlashSynthParams, arpAdvanced) == 936);

    CHECK(PRESET_VERSION1 == 0);
    CHECK(PRESET_VERSION2 == 292928062);
    CHECK(PRESET_CURRENT_VERSION == PRESET_VERSION1);

    CHECK(ARP_ADVANCED_MAGIC == 0x41525041u);
}
