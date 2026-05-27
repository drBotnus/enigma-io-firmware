#include "esp_err.h"
#include "matrix_keyboard.h"

#include "esp_log.h"
#include "driver/dedic_gpio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "matrix_kbd";

typedef struct matrix_kbd_t matrix_kbd_t;

typedef struct {
        uint32_t row;
        uint32_t col;
        bool valid;
} matrix_position_t;

struct matrix_kbd_t {
        // Row Group
        gpio_num_t *rows;
        // Column Group
        gpio_num_t *cols;
        uint32_t nrows;
        uint32_t ncols;

        // Task Handle to avoid duplicate tasks by multiple calls to start fn.
        TaskHandle_t scan_task;
        // Handler function provided by initializer (in function pointer)
        matrix_kbd_event_handler event_handler;
        // Additional arguments to pass to matrix_kbd_t.event_handler
        void *event_handler_args;

        matrix_position_t stable;
        matrix_position_t pending;

        TickType_t pending_since;
        TickType_t debounce_ticks;
};

static bool position_equal(matrix_position_t a, matrix_position_t b)
{
        return a.valid == b.valid && a.row == b.row && a.col == b.col;
}

static int detect_active_line(const gpio_num_t *gpios, uint32_t count)
{
        int active = -1;

        for (uint32_t i = 0; i < count; i++) {
                // ACTIVE LOW
                if (!gpio_get_level(gpios[i])) {
                        if (active != -1) {
                                return -2;
                        }

                        active = (int)i;
                }
        }

        return active;
}

static matrix_position_t read_position(const matrix_kbd_t *mkbd)
{
        matrix_position_t pos = {
                .row = -1,
                .col = -1,
                .valid = false,
        };

        int row = detect_active_line(mkbd->rows, mkbd->nrows);
        int col = detect_active_line(mkbd->cols, mkbd->ncols);

        if (row < 0 || col < 0) {
                return pos;
        }

        pos.row = row;
        pos.col = col;
        pos.valid = true;

        return pos;
}

esp_err_t matrix_kbd_init(const matrix_kbd_config_t *config,
                          matrix_kbd_handle_t *mkbd_handle)
{
        if (!config || !mkbd_handle) {
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

        memcpy(mkbd->rows, config->rows, sizeof(gpio_num_t) * config->nrows);

        memcpy(mkbd->cols, config->cols, sizeof(gpio_num_t) * config->ncols);

        for (uint32_t i = 0; i < config->nrows; i++) {
                gpio_config_t io_conf = {
                        .pin_bit_mask = (1ULL << mkbd->rows[i]),
                        .mode = GPIO_MODE_INPUT,
                        .pull_up_en = GPIO_PULLUP_ENABLE,
                        .pull_down_en = GPIO_PULLDOWN_DISABLE,
                        .intr_type = GPIO_INTR_DISABLE,
                };

                ESP_ERROR_CHECK(gpio_config(&io_conf));
        }

        for (uint32_t i = 0; i < config->ncols; i++) {
                gpio_config_t io_conf = {
                        .pin_bit_mask = (1ULL << mkbd->cols[i]),
                        .mode = GPIO_MODE_INPUT,
                        .pull_up_en = GPIO_PULLUP_ENABLE,
                        .pull_down_en = GPIO_PULLDOWN_DISABLE,
                        .intr_type = GPIO_INTR_DISABLE,
                };

                ESP_ERROR_CHECK(gpio_config(&io_conf));
        }

        mkbd->debounce_ticks = pdMS_TO_TICKS(config->debounce_ms) == 0 ? 1 : pdMS_TO_TICKS(config->debounce_ms);

        mkbd->stable = read_position(mkbd);
        mkbd->pending = mkbd->stable;

        *mkbd_handle = mkbd;

        ESP_LOGI(TAG, "initialized detector (%" PRIu32 "x%" PRIu32 ")",
                 mkbd->nrows, mkbd->ncols);

        return ESP_OK;
}

esp_err_t matrix_kbd_deinit(matrix_kbd_handle_t mkbd_handle)
{
        if (!mkbd_handle) {
                return ESP_ERR_INVALID_ARG;
        }

        matrix_kbd_t *mkbd = mkbd_handle;

        matrix_kbd_stop(mkbd);

        free(mkbd->rows);
        free(mkbd->cols);
        free(mkbd);

        return ESP_OK;
}

void matrix_kbd_scan_task(void *pvParameters)
{
        ESP_LOGD("mkbd_scan", "Entered scan task");
        matrix_kbd_t *mkbd = pvParameters;

        while (true) {
                matrix_position_t current = read_position(mkbd);
                TickType_t now = xTaskGetTickCount();

                if (!position_equal(current, mkbd->pending)) {
                        mkbd->pending = current;
                        mkbd->pending_since = now;

                        ESP_LOGD(TAG, "Edge detected");

                }

                if (!position_equal(mkbd->pending, mkbd->stable)) {
                        TickType_t elapsed = now - mkbd->pending_since;

                        if (elapsed >= mkbd->debounce_ticks) {
                                bool was_valid = mkbd->stable.valid;
                                bool now_valid = mkbd->pending.valid;

                                matrix_kbd_event_id_t event;

                                if (!was_valid && now_valid) {
                                        event = MATRIX_KBD_EVENT_DOWN;
                                } else if (was_valid && !now_valid) {
                                        event = MATRIX_KBD_EVENT_UP;
                                } else {
                                        event = MATRIX_KBD_EVENT_DOWN;
                                }

                                matrix_position_t old = mkbd->stable;

                                mkbd->stable = mkbd->pending;

                                if (mkbd->event_handler) {

                                        matrix_kbd_event_data_t event_data = {
                                                .row = (uint32_t)(
                                                    old.valid ? old.row : mkbd->stable.row),

                                                .col = (uint32_t)(
                                                    old.valid ? old.col : mkbd->stable.col),
                                            };

                                        mkbd->event_handler(
                                            mkbd,
                                            event,
                                            &event_data,
                                            mkbd->event_handler_args);
                                }
                        }
                }

                vTaskDelay(1);
        }
}

esp_err_t matrix_kbd_start(matrix_kbd_handle_t mkbd_handle)
{
        ESP_LOGD("mkbd_scan", "Entered start task");
        if (!mkbd_handle) {
                return ESP_ERR_INVALID_ARG;
        }

        matrix_kbd_t *mkbd = mkbd_handle;

        if (mkbd->scan_task) {
                return ESP_OK;
        }

        BaseType_t ok = xTaskCreatePinnedToCore(matrix_kbd_scan_task,
                                                "matrix_scan", 4096, mkbd, 5,
                                                &mkbd->scan_task, 0);

        return ok == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t matrix_kbd_stop(matrix_kbd_handle_t mkbd_handle)
{
        if (!mkbd_handle) {
                return ESP_ERR_INVALID_ARG;
        }

        matrix_kbd_t *mkbd = mkbd_handle;

        if (mkbd->scan_task) {
                vTaskDelete(mkbd->scan_task);
                mkbd->scan_task = NULL;
        }

        return ESP_OK;
}

esp_err_t matrix_kbd_register_event_handler(matrix_kbd_handle_t mkbd_handle,
                                            matrix_kbd_event_handler handler,
                                            void *handler_args)
{
        ESP_LOGD("matrix_kbd_register_event_handler", "Entered");

        if (!mkbd_handle) {
                return ESP_ERR_INVALID_ARG;
        }

        matrix_kbd_t *mkbd = mkbd_handle;

        mkbd->event_handler = handler;
        mkbd->event_handler_args = handler_args;

        return ESP_OK;
}