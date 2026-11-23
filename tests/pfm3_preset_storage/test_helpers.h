#ifndef TEST_HELPERS_H_
#define TEST_HELPERS_H_

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Pfm3Params.h"
#include "Pfm3Preset.h"
#include "TimbreParams.h"

struct RuntimeState {
    Pfm3TimbreParams params;
    TimbreSharedParams shared;
};

inline std::vector<uint8_t> readFixture(const char* name) {
    const std::string path = std::string(TEST_FIXTURE_DIR) + "/" + name;
    std::ifstream file(path.c_str(), std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

static inline void setEnvCurve(EnvelopeCurveParams& curve) {
    curve.attackCurve = 1.0f;
    curve.decayCurve = 0.0f;
    curve.sustainCurve = 1.0f;
    curve.releaseCurve = 0.0f;
}

/*
 * Fill every persisted field with deterministic, finite values, while avoiding
 * the "all zero" / out-of-range normalisation in pfm3FlashToParams.
 */
inline void fillRuntime(RuntimeState& state, uint32_t seed) {
    std::memset(&state, 0, sizeof(state));

    float* values = reinterpret_cast<float*>(&state.params);
    const size_t count = sizeof(state.params) / sizeof(float);
    for (size_t i = 0; i < count; ++i) {
        values[i] = (float)((int)((i * 37u + seed * 11u) % 200u) - 100) * 0.01f;
    }

    state.params.engine2.pfm3Version = PFM3_PATCH_VERSION;
    state.params.engine1.playMode = 1.0f;
    state.params.performance1.perf1 = 0.0f;
    state.params.performance1.perf2 = 0.0f;
    state.params.performance1.perf3 = 0.0f;
    state.params.performance1.perf4 = 0.0f;

    setEnvCurve(state.params.env1Curve);
    setEnvCurve(state.params.env2Curve);
    setEnvCurve(state.params.env3Curve);
    setEnvCurve(state.params.env4Curve);
    setEnvCurve(state.params.env5Curve);
    setEnvCurve(state.params.env6Curve);

    state.params.midiNote1Curve.curveBefore = 1.0f;
    state.params.midiNote1Curve.breakNote = 60.0f;
    state.params.midiNote1Curve.curveAfter = 1.0f;
    state.params.midiNote2Curve.curveBefore = 1.0f;
    state.params.midiNote2Curve.breakNote = 60.0f;
    state.params.midiNote2Curve.curveAfter = 1.0f;

    state.shared.engineArp1.clock = 1.0f;
    state.shared.engineArp1.BPM = 120.0f;
    state.shared.engineArp1.direction = 3.0f;
    state.shared.engineArp1.octave = 2.0f;
    state.shared.engineArp2.pattern = 4.0f;
    state.shared.engineArp2.division = 8.0f;
    state.shared.engineArp2.duration = 10.0f;
    state.shared.engineArp2.latche = 1.0f;

    state.shared.engineArpUserPatterns.patterns[0] = ARP_PATTERN_ACCENT(0xAAAAu, 0x5555u);
    state.shared.engineArpUserPatterns.patterns[1] = ARP_PATTERN_ACCENT(0x0F0Fu, 0x00F0u);
    state.shared.engineArpUserPatterns.patterns[2] = ARP_PATTERN_ACCENT(0x1234u, 0x0001u);
    state.shared.engineArpUserPatterns.patterns[3] = ARP_PATTERN_ACCENT(0xFFFFu, 0xFFFFu);

    state.shared.effect1.type = 1.0f;
    state.shared.effect1.param1 = 0.25f;
    state.shared.effect1.param2 = 0.5f;
    state.shared.effect1.param3 = 1.0f;
    state.shared.effect2.type = 2.0f;
    state.shared.effect2.param1 = 0.5f;
    state.shared.effect2.param2 = 0.25f;
    state.shared.effect2.param3 = 0.75f;

    std::memset(state.shared.presetName, 0, sizeof(state.shared.presetName));
    std::memcpy(state.shared.presetName, "TESTPATCH", 9);

    arpAdvancedDefaults(state.shared.arpAdvanced);
    arpAdvancedSetEuclideanSeed(state.shared.arpAdvanced, (int)(seed & 0x0fu));
    for (int pattern = 0; pattern < 4; ++pattern) {
        arpAdvancedSetUserLength(state.shared.arpAdvanced, pattern, 1 + (int)((pattern + seed) % 16u));
        for (int step = 0; step < 16; ++step) {
            const int mode = (step + pattern + (int)seed) % ARP_STEP_MODE_COUNT;
            arpAdvancedSetStepMode(state.shared.arpAdvanced, pattern, step, mode);
        }
    }
    arpAdvancedSetSwing(state.shared.arpAdvanced, 33);
    arpAdvancedSetGroove(state.shared.arpAdvanced, 5);
    arpAdvancedSetHumanize(state.shared.arpAdvanced, 40);
}

#endif /* TEST_HELPERS_H_ */
