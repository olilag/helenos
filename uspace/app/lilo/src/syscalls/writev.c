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
#include <stdio.h>
#include <vfs/vfs.h>
#include <errno.h>

#include "syscalls.h"
#include "files.h"

struct iovec {
	void *iov_base;
	size_t iov_len;
};

DEFINE_LINUX_SYSCALL3(writev, int, fd, const struct iovec *, iov, int, count)
{
	openedfile_t *file = openedfile_get_by_fd(fd);
	if (file == NULL) {
		return -EBADF;
	}
	
	int ret = 0;
	for (int i = 0; i < count; i++) {
		if (iov[i].iov_len == 0) {
			continue;
		}
		ssize_t written = openedfile_ops[file->type].write(file, iov[i].iov_base, iov[i].iov_len);
		if (written <= 0) {
			if (ret > 0) {
				// Managed to write at least something. We will report
				// the error next time.
				return ret;
			} else {
				return written;
			}
		}

		ret += written;
		if ((size_t) written != iov[i].iov_len) {
			// Not written the whole block, need to tell caller
			return ret;
		}
	}

	return ret;
}

/** @}
 */
