#ifndef LILO_TERMINAL_H
#define LILO_TERMINAL_H

#include <adt/list.h>
#include <fibril_synch.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    link_t link;
    char* line;
    size_t head;
    size_t length;
} line_t;

typedef struct {
    list_t lines;
    fibril_condvar_t lines_cv;

    unsigned short cols;
    unsigned short rows;

    FILE* ofile;

    fibril_mutex_t mutex;
} tty_t;

errno_t terminal_init(tty_t* terminal, FILE* ifile, FILE* ofile);
line_t* terminal_get_line(tty_t* terminal);
void terminal_put_line(tty_t* terminal, line_t* line);
void terminal_get_size(tty_t* terminal, unsigned short* cols, unsigned short* rows);

#endif /* LILO_INPUT_H */
