#include "doctest.h"

#include <cstring>

#include "PersistenceMagic.h"
#include "Pfm3FlashFormat.h"
#include "Pfm3Preset.h"
#include "Pfm3StorageBlob.h"
#include "test_helpers.h"

TEST_CASE("save always stamps PFM3_PATCH_VERSION") {
    const float versions[] = {0.0f, 1.0f, 2.0f, 2.5f};
    for (float version : versions) {
        RuntimeState state;
        std::memset(&state, 0, sizeof(state));
        state.params.engine2.pfm3Version = version;
        state.shared.presetName[0] = 'X';

        uint8_t blob[1024] = {0};
        REQUIRE(pfm3BuildStorageBlob(state.params, state.shared, blob, 1019) == sizeof(FlashSynthParams));

        const FlashSynthParams* flash = reinterpret_cast<const FlashSynthParams*>(blob);
        CHECK(flash->engine2.pfm3Version == PFM3_PATCH_VERSION);
    }
}

TEST_CASE("load applies the pfm2 migration for version 0") {
    RuntimeState state;
    std::memset(&state, 0, sizeof(state));
    state.params.engine1.playMode = 4.0f;
    state.params.engine2.unisonSpread = 0.0f;
    state.params.engine2.unisonDetune = 0.0f;

    uint8_t blob[1024] = {0};
    REQUIRE(pfm3BuildStorageBlob(state.params, state.shared, blob, 1019) == sizeof(FlashSynthParams));
    reinterpret_cast<FlashSynthParams*>(blob)->engine2.pfm3Version = 0.0f;

    Pfm3Preset out{};
    REQUIRE(pfm3LoadStorageBlob(blob, 1019, PRESET_VERSION1, out));
    CHECK(out.engine2.pfm3Version == PFM3_PATCH_VERSION);
    CHECK(out.engine1.playMode == (float)PLAY_MODE_POLY);
    CHECK(out.engine2.unisonSpread != 0.0f);
}

TEST_CASE("load of version 1.0 keeps its values and bumps the version") {
    RuntimeState state;
    std::memset(&state, 0, sizeof(state));
    state.params.engine1.playMode = 4.0f;

    uint8_t blob[1024] = {0};
    REQUIRE(pfm3BuildStorageBlob(state.params, state.shared, blob, 1019) == sizeof(FlashSynthParams));
    reinterpret_cast<FlashSynthParams*>(blob)->engine2.pfm3Version = 1.0f;

    Pfm3Preset out{};
    REQUIRE(pfm3LoadStorageBlob(blob, 1019, PRESET_VERSION1, out));
    CHECK(out.engine1.playMode == 4.0f);
    CHECK(out.engine2.pfm3Version == PFM3_PATCH_VERSION);
}
