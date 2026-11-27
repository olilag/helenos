#include "console_input.h"
#include "files.h"
#include "syscalls.h"
#include "terminal.h"

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
        terminal_get_size(file->terminal, &winsize->ws_col, &winsize->ws_row);
        ret = EOK;
        LOG_SYSCALL_SUCCESS(ret, "%d, TIOCGWINSZ, {ws_col=%u, ws_row=%u, ws_xpixel=%u, ws_ypixel=%u}", fd, winsize->ws_col, winsize->ws_row, winsize->ws_xpixel, winsize->ws_ypixel);
        goto finish;
    }

    ret = -EINVAL;

finish:
    LOG_SYSCALL_ERROR(ret, "%d, %lX", fd, request);
    return ret;
}
