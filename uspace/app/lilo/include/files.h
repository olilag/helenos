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

#ifndef _LIBINUX_FILES_H
#define _LIBINUX_FILES_H

#include <vfs/vfs.h>

typedef enum {
	FILE_TYPE_STDIN = 0,
	FILE_TYPE_STDOUTS = 1,
	FILE_TYPE_REGULAR_FILE = 2,
	FILE_TYPE_LAST,
} file_type_t;

typedef struct {
	int handle;
	aoff64_t position;
} regular_openedfile_t;

typedef struct openedfile openedfile_t;

typedef struct {
    ssize_t (*read)(openedfile_t *, void *, size_t);
    ssize_t (*write)(openedfile_t *, void *, size_t);
} openedfile_ops_t;

struct openedfile {
	int used;
	file_type_t type;
	union {
		FILE** stdio;
		regular_openedfile_t vfs;
	};
};

extern openedfile_t* openedfile_get_by_fd(int);
int openedfile_dup2(int fd1, int fd2);
extern int openedfile_get_free_fd(openedfile_t **);
extern void openedfile_release(int);

extern openedfile_ops_t openedfile_ops[];

extern openedfile_ops_t openedfile_ops_regularfile;
extern openedfile_ops_t openedfile_ops_stdin;
extern openedfile_ops_t openedfile_ops_stdout;

#endif

/** @}
 */
