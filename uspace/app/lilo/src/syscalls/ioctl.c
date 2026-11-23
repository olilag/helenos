#include "console_input.h"
#include "files.h"
#include "syscalls.h"

#define TIOCGWINSZ 21523

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;  /* unused */
    unsigned short ws_ypixel;  /* unused */
};

DEFINE_LINUX_SYSCALL3(ioctl, int, fd, unsigned long, request, void*, data) {
    long ret;

    openedfile_t* file = openedfile_get_by_fd(fd);
    if (file == NULL) {
        ret = -EBADF;
        goto finish;
    }

    if (file->type != FILE_TYPE_CONSOLE) {
        ret = -ENOTTY;
        goto finish;
    }

    if (request == TIOCGWINSZ) {
        struct winsize* winsize = data;
        sysarg_t col, row;
        console_input_get_size(file->console, &col, &row);
        winsize->ws_col = col;
        winsize->ws_row = row;
        ret = EOK;
        goto finish;
    }

    ret = -EINVAL;

finish:
    LOG_SYSCALL(ret, "%d, %lX", fd, request);
    return ret;
}
