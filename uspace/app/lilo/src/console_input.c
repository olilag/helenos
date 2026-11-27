#include <stdio.h>
#include <stdlib.h>
#include <str.h>
#include <io/console.h>
#include <io/keycode.h>
#include <macros.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <timeint.h>

#include "console_input.h"

#define LIN_TO_COL(ti, lpos) ((lpos) % ((ti)->con_cols))
#define LIN_TO_ROW(ti, lpos) ((lpos) / ((ti)->con_cols))
#define LIN_POS(ti, col, row) ((col) + (row) * (ti)->con_cols)

static void console_input_update_origin(console_input_t*);
static void console_input_key_ctrl(console_input_t*, kbd_event_t*);
static void console_input_key_unmod(console_input_t*, kbd_event_t*);

typedef enum {
    seek_backward = -1,
    seek_forward = 1
} seek_dir_t;

static void console_input_console_set_lpos(console_input_t* ci, unsigned lpos) {
    unsigned col = LIN_TO_COL(ci, lpos);
    unsigned row = LIN_TO_ROW(ci, lpos);

    assert(col < ci->con_cols);
    assert(row < ci->con_rows);
    console_set_pos(ci->console, col, row);
}

console_input_t* console_input_new(FILE* ifile, FILE* ofile) {
    console_input_t* ci = calloc(1, sizeof(console_input_t));
    if (ci == NULL) {
        return NULL;
    }

    ci->console = console_init(ifile, ofile);
    if (ci->console == NULL) {
        free(ci);
        return NULL;
    }
    ci->ofile = ofile;

    console_flush(ci->console);
    if (console_get_size(ci->console, &ci->con_cols, &ci->con_rows) != EOK) {
        console_done(ci->console);
        free(ci);
        return NULL;
    }

    return ci;
}

void console_input_destroy(console_input_t* ci) {
    console_done(ci->console);
    free(ci);
}

static char* console_input_get_str(console_input_t* ci) {
    return wstr_to_astr(ci->buffer);
}

static void console_input_position_caret(console_input_t* ci) {
    console_input_update_origin(ci);
    console_input_console_set_lpos(ci, ci->text_coord + ci->nc);
}

static void console_input_update_origin_coord(console_input_t* ci, unsigned end_coord) {
    unsigned end_row = LIN_TO_ROW(ci, end_coord);

    /* Update text origin if the screen scrolled. */
    if (end_row >= ci->con_rows) {
        unsigned scroll_rows = end_row - ci->con_rows + 1;
        ci->text_coord -= ci->con_cols * scroll_rows;
    }
}

/** Update text_coord in case the screen could have scrolled. */
static void console_input_update_origin(console_input_t* ci) {
    /* Account for scrolling until the end of the input text */
    console_input_update_origin_coord(ci, ci->text_coord + ci->nc);
}

static errno_t console_input_update_pos(console_input_t* ci) {
    sysarg_t col0, row0;

    if (console_get_pos(ci->console, &col0, &row0) != EOK) {
        return EIO;
    }

    ci->text_coord = row0 * ci->con_cols + col0 - ci->nc;

    console_input_position_caret(ci);

    return EOK;
}

static void console_input_insert_char(console_input_t* ci, char32_t c) {
    if (ci->nc == INPUT_MAX_SIZE) {
        return;
    }

    /* Disallow text longer than 1 page for now. */
    if (ci->text_coord + ci->nc + 1 >= ci->con_cols * ci->con_rows) {
        return;
    }

    ci->buffer[ci->nc] = c;
    ci->nc += 1;
    ci->buffer[ci->nc] = '\0';

    fputuc(c, ci->ofile);
    console_flush(ci->console);
    console_input_position_caret(ci);
}

static void console_input_backspace(console_input_t* ci) {
    if (ci->nc == 0) {
        return;
    }

    ci->nc -= 1;
    ci->buffer[ci->nc] = '\0';

    console_input_console_set_lpos(ci, ci->text_coord + ci->nc);
    fputuc(' ', ci->ofile);
    console_flush(ci->console);
    console_input_position_caret(ci);
}

/** Handle key press event. */
static void console_input_key_press(console_input_t* ci, kbd_event_t* kev) {
    if (((kev->mods & KM_CTRL) != 0) &&
        ((kev->mods & (KM_ALT | KM_SHIFT)) == 0)) {
        console_input_key_ctrl(ci, kev);
    } else if ((kev->mods & (KM_CTRL | KM_ALT)) == 0) {
        console_input_key_unmod(ci, kev);
    }

    if (((kev->mods & (KM_CTRL | KM_ALT)) == 0) && kev->c >= ' ') {
        console_input_insert_char(ci, kev->c);
    }
}

static errno_t console_input_resize(console_input_t* ci) {
    errno_t rc = console_get_size(ci->console, &ci->con_cols, &ci->con_rows);
    if (rc != EOK) {
        return rc;
    }

    if (ci->on_resize != NULL) {
        ci->on_resize(ci->con_cols, ci->con_rows, ci->on_resize_arg);
    }

    sysarg_t col, row;
    rc = console_get_pos(ci->console, &col, &row);
    if (rc != EOK) {
        return rc;
    }

    size_t new_caret_coord = row * ci->con_cols + col;
    if (ci->nc <= new_caret_coord) {
        ci->text_coord = new_caret_coord;
    } else {
        /* Input no longer fits. Clear and redraw what fits. */
        console_clear(ci->console);
        ci->nc = min(ci->nc, ci->con_cols * ci->con_rows - 1);
        ci->text_coord = 0;
        console_cursor_visibility(ci->console, false);
        console_input_console_set_lpos(ci, ci->text_coord);
        ci->buffer[ci->nc] = '\0';
        fprintf(ci->ofile, "%ls", ci->buffer);
        console_flush(ci->console);
        console_input_position_caret(ci);
        console_cursor_visibility(ci->console, true);
    }

    assert(ci->nc + ci->text_coord < ci->con_cols * ci->con_rows);
    return EOK;
}

errno_t console_input_read(console_input_t* ci, char** dstr) {
    errno_t rc;

    ci->nc = 0;
    ci->buffer[ci->nc] = '\0';

    ci->done = false;
    ci->exit = false;

    while (!ci->done) {
        console_flush(ci->console);
        if (console_input_update_pos(ci) != EOK) {
            return EIO;
        }

        cons_event_t ev;
        usec_t timeout = MSEC2USEC(250);
        rc = console_get_event_timeout(ci->console, &ev, &timeout);
        if (rc == ETIMEOUT) {
            continue;
        }
        if (rc != EOK) {
            return EIO;
        }

        switch (ev.type) {
        case CEV_KEY:
            if (ev.ev.key.type == KEY_PRESS)
                console_input_key_press(ci, &ev.ev.key);
            break;
        case CEV_RESIZE:
            console_input_resize(ci);
            break;
        default:
            break;
        }
    }

    fputuc('\n', ci->ofile);
    console_flush(ci->console);

    if (ci->exit) {
        return ENOENT;
    }

    char* str = console_input_get_str(ci);
    if (str == NULL) {
        return ENOMEM;
    }

    *dstr = str;
    return EOK;
}

void console_input_get_size(console_input_t* ci, sysarg_t* cols, sysarg_t* rows) {
    *cols = ci->con_cols;
    *rows = ci->con_rows;
}

static void console_input_key_ctrl(console_input_t* ci, kbd_event_t* ev) {
    switch (ev->key) {
    case KC_D:
        /* Signal libary client to quit interactive loop. */
        ci->done = true;
        break;
    case KC_C:
        /* Signal libary client to quit interactive loop. */
        ci->done = true;
        ci->exit = true;
        break;
    default:
        break;
    }
}

static void console_input_key_unmod(console_input_t* ci, kbd_event_t* ev) {
    switch (ev->key) {
    case KC_ENTER:
    case KC_NENTER:
        ci->buffer[ci->nc] = '\n';
        ci->nc++;
        ci->buffer[ci->nc] = '\0';
        ci->done = true;
        break;
    case KC_BACKSPACE:
        console_input_backspace(ci);
        break;
    default:
        break;
    }
}
