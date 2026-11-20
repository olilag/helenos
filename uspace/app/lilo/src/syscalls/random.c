#include <assert.h>
#include <mem.h>
#include <rndgen.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "syscalls.h"

static bool rng_initialized = false;
static rndgen_t rng;

static void init_rng(void) {
    struct timespec ts;
    getuptime(&ts);
	rng.seed = ts.tv_sec ^ ts.tv_nsec;
}

DEFINE_LINUX_SYSCALL3(getrandom, void*, buf, size_t, size, unsigned int, flags) {
    long ret;

    if (!rng_initialized) {
        init_rng();
    }

    uint8_t* ptr = buf;
    for (size_t i = 0; i < size; i++) {
        rndgen_uint8(&rng, ptr + i);
    }

    ret = size;
    LOG_SYSCALL(ret, "%p, %lu, %u", buf, size, flags);
    return ret;
}
