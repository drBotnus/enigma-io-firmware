idf_component_register(SRCS "io_firmware.c"
                    PRIV_REQUIRES matrix_keyboard
                    PRIV_REQUIRES hid_keyboard
                    REQUIRES esp_tinyusb
                    INCLUDE_DIRS ".")
