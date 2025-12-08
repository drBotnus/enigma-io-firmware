#ifndef HID_BACKEND_H
#define HID_BACKEND_H

#include "esp_err.h"
#include "hid_report.h"

enum hid_backend_type {
	HID_BACKEND_USB = 0,
	HID_BACKEND_BLE,
};

esp_err_t hid_backend_init(enum hid_backend_type backend);

esp_err_t hid_backend_send_report(const struct hid_keyboard_report *report);

void hid_backend_deinit(void);

#endif
