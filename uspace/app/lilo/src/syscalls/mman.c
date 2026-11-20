#include <as.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>

#include "syscalls.h"

typedef long off_t;


DEFINE_LINUX_SYSCALL6(mmap, void*, addr, size_t, length, int, prot, int, flags, int, fd, off_t, offset) {
    long ret;
    if (length == 0 || offset != 0 || fd != -1) {
        ret = -EINVAL;
        goto finish;
    }

    ret = (long)as_area_create(addr == NULL ? AS_AREA_ANY : addr, length,
                                AS_AREA_READ | AS_AREA_WRITE | AS_AREA_CACHEABLE, AS_AREA_UNPAGED);

    if (ret == (long)AS_MAP_FAILED) {
        ret = -ENOMEM;
        goto finish;
    }

    assert((flags & 0x22) == 0x22);  // MAP_ANON | MAP_COPY

finish:
    LOG_SYSCALL(ret, "%p, %lu, 0x%X, 0x%X, %d, 0x%lX", addr, length, prot, flags, fd, offset);
    return ret;
}

DEFINE_LINUX_SYSCALL2(munmap, void*, addr, size_t, length) {
    const long ret = as_area_destroy(addr);
    LOG_SYSCALL(ret, "%p, %lu", addr, length);
    return ret;
}

DEFINE_LINUX_SYSCALL5(mremap, void*, old_address, size_t, old_size, size_t, new_size, int, flags, void*, new_address) {
    long ret = -EINVAL;
    LOG_SYSCALL(ret, "%p, %lu, %lu, %d", old_address, old_size, new_size, flags);
    return ret;
}
