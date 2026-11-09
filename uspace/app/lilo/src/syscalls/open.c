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

#include <stddef.h>
#include <ipc/vfs.h>
#include <errno.h>

#include "consts.h"
#include "syscalls.h"
#include "files.h"

#define FLAG_IS_SET(where, flag) (((where) & (flag)) == (flag))

static sysarg_t do_open(const char* filename, int open_flags, int permissions) {
	//logger(LVL_DEBUG, "open(\"%s\", %d, %d)", filename, open_flags, permissions);

	openedfile_t *file = NULL;
	int fd = openedfile_get_free_fd(&file);
	if (fd < 0) {
		return -EMFILE;
	}

	int lookup_flags = WALK_REGULAR;
	if (open_flags & LINUX_O_CREAT) {
		if (open_flags & LINUX_O_EXCL) {
			lookup_flags |= WALK_MUST_CREATE;
		} else {
			lookup_flags |= WALK_MAY_CREATE;
		}
	}

	errno_t rc = vfs_lookup(filename, lookup_flags, &file->vfs.handle);
	if (rc != EOK) {
		// DPRINTF("Lookup %s failed!\n", filename);
		return -rc;
	}

	int open_mode =
	    ((open_flags & LINUX_O_RDWR) ? MODE_READ | MODE_WRITE : 0) |
	    ((open_flags & LINUX_O_RDONLY) ? MODE_READ : 0) |
	    ((open_flags & LINUX_O_WRONLY) ? MODE_WRITE : 0) |
	    ((open_flags & LINUX_O_APPEND) ? MODE_APPEND : 0);
	if ((open_mode == 0) & (FLAG_IS_SET(open_flags, LINUX_O_RDONLY))) {
		open_mode = MODE_READ;
	}

	rc = vfs_open(file->vfs.handle, open_mode);
	if (rc != EOK) {
		vfs_put(file->vfs.handle);
		// DPRINTF("Opening %s failed!\n", filename);
		return -rc;
	}

	if (open_flags & LINUX_O_TRUNC) {
		if (open_flags & (LINUX_O_RDWR | LINUX_O_WRONLY)) {
			rc = vfs_resize(file->vfs.handle, 0);
			if (rc != EOK) {
				vfs_put(file->vfs.handle);
				return -rc;
			}
		}
	}


	file->vfs.position = 0;
	file->type = FILE_TYPE_REGULAR_FILE;

	// Last operation: mark the file as used
	file->used = 1;

	return fd;
}

DEFINE_LINUX_SYSCALL3(open, const char *, filename, int, open_flags, int, permissions) {
	const long ret = do_open(filename, open_flags, permissions);
	LOG_SYSCALL(ret, "%s, %X, %X", filename, open_flags, permissions);
	return ret;
}

DEFINE_LINUX_SYSCALL4(openat, int, fd, const char *, filename, int, open_flags, int, permissions) {
    const long ret = do_open(filename, open_flags, permissions);
    LOG_SYSCALL(ret, "%d, %s, %X, %X", fd, filename, open_flags, permissions);
	return ret;
}

/** @}
 */
