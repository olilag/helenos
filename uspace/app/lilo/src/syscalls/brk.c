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
#include <as.h>
#include <errno.h>

#include "syscalls.h"
#include "files.h"

uint8_t *break_area = NULL;
size_t break_area_size = 0;

#define BRK_ERROR ((sysarg_t) -1)
#define BRK_OK ((sysarg_t) (break_area + break_area_size))


DEFINE_LINUX_SYSCALL1(brk, const uint8_t *, addr)
{
	long ret;
	if (addr == NULL) {
		if (break_area == NULL) {
			break_area = as_area_create(AS_AREA_ANY, 4096, AS_AREA_READ | AS_AREA_WRITE | AS_AREA_CACHEABLE, 0);
			if (break_area == AS_MAP_FAILED) {
				break_area = NULL;
				return BRK_ERROR;
			}
			break_area_size = 4096;
		}

		ret = BRK_OK;
		goto finish;
	}

	if (addr < break_area) {
		ret = BRK_ERROR;
		goto finish;
	}

	size_t new_size = addr - break_area;
	if (new_size <= break_area_size) {
		ret = BRK_OK;
		goto finish;
	}

	int rc = as_area_resize(break_area, new_size, 0);
	if (rc != EOK) {
		ret = BRK_ERROR;
		goto finish;
	}

	break_area_size = new_size;

finish:
	ret = BRK_OK;
	LOG_SYSCALL("%p", addr, ret);
	return ret;
}

/** @}
 */
