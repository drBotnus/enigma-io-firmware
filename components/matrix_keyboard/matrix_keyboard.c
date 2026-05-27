#include "esp_err.h"
#include "matrix_keyboard.h"

#include "esp_log.h"
#include "driver/dedic_gpio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "matrix_kbd";

typedef struct matrix_kbd_t matrix_kbd_t;

struct matrix_kbd_event_data_t {
        uint32_t row;
        uint32_t col;
};

struct matrix_kbd_t {
        // Row Group
        gpio_num_t *rows;
        // Column Group
        gpio_num_t *cols;
        uint32_t nrows;
        uint32_t ncols;

        // Debounce Timer
        TimerHandle_t timer;
        // Handler function provided by initializer (in function pointer)
        matrix_kbd_event_handler event_handler;
        // Additional arguments to pass to matrix_kbd_t.event_handler
        void *event_handler_args;

        uint32_t pending_row_state;
        uint32_t pending_col_state;
        uint32_t stable_row_state;
        uint32_t stable_col_state;

        bool running;
};

static uint32_t read_gpio_group(const gpio_num_t *gpios, const uint32_t count)
{
        uint32_t state = 0;

        for (uint32_t i = 0; i < count; i++) {
                if (gpio_get_level(gpios[i])) {
                        state |= (1U << i);
                }
        }

        return state;
}

static void debounce_timer_cb(TimerHandle_t xTimer)
{
        matrix_kbd_t *mkbd = (matrix_kbd_t *)pvTimerGetTimerID(xTimer);

        if (!mkbd) {
                return;
        }

        const uint32_t row_state = read_gpio_group(mkbd->rows, mkbd->nrows);

        const uint32_t col_state = read_gpio_group(mkbd->cols, mkbd->ncols);

        if (row_state != mkbd->pending_row_state ||
            col_state != mkbd->pending_col_state) {
                return;
        }

        const uint32_t changed_rows = row_state ^ mkbd->stable_row_state;
        const uint32_t changed_cols = col_state ^ mkbd->stable_col_state;

        if (!changed_rows && !changed_cols) {
                return;
        }

        for (uint32_t r = 0; r < mkbd->nrows; r++) {
                if (!(changed_rows & (1U << r))) {
                        continue;
                }

                for (uint32_t c = 0; c < mkbd->ncols; c++) {
                        if (!(changed_cols & (1U << c))) {
                                continue;
                        }

                        bool pressed = (row_state & (1U << r)) &&
                                       (col_state & (1U << c));

                        struct matrix_kbd_event_data_t event_data = {
                                .row = r,
                                .col = c,
                        };

                        const matrix_kbd_event_id_t event =
                                pressed ? MATRIX_KBD_EVENT_DOWN :
                                          MATRIX_KBD_EVENT_UP;

                        ESP_LOGD(TAG,
                                 "row_state=0x%08" PRIx32
                                 " col_state=0x%08" PRIx32,
                                 row_state, col_state);
                        ESP_LOGD(TAG,
                                 "changed_row_state=0x%08" PRIx32
                                 " changed_col_state=0x%08" PRIx32,
                                 changed_rows, changed_cols);

                        if (mkbd->event_handler) {
                                mkbd->event_handler(mkbd, event, &event_data,
                                                    mkbd->event_handler_args);
                        }
                }
        }

        // Commit stable state only after debounce success
        mkbd->stable_row_state = row_state;
        mkbd->stable_col_state = col_state;
}

esp_err_t matrix_kbd_trigger_debounce(matrix_kbd_handle_t mkbd_handle)
{
        if (!mkbd_handle) {
                return ESP_ERR_INVALID_ARG;
        }

        matrix_kbd_t *mkbd = mkbd_handle;

        mkbd->pending_row_state = read_gpio_group(mkbd->rows, mkbd->nrows);
        mkbd->pending_col_state = read_gpio_group(mkbd->cols, mkbd->ncols);

        const BaseType_t ok = xTimerReset(mkbd->timer, 0);

        return ok == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t matrix_kbd_init(const matrix_kbd_config_t *config,
                          matrix_kbd_handle_t *mkbd_handle)
{
        if (!config || !mkbd_handle) {
                return ESP_ERR_INVALID_ARG;
        }

        if (!config->rows || !config->cols || !config->nrows ||
            !config->ncols) {
                return ESP_ERR_INVALID_ARG;
        }

        matrix_kbd_t *mkbd = calloc(1, sizeof(matrix_kbd_t));

        if (!mkbd) {
                return ESP_ERR_NO_MEM;
        }

        mkbd->nrows = config->nrows;
        mkbd->ncols = config->ncols;

        mkbd->rows = calloc(config->nrows, sizeof(gpio_num_t));
        mkbd->cols = calloc(config->ncols, sizeof(gpio_num_t));

        if (!mkbd->rows || !mkbd->cols) {
                free(mkbd->rows);
                free(mkbd->cols);
                free(mkbd);
                return ESP_ERR_NO_MEM;
        }

        for (uint32_t i = 0; i < config->nrows; i++) {
                mkbd->rows[i] = config->rows[i];
        }

        for (uint32_t i = 0; i < config->ncols; i++) {
                mkbd->cols[i] = config->cols[i];
        }

        for (uint32_t i = 0; i < config->nrows; i++) {
                gpio_config_t io_conf = {
                        .pin_bit_mask = (1ULL << config->rows[i]),

                        .mode = GPIO_MODE_INPUT,

                        .pull_up_en = GPIO_PULLUP_ENABLE,

                        .pull_down_en = GPIO_PULLDOWN_DISABLE,

                        .intr_type = GPIO_INTR_DISABLE,
                };

                ESP_ERROR_CHECK(gpio_config(&io_conf));
        }

        for (uint32_t i = 0; i < config->ncols; i++) {
                gpio_config_t io_conf = {
                        .pin_bit_mask = (1ULL << config->cols[i]),

                        .mode = GPIO_MODE_INPUT,

                        .pull_up_en = GPIO_PULLUP_ENABLE,

                        .pull_down_en = GPIO_PULLDOWN_DISABLE,

                        .intr_type = GPIO_INTR_DISABLE,
                };

                ESP_ERROR_CHECK(gpio_config(&io_conf));
        }

        mkbd->running = false;

        mkbd->timer = xTimerCreate("matrix_db",
                                   pdMS_TO_TICKS(config->debounce_ms), pdFALSE,
                                   mkbd, debounce_timer_cb);

        if (!mkbd->timer) {
                free(mkbd->rows);
                free(mkbd->cols);
                free(mkbd);
                return ESP_ERR_NO_MEM;
        }

        // Initialize stable state
        mkbd->stable_row_state = read_gpio_group(mkbd->rows, mkbd->nrows);

        mkbd->stable_col_state = read_gpio_group(mkbd->cols, mkbd->ncols);

        mkbd->pending_row_state = mkbd->stable_row_state;

        mkbd->pending_col_state = mkbd->stable_col_state;

        *mkbd_handle = mkbd;

        ESP_LOGI(TAG, "initialized matrix detector (%" PRIu32 "x%" PRIu32 ")",
                 mkbd->nrows, mkbd->ncols);

        return ESP_OK;
}

esp_err_t matrix_kbd_deinit(matrix_kbd_handle_t mkbd_handle)
{
        if (!mkbd_handle) {
                return ESP_ERR_INVALID_ARG;
        }

        matrix_kbd_t *mkbd = mkbd_handle;

        mkbd->running = false;

        if (mkbd->timer) {
                xTimerStop(mkbd->timer, portMAX_DELAY);
                xTimerDelete(mkbd->timer, portMAX_DELAY);
        }

        free(mkbd->rows);
        free(mkbd->cols);

        free(mkbd);

        return ESP_OK;
}

esp_err_t matrix_kbd_start(matrix_kbd_handle_t mkbd_handle)
{
        if (!mkbd_handle) {
                return ESP_ERR_INVALID_ARG;
        }

        matrix_kbd_t *mkbd = mkbd_handle;

        mkbd->running = true;

        // Take initial snapshot so first edge isn't noisy
        mkbd->stable_row_state = read_gpio_group(mkbd->rows, mkbd->nrows);

        mkbd->stable_col_state = read_gpio_group(mkbd->cols, mkbd->ncols);

        mkbd->pending_row_state = mkbd->stable_row_state;
        mkbd->pending_col_state = mkbd->stable_col_state;

        while (mkbd->running) {
                const uint32_t current_rows =
                        read_gpio_group(mkbd->rows, mkbd->nrows);
                const uint32_t current_cols =
                        read_gpio_group(mkbd->cols, mkbd->ncols);

                if (current_rows != mkbd->stable_row_state ||
                    current_cols != mkbd->stable_col_state) {
                        mkbd->pending_row_state = current_rows;
                        mkbd->pending_col_state = current_cols;
                        xTimerStart(mkbd->timer, portMAX_DELAY);
                }

                if (current_rows != mkbd->stable_row_state) {
                        mkbd->pending_row_state = current_rows;
                        xTimerStart(mkbd->timer, portMAX_DELAY);
                }

                if (current_cols != mkbd->stable_col_state) {
                        mkbd->pending_col_state = current_cols;
                        xTimerStart(mkbd->timer, portMAX_DELAY);
                }
        }

        return ESP_OK;
}

esp_err_t matrix_kbd_stop(matrix_kbd_handle_t mkbd_handle)
{
        if (!mkbd_handle) {
                return ESP_ERR_INVALID_ARG;
        }

        matrix_kbd_t *mkbd = mkbd_handle;

        mkbd->running = false;

        if (mkbd->timer) {
                xTimerStop(mkbd->timer, portMAX_DELAY);
        }

        return ESP_OK;
}

esp_err_t matrix_kbd_register_event_handler(matrix_kbd_handle_t mkbd_handle,
                                            matrix_kbd_event_handler handler,
                                            void *handler_args)
{
        if (!mkbd_handle) {
                return ESP_ERR_INVALID_ARG;
        }

        matrix_kbd_t *mkbd = mkbd_handle;

        mkbd->event_handler = handler;
        mkbd->event_handler_args = handler_args;

        return ESP_OK;
}