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

#include <as.h>
#include <assert.h>
#include <elf/elf_mod.h>
#include <errno.h>
#include <getopt.h>
#include <malloc.h>
#include <masq.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <str.h>
#include <str_error.h>

#include "files.h"
#include "linux/auxvec.h"
#include "syscalls.h"
#include "x86_64-linux-generic/asm/unistd_64.h"

/* ------------------------- MACROS ----------------------------------------- */

#define STACK_SIZE (4 * 1024 * 1024)
#define SYSCALL_ENTRY(name) [__NR_##name] = sys_##name

/* ------------------------- TYPE DECLARATIONS ------------------------------ */

typedef long (*syscall_handler)(long a1, long a2, long a3, long a4, long a5, long a6, masq_t *masq);

typedef struct {
    long int a_type; /* Entry type */
    union {
        long int a_val;      /* Integer value */
        void *a_ptr;         /* Pointer value */
        void (*a_fcn)(void); /* Function pointer value */
    } a_un;
} auxv_t;

/* ------------------------- EXTERNAL FUNCTION DECLARATIONS ----------------- */

extern void run_tests(void);

/* ------------------------- LOCAL FUNCTION DECLARATIONS -------------------- */

static int run_prog(int argc, char **argv);
static void *auxv_create(void *auxv, int argc, char **argv_p);

/* ------------------------- OBJECT DEFINITIONS ----------------------------- */

#undef __syscall_entry
#define __syscall_entry(name) SYSCALL_ENTRY(name),

const syscall_handler handlers[334] = {
#include "syscalls.in"
};

long random = (long) 0xdeadbeefcafefeed;

int verbose = 0;

/* ------------------------- PUBLIC FUNCTION DEFINITIONS -------------------- */

void print_log(const char* fmt, ...) {
    if (verbose != 1) {
        return;
    }

    va_list args;
    va_start(args, fmt);

    vprintf(fmt, args);

    va_end(args);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("Running tests\n");
        run_tests();
    } else {
        int c;

        while ((c = getopt(argc, argv, "+v")) != -1) {
            switch (c) {
            case 'v':
                verbose = 1;
                break;
            default:
                printf("Usage: %s [-v] executable [args...]\n", argv[0]);
                return 1;
            }
        }

        if (optind >= argc) {
            printf("Missing executable\n");
            return 1;
        }

        run_prog(argc - optind, &argv[optind]);
    }

    return 0;
}

static void *auxv_create(void *auxv_p, int argc, char *argv_p[]) {
    auxv_t *auxv = auxv_p;

    auxv[-1].a_type = AT_NULL;
    auxv[-2] = (auxv_t){.a_type = AT_RANDOM, .a_un.a_ptr = &random};
    auxv[-3] = (auxv_t){.a_type = AT_EGID, .a_un.a_val = 0};
    auxv[-4] = (auxv_t){.a_type = AT_GID, .a_un.a_val = 0};
    auxv[-5] = (auxv_t){.a_type = AT_EUID, .a_un.a_val = 0};
    auxv[-6] = (auxv_t){.a_type = AT_UID, .a_un.a_val = 0};

    const char **envp = (const char **)&auxv[-6];
    envp[-1] = NULL;

    const char **argv = &envp[-1];
    argv[-1] = NULL;

    for (int i = 0; i < argc; ++i) {
        argv[-2 - i] = argv_p[argc - 1 - i];
    }
    argv[-argc - 2] = (char *)(uintptr_t)argc;

    return &argv[-argc - 2];
}

/**
 * @brief Load and run the guest program
 *
 * @param argc Argument count of the guest program
 * @param argv Values of the guest's program arguments
 *
 * @returns The guest's program exit value
 */
static int run_prog(int argc, char **argv) {
    elf_finfo_t finfo;
    eld_flags_t flags = 0;
    const char *filename = argv[0];
    errno_t err = elf_load_file_name(filename, flags, &finfo);
    switch(err){
    case EIO:
        printf("Executable file not found: %s\n", filename);
        return err;
    case ENOMEM:
        printf("Failed loading the executable to the memory\n");
        return err;
    case EOK:
        print_log("Loaded %s at %p\n", filename, finfo.base);
        break;
    default:
        printf("Error code %d while loading %s\n", err, filename);
        return err;
    }

    uint8_t* emu_stack = as_area_create(AS_AREA_ANY, STACK_SIZE, AS_AREA_READ | AS_AREA_WRITE | AS_AREA_CACHEABLE, AS_AREA_UNPAGED);
    if (emu_stack == AS_MAP_FAILED) {
        return ENOMEM;
    }

    err = files_init();
    if (err != EOK) {
        as_area_destroy(emu_stack);
        return err;
    }

    masq_t context = {
        .fsbase = (uint64_t) memalign(PAGE_SIZE, PAGE_SIZE),
        .rcx = (uint64_t) finfo.entry,
        // Create the AUXV (https://www.elttam.com/blog/playing-with-canaries/)
        // http://articles.manugarg.com/aboutelfauxiliaryvectors
        // https://refspecs.linuxfoundation.org/LSB_1.3.0/IA64/spec/auxiliaryvector.html
        .rsp = (uint64_t) auxv_create(&emu_stack[STACK_SIZE], argc, argv)
    };

    bool forked = false;
    masq_t backup_context;
    while (true) {
        masq(&context);
        const long id = context.rax;
        if (id == __NR_exit || id == __NR_exit_group) {
            print_log("exit(%ld)\n", context.rdi);
            files_done();
            as_area_destroy(emu_stack);
            return context.rdi;
        } else if (id == __NR_vfork) {
            backup_context = context;
            forked = true;
            // For now return 0. The child should run first
            context.rax = 0;
        } else if (handlers[id] == NULL) {
            printf("Unhandled syscall: %ld\n", id);
            break;
        } else {
            assert(!forked || id == __NR_execve);
            context.rax = handlers[id](context.rdi, context.rsi, context.rdx, context.r10, context.r8, context.r9, &context);

            if(forked) {
                forked = false;
                // Previous syscall was execve, the rax holds child pid
                backup_context.rax = context.rax;
                context = backup_context;
            }
        }
    }

    return INT32_MAX;
}

/** @}
 */
