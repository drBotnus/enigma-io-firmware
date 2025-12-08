idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "../hid_device/")

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)

target_link_libraries(${tusb_lib} PRIVATE ${COMPONENT_LIB})
