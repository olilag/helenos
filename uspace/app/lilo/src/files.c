/*
 * Copyright (c) 2017 Vojtech Horky
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

/** @addtogroup libinux
 * @{
 */

#include <macros.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <str.h>
#include <vfs/vfs.h>

#include "files.h"
#include "console_input.h"

#define MAX_FILES 16

static openedfile_t files[MAX_FILES];

static ssize_t read_regular_file(openedfile_t *file, void *buffer, size_t size) {
    assert(file->type == FILE_TYPE_REGULAR_FILE);
    size_t nbyte;
    const unsigned long rc = vfs_read(file->vfs.handle, &file->vfs.position, buffer, size, &nbyte);
    return (rc == EOK) ? nbyte : -rc;
}

static ssize_t read_stdin(openedfile_t *file, void *buffer, size_t size) {
    assert(file->type == FILE_TYPE_STDIN);
    return fread(buffer, 1, size, *(file->stdio));
}

static ssize_t read_stdout(openedfile_t *file, void *buffer, size_t size) {
    assert(file->type == FILE_TYPE_STDOUTS);
    return -EBADF;
}

static ssize_t read_console(openedfile_t *file, void *buffer, size_t size) {
    assert(file->type == FILE_TYPE_CONSOLE);

    char* dest;
    if (console_input_read(file->console, &dest) != EOK) {
        // handle ENOENT emulate SIGINT
        return -1;
    }
    size_t dest_length = str_length(dest);
    memcpy(buffer, dest, min(size, dest_length));
    free(dest);

    return min(size, dest_length);
}

static ssize_t write_regular_file(openedfile_t *file, void *buffer, size_t size) {
    assert(file->type == FILE_TYPE_REGULAR_FILE);
    size_t nbyte;
    const unsigned long rc = vfs_write(file->vfs.handle, &file->vfs.position, buffer, size, &nbyte);
    return (rc == EOK) ? nbyte : -rc;
}

static ssize_t write_stdout(openedfile_t *file, void *buffer, size_t size) {
    assert(file->type == FILE_TYPE_STDOUTS);
    return fwrite(buffer, 1, size, *(file->stdio));
}

static ssize_t write_stdin(openedfile_t *file, void *buffer, size_t size) {
    assert(file->type == FILE_TYPE_STDIN);
    return -EBADF;
}

static ssize_t write_console(openedfile_t *file, void *buffer, size_t size) {
    assert(file->type == FILE_TYPE_CONSOLE);
    return fwrite(buffer, 1, size, file->console->ofile);
}

int openedfile_dup2(int fd1, int fd2) {
    if ((fd1 < 0) || (fd1 >= MAX_FILES) || (fd2 < 0) || (fd2 >= MAX_FILES)) {
        return -EBADF;
    }
    // TODO: vfs_put the file if it is not opened anywhere else
    files[fd2] = files[fd1];
    return fd2;
}

openedfile_t* openedfile_get_by_fd(int fd) {
    if ((fd < 0) || (fd >= MAX_FILES)) {
        return NULL;
    }
    openedfile_t *res = &files[fd];
    if (!res->used) {
        return NULL;
    }
    return res;
}

int openedfile_get_free_fd(openedfile_t **res) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            *res = &files[i];
            return i;
        }
    }
    return -1;
}

void openedfile_release(int fd) {
    if ((fd < 0) || (fd >= MAX_FILES)) {
        return;
    }

    files[fd].used = 0;
}

openedfile_ops_t openedfile_ops[] = {
    // FILE_TYPE_STDIN
    {
        .read = read_stdin,
        .write = write_stdin
    },
    // FILE_TYPE_STDOUTS
    {
        .read = read_stdout,
        .write = write_stdout
    },
    // FILE_TYPE_REGULAR_FILE
    {
        .read = read_regular_file,
        .write = write_regular_file,
    },
    // FILE_TYPE_CONSOLE
    {
        .read = read_console,
        .write = write_console,
    },
};

static console_input_t* cinput;

static bool is_service(FILE* file) {
    int fd = fileno(file);
    if (fd == EOF) {
        return false;
    }

    vfs_stat_t stat;
    if (vfs_stat(fd, &stat) != EOK) {
        return false;
    }

    return stat.service != 0;
}

static errno_t file_clone_open(FILE* file, int mode, const char* fmode, FILE** clone) {
    int fd = fileno(file);
    if (fd == EOF) {
        return errno;
    }

    int new_fd;
    errno_t err = vfs_clone(fd, -1, true, &new_fd);
    if (err != EOK) {
        return err;
    }

    err = vfs_open(new_fd, mode);
    if (err != EOK) {
        vfs_put(new_fd);
        return err;
    }

    *clone = fdopen(new_fd, fmode);
    if (*clone == NULL) {
        vfs_put(new_fd);
        return errno;
    }

    return EOK;
}

errno_t files_init(void) {
    bool stdin_service = is_service(stdin);
    bool stdout_service = is_service(stdout);

    // this code does not really work, when stdout is redirected by bdsh output is misaligned
    FILE *console_in, *console_out;
    if (stdin_service && stdout_service) {
        console_in = stdin;
        console_out = stdout;
    } else if (stdin_service && !stdout_service) {
        console_in = stdin;
        errno_t err = file_clone_open(stdin, MODE_WRITE, "w", &console_out);
        if (err != EOK) {
            return err;
        }
    } else if (!stdin_service && stdout_service) {
        console_out = stdout;
        errno_t err = file_clone_open(stdout, MODE_READ, "r", &console_in);
        if (err != EOK) {
            return err;
        }
    } else {
        // use stderr as fallback - it can't currently be redirected => will always be console write
        errno_t err = file_clone_open(stderr, MODE_READ, "r", &console_in);
        if (err != EOK) {
            return err;
        }

        err = file_clone_open(stderr, MODE_WRITE, "w", &console_out);
        if (err != EOK) {
            fclose(console_in);
            return err;
        }
    }

    cinput = console_input_new(console_in, console_out);
    if (cinput == NULL) {
        if (!stdin_service) {
            fclose(console_in);
        }

        if (!stdout_service) {
            fclose(console_out);
        }
        return ENOMEM;
    }

    openedfile_t _stdin = {.used = 1};
    if (stdin_service) {
        _stdin.type = FILE_TYPE_CONSOLE;
        _stdin.console = cinput;
    } else {
        _stdin.type = FILE_TYPE_STDIN;
        _stdin.stdio = &stdin;
    }

    openedfile_t _stdout = {.used = 1};
    if (stdout_service) {
        _stdout.type = FILE_TYPE_CONSOLE;
        _stdout.console = cinput;
    } else {
        _stdout.type = FILE_TYPE_STDOUTS;
        _stdout.stdio = &stdout;
    }

    // stdin
    files[0] = _stdin;

    // stdout
    files[1] = _stdout;

    // stderr
    files[2].used = 1;
    files[2].type = FILE_TYPE_STDOUTS;
    files[2].stdio = &stderr;

    return EOK;
}

void files_done(void) {
    console_input_destroy(cinput);
    // TODO: loop over open files and close them
    return;
}

/** @}
 */
