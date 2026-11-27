#ifndef LILO_CONSOLE_INPUT_H
#define LILO_CONSOLE_INPUT_H

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <io/console.h>

#define INPUT_MAX_SIZE 1024

typedef void(*on_resize_cb_t)(sysarg_t, sysarg_t, void*);

typedef struct {
    console_ctrl_t* console;
    FILE* ofile;
    char32_t buffer[INPUT_MAX_SIZE + 2];
    unsigned text_coord;
    sysarg_t con_cols;
    sysarg_t con_rows;
    size_t nc;
    bool done;
    bool exit;

    on_resize_cb_t on_resize;
    void* on_resize_arg;
} console_input_t;

console_input_t* console_input_new(FILE* ifile, FILE* ofile);
void console_input_destroy(console_input_t* console_input);
errno_t console_input_read(console_input_t* console_input, char** line);
void console_input_get_size(console_input_t* console_input, sysarg_t* cols, sysarg_t* rows);

#endif /* LILO_INPUT_H */
