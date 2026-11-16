/*
 * Minimal stand-in for the MTS-ESP client API used by Dexed's MSFA engine.
 *
 * preenFM3 does not implement MTS-ESP microtuning yet, so the stubs in
 * platform_stubs.cc report that no MTS master is present. The standard
 * TuningState path is used instead.
 */

#ifndef MSFA_LIBMTS_CLIENT_STUB_H
#define MSFA_LIBMTS_CLIENT_STUB_H

#ifdef __cplusplus
extern "C" {
#endif

struct MTSClient;

bool MTS_HasMaster(MTSClient *client);
double MTS_NoteToFrequency(MTSClient *client, int note, int channel);

#ifdef __cplusplus
}
#endif

#endif  // MSFA_LIBMTS_CLIENT_STUB_H
