#ifndef IO_FIRMWARE_HID_KEYBOARD_H
#define IO_FIRMWARE_HID_KEYBOARD_H

#include <esp_err.h>
#include <stdint.h>
#include <class/hid/hid.h>

#define MAKE_KEY_CODE(row, col) ((row << 8) | (col))
#define GET_KEY_CODE_ROW(code) ((code >> 8) & 0xFF)
#define GET_KEY_CODE_COL(code) (code & 0xFF)

typedef struct hid_keyboard_t *hid_keyboard_handle_t;

esp_err_t hid_keyboard_init(hid_keyboard_handle_t *hid_handle);
esp_err_t hid_keyboard_deinit(hid_keyboard_handle_t hid_handle);

esp_err_t hid_press_keycode(uint16_t keycode, hid_keyboard_handle_t hid_handle);
esp_err_t hid_release_keycode(uint16_t keycode, hid_keyboard_handle_t hid_handle);
esp_err_t hid_release_all_keys(hid_keyboard_handle_t hid_handle);

esp_err_t hid_press_modifier(uint8_t modifier, hid_keyboard_handle_t hid_handle);
esp_err_t hid_release_modifier(hid_keyboard_handle_t hid_handle);

#endif //IO_FIRMWARE_HID_KEYBOARD_H
