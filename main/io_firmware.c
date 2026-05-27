#include "class/hid/hid.h"
#include "device/usbd.h"
#include "matrix_keyboard.h"
#include "hid_keyboard.h"
#include <stdint.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"

#define APP_BUTTON (GPIO_NUM_0)
#define ROW_BUTTON (GPIO_NUM_11)
#define COL_BUTTON (GPIO_NUM_10)
static const char *TAG = "enigma i/o";

#define TUSB_DESC_TOTAL_LEN \
        (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

typedef struct {
        int modifier;
        int keycode;
} keycode_mapping_t;

typedef struct {
        uint16_t keycode;
        uint8_t hid_code;
} keymap_t;

keycode_mapping_t conv_table[] = { HID_ASCII_TO_KEYCODE };

const uint8_t hid_report_descriptor[] = { TUD_HID_REPORT_DESC_KEYBOARD(
        HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)) };

static const keymap_t keymap[] = {
        { MAKE_KEY_CODE(0, 0), HID_KEY_A }, { MAKE_KEY_CODE(0, 1), HID_KEY_B },
        { MAKE_KEY_CODE(0, 2), HID_KEY_C }, { MAKE_KEY_CODE(0, 3), HID_KEY_D },
        { MAKE_KEY_CODE(1, 0), HID_KEY_E }, { MAKE_KEY_CODE(1, 1), HID_KEY_F },
        { MAKE_KEY_CODE(1, 2), HID_KEY_G }, { MAKE_KEY_CODE(1, 3), HID_KEY_H },
        { MAKE_KEY_CODE(2, 0), HID_KEY_I }, { MAKE_KEY_CODE(2, 1), HID_KEY_J },
        { MAKE_KEY_CODE(2, 2), HID_KEY_K }, { MAKE_KEY_CODE(2, 3), HID_KEY_L },
        { MAKE_KEY_CODE(3, 0), HID_KEY_M }, { MAKE_KEY_CODE(3, 1), HID_KEY_N },
        { MAKE_KEY_CODE(3, 2), HID_KEY_O }, { MAKE_KEY_CODE(3, 3), HID_KEY_P },
};

uint8_t lookup_hid(uint16_t keycode)
{
        for (int i = 0; i < 16; i++) {
                if (keymap[i].keycode == keycode) {
                        return keymap[i].hid_code;
                }
        }
        return 0;
}

const char *hid_string_descriptor[5] = {
        (char[]){ 0x09, 0x04 }, // supported language is english
        "enigma", // usb manufacturer
        "enigma", // usb product
        "2AD6X-ESPS3N16R8", // serials, [WARN] use chip id
        "Enigma I/O Link", // HID
};

static const uint8_t hid_configuration_descriptor[] = {
        // Configuration number, interface count, string index, total length, attribute, power in mA
        TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN,
                              TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

        // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
        TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16,
                           10),
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
        return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
        (void)instance;
        (void)report_id;
        (void)report_type;
        (void)buffer;
        (void)reqlen;

        return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, const uint8_t *buffer,
                           uint16_t bufsize)
{
}

uint8_t char_to_hid_keycode(unsigned char c, uint8_t *modifier)
{
        uint8_t mod = 0;
        uint8_t keycode = 0;
        if (c < sizeof(conv_table) / sizeof(conv_table[0])) {
                mod = conv_table[(uint8_t)c].modifier;
                keycode = conv_table[c].keycode;
        } else {
                mod = 0;
                keycode = 0;
        }

        if (modifier != NULL) {
                *modifier = (mod != 0) ? KEYBOARD_MODIFIER_LEFTSHIFT : 0;
        }
        return keycode;
}

void send_string(const char *str)
{
        ESP_LOGI(TAG, "Sending Keyboard report for string: %s", str);
        uint8_t keycodes[6] = { 0 };
        uint8_t modifier = 0;

        for (size_t i = 0; str[i] != '\0' && i < strlen(str); i++) {
                keycodes[0] = char_to_hid_keycode(str[i], &modifier);

                if (keycodes[0] != 0) {
                        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD,
                                                modifier, keycodes);
                        vTaskDelay(pdMS_TO_TICKS(10));
                        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0,
                                                NULL);
                        vTaskDelay(pdMS_TO_TICKS(10));
                } else {
                        ESP_LOGW(TAG, "Invalid character: %c", str[i]);
                }
        }
}

void send_key(uint8_t keycode)
{
        ESP_LOGI(TAG, "Sending Keyboard report for keycode: 0x%02X", keycode);
        uint8_t keycodes[6] = { keycode };
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycodes);
        vTaskDelay(pdMS_TO_TICKS(10));
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        vTaskDelay(pdMS_TO_TICKS(10));
}

void send_key_with_modifier(unsigned char keycode, uint8_t *modifier)
{
        ESP_LOGI(TAG, "Sending Keyboard report for keycode: 0x%02X", keycode);
        uint8_t keycodes[6] = { char_to_hid_keycode(keycode, modifier) };
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, *modifier, keycodes);
        vTaskDelay(pdMS_TO_TICKS(10));
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, *modifier, NULL);
        vTaskDelay(pdMS_TO_TICKS(10));
}

static void send_hid_report(char c)
{
        uint8_t modifier = 0;
        send_key_with_modifier(c, &modifier);
}

esp_err_t enigma_matrix_event_handler(matrix_kbd_handle_t mkbd_handle,
                                      matrix_kbd_event_id_t event,
                                      void *event_data, void *handler_args)
{
        ESP_LOGD(TAG, "Enigma matrix event");
        matrix_kbd_event_data_t *data = event_data;

        ESP_LOGD(TAG, "row=%" PRIx32 " col=%" PRIx32, data->row, data->col);

        uint16_t keycode = MAKE_KEY_CODE(data->row, data->col);
        uint8_t hid_code = lookup_hid(keycode);
        ESP_LOGD(TAG, "keycode=%" PRIu16 ", hid_code=%" PRIu8, keycode, hid_code);

        send_key(hid_code);

        switch (event) {
        case MATRIX_KBD_EVENT_DOWN:
                ESP_LOGI(TAG, "press event");
                break;
        case MATRIX_KBD_EVENT_UP:
                ESP_LOGI(TAG, "release event");
                break;
        }

        return ESP_OK;
}

void app_main(void)
{
        matrix_kbd_handle_t kbd;

        matrix_kbd_config_t config = MATRIX_KEYBOARD_DEFAULT_CONFIG();
        config.cols = (int[]){ 10, 11, 12, 13 };
        config.ncols = 4;
        config.rows = (int[]){ 15, 16, 17, 18 };
        config.nrows = 4;

        matrix_kbd_init(&config, &kbd);

        matrix_kbd_register_event_handler(kbd, enigma_matrix_event_handler,
                                          NULL);

        matrix_kbd_start(kbd);

        ESP_LOGI(TAG, "USB initialization");

        tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

        tusb_cfg.descriptor.device = NULL;
        tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
        tusb_cfg.descriptor.string = hid_string_descriptor;
        tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) /
                                           sizeof(hid_string_descriptor[0]);

        ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
        ESP_LOGI(TAG, "USB initialization DONE");
}
