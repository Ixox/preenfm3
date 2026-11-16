/*
 * Platform stubs for the vendored Dexed MSFA engine.
 *
 * Dexed's Dx7Note depends on MTS-ESP microtuning and on the Surge Tuning
 * Library. preenFM3 only needs standard 12-TET tuning for the first MSFA
 * iteration, so this file provides:
 *
 *   - createStandardTuning(): the DX7 standard tuning table.
 *   - MTS_HasMaster() / MTS_NoteToFrequency(): no-op MTS-ESP client.
 *
 * The MTS functions use standard pitch so callers that unconditionally query
 * the client (for example Dx7Note::updateBasePitches) get sane values.
 */

#include <cmath>
#include <memory>

#include "libMTSClient.h"
#include "tuning.h"

namespace {

class StandardTuning : public TuningState {
 public:
  StandardTuning() {
    const int base = 50857777;  // (1 << 24) * (log(440) / log(2) - 69/12)
    const int step = (1 << 24) / 12;
    for (int note = 0; note < 128; ++note) {
      current_logfreq_table_[note] = base + step * note;
    }
  }

  int32_t midinote_to_logfreq(int midinote) override {
    if (midinote < 0) {
      midinote = 0;
    } else if (midinote > 127) {
      midinote = 127;
    }
    return current_logfreq_table_[midinote];
  }

 private:
  int current_logfreq_table_[128];
};

}  // namespace

std::shared_ptr<TuningState> createStandardTuning() {
  return std::make_shared<StandardTuning>();
}

extern "C" {

bool MTS_HasMaster(MTSClient *client) {
  (void)client;
  return false;
}

double MTS_NoteToFrequency(MTSClient *client, int note, int channel) {
  (void)client;
  (void)channel;
  return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

}  // extern "C"
