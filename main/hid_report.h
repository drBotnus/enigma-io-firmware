#pragma once

#include <stdint.h>
typedef struct {
        uint8_t modifier;
        uint8_t reserved;
        uint8_t keycodes[6];
} hid_keyboard_report_t;
