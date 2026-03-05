/* Stub sys/timerfd.h for Emscripten/WASM builds. Never called at runtime. */
#pragma once
#include <time.h>

#define TFD_NONBLOCK  04000
#define TFD_CLOEXEC   02000000
#define TFD_TIMER_ABSTIME (1 << 0)

static inline int timerfd_create(int, int) { return -1; }
static inline int timerfd_settime(int, int, const struct itimerspec*, struct itimerspec*) { return -1; }
static inline int timerfd_gettime(int, struct itimerspec*) { return -1; }
