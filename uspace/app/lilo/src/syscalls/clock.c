#include <mem.h>
#include <vfs/vfs.h>
#include <time.h>

#include "syscalls.h"

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

DEFINE_LINUX_SYSCALL2(clock_gettime, int, clock_id, struct timespec*, tp) {
    long ret;

    switch (clock_id) {
    case CLOCK_REALTIME:
        getrealtime(tp);
        ret = 0;
        break;
    case CLOCK_MONOTONIC:
        getuptime(tp);
        ret = 0;
        break;
    default:
        ret = -EINVAL;
        break;
    }

    LOG_SYSCALL_SUCCESS(ret, "%d, {tv_sec=%lld, tv_nsec=%ld}", clock_id, tp->tv_sec, tp->tv_nsec);
    LOG_SYSCALL_ERROR(ret, "%d, %p", clock_id, tp);
    return ret;
}
