/* Stub sys/eventfd.h for Emscripten/WASM builds. Never called at runtime. */
#pragma once
#include <stdint.h>

typedef uint64_t eventfd_t;
#define EFD_NONBLOCK  04000
#define EFD_SEMAPHORE 00001
#define EFD_CLOEXEC   02000000

static inline int eventfd(unsigned int, int) { return -1; }
static inline int eventfd_read(int, eventfd_t*) { return -1; }
static inline int eventfd_write(int, eventfd_t) { return -1; }
