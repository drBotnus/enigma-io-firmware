#pragma once

#include <stdint.h>
#include "esp_err.h"

#define MAKE_KEY_CODE(row, col) ((row << 8) | (col))
#define GET_KEY_CODE_ROW(code) ((code >> 8) & 0xFF)
#define GET_KEY_CODE_COL(code) (code & 0xFF)

typedef struct matrix_kbd_t *matrix_kbd_handle_t;

typedef enum {
        MATRIX_KBD_EVENT_DOWN,
        MATRIX_KBD_EVENT_UP,
} matrix_kbd_event_id_t;

typedef esp_err_t (*matrix_kbd_event_handler)(matrix_kbd_handle_t mkbd_handle, matrix_kbd_event_id_t event, void *event_data, void *handler_args);

typedef struct {
        const int *rows;
        const int *cols;
        uint32_t nrows;
        uint32_t ncols;
        uint32_t debounce_ms;
} matrix_kbd_config_t;

#define MATRIX_KEYBOARD_DEFAULT_CONFIG() \
{                                        \
        .rows = NULL,                    \
        .cols = NULL,                    \
        .nrows = 0,                      \
        .ncols = 0,                      \
        .debounce_ms = 20                \
}

esp_err_t matrix_kbd_init(const matrix_kbd_config_t *config, matrix_kbd_handle_t *mkbd_handle);
esp_err_t matrix_kbd_deinit(matrix_kbd_handle_t mkbd_handle);
esp_err_t matrix_kbd_start(matrix_kbd_handle_t mkbd_handle);
esp_err_t matrix_kbd_stop(matrix_kbd_handle_t mkbd_handle);
esp_err_t matrix_kbd_register_event_handler(matrix_kbd_handle_t mkbd_handle, matrix_kbd_event_handler handler, void *handler_args);