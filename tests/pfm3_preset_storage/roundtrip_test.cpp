#include "doctest.h"

#include <cstring>

#include "PersistenceMagic.h"
#include "Pfm3FlashFormat.h"
#include "Pfm3Preset.h"
#include "Pfm3PresetIO.h"
#include "Pfm3StorageBlob.h"
#include "test_helpers.h"

TEST_CASE("PFM3 runtime -> flash -> runtime round-trip") {
    for (uint32_t seed = 1; seed <= 5; ++seed) {
        RuntimeState a;
        fillRuntime(a, seed);

        Pfm3Preset mirror{};
        pfm3SerializePreset(a.params, a.shared, mirror);

        uint8_t blob[1024];
        std::memset(blob, 0xA5, sizeof(blob));
        REQUIRE(pfm3BuildStorageBlob(a.params, a.shared, blob, 1019) == sizeof(FlashSynthParams));

        Pfm3Preset loaded{};
        REQUIRE(pfm3LoadStorageBlob(blob, 1019, PRESET_VERSION1, loaded));

        RuntimeState b;
        std::memset(&b, 0, sizeof(b));
        pfm3DeserializePreset(loaded, b.params, b.shared);

        CHECK(std::memcmp(&a.params, &b.params, sizeof(a.params)) == 0);
        CHECK(std::memcmp(&a.shared, &b.shared, sizeof(a.shared)) == 0);
        CHECK(loaded.engine2.pfm3Version == PFM3_PATCH_VERSION);
        CHECK(loaded.arpAdvanced.magic == ARP_ADVANCED_MAGIC);
    }
}

TEST_CASE("save writes only the known FlashSynthParams bytes") {
    RuntimeState a;
    fillRuntime(a, 7);

    uint8_t blob[1024];
    std::memset(blob, 0xA5, sizeof(blob));

    REQUIRE(pfm3BuildStorageBlob(a.params, a.shared, blob, 1019) == sizeof(FlashSynthParams));

    for (size_t i = sizeof(FlashSynthParams); i < 1019; ++i) {
        CHECK(blob[i] == 0xA5);
    }
    CHECK(blob[1023] == 0xA5);

    const uint32_t magic = PRESET_VERSION1;
    std::memcpy(blob + 1019, &magic, 4);
    uint32_t readMagic = 0;
    std::memcpy(&readMagic, blob + 1019, 4);
    CHECK(readMagic == PRESET_VERSION1);

    CHECK(pfm3BuildStorageBlob(a.params, a.shared, blob, sizeof(FlashSynthParams) - 1) == 0);
}
