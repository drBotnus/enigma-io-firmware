#include <stdint.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "sdkconfig.h"

#define INPUT_PIN 32
#define LED_PIN 12

static void IRAM_ATTR button_isr_handler(void *args)
{
    printf("Flipping state!");
    uint32_t* led_state = (uint32_t*) args;
    *led_state = !(*led_state);
    gpio_set_level(LED_PIN, *led_state);
}

void app_main(void)
{
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    gpio_reset_pin(INPUT_PIN);
    gpio_set_direction(INPUT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(INPUT_PIN, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(INPUT_PIN, GPIO_INTR_NEGEDGE);

    static uint32_t led_state = 0;

    gpio_install_isr_service(0);
    gpio_isr_handler_add(INPUT_PIN, button_isr_handler, (void *)&led_state);

    printf("Initialized!");

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
