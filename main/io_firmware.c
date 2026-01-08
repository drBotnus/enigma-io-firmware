#include "class/hid/hid.h"
#include "device/usbd.h"
#include "projdefs.h"
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"

#define APP_BUTTON (GPIO_NUM_0)
static const char *TAG = "enigma i/o";

#define TUSB_DESC_TOTAL_LEN \
	(TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

struct keycode_mapping {
	int modifier;
	int keycode;
};

struct keycode_mapping conv_table[] = {HID_ASCII_TO_KEYCODE};

const uint8_t hid_report_descriptor[] = { TUD_HID_REPORT_DESC_KEYBOARD(
	HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)) };

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

uint8_t char_to_hid_keycode(unsigned char c, uint8_t *modifier) {
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
	uint8_t keycodes[6] = {0};
	uint8_t modifier = 0;

	for (size_t i = 0; str[i] != '\0' && i < strlen(str); i++) {
		keycodes[0] = char_to_hid_keycode(str[i], &modifier);


		if (keycodes[0] != 0) {
			tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, modifier, keycodes);
			vTaskDelay(pdMS_TO_TICKS(10));
			tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
			vTaskDelay(pdMS_TO_TICKS(10));
		} else {
			ESP_LOGW(TAG, "Invalid character: %c", str[i]);
		}
	}
}

static void send_hid_report(void)
{
	const char* message = "Hello World ESP32";
	send_string(message);
}

void app_main(void)
{
	const gpio_config_t boot_button_config = {
		.pin_bit_mask = BIT64(APP_BUTTON),
		.mode = GPIO_MODE_INPUT,
		.intr_type = GPIO_INTR_DISABLE,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
	};
	ESP_ERROR_CHECK(gpio_config(&boot_button_config));

	ESP_LOGI(TAG, "USB initialization");

	tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

	tusb_cfg.descriptor.device = NULL;
	tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
	tusb_cfg.descriptor.string = hid_string_descriptor;
	tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);

	ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
	ESP_LOGI(TAG, "USB initialization DONE");

	while (1) {
		if (tud_mounted()) {
			static bool send_hid_data = true;
			if (send_hid_data) {
				send_hid_report();
			}
			send_hid_data = !gpio_get_level(APP_BUTTON);
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}
