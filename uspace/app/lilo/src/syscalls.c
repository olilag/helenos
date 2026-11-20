/*
 * Copyright (c) 2021 Vit Kabele
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup lilo
 * @{
 */
/**
 * @file
 */

/* ------------------------- FILE INCLUSION --------------------------------- */

#include <assert.h>
#include <errno.h>
#include <fibril.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <str.h>
#include <time.h>
#include <timeint.h>
#include <vfs/vfs.h>
#include <task.h>

#include "syscalls.h"
#include "files.h"

#include "linux/fs.h"
#include "x86_64-linux-generic/asm/prctl.h"

#include "lib/c/generic/private/thread.h"

/* ------------------------- MACROS ----------------------------------------- */

#define UTSLEN 65

/* ------------------------- TYPE DECLARATIONS ------------------------------ */

struct utsname {
    char sysname[UTSLEN];  /* Operating system name (e.g., "Linux") */
    char nodename[UTSLEN]; /* Name within "some implementation-defined
                               network" */
    char release[UTSLEN];  /* Operating system release (e.g., "2.6.28") */
    char version[UTSLEN];  /* Operating system version */
    char machine[UTSLEN];  /* Hardware identifier */
#ifdef _GNU_SOURCE
    char domainname[]; /* NIS or YP domain name */
#endif
};

typedef long off_t;

/* ------------------------- EXTERNAL FUNCTION DECLARATIONS ----------------- */

/* ------------------------- LOCAL FUNCTION DECLARATIONS -------------------- */

/* ------------------------- OBJECT DEFINITIONS ----------------------------- */

const struct utsname my_utsname = {
    .sysname = "Linux",
    .nodename = "HelenOS PC",
    .release = "4.19.0-14-amd64",
    .version = "#1 SMP Debian 4.19.171-2 (2021-01-30)",
    .machine = "x86_64",
};

#undef __entry
#define __entry(name, linux_num) name,

static const int err_num[] = {
#include "errno_map.in"
};

#undef __entry
#define __entry(name, linux_num) linux_num,

static const int err_map[] = {
#include "errno_map.in"
};

/* ------------------------- PUBLIC FUNCTION DEFINITIONS -------------------- */

static int find_errno(int e) {
    int len = sizeof(err_num) / sizeof(errno_t);

    for (int i = 0; i < len; i++) {
        if (err_num[i] == e) {
            return i;
        }
    }

    return -1;
}

long translate_error_code(long ret) {
    if (ret >= 0) {
        return ret;
    }

    int idx = find_errno(-ret);
    if (idx == -1) {
        // unknown error code
        DPRINTF("unknown error code %lu\n", -ret);
        assert(false);
        return -131;
    }
    int linux_err = err_map[idx];
    return -linux_err;
}

DEFINE_LINUX_SYSCALL0(geteuid) {
    const long ret = 0;
    LOG_SYSCALL(ret, "");
    return ret;
}

DEFINE_LINUX_SYSCALL0(getegid) {
    const long ret = 0;
    LOG_SYSCALL(ret, "");
    return ret;
}

DEFINE_LINUX_SYSCALL0(getuid) {
    const long ret = 0;
    LOG_SYSCALL(ret, "");
    return ret;
}

DEFINE_LINUX_SYSCALL0(getpid) {
    const long ret = task_get_id(); // Zero is suspicious here...
    LOG_SYSCALL(ret, "");
    return ret;
}

DEFINE_LINUX_SYSCALL0(getppid) {
    const long ret = task_get_id() - 1; // Zero is suspicious here...
    LOG_SYSCALL(ret, "");
    return ret;
}

DEFINE_LINUX_SYSCALL0(getgid) {
    const long ret = 0;
    LOG_SYSCALL(ret, "");
    return ret;
}

DEFINE_LINUX_SYSCALL1(uname, struct utsname*, req) {
    const long ret = 0;
    assert(req != NULL);
    *req = my_utsname;
    LOG_SYSCALL(ret, "%p", req);
    return ret;
}

DEFINE_LINUX_SYSCALL1(close, int, fd) {
    long ret = 0;

    int vfs_handle;
    openedfile_t *file = openedfile_get_by_fd(fd);
    if (file == NULL) {
        ret = -EBADF;
        goto finish;
    }
    switch (file->type) {
    case FILE_TYPE_STDIN:
    case FILE_TYPE_STDOUTS:
        vfs_handle = fileno(*(file->stdio));
        break;
    case FILE_TYPE_REGULAR_FILE:
        vfs_handle = file->vfs.handle;
        break;
    default:
        ret = -EBADF;
        goto finish;
    }

    ret = vfs_put(vfs_handle);
    if (ret != EOK) {
        ret = -ret;
        goto finish;
    }

    if (file->type == FILE_TYPE_REGULAR_FILE) {
        openedfile_release(fd);
    }

finish:
    LOG_SYSCALL(ret, "%d", fd);
    return ret;
}

DEFINE_LINUX_SYSCALL2(arch_prctl, long, operation, unsigned long, addr) {
    long ret;
    const char __debugonly *op = "";
    switch (operation) {
    case ARCH_GET_FS:
        op = nameof(ARCH_GET_FS);
        *(unsigned long*)addr = masq->fsbase;
        ret = EOK;
        break;
    case ARCH_SET_FS:
        op = nameof(ARCH_SET_FS);
        masq->fsbase = addr;
        ret = EOK;
        break;
    case ARCH_GET_GS:
        op = nameof(ARCH_GET_GS);
        *(unsigned long*)addr = masq->gsbase;
        ret = EOK;
        break;
    case ARCH_SET_GS:
        op = nameof(ARCH_SET_GS);
        masq->gsbase = addr;
        ret = 0;
        break;
    case ARCH_GET_CPUID:
    case ARCH_SET_CPUID:
    default:
        ret = 0;
    }

    LOG_SYSCALL(ret, "%s, 0x%lX", op, addr);
    return ret;
}

DEFINE_LINUX_SYSCALL2(dup2, int, fd1, int, fd2) {
    const long ret = openedfile_dup2(fd1, fd2);
    LOG_SYSCALL(ret, "%d, %d", fd1, fd2);
    return ret;
}

DEFINE_LINUX_SYSCALL2(getcwd, char*, buff, size_t, length) {
    long ret = vfs_cwd_get(buff, length);
    if (ret != EOK) {
        ret = -ret;
    } else {
        ret = str_size(buff) + 1;
    }
    LOG_SYSCALL_SUCCESS(ret, "\"%s\", %ld", buff, length);
    LOG_SYSCALL_ERROR(ret, "%p, %ld", buff, length);
    return ret;
}

struct timeval {
    time_t tv_sec;
    long tv_usec;
};

DEFINE_LINUX_SYSCALL2(gettimeofday, struct timeval*, tv, void*, tz) {
    long ret = 0;

    struct timespec ts;
    getrealtime(&ts);
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = NSEC2USEC(ts.tv_nsec);

    LOG_SYSCALL(ret, "%p, %p", tv, tz);
    return ret;
}

DEFINE_LINUX_SYSCALL3(readlink, char*, path, char*, buf, size_t, bufsz) {
    long ret = -EINVAL;
    LOG_SYSCALL(ret, "%s, %p, %ld", path, buf, bufsz);
    return ret;
    // if (path == NULL || buf == NULL) {
    //     return (long) -1;
    // }
    // const size_t path_len = str_length(path);
    // const size_t to_copy = (path_len > bufsz) ? bufsz : path_len;
    // str_cpy(buf, to_copy, path);
    // LOG_SYSCALL(ret, "%s, %p, %ld", path, buf, bufsz);
    // return to_copy;
}

DEFINE_LINUX_SYSCALL3(mprotect, void*, addr, size_t, length, int, prot) {
    const long ret = 0;
    LOG_SYSCALL(ret, "%p, 0x%lX, 0x%X", addr, length, prot);
    return ret;
}

DEFINE_LINUX_SYSCALL3(lseek, int, fd, off_t, off, int, whence) {
    long ret;
    openedfile_t *file = openedfile_get_by_fd(fd);
    if(file == NULL){
        ret = -EBADF;
        goto finish;
    }
    if (file->type != FILE_TYPE_REGULAR_FILE) {
        ret = -EBADF;
        goto finish;
    }
    if (off < 0 && -off > (off_t)file->vfs.position) {
        ret = -EINVAL;
        goto finish;
    }

    switch (whence) {
    case SEEK_CUR: {
        file->vfs.position += off;
        ret = file->vfs.position;
        break;
    }
    case SEEK_SET:
        file->vfs.position = off;
        ret = off;
        break;
    case SEEK_END:
    default:
        ret = -1;
        break;
    }

finish:
    LOG_SYSCALL(ret, "%d, %ld, %d", fd, off, whence);
    return ret;
}

DEFINE_LINUX_SYSCALL3(rt_sigaction, int, signum, void*, oldaction, void*, newaction) {
    long ret = 0;
    LOG_SYSCALL(ret, "%d, %p, %p", signum, oldaction, newaction);
    return ret;
}

DEFINE_LINUX_SYSCALL4(sendfile, int, out_fd, int, in_fd, off_t, offset, size_t, count) {
    long ret = -EINVAL;
    LOG_SYSCALL(ret, "%d, %d, %ld, %lu", out_fd, in_fd, offset, count);
    return ret;
}

DEFINE_LINUX_SYSCALL4(fadvise64, int, fd, off_t, offset, off_t, len, int, advice) {
    long ret = 0;
    LOG_SYSCALL(ret, "%d, %ld, %ld, %d", fd, offset, len, advice);
    return ret;
}

DEFINE_LINUX_SYSCALL3(ioctl, int, fd, unsigned long, request, void*, data) {
    const long ret = -ENOTTY;
    LOG_SYSCALL(ret, "%d, %lX", fd, request);
    return ret;
}

DEFINE_LINUX_SYSCALL3(execve, const char*, filename, char **const, argv, char **const, envp) {
    unsigned long ret = 0, i = 0;
    const char * new_argv[128];
    new_argv[0] = "/app/lilo";
    while(argv[i++] != NULL && i < 128) {
        DPRINTF("argv[%ld] = %s\n", i - 1, argv[i - 1]);
        new_argv[i] = argv[i - 1];
    }
    assert(argv[i - 1] == NULL);
    new_argv[i] = NULL;

    if(i > 1) {
        /*
         * The filename is absolute path to the binary, whether the argv[1]
         * might be only the binary filename. Since the lilo must also find
         * the executable by the absolute path, we substitute it's argv[1]
         */
        new_argv[1] = filename;
    }

    const errno_t err = task_spawnvf(&ret, NULL, new_argv[0], new_argv, -1, -1, -1);
    assert(err == 0);

    LOG_SYSCALL(ret, "%s", filename);
    return ret;
}

DEFINE_LINUX_SYSCALL4(wait4, long, pid, int*, wstatus, int, options, void*, rusage) {
    long ret;
    if (pid == -1) {
        ret = -ECHILD;
        goto finish;
    } else {
        ret = pid;
    }
    int rr;
    task_wait_t wait;
    task_exit_t ex;
    errno_t rc = task_setup_wait(pid, &wait);
    assert(rc == EOK);
    rc = task_wait(&wait, &ex, &rr);
    assert(rc == EOK);
    assert(ex == TASK_EXIT_NORMAL);

finish:
    LOG_SYSCALL(ret, "%ld, %p, %d, %p", pid, wstatus, options, rusage);
    return ret;
}

DEFINE_LINUX_SYSCALL1(sysinfo, void*, info) {
    long ret = 0;
    LOG_SYSCALL(ret, "%p", info);
    return ret;
}

DEFINE_LINUX_SYSCALL4(prlimit64, long, pid, int, resource, void*, old_limit, void*, new_limit) {
    long ret = 0;
    LOG_SYSCALL(ret, "%ld, %d, %p, %p", pid, resource, old_limit, new_limit);
    return ret;
}

DEFINE_LINUX_SYSCALL1(set_tid_address, int*, address) {
    long ret = thread_get_id();
    LOG_SYSCALL(ret, "%p", address);
    return ret;
}

#define _SIGSET_NWORDS (1024 / (8 * sizeof (unsigned long int)))
typedef struct {
  unsigned long int __val[_SIGSET_NWORDS];
} sigset_t;

DEFINE_LINUX_SYSCALL3(rt_sigprocmask, int, how, const sigset_t*, set, sigset_t*, oldset) {
    long ret = -1;
    LOG_SYSCALL(ret, "%d, %p", how, set);
    return ret;
}

DEFINE_LINUX_SYSCALL1(unlink, const char*, path) {
    long ret = vfs_unlink_path(path);
    if (ret != EOK) {
        ret = -ret;
    }
    LOG_SYSCALL(ret, "%s", path);
    return ret;
}

DEFINE_LINUX_SYSCALL2(chmod, const char*, path, unsigned short, mode) {
    vfs_stat_t stat;
    long ret = vfs_stat_path(path, &stat);
    if (ret != EOK) {
        ret = -ret;
    }
    LOG_SYSCALL(ret, "%s, %u", path, mode);
    return ret;
}

DEFINE_LINUX_SYSCALL2(fcntl, int, fd, int, op) {
    long ret = 0;
    LOG_SYSCALL(ret, "%d, %d", fd, op);
    return ret;
}

DEFINE_LINUX_SYSCALL2(pipe2, int*, fd, int, flags) {
    long ret = -EFAULT;
    LOG_SYSCALL(ret, "[%d, %d], %d", fd[0], fd[1], flags);
    return ret;
}

DEFINE_LINUX_SYSCALL3(sched_getaffinity, long, pid, size_t, cpusetsize, void*, mask) {
    long ret = -EFAULT;
    LOG_SYSCALL(ret, "%ld, %lu, %p", pid, cpusetsize, mask);
    return ret;
}

DEFINE_LINUX_SYSCALL3(socket, int, domain, int, type, int, protocol) {
    long ret = -EAFNOSUPPORT;
    LOG_SYSCALL(ret, "%d, %d, %d", domain, type, protocol);
    return ret;
}

DEFINE_LINUX_SYSCALL2(kill, long, pid, int, sig) {
    long ret = -EPERM;
    LOG_SYSCALL(ret, "%ld, %d", pid, sig);
    return ret;
}

DEFINE_LINUX_SYSCALL1(umask, int, mask) {
    long ret = 0777;
    LOG_SYSCALL(ret, "%d", mask);
    return ret;
}

DEFINE_LINUX_SYSCALL2(rename, const char*, oldname, const char*, newname) {
    long ret = vfs_rename_path(oldname, newname);
    if (ret != EOK) {
        ret = -ret;
    }
    LOG_SYSCALL(ret, "%s, %s", oldname, newname);
    return ret;
}

struct statfs {
    unsigned long f_type;
    unsigned long f_bsize;
    unsigned long f_blocks;
    unsigned long f_bfree;
    unsigned long f_bavail;
    unsigned long f_files;
    unsigned long f_ffree;
    struct {
        int	val[2];
    } f_fsid;
    unsigned long f_namelen;
    unsigned long f_frsize;
    unsigned long f_flags;
    unsigned long f_spare[4];
};

#define EXT4_SUPER_MAGIC 0xef53
#define TMPFS_MAGIC 0x01021994

DEFINE_LINUX_SYSCALL2(statfs, const char*, path, struct statfs*, buf) {
    vfs_statfs_t st;
    long ret = vfs_statfs_path(path, &st);
    if (ret != EOK) {
        ret = -ret;
        goto finish;
    }

    memset(buf, 0, sizeof(struct statfs));
    if (str_cmp(st.fs_name, "ext4fs") == 0) {
        buf->f_type = EXT4_SUPER_MAGIC;
    } else if (str_cmp(st.fs_name, "tmpfs") == 0) {
        buf->f_type = TMPFS_MAGIC;
    }
    buf->f_bsize = st.f_bsize;
    buf->f_bfree = st.f_bfree;
    buf->f_bsize = st.f_bsize;

finish:
    LOG_SYSCALL(ret, "%s, %p", path, buf);
    return ret;
}

DEFINE_LINUX_SYSCALL5(select, int, nfds, void*, readfs, void*, writefds, void*, exceptfds, struct timeval*, timeout) {
    long ret;
    if (nfds != 0) {
        ret = -EINVAL;
        goto finish;
    }

    usec_t duration = SEC2USEC(timeout->tv_sec) + timeout->tv_usec;
    fibril_usleep(duration);
    ret = EOK;

finish:
    LOG_SYSCALL(ret, "%d, %p, %p, %p, %p", nfds, readfs, writefds, exceptfds, timeout);
    return ret;
}

DEFINE_LINUX_SYSCALL1(epoll_create1, int, flags) {
    long ret = -ENOMEM;
    LOG_SYSCALL(ret, "%d", flags);
    return ret;
}

DEFINE_LINUX_SYSCALL2(ftruncate, int, fd, off_t, length) {
    long ret;
    openedfile_t *file = openedfile_get_by_fd(fd);
    if(file == NULL) {
        ret = -EBADF;
        goto finish;
    }
    if (file->type != FILE_TYPE_REGULAR_FILE) {
        ret = -EBADF;
        goto finish;
    }
    if (length < 0) {
        ret = -EINVAL;
        goto finish;
    }

    ret = vfs_resize(file->vfs.handle, length);
    if (ret != EOK) {
        ret = -ret;
        goto finish;
    }

finish:
    LOG_SYSCALL(ret, "%d, %ld", fd, length);
    return ret;
}

DEFINE_LINUX_SYSCALL2(symlink, const char*, target, const char*, linkpath) {
    long ret = -ENOTSUP;
    LOG_SYSCALL(ret, "%s, %s", target, linkpath);
    return ret;
}
