#ifndef HID_REPORT_H
#define HID_REPORT_H

#include <stdint.h>

struct hid_keyboard_report {
	uint8_t modifier; // always 0
	uint8_t reserved;
	uint8_t keycodes
		[6]; // 6 simul, but only one will be pressed. used for buffer
};

#endif
