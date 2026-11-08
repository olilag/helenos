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

#include <stdio.h>
#include <errno.h>

#include "files.h"

#define MAX_FILES 16

static openedfile_t files[MAX_FILES] = {
	{
		.used = 1,
		.type = FILE_TYPE_STDIN,
		.stdio = &stdin
	},
	{
		.used = 1,
		.type = FILE_TYPE_STDOUTS,
		.stdio = &stdout
	},
	{
		.used = 1,
		.type = FILE_TYPE_STDOUTS,
		.stdio = &stderr
	}
};

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
	}
};



/** @}
 */
