#include "doctest.h"

#include <cstring>

#include "PersistenceMagic.h"
#include "Pfm3FlashFormat.h"
#include "Pfm3Preset.h"
#include "Pfm3PresetIO.h"
#include "Pfm3StorageBlob.h"
#include "test_helpers.h"

TEST_CASE("arp advanced data survives save/load") {
    RuntimeState a;
    fillRuntime(a, 3);

    uint8_t blob[1024] = {0};
    REQUIRE(pfm3BuildStorageBlob(a.params, a.shared, blob, 1019) == sizeof(FlashSynthParams));

    Pfm3Preset loaded{};
    REQUIRE(pfm3LoadStorageBlob(blob, 1019, PRESET_VERSION1, loaded));

    CHECK(loaded.arpAdvanced.magic == ARP_ADVANCED_MAGIC);
    CHECK(arpAdvancedGetEuclideanSeed(loaded.arpAdvanced) == 3);

    for (int pattern = 0; pattern < 4; ++pattern) {
        CHECK(arpAdvancedGetUserLength(loaded.arpAdvanced, pattern) == arpAdvancedGetUserLength(a.shared.arpAdvanced, pattern));
        for (int step = 0; step < 16; ++step) {
            CHECK(arpAdvancedGetStepMode(loaded.arpAdvanced, pattern, step) == arpAdvancedGetStepMode(a.shared.arpAdvanced, pattern, step));
        }
    }

    CHECK(arpAdvancedGetSwing(loaded.arpAdvanced) == 33);
    CHECK(arpAdvancedGetGroove(loaded.arpAdvanced) == 5);
    CHECK(arpAdvancedGetHumanize(loaded.arpAdvanced) == 40);

    for (int pattern = 0; pattern < 4; ++pattern) {
        CHECK(loaded.engineArpUserPatterns.patterns[pattern] == a.shared.engineArpUserPatterns.patterns[pattern]);
    }
}

TEST_CASE("saveArp=false resets the arp block") {
    RuntimeState a;
    fillRuntime(a, 1);

    Pfm3Preset mirror{};
    pfm3SerializePreset(a.params, a.shared, mirror);

    FlashSynthParams flash{};
    pfm3ParamsToFlash(&mirror, &flash, false);

    CHECK(!arpAdvancedIsValid(flash.arpAdvanced));
    CHECK(flash.arpAdvanced.magic == 0);
    for (int pattern = 0; pattern < 4; ++pattern) {
        CHECK(flash.engineArpUserPatterns.patterns[pattern] == 0);
    }
    CHECK(flash.engineArp1.BPM == 90.0f);
    CHECK(flash.engineArp2.pattern == 2.0f);
}

TEST_CASE("loadArp=false uses arp defaults") {
    FlashSynthParams flash{};
    flash.arpAdvanced.magic = 0x12345678u;

    Pfm3Preset out{};
    out.arpAdvanced.magic = 0;
    pfm3FlashToParams(&flash, &out, false);

    CHECK(out.arpAdvanced.magic == ARP_ADVANCED_MAGIC);
    CHECK(arpAdvancedGetEuclideanSeed(out.arpAdvanced) == 0);
    CHECK(arpAdvancedGetUserLength(out.arpAdvanced, 0) == 16);
}

TEST_CASE("invalid arpAdvanced magic falls back to defaults") {
    FlashSynthParams flash{};
    std::memset(&flash, 0, sizeof(flash));

    Pfm3Preset out{};
    pfm3FlashToParams(&flash, &out, true);
    CHECK(out.arpAdvanced.magic == ARP_ADVANCED_MAGIC);
    CHECK(arpAdvancedGetEuclideanSeed(out.arpAdvanced) == 0);
}
