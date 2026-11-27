#include <adt/list.h>
#include <assert.h>
#include <fibril.h>
#include <fibril_synch.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <str.h>
#include <task.h>

#include "console_input.h"
#include "terminal.h"

typedef struct {
    tty_t* terminal;
    console_input_t* input;
} reader_args_t;

static void on_console_resize(sysarg_t cols, sysarg_t rows, void* arg) {
    tty_t* terminal = arg;

    fibril_mutex_lock(&terminal->mutex);

    terminal->cols = cols;
    terminal->rows = rows;

    fibril_mutex_unlock(&terminal->mutex);
}

static errno_t input_reader(void* arg) {
    reader_args_t* args = arg;
    tty_t* terminal = args->terminal;
    console_input_t* input = args->input;
    char* line;
    errno_t err;

    input->on_resize = on_console_resize;
    input->on_resize_arg = terminal;

    while (true) {
        err = console_input_read(input, &line);
        if (err == ENOENT) {
            // TODO: handle Ctrl+C
            printf("Ctrl+C pressed, exiting...\n");
            task_kill(task_get_id());
            return EOK;
        } else if (err != EOK) {
            // TODO: end program
            printf("read failed, exiting...\n");
            task_kill(task_get_id());
            return err;
        }

        line_t* item = malloc(sizeof(line_t));
        if (item == NULL) {
            // TODO: end program?
            free(line);
            return ENOMEM;
        }

        link_initialize(&item->link);
        item->line = line;
        item->head = 0;
        item->length = str_length(line);

        fibril_mutex_lock(&terminal->mutex);

        list_append(&item->link, &terminal->lines);
        fibril_condvar_signal(&terminal->lines_cv);

        fibril_mutex_unlock(&terminal->mutex);
    }

    return EOK;
}

static reader_args_t arg;

errno_t terminal_init(tty_t* terminal, FILE* ifile, FILE* ofile) {
    console_input_t* cinput = console_input_new(ifile, ofile);
    if (cinput == NULL) {
        return ENOMEM;
    }

    list_initialize(&terminal->lines);
    fibril_condvar_initialize(&terminal->lines_cv);
    fibril_mutex_initialize(&terminal->mutex);
    sysarg_t cols, rows;
    console_input_get_size(cinput, &cols, &rows);
    terminal->cols = cols;
    terminal->rows = rows;
    terminal->ofile = ofile;

    arg.terminal = terminal;
    arg.input = cinput;
    fid_t input_fibril =  fibril_create(input_reader, &arg);
    if (input_fibril == NULL) {
        console_input_destroy(cinput);
        return ENOMEM;
    }

    fibril_start(input_fibril);

    return EOK;
}

line_t* terminal_get_line(tty_t* terminal) {
    fibril_mutex_lock(&terminal->mutex);

    if (list_empty(&terminal->lines)) {
        fibril_condvar_wait(&terminal->lines_cv, &terminal->mutex);
    }

    line_t* line = list_get_instance(list_first(&terminal->lines), line_t, link);
    return line;
}

void terminal_put_line(tty_t* terminal, line_t* line) {
    assert(fibril_mutex_is_locked(&terminal->mutex));
    assert(line == list_get_instance(list_first(&terminal->lines), line_t, link));

    if (line->head == line->length) {
        list_remove(&line->link);
        free(line->line);
        free(line);
    }

    fibril_mutex_unlock(&terminal->mutex);
}

void terminal_get_size(tty_t* terminal, unsigned short* cols, unsigned short* rows) {
    fibril_mutex_lock(&terminal->mutex);

    *cols = terminal->cols;
    *rows = terminal->rows;

    fibril_mutex_unlock(&terminal->mutex);
}
