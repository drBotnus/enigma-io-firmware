#include "hid_keyboard.h"

#include <esp_log.h>
#include <tinyusb.h>
#include <tinyusb_default_config.h>
#include <class/hid/hid_device.h>
#include <device/usbd.h>

static const char *TAG = "usb hid keyboard";

#define TUSB_DESC_TOTAL_LEN 34

#define PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))

#define USB_PID (0x4000 | PID_MAP(HID, 0))
#define USB_VID 0xCafe
#define USB_BCD 0x0110

const tusb_desc_device_t desc_device = {
        .bLength = sizeof(tusb_desc_device_t),
        .bDescriptorType = TUSB_DESC_DEVICE,
        .bcdUSB = USB_BCD,

        .bDeviceClass = 0x0,
        .bDeviceSubClass = 0x0,
        .bDeviceProtocol = 0x0,

        .idVendor = USB_VID,
        .idProduct = USB_PID,
        .bcdDevice = 0x0100,

        .iManufacturer = 0x01,
        .iProduct = 0x02,
        .iSerialNumber = 0x03,

        .bNumConfigurations = 0x01,
        .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
};

uint8_t const *tud_device_descriptor_cb(void)
{
        return (uint8_t const *)&desc_device;
}

typedef struct hid_keyboard_t hid_keyboard_t;

struct hid_keyboard_t {
        hid_keyboard_report_t report;
};

const uint8_t hid_report_descriptor[] = {
        TUD_HID_REPORT_DESC_KEYBOARD(),
};

typedef struct {
        uint16_t keycode;
        uint8_t hid_code;
} hid_keymap_t;

static const hid_keymap_t keymap[] = {
        { MAKE_KEY_CODE(0, 0), HID_KEY_A },
        { MAKE_KEY_CODE(0, 1), HID_KEY_B },
        { MAKE_KEY_CODE(0, 2), HID_KEY_C },
        { MAKE_KEY_CODE(0, 3), HID_KEY_D },
        { MAKE_KEY_CODE(0, 4), HID_KEY_E },
        { MAKE_KEY_CODE(0, 5), HID_KEY_F },
        { MAKE_KEY_CODE(1, 0), HID_KEY_G },
        { MAKE_KEY_CODE(1, 1), HID_KEY_H },
        { MAKE_KEY_CODE(1, 2), HID_KEY_I },
        { MAKE_KEY_CODE(1, 3), HID_KEY_J },
        { MAKE_KEY_CODE(1, 4), HID_KEY_K },
        { MAKE_KEY_CODE(1, 5), HID_KEY_L },
        { MAKE_KEY_CODE(2, 0), HID_KEY_M },
        { MAKE_KEY_CODE(2, 1), HID_KEY_N },
        { MAKE_KEY_CODE(2, 2), HID_KEY_O },
        { MAKE_KEY_CODE(2, 3), HID_KEY_P },
        { MAKE_KEY_CODE(2, 4), HID_KEY_Q },
        { MAKE_KEY_CODE(2, 5), HID_KEY_R },
        { MAKE_KEY_CODE(3, 0), HID_KEY_S },
        { MAKE_KEY_CODE(3, 1), HID_KEY_T },
        { MAKE_KEY_CODE(3, 2), HID_KEY_U },
        { MAKE_KEY_CODE(3, 3), HID_KEY_V },
        { MAKE_KEY_CODE(3, 4), HID_KEY_W },
        { MAKE_KEY_CODE(3, 5), HID_KEY_X },
        { MAKE_KEY_CODE(4, 0), HID_KEY_Y },
        { MAKE_KEY_CODE(4, 1), HID_KEY_Z },
        { MAKE_KEY_CODE(4, 2), HID_KEY_COMMA },
        { MAKE_KEY_CODE(4, 3), HID_KEY_APOSTROPHE },
        { MAKE_KEY_CODE(4, 4), HID_KEY_NONE },
        { MAKE_KEY_CODE(4, 5), HID_KEY_NONE },
        { MAKE_KEY_CODE(5, 0), HID_KEY_NONE },
        { MAKE_KEY_CODE(5, 1), HID_KEY_NONE },
        { MAKE_KEY_CODE(5, 2), HID_KEY_ENTER },
        { MAKE_KEY_CODE(5, 3), HID_KEY_SPACE },
        { MAKE_KEY_CODE(5, 4), HID_KEY_BACKSPACE },
        { MAKE_KEY_CODE(5, 5), HID_KEY_PERIOD },
};

uint8_t lookup_hid(uint16_t keycode)
{
        for (int i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++) {
                if (keymap[i].keycode == keycode) {
                        return keymap[i].hid_code;
                }
        }
        return 0;
}

const char *hid_string_descriptor[5] = {
        (const char[]){ 0x09, 0x04 }, // supported language is english
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
        TUD_HID_DESCRIPTOR(0, 4, HID_ITF_PROTOCOL_KEYBOARD,
                           sizeof(hid_report_descriptor), 0x81, 8, 10),
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
        return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                      // ReSharper disable once CppParameterMayBeConstPtrOrRef
                      hid_report_type_t report_type, uint8_t *buffer, // NOLINT(*-non-const-parameter)
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
        (void)instance;
        (void)report_id;
        (void)report_type;
        (void)buffer;
        (void)bufsize;
}

esp_err_t hid_keyboard_init(hid_keyboard_handle_t *hid_handle)
{
        ESP_LOGI(TAG, "USB init");
        hid_keyboard_t *hid_keyboard = calloc(1, sizeof(hid_keyboard_t));

        if (!hid_keyboard) {
                return ESP_ERR_NO_MEM;
        }

        tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
        tusb_cfg.descriptor.device = &desc_device;
        tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
        tusb_cfg.descriptor.string = hid_string_descriptor;
        tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) /
                                           sizeof(hid_string_descriptor[0]);

        ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

        *hid_handle = hid_keyboard;

        ESP_LOGI(TAG, "USB init DONE");

        return ESP_OK;
}

esp_err_t hid_press_keycode(uint16_t keycode, hid_keyboard_handle_t hid_handle)
{
        if (!tud_mounted() || !tud_hid_ready()) {
                return ESP_FAIL;
        }

        ESP_LOGI(TAG, "press keycode=%" PRIu16, keycode);
        uint8_t hid_keycode = lookup_hid(keycode);

        hid_keyboard_t *hid_keyboard = hid_handle;

        for (int i = 0; i < 6; i++) {
                if (hid_keyboard->report.keycode[i]) {
                        continue;
                }

                hid_keyboard->report.keycode[i] = hid_keycode;
                break;
        }

        if (!tud_hid_report(0, &hid_keyboard->report,
                            sizeof(hid_keyboard->report))) {
                ESP_LOGE(TAG, "press failed, keycode=%" PRIu16, keycode);
                return ESP_FAIL;
        }

        return ESP_OK;
}

esp_err_t hid_release_keycode(uint16_t keycode,
                              hid_keyboard_handle_t hid_handle)
{
        if (!tud_mounted() || !tud_hid_ready()) {
                return ESP_FAIL;
        }

        ESP_LOGI(TAG, "release keycode=%" PRIu16, keycode);
        uint8_t hid_keycode = lookup_hid(keycode);

        hid_keyboard_t *hid_keyboard = hid_handle;

        for (int i = 0; i < 6; i++) {
                if (hid_keyboard->report.keycode[i] == hid_keycode) {
                        hid_keyboard->report.keycode[i] = 0;
                }
        }

        if (!tud_hid_report(0, &hid_keyboard->report,
                            sizeof(hid_keyboard->report))) {
                ESP_LOGE(TAG, "release failed, keycode=%" PRIu16, keycode);
                return ESP_FAIL;
        }

        return ESP_OK;
}

esp_err_t hid_release_all(hid_keyboard_handle_t hid_handle)
{
        hid_keyboard_t *hid_keyboard = hid_handle;

        for (int i = 0; i < 6; i++) {
                hid_keyboard->report.keycode[i] = 0;
        }

        tud_hid_report(0, &hid_keyboard->report, sizeof(hid_keyboard->report));

        return ESP_OK;
}

esp_err_t hid_press_modifier(uint8_t modifier, hid_keyboard_handle_t hid_handle)
{
        hid_keyboard_t *hid_keyboard = hid_handle;
        hid_keyboard->report.modifier = modifier;
        tud_hid_report(0, &hid_keyboard->report, sizeof(hid_keyboard->report));

        return ESP_OK;
}

esp_err_t hid_release_modifier(hid_keyboard_handle_t hid_handle)
{
        hid_keyboard_t *hid_keyboard = hid_handle;
        hid_keyboard->report.modifier = 0;
        tud_hid_report(0, &hid_keyboard->report, sizeof(hid_keyboard->report));

        return ESP_OK;
}
