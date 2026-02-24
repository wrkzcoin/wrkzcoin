/* Stub sys/epoll.h for Emscripten/WASM builds.
 * The Linux platform System library is compiled but the networking
 * code (Dispatcher/TcpConnection) is never called at runtime in WASM;
 * Nigel uses synchronous XMLHttpRequest instead of kernel I/O. */
#pragma once

#include <stdint.h>

typedef union epoll_data {
    void*    ptr;
    int      fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

struct epoll_event {
    uint32_t     events;
    epoll_data_t data;
};

#define EPOLLIN      0x001
#define EPOLLOUT     0x004
#define EPOLLERR     0x008
#define EPOLLHUP     0x010
#define EPOLLRDHUP   0x2000
#define EPOLLET      (1u << 31)
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

static inline int epoll_create(int) { return -1; }
static inline int epoll_create1(int) { return -1; }
static inline int epoll_ctl(int, int, int, struct epoll_event*) { return -1; }
static inline int epoll_wait(int, struct epoll_event*, int, int) { return -1; }
