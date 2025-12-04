#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "portmacro.h"
#include "xtensa/config/system.h"

/**
 * Brief:
 * This file contains the main function and the button interrupt handler for the ESP32.
 * It initializes the GPIO pins for the LED and the button, and sets up the interrupt handler.
 * The button interrupt handler toggles the state of the LED.
 *
 * GPIO status:
 * GPIO_SWITCH_INPUT_PIN_0: Input, Pull-up, interrupt from rising edge and falling edge
 * GPIO_SWITCH_INPUT_PIN_1: Input, Pull-up, interrupt from falling edge
 * GPIO_LED_OUTPUT_PIN_0: Output
 * GPIO_LED_OUTPUT_PIN_1: Output
 *
 * Test:
 * Connect GPIO_SWITCH_INPUT_PIN_0 with GPIO_LED_OUTPUT_PIN_0
 * Connect GPIO_SWITCH_INPUT_PIN_1 with GPIO_LED_OUTPUT_PIN_1
 * Generate pulses on GPIO_LED_OUTPUT_PIN_0/1, that triggers interrupts on GPIO_SWITCH_INPUT_PIN_0/1
 */

#define GPIO_SWITCH_INPUT_PIN_0 32
#define GPIO_SWITCH_INPUT_PIN_1 33
#define GPIO_INPUT_PIN_MASK ((1ULL<<GPIO_SWITCH_INPUT_PIN_0) | (1ULL<<GPIO_SWITCH_INPUT_PIN_1))
/*
 * In binary, these are represented by two bits in a 32-bit unsigned integer.
 * 1ULL<<GPIO_SWITCH_INPUT_PIN_0 is equal to 0b000100000000000000000000000000000000
 * 1ULL<<GPIO_SWITCH_INPUT_PIN_1 is equal to 0b001000000000000000000000000000000000
 * GPIO_INPUT_PIN_MASK is equal to 0b001100000000000000000000000000000000
 */

#define GPIO_LED_OUTPUT_PIN_0 12
#define GPIO_LED_OUTPUT_PIN_1 13
#define GPIO_OUTPUT_PIN_MASK ((1ULL<<GPIO_LED_OUTPUT_PIN_0) | (1ULL<<GPIO_LED_OUTPUT_PIN_1))
/*
 * Again in binary, these are represented by two bits in a 32-bit unsigned integer.
 * 1ULL<<GPIO_LED_OUTPUT_PIN_0 is equal to 0b00000000000000000001000000000000
 * 1ULL<<GPIO_LED_OUTPUT_PIN_1 is equal to 0b00000000000000000010000000000000
 * GPIO_OUTPUT_PIN_MASK is equal to 0b00000000000000000011000000000000
 */

#define ESP_INTR_FLAG_DEFAULT 0
#define DEBOUNCE_DELAY_MS 50

/*
 * Fancy FIFO void * abstraction with methods attached to it. Safer than using a raw array (and recommended).
 * Initialized later using xQueueCreate()
 * To enqueue, use xQueueSend()
 * To dequeue, use xQueueReceive()
 */
static QueueHandle_t gpio_button_evt_queue = NULL;

static TimerHandle_t debounce_timer_0 = NULL;
static TimerHandle_t debounce_timer_1 = NULL;

/*
 * Interrupt Service Routine (isr) for GPIO button.
 * Do NOT use blocking functions, instead use ISR versions of functions (often (function)FromISR in FreeRTOS):
 *      * xQueueSend() -> xQueueSendFromISR()
 *      * xQueueReceive() -> xQueueReceiveFromISR()
 *
 * **OR**
 *
 * use the queue to pass events to the task. ISRs should be kept short and simple.
 * Thus, you should not use printf(), malloc() and free(), fs ops, floating point math, or large data processing.
 * That will break real-time behaviour.
 *
 * You should also never use a function that relies on an interrupt, such as HAL_Delay(), vTaskDelay(), or busy-waiting.
 *
 * You can not switch tasks manually, ask the scheduler to delay execution, or use power-saving or sleep API calls.
 *
 * Keep in mind that backtraces are unreadable on embedded
 */
static void IRAM_ATTR gpio_button_isr_handler(void *args)
{
        uint32_t gpio_num = (uint32_t) args;
        if (gpio_num == GPIO_SWITCH_INPUT_PIN_0) {
                xTimerStartFromISR(debounce_timer_0, NULL);
        } else if (gpio_num == GPIO_SWITCH_INPUT_PIN_1) {
                xTimerStartFromISR(debounce_timer_1, NULL);
        }
}

static void debounce_timer_callback(TimerHandle_t xTimer)
{
        uint32_t gpio_num = (uint32_t) pvTimerGetTimerID(xTimer);

        if (gpio_get_level(gpio_num) == 0) {
                xQueueSend(gpio_button_evt_queue, &gpio_num, portMAX_DELAY);
        }
}

/*
 * Button task to handle button events from interrupts
 *
 * Logic:
 *      - Wait for button press event
 *      - Toggle LED state on GPIO_LED_OUTPUT_PIN_0
 */
static void gpio_button_task(void *args)
{
        uint32_t gpio_num;
        uint32_t led_state_0 = 0;
        uint32_t led_state_1 = 0;

        while (1) {
                if (xQueueReceive(gpio_button_evt_queue, &gpio_num, portMAX_DELAY)) {
                        uint32_t *led_state = (gpio_num == GPIO_SWITCH_INPUT_PIN_0) ? &led_state_0 : &led_state_1;
                        *led_state = !*led_state;

                        uint32_t led_pin = (gpio_num == GPIO_SWITCH_INPUT_PIN_0) ? GPIO_LED_OUTPUT_PIN_0 : GPIO_LED_OUTPUT_PIN_1;
                        gpio_set_level(led_pin, *led_state);

                        printf("Button pressed on GPIO %"PRIu32"\n", gpio_num);
                }
        }
}

void app_main(void)
{
        // Init OUTPUT pins
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_MASK;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);

        // Init INPUT pins
        io_conf.intr_type = GPIO_INTR_NEGEDGE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = GPIO_INPUT_PIN_MASK;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);

        // Create queue for button events
        gpio_button_evt_queue = xQueueCreate(10, sizeof(uint32_t));

        /*
         * Function to start task
         *
         * Parameters:
         * - pxTaskFunction: Pointer to the task function (TaskFunction_t aka void (*)(void *))
         * - pcName: Name of the task (const char * const)
         * - usStackDepth: Stack depth of the task (const unsigned int)
         * - pvParameters: Parameters to pass to the task function (void * const)
         * - uxPriority: Priority of the task (UBaseType_t aka unsigned int)
         * - pxCreatedTask: Pointer to a variable to store the created task handle (TaskHandle_t * const)
         */

        debounce_timer_0 = xTimerCreate("debounce_timer_0", pdMS_TO_TICKS(DEBOUNCE_DELAY_MS), pdFALSE, (void *) GPIO_SWITCH_INPUT_PIN_0, debounce_timer_callback);
        debounce_timer_1 = xTimerCreate("debounce_timer_1", pdMS_TO_TICKS(DEBOUNCE_DELAY_MS), pdFALSE, (void *) GPIO_SWITCH_INPUT_PIN_1, debounce_timer_callback);

        xTaskCreate(gpio_button_task, "gpio_button_task", 2048, NULL, 10, NULL);

        /*
         * Initialize GPIO interrupt service
         *
         * - ESP_INTR_FLAG_DEFAULT: Default interrupt flags (aka 0)
         */
        gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

        /*
         * Register GPIO interrupt handlers
         *
         * - gpio_isr_handler_add: Function to register GPIO interrupt handlers
         * - (void *) GPIO_SWITCH_INPUT_PIN_0: Parameter to pass to the interrupt handler function for input 0 (notice the void *args)
         */
        gpio_isr_handler_add(GPIO_SWITCH_INPUT_PIN_0, gpio_button_isr_handler, (void*) GPIO_SWITCH_INPUT_PIN_0);
        gpio_isr_handler_add(GPIO_SWITCH_INPUT_PIN_1, gpio_button_isr_handler, (void*) GPIO_SWITCH_INPUT_PIN_1);

        printf("Minimum free heap size: %"PRIu32" bytes\n", esp_get_minimum_free_heap_size());
        printf("MAIN Initialized!\n");

        while (1) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
}
