#include "doctest.h"

#include <cstring>
#include <vector>

#include "PersistenceMagic.h"
#include "Pfm3FlashFormat.h"
#include "Pfm3Preset.h"
#include "Pfm3PresetIO.h"
#include "Pfm3StorageBlob.h"
#include "test_helpers.h"

static void checkOldFixture(const char* file, const char* name) {
    const std::vector<uint8_t> slot = readFixture(file);
    REQUIRE(slot.size() == 1024);

    uint32_t magic = 0;
    std::memcpy(&magic, &slot[1019], 4);
    CHECK(magic == PRESET_VERSION1);

    float version = 0.0f;
    std::memcpy(&version, &slot[892], 4);
    CHECK(version == 1.0f);

    for (int i = 936; i < 976; ++i) {
        CHECK(slot[i] == 0);
    }
    CHECK(std::memcmp(&slot[720], name, std::strlen(name)) == 0);

    Pfm3Preset out{};
    REQUIRE(pfm3LoadStorageBlob(slot.data(), 1019, magic, out));

    CHECK(out.engine2.pfm3Version == PFM3_PATCH_VERSION);
    CHECK(out.arpAdvanced.magic == ARP_ADVANCED_MAGIC);
    CHECK(arpAdvancedGetEuclideanSeed(out.arpAdvanced) == 0);
    for (int pattern = 0; pattern < 4; ++pattern) {
        CHECK(arpAdvancedGetUserLength(out.arpAdvanced, pattern) == 16);
        for (int step = 0; step < 16; ++step) {
            CHECK(arpAdvancedGetStepMode(out.arpAdvanced, pattern, step) == 0);
        }
    }
    CHECK(std::memcmp(&out.presetName, name, std::strlen(name)) == 0);

    // After the one-time migration, save/load must be idempotent.
    RuntimeState state;
    std::memset(&state, 0, sizeof(state));
    pfm3DeserializePreset(out, state.params, state.shared);

    uint8_t blob[1024] = {0};
    REQUIRE(pfm3BuildStorageBlob(state.params, state.shared, blob, 1019) == sizeof(FlashSynthParams));

    Pfm3Preset out2{};
    REQUIRE(pfm3LoadStorageBlob(blob, 1019, PRESET_VERSION1, out2));
    CHECK(std::memcmp(&out, &out2, sizeof(Pfm3Preset)) == 0);
}

TEST_CASE("old V1 preset fixtures load") {
    checkOldFixture("preen_00.slot", "Preen");
    checkOldFixture("boogalu_00.slot", "WhyEy");
    checkOldFixture("elements_00.slot", "Hydrogen");
}

TEST_CASE("legacy V2 1008-byte raw preset loads") {
    Pfm3Preset src{};
    src.engine1.algo = 3.0f;
    src.engine1.playMode = 1.0f;
    src.engine2.pfm3Version = 1.0f;
    std::memcpy(src.presetName, "V2LEGACY", 8);
    src.arpAdvanced.magic = 0xdeadbeefu;

    uint8_t slot[1024] = {0};
    std::memcpy(slot, &src, PFM3_LEGACY_PRESET_SIZE);
    const uint32_t magic = PRESET_VERSION2;
    std::memcpy(slot + 1019, &magic, 4);

    Pfm3Preset out{};
    REQUIRE(pfm3LoadStorageBlob(slot, 1019, magic, out));

    CHECK(std::memcmp(&src, &out, PFM3_LEGACY_PRESET_SIZE) == 0);
    CHECK(out.engine2.pfm3Version == 1.0f);
    CHECK(out.arpAdvanced.magic == ARP_ADVANCED_MAGIC);
    CHECK(arpAdvancedGetEuclideanSeed(out.arpAdvanced) == 0);
    CHECK(std::memcmp(&out.presetName, "V2LEGACY", 8) == 0);
}
