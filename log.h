#ifndef LOG_H
#define LOG_H

#include <psp2/kernel/clib.h>

#ifndef NDEBUG
#define LOG(fmt, ...) sceClibPrintf("[vdbtcp] " fmt, ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

#endif // LOG_H
