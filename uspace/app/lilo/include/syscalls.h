#ifndef LILO_SYSCALLS_H
#define LILO_SYSCALLS_H

#include <abi/arch/masq.h>

#define __unused __attribute__((unused))

#define DECLARE_LINUX_SYSCALL(name) \
        long sys_##name(long a1, long a2, long a3, long a4, long a5, long a6, masq_t *masq)

#define DEFINE_LINUX_SYSCALL0(name) \
        static sysarg_t linux_syscall_##name(masq_t*); \
        DECLARE_LINUX_SYSCALL(name) {                       \
               return linux_syscall_##name(masq);                     \
	}                                  \
        sysarg_t linux_syscall_##name(masq_t *masq)

#define DEFINE_LINUX_SYSCALL1(name, p1t, p1n) \
        static sysarg_t linux_syscall_##name(p1t, masq_t*); \
        DECLARE_LINUX_SYSCALL(name) {         \
                return linux_syscall_##name((p1t) a1, masq);                   \
	} \
        sysarg_t linux_syscall_##name(p1t p1n, masq_t *masq)

#define DEFINE_LINUX_SYSCALL2(name, p1t, p1n, p2t, p2n) \
        static sysarg_t linux_syscall_##name(p1t, p2t, masq_t*); \
        DECLARE_LINUX_SYSCALL(name) {         \
                return linux_syscall_##name((p1t) a1, (p2t) a2, masq);         \
	} \
        sysarg_t linux_syscall_##name(p1t p1n, p2t p2n, masq_t *masq)

#define DEFINE_LINUX_SYSCALL3(name, p1t, p1n, p2t, p2n, p3t, p3n) \
        static sysarg_t linux_syscall_##name(p1t, p2t, p3t, masq_t*); \
        DECLARE_LINUX_SYSCALL(name) {         \
                return linux_syscall_##name((p1t) a1, (p2t) a2, (p3t) a3, masq);         \
	} \
        sysarg_t linux_syscall_##name(p1t p1n, p2t p2n, p3t p3n, masq_t *masq)

#define DEFINE_LINUX_SYSCALL4(name, p1t, p1n, p2t, p2n, p3t, p3n, p4t, p4n) \
        static sysarg_t linux_syscall_##name(p1t, p2t, p3t, p4t, masq_t*); \
        DECLARE_LINUX_SYSCALL(name) {         \
                return linux_syscall_##name((p1t) a1, (p2t) a2, (p3t) a3, (p4t) a4, masq);         \
	} \
        sysarg_t linux_syscall_##name(p1t p1n, p2t p2n, p3t p3n, p4t p4n, masq_t *masq)

#define DEFINE_LINUX_SYSCALL5(name, p1t, p1n, p2t, p2n, p3t, p3n, p4t, p4n, p5t, p5n) \
        static sysarg_t linux_syscall_##name(p1t, p2t, p3t, p4t, p5t, masq_t*); \
        DECLARE_LINUX_SYSCALL(name) {         \
                return linux_syscall_##name((p1t) a1, (p2t) a2, (p3t) a3, (p4t) a4, (p5t) a5, masq);         \
	} \
        sysarg_t linux_syscall_##name(p1t p1n, p2t p2n, p3t p3n, p4t p4n, p5t p5n, masq_t *masq)

#define DEFINE_LINUX_SYSCALL6(name, p1t, p1n, p2t, p2n, p3t, p3n, p4t, p4n, p5t, p5n, p6t, p6n) \
        static sysarg_t linux_syscall_##name(p1t, p2t, p3t, p4t, p5t, p6t, masq_t*); \
        DECLARE_LINUX_SYSCALL(name) {         \
                return linux_syscall_##name((p1t) a1, (p2t) a2, (p3t) a3, (p4t) a4, (p5t) a5, (p6t) a6, masq);         \
	} \
        sysarg_t linux_syscall_##name(p1t p1n, p2t p2n, p3t p3n, p4t p4n, p5t p5n, p6t p6n, masq_t *masq)

#define nameof(x) #x
#define quote_me(x) nameof(x)

#define NDEBUG
#ifndef NDEBUG
#define DPRINTF(msg, ...) printf(msg, __VA_ARGS__)
#define __debugonly
#else
#define __debugonly __attribute__((unused))
#define DPRINTF(msg, ...) ((void) 0)
#endif


#define LOG_SYSCALL(msg, ...) \
	DPRINTF("%s(" msg ") = %ld\n", __FUNCTION__, __VA_ARGS__)

// syscall 0
DECLARE_LINUX_SYSCALL(geteuid);
DECLARE_LINUX_SYSCALL(getegid);
DECLARE_LINUX_SYSCALL(getuid);
DECLARE_LINUX_SYSCALL(getgid);
DECLARE_LINUX_SYSCALL(getpid);
DECLARE_LINUX_SYSCALL(getppid);

// syscall 1
DECLARE_LINUX_SYSCALL(brk);
DECLARE_LINUX_SYSCALL(uname);
DECLARE_LINUX_SYSCALL(close);
DECLARE_LINUX_SYSCALL(times);
DECLARE_LINUX_SYSCALL(sysinfo);
DECLARE_LINUX_SYSCALL(prlimit64);

// syscall 2
DECLARE_LINUX_SYSCALL(munmap);
DECLARE_LINUX_SYSCALL(fstat);
DECLARE_LINUX_SYSCALL(arch_prctl);
DECLARE_LINUX_SYSCALL(dup2);
DECLARE_LINUX_SYSCALL(getcwd);
DECLARE_LINUX_SYSCALL(lstat);
DECLARE_LINUX_SYSCALL(stat);
DECLARE_LINUX_SYSCALL(gettimeofday);

// syscall 3
DECLARE_LINUX_SYSCALL(readlink);
DECLARE_LINUX_SYSCALL(openat);
DECLARE_LINUX_SYSCALL(open);
DECLARE_LINUX_SYSCALL(writev);
DECLARE_LINUX_SYSCALL(mprotect);
DECLARE_LINUX_SYSCALL(write);
DECLARE_LINUX_SYSCALL(read);
DECLARE_LINUX_SYSCALL(lseek);
DECLARE_LINUX_SYSCALL(rt_sigaction);
DECLARE_LINUX_SYSCALL(access);
DECLARE_LINUX_SYSCALL(execve);

DECLARE_LINUX_SYSCALL(sendfile);
DECLARE_LINUX_SYSCALL(fadvise64);
DECLARE_LINUX_SYSCALL(wait4);

DECLARE_LINUX_SYSCALL(mmap);
DECLARE_LINUX_SYSCALL(ioctl);

#endif /* LILO_SYSCALLS_H */
