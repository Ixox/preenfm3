#ifndef MSFA_DEBUG_H_
#define MSFA_DEBUG_H_

#include <stdint.h>

// Enable with -DMSFA_DEBUG_LOG=1. The log is written to RAM_D3 and can be
// dumped over SWD with:
//   st-flash --hot-plug read log.bin <msfaDebugBuffer addr> 8192
#ifndef MSFA_DEBUG_LOG
#define MSFA_DEBUG_LOG 0
#endif

#define MSFA_DEBUG_BUFFER_SIZE 8192

void msfaDebugInit();

#if MSFA_DEBUG_LOG
extern volatile char msfaDebugBuffer[MSFA_DEBUG_BUFFER_SIZE];
extern volatile uint32_t msfaDebugPos;
void msfaDebugLog(const char *fmt, ...);
#else
#define msfaDebugLog(...) do {} while (0)
#endif

#endif
