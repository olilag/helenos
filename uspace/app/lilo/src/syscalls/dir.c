#include <align.h>
#include <dirent.h>
#include <errno.h>
#include <mem.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <str.h>
#include <str_error.h>
#include <vfs/vfs.h>

#include "files.h"
#include "syscalls.h"

DEFINE_LINUX_SYSCALL2(mkdir, const char*, path, unsigned short, mode) {
    int fd;
    long ret = vfs_link_path(path, KIND_DIRECTORY, &fd);
    if (ret != EOK) {
        ret = -ret;
    } else {
        vfs_put(fd);
    }
    LOG_SYSCALL(ret, "%s, %u", path, mode);
    return ret;
}

DEFINE_LINUX_SYSCALL1(rmdir, const char*, path) {
    long ret = vfs_unlink_path(path);
    if (ret != EOK) {
        ret = -ret;
    }
    LOG_SYSCALL(ret, "%s", path);
    return ret;
}

DEFINE_LINUX_SYSCALL1(chdir, const char*, path) {
    long ret = vfs_cwd_set(path);
    if (ret != EOK) {
        ret = -ret;
    }
    LOG_SYSCALL(ret, "%s", path);
    return ret;
}

struct linux_dirent64 {
    int64_t        d_ino;    /* 64-bit inode number */
    off64_t         d_off;    /* Not an offset; see getdents() */
    unsigned short d_reclen; /* Size of this dirent */
    unsigned char  d_type;   /* File type */
    char           d_name[]; /* Filename (null-terminated) */
};

struct __dirstream {
    int fd;
    struct dirent res;
    aoff64_t pos;
};

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14

DEFINE_LINUX_SYSCALL3(getdents64, unsigned int, fd, uint8_t*, buf, size_t, count) {
    long ret = 0;

    int vfs_handle;
    openedfile_t *file = openedfile_get_by_fd(fd);
    if (file == NULL) {
        ret = -EBADF;
        goto finish;
    }

    if (file->type != FILE_TYPE_REGULAR_FILE) {
        ret = -ENOTDIR;
        goto finish;
    }

    vfs_handle = file->vfs.handle;
    errno_t err;
    vfs_stat_t stat;
    err = vfs_stat(vfs_handle, &stat);
    if (err != EOK) {
        ret = -ENOENT;
        goto finish;
    }
    if (!stat.is_directory) {
        ret = -ENOTDIR;
        goto finish;
    }

    struct dirent *dp;
    char path_buff[257] = "/";
    int child_fd;
    size_t byte = 0;
    DIR dirp = {.fd = vfs_handle, .pos = file->vfs.position};
    while (byte < count) {
        size_t previous_pos = dirp.pos;
        dp = readdir(&dirp);
        if (!dp) {
            break;
        }

        str_cpy(&path_buff[1], 256, dp->d_name);

        err = vfs_walk(vfs_handle, path_buff, 0, &child_fd);
        if (err != EOK)
            continue;

        err = vfs_stat(child_fd, &stat);
        if (err != EOK) {
            vfs_put(child_fd);
            continue;
        }

        size_t name_length = str_size(dp->d_name) + 1;
        size_t reclen = ALIGN_UP(offsetof(struct linux_dirent64, d_name) + name_length, 8);
        if (reclen > count - byte) {
            vfs_put(child_fd);
            dirp.pos = previous_pos;

            if (byte == 0) {
                ret = -EINVAL;
                goto finish;
            }

            goto success;
        }

        struct linux_dirent64 *dirent = (struct linux_dirent64 *)&buf[byte];
        dirent->d_ino = stat.index;
        dirent->d_off = dirp.pos;
        dirent->d_reclen = reclen;
        dirent->d_type = stat.is_directory ? DT_DIR :
                         stat.is_file ? DT_REG : DT_UNKNOWN;
        memcpy(dirent->d_name, dp->d_name, name_length);

        byte += reclen;
        vfs_put(child_fd);
    }

    // for (uint8_t* ptr = buf; ptr < buf + byte;) {
    //     struct linux_dirent64* dirent = (struct linux_dirent64*)ptr;
    //     printf("{d_ino=%ld, d_off=%lu, d_reclen=%u, d_type=%u, d_name=%s}\n", dirent->d_ino, dirent->d_off, dirent->d_reclen, dirent->d_type, dirent->d_name);
    //     ptr += dirent->d_reclen;
    // }

success:
    ret = byte;
    file->vfs.position = dirp.pos;
finish:
    LOG_SYSCALL(ret, "%u, %p, %lu", fd, buf, count);
    return ret;
}
