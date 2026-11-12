/*
 * Minimal stand-in for Dexed's debugger/Logger.h.
 *
 * The MSFA engine includes this header, but the vendored sources only use the
 * logging macros inside VERBOSE-only code paths. Define them as no-ops so the
 * engine can be built on bare metal without the Dexed/JUCE logger.
 */

#ifndef MSFA_DEBUGGER_LOGGER_H
#define MSFA_DEBUGGER_LOGGER_H

#ifndef DXLOG
#define DXLOG(...) do { } while (0)
#endif

#ifndef TRACE
#define TRACE(...) do { } while (0)
#endif

#endif  // MSFA_DEBUGGER_LOGGER_H
