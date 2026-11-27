#include <stdio.h>
#include <stdlib.h>
#include <str.h>
#include <io/console.h>
#include <io/keycode.h>
#include <io/style.h>
#include <macros.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>

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

static void console_input_display_tail(console_input_t* ci, size_t start, size_t pad) {
    console_input_console_set_lpos(ci, ci->text_coord + start);
    console_set_style(ci->console, STYLE_NORMAL);

    ci->buffer[ci->nc] = L'\0';
    fprintf(ci->ofile, "%ls", &ci->buffer[start]);

    for (; pad > 0; pad--) {
        fputuc(' ', ci->ofile);
    }

    console_flush(ci->console);
}

static char* console_input_get_str(console_input_t* ci) {
    return wstr_to_astr(ci->buffer);
}

static void console_input_position_caret(console_input_t* ci) {
    console_input_update_origin(ci);
    console_input_console_set_lpos(ci, ci->text_coord + ci->pos);
}

static void console_input_update_origin_coord(console_input_t* ci, unsigned end_coord) {
    unsigned end_row = LIN_TO_ROW(ci, end_coord);
    unsigned scroll_rows;

    /* Update text origin if the screen scrolled. */
    if (end_row >= ci->con_rows) {
        scroll_rows = end_row - ci->con_rows + 1;
        ci->text_coord -= ci->con_cols * scroll_rows;
    }
}

/** Update text_coord in case the screen could have scrolled. */
static void console_input_update_origin(console_input_t* ci) {
    /* Account for scrolling until the end of the input text */
    console_input_update_origin_coord(ci, ci->text_coord + ci->nc);
}

static errno_t console_input_display(console_input_t* ci) {
    sysarg_t col0, row0;

    if (console_get_pos(ci->console, &col0, &row0) != EOK) {
        return EIO;
    }

    ci->text_coord = row0 * ci->con_cols + col0;
    console_input_display_tail(ci, 0, 0);

    /* The screen might have scrolled after printing the text. */
    console_input_update_origin(ci);
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

    size_t i;
    for (i = ci->nc; i > ci->pos; i--) {
        ci->buffer[i] = ci->buffer[i - 1];
    }

    ci->buffer[ci->pos] = c;
    ci->pos += 1;
    ci->nc += 1;
    ci->buffer[ci->nc] = '\0';

    console_input_display_tail(ci, ci->pos - 1, 0);
    console_input_position_caret(ci);
}

static void console_input_backspace(console_input_t* ci) {
    if (ci->pos == 0) {
        return;
    }

    size_t i;
    for (i = ci->pos; i < ci->nc; i++) {
        ci->buffer[i - 1] = ci->buffer[i];
    }

    ci->pos -= 1;
    ci->nc -= 1;
    ci->buffer[ci->nc] = '\0';

    console_input_display_tail(ci, ci->pos, 1);
    console_input_position_caret(ci);
}

static void console_input_delete(console_input_t* ci) {
    if (ci->pos == ci->nc) {
        return;
    }

    ci->pos += 1;

    console_input_backspace(ci);
}

static void console_input_post_seek(console_input_t* ci) {
    console_input_position_caret(ci);
}

static void console_input_seek_cell(console_input_t* ci, seek_dir_t dir) {
    if (dir == seek_forward) {
        if (ci->pos < ci->nc) {
            ci->pos += 1;
        }
    } else {
        if (ci->pos > 0) {
            ci->pos -= 1;
        }
    }

    console_input_post_seek(ci);
}

static void console_input_seek_word(console_input_t* ci, seek_dir_t dir) {
    if (dir == seek_forward) {
        if (ci->pos == ci->nc)
            return;

        while (true) {
            ci->pos += 1;

            if (ci->pos == ci->nc) {
                break;
            }

            if ((ci->buffer[ci->pos - 1] == ' ') &&
                (ci->buffer[ci->pos] != ' ')) {
                break;
            }
        }
    } else {
        if (ci->pos == 0) {
            return;
        }

        while (true) {
            ci->pos -= 1;

            if (ci->pos == 0) {
                break;
            }

            if (ci->buffer[ci->pos - 1] == ' ' &&
                ci->buffer[ci->pos] != ' ') {
                break;
            }
        }

    }

    console_input_post_seek(ci);
}

static void console_input_seek_vertical(console_input_t* ci, seek_dir_t dir) {
    if (dir == seek_forward) {
        if (ci->pos + ci->con_cols <= ci->nc) {
            ci->pos = ci->pos + ci->con_cols;
        }
    } else {
        if (ci->pos >= ci->con_cols) {
            ci->pos = ci->pos - ci->con_cols;
        }
    }

    console_input_post_seek(ci);
}

static void console_input_seek_scrpos(console_input_t* ci, int col, int line) {
    unsigned lpos = LIN_POS(ci, col, line);

    if (lpos > ci->text_coord) {
        ci->pos = lpos -  ci->text_coord;
    } else {
        ci->pos = 0;
    }
    if (ci->pos > ci->nc) {
        ci->pos = ci->nc;
    }
    console_input_post_seek(ci);
}

static void console_input_seek_max(console_input_t* ci, seek_dir_t dir) {
    if (dir == seek_backward) {
        ci->pos = 0;
    } else {
        ci->pos = ci->nc;
    }
    console_input_post_seek(ci);
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

/** Position event */
static void console_input_pos(console_input_t* ci, pos_event_t* ev) {
    if (ev->type == POS_PRESS) {
        console_input_seek_scrpos(ci, ev->hpos, ev->vpos);
    }
}

static errno_t console_input_resize(console_input_t* ci) {
    errno_t rc = console_get_size(ci->console, &ci->con_cols, &ci->con_rows);
    if (rc != EOK) {
        return rc;
    }

    sysarg_t col, row;
    rc = console_get_pos(ci->console, &col, &row);
    if (rc != EOK) {
        return rc;
    }

    size_t new_caret_coord = row * ci->con_cols + col;
    if (ci->pos <= new_caret_coord) {
        ci->text_coord = new_caret_coord - ci->pos;
    } else {
        /* Input no longer fits. Clear and redraw what fits. */
        console_clear(ci->console);
        ci->nc = min(ci->nc, ci->con_cols * ci->con_rows - 1);
        ci->pos = min(ci->pos, ci->nc);
        ci->text_coord = 0;
        console_cursor_visibility(ci->console, false);
        console_input_display_tail(ci, 0, 0);
        console_input_position_caret(ci);
        console_cursor_visibility(ci->console, true);
    }

    assert(ci->nc + ci->text_coord < ci->con_cols * ci->con_rows);
    return EOK;
}

errno_t console_input_read(console_input_t* ci, char** dstr) {
    errno_t rc;

    console_flush(ci->console);
    if (console_get_size(ci->console, &ci->con_cols, &ci->con_rows) != EOK) {
        return EIO;
    }

    ci->nc = 0;
    ci->pos = 0;

    ci->done = false;
    ci->exit = false;

    if (console_input_display(ci) != EOK) {
        return EIO;
    }

    while (!ci->done) {
        console_flush(ci->console);

        cons_event_t ev;
        rc = console_get_event(ci->console, &ev);
        if (rc != EOK) {
            return EIO;
        }

        switch (ev.type) {
        case CEV_KEY:
            if (ev.ev.key.type == KEY_PRESS)
                console_input_key_press(ci, &ev.ev.key);
            break;
        case CEV_POS:
            console_input_pos(ci, &ev.ev.pos);
            break;
        case CEV_RESIZE:
            console_input_resize(ci);
            break;
        }
    }

    ci->pos = ci->nc;
    console_input_position_caret(ci);
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
    case KC_LEFT:
        console_input_seek_word(ci, seek_backward);
        break;
    case KC_RIGHT:
        console_input_seek_word(ci, seek_forward);
        break;
    case KC_UP:
        console_input_seek_vertical(ci, seek_backward);
        break;
    case KC_DOWN:
        console_input_seek_vertical(ci, seek_forward);
        break;
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
    case KC_DELETE:
        console_input_delete(ci);
        break;
    case KC_LEFT:
        console_input_seek_cell(ci, seek_backward);
        break;
    case KC_RIGHT:
        console_input_seek_cell(ci, seek_forward);
        break;
    case KC_HOME:
        console_input_seek_max(ci, seek_backward);
        break;
    case KC_END:
        console_input_seek_max(ci, seek_forward);
        break;
    default:
        break;
    }
}
