#include <assert.h>
#include <mem.h>
#include <rndgen.h>
#include <stddef.h>
#include <stdint.h>

#include "syscalls.h"

static rndgen_t* rng;

static void init_rng(void) {
    assert(rndgen_create(&rng) == EOK);
}

DEFINE_LINUX_SYSCALL3(getrandom, void*, buf, size_t, size, unsigned int, flags) {
    long ret;

    if (rng == NULL) {
        init_rng();
    }

    uint8_t* ptr = buf;
    for (size_t i = 0; i < size; i++) {
        rndgen_uint8(rng, ptr + i);
    }

    ret = EOK;
    LOG_SYSCALL(ret, "%p, %lu, %u", buf, size, flags);
    return ret;
}
