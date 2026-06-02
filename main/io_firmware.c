// Internal APIs
#include "hid_keyboard.h"
#include "matrix_keyboard.h"

// FreeRTOS + ESP-IDF
#include "esp_log.h"
#include "tinyusb.h"
#include "device/usbd.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

static const char *TAG = "enigma i/o";

esp_err_t enigma_matrix_event_handler(matrix_kbd_handle_t mkbd_handle,
                                      matrix_kbd_event_id_t event,
                                      void *event_data, void *handler_args)
{
        ESP_LOGD(TAG, "Enigma matrix event");
        matrix_kbd_event_data_t *data = event_data;

        hid_keyboard_handle_t hid = (hid_keyboard_handle_t)handler_args;

        ESP_LOGD(TAG, "row=%" PRIx32 " col=%" PRIx32, data->row, data->col);

        uint16_t keycode = MAKE_KEY_CODE(data->row, data->col);
        ESP_LOGD(TAG, "keycode=%" PRIu16, keycode);

        switch (event) {
        case MATRIX_KBD_EVENT_DOWN:
                hid_press_keycode(keycode, hid);
                ESP_LOGI(TAG, "press event");
                break;
        case MATRIX_KBD_EVENT_UP:
                hid_release_keycode(keycode, hid);
                ESP_LOGI(TAG, "release event");
                break;
        }

        return ESP_OK;
}

void app_main(void)
{
        hid_keyboard_handle_t hid;
        hid_keyboard_init(&hid);

        matrix_kbd_handle_t kbd;

        matrix_kbd_config_t config = MATRIX_KEYBOARD_DEFAULT_CONFIG();
        config.cols = (int[]){ 46, 9, 10, 11, 12, 13 };
        config.ncols = 6;
        config.rows = (int[]){ 35, 36, 37, 38, 39, 40 };
        config.nrows = 6;

        matrix_kbd_init(&config, &kbd);

        matrix_kbd_register_event_handler(kbd, enigma_matrix_event_handler,
                                          hid);

        matrix_kbd_start(kbd);

        while (true) {
                tud_task();
                vTaskDelay(20 / portTICK_PERIOD_MS);
        }
}
