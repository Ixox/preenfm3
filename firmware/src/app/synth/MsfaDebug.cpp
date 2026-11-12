#include "MsfaDebug.h"

#if MSFA_DEBUG_LOG

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

volatile char msfaDebugBuffer[MSFA_DEBUG_BUFFER_SIZE] __attribute__((section(".ram_d3"), used));
volatile uint32_t msfaDebugPos __attribute__((section(".ram_d3"), used));

void msfaDebugLog(const char *fmt, ...) {
    char temp[160];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);

    if (len <= 0) {
        return;
    }
    if (len > (int) sizeof(temp) - 1) {
        len = sizeof(temp) - 1;
    }

    uint32_t pos = msfaDebugPos;
    if (pos + len + 1 >= MSFA_DEBUG_BUFFER_SIZE) {
        return;
    }

    memcpy((void *) &msfaDebugBuffer[pos], temp, len);
    pos += len;
    msfaDebugBuffer[pos] = 0;
    msfaDebugPos = pos;
}

#endif

void msfaDebugInit() {
#if MSFA_DEBUG_LOG
    msfaDebugPos = 0;
    msfaDebugBuffer[0] = 0;
#endif
}
