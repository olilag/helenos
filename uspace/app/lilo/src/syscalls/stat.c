#include <errno.h>
#include <mem.h>
#include <vfs/vfs.h>

#include "syscalls.h"

struct stat {
    unsigned long st_dev;
	unsigned long st_ino;
	unsigned long st_nlink;

	unsigned int st_mode;
	unsigned int st_uid;
	unsigned int st_gid;
	unsigned int __pad0;
	unsigned long st_rdev;
	long st_size;
	long st_blksize;
	long st_blocks;	/* Number 512-byte blocks allocated. */

	unsigned long st_atime;
	unsigned long st_atime_nsec;
	unsigned long st_mtime;
	unsigned long st_mtime_nsec;
	unsigned long st_ctime;
	unsigned long st_ctime_nsec;
	long _unused[3];
};

#define S_IFMT     0170000   /* all file types */
#define S_IFSOCK   0140000   /* socket */
#define S_IFLNK    0120000   /* symbolic link */
#define S_IFREG    0100000   /* regular file */
#define S_IFBLK    0060000   /* block device */
#define S_IFDIR    0040000   /* directory */
#define S_IFCHR    0020000   /* character device */
#define S_IFIFO    0010000   /* FIFO */

#define S_ISUID    0004000   /* SUID */
#define S_ISGID    0002000   /* SGID */
#define S_ISVTX    0001000   /* sticky */

#define S_IRWXU    00700     /* owner permissions */
#define S_IRUSR    00400
#define S_IWUSR    00200
#define S_IXUSR    00100

#define S_IRWXG    00070     /* group permissions */
#define S_IRGRP    00040
#define S_IWGRP    00020
#define S_IXGRP    00010

#define S_IRWXO    00007     /* other permissions */
#define S_IROTH    00004
#define S_IWOTH    00002
#define S_IXOTH    00001

#define S_ISREG(m) ((m & S_IFREG) != 0)
#define S_ISDIR(m) ((m & S_IFDIR) != 0)
#define S_ISCHR(m) ((m & S_IFCHR) != 0)
#define S_ISBLK(m) ((m & S_IFBLK) != 0)
#define S_ISFIFO(m) ((m & S_IFIFO) != 0)
#define S_ISLNK(m) ((m & S_IFLNK) != 0) /* symbolic link? (Not in POSIX.1-1996.) */
#define S_ISSOCK(m) ((m & S_IFSOCK) != 0) /* socket? (Not in POSIX.1-1996.) */

static int stat_to_posix(struct stat *dest, const vfs_stat_t *src) {
	memset(dest, 0, sizeof(struct stat));

	dest->st_dev = src->service;
	dest->st_ino = src->index;

	/* HelenOS doesn't support permissions, so we set them all */
	dest->st_mode = S_IRWXU | S_IRWXG | S_IRWXO;
	if (src->is_file) {
		dest->st_mode |= S_IFREG;
	}
	if (src->is_directory) {
		dest->st_mode |= S_IFDIR;
	}

	dest->st_nlink = src->lnkcnt;
	dest->st_size = src->size;

	if (src->size > INT64_MAX) {
		return -ERANGE;
	}

	return 0;
}

DEFINE_LINUX_SYSCALL2(stat, char*, filename, struct stat*, stat) {
    long ret = 0;

    vfs_stat_t hst;
    ret = vfs_stat_path(filename, &hst);
    if (ret != EOK) {
        goto finish;
    }

    ret = stat_to_posix(stat, &hst);

finish:
    LOG_SYSCALL("%s, %p", filename, stat, ret);
    return ret;
}

DEFINE_LINUX_SYSCALL2(fstat, int, fd, void*, stat)
{
    long ret = 0;

    vfs_stat_t hst;
    ret = vfs_stat(fd, &hst);
    if (ret != EOK) {
        goto finish;
    }

    ret = stat_to_posix(stat, &hst);

finish:
    LOG_SYSCALL("%d, %p", fd, stat, ret);
    return ret;
}

DEFINE_LINUX_SYSCALL2(lstat, char*, filename, void*, stat) {
    long ret = 0;

    vfs_stat_t hst;
    ret = vfs_stat_path(filename, &hst);
    if (ret != EOK) {
        goto finish;
    }

    ret = stat_to_posix(stat, &hst);

finish:
    LOG_SYSCALL("%s, %p", filename, stat, ret);
    return ret;
}
