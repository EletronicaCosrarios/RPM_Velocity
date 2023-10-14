#include "../include/tester.h"
#include "../include/utils.h"
#include "driver/gpio.h"

#define TAG "tester_mode"

void tester_mode_init(void *p1) {
  ARGUNSED(p1);
  static uint8_t s_led_state = 0;
  gpio_num_t gpios[] = {GPIO_NUM_2, GPIO_NUM_15};
  for (uint8_t i = 0; i < (sizeof(gpios) / sizeof(*gpios)); i++) {
    gpio_reset_pin(gpios[i]);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(gpios[i], GPIO_MODE_OUTPUT);
  }
  while (1) {
    // ESP_LOGI(TAG, "Testing interreputs %s!", s_led_state == true ? "ON" :
    // "OFF");
    for (uint8_t i = 0; i < (sizeof(gpios) / sizeof(*gpios)); i++) {
      gpio_set_level(gpios[i], s_led_state);
    }
    /* Toggle the LED state */
    s_led_state = !s_led_state;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
