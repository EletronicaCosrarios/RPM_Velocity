#include "../include/esp_now_tool.h"
#include "../include/motor_rpm.h"
#include "../include/tester.h"
#include "../include/utils.h"
#include "../include/velocity_tool.h"
#include "esp_now.h"
#include <stdio.h>

#define ESP_NOW_MEMORY 4096
#define ESP_NOW_PRIORITY 1

#define MOTOR_RPM_STACK_MEMORY 2048
#define MOTOR_RPM_PRIORITY 2

#define VELOCITY_TOOL_STACK_MEMORY 2048
#define VELOCITY_TOOL_PRIORITY 2

#define TAG "main"
#ifdef CONFIG_ESPNOW_SENDER_MODE
TaskHandle_t motor_rpm;
TaskHandle_t velocity_tool;
TaskHandle_t sender_esp_now;

RingbufHandle_t ring_buf;

#endif
#ifdef CONFIG_ESPNOW_RECEIVER_MODE
TaskHandle_t receiver_esp_now;
#endif

#ifdef CONFIG_ESPNOW_TESTER_MODE
TaskHandle_t tester_mode;
#endif

void app_main(void) {
  ESP_LOGI(TAG, "Starting the system...");
#ifdef CONFIG_ESPNOW_SENDER_MODE
  ring_buf = xRingbufferCreate(1026, RINGBUF_TYPE_NOSPLIT);

  xTaskCreatePinnedToCore(motor_rpm_init, "init motor rpm",
                          MOTOR_RPM_STACK_MEMORY, &ring_buf, MOTOR_RPM_PRIORITY,
                          &motor_rpm, 0);
  xTaskCreatePinnedToCore(esp_now_sender_init, "init sender espnow motor",
                          ESP_NOW_MEMORY, &ring_buf, ESP_NOW_PRIORITY,
                          &sender_esp_now, 1);
  xTaskCreatePinnedToCore(velocity_tool_init, "init velocity tool",
                          VELOCITY_TOOL_STACK_MEMORY, &ring_buf,
                          VELOCITY_TOOL_PRIORITY, &velocity_tool, 0);
#endif
#ifdef CONFIG_ESPNOW_RECEIVER_MODE
  xTaskCreatePinnedToCore(esp_now_receiver_init, "init receiver espnow",
                          ESP_NOW_MEMORY, NULL, ESP_NOW_PRIORITY,
                          &receiver_esp_now, 0);
#endif
#ifdef CONFIG_ESPNOW_TESTER_MODE
  xTaskCreatePinnedToCore(tester_mode_init, "init tester mode", ESP_NOW_MEMORY,
                          NULL, ESP_NOW_PRIORITY, &tester_mode, 0);
#endif
  vTaskDelay(portMAX_DELAY);
}
