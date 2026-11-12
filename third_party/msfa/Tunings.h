/*
 * Minimal stand-in for the Surge Tuning Library type referenced by Dexed's
 * tuning.h. preenFM3 only uses the standard tuning path for now, so an empty
 * Tuning type is sufficient to keep the vendored interface intact.
 */

#ifndef MSFA_TUNINGS_STUB_H
#define MSFA_TUNINGS_STUB_H

namespace Tunings {
class Tuning {
};
}  // namespace Tunings

#endif  // MSFA_TUNINGS_STUB_H
