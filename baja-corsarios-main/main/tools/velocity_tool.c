#include "../include/velocity_tool.h"
#include "../include/utils.h"
#include "driver/gpio.h"
#include "soc/rtc_wdt.h"

#define TAG "velocity_tool"

#define VELOCITY_TOOL_PIN 21
#define PULSES_PER_REVOLUTION 4

gpio_config_t velocity_tool_gpio_config;

SemaphoreHandle_t velocity_tool_sem;
static TickType_t countTicks = 0;

static void IRAM_ATTR velocity_tool_interrupt_handler(void *args) {
  static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  countTicks = xTaskGetTickCountFromISR();
  xSemaphoreGiveFromISR(velocity_tool_sem, pdFALSE);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

static inline void set_gpio_velocity_tool() {
  gpio_config(&velocity_tool_gpio_config);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(VELOCITY_TOOL_PIN, velocity_tool_interrupt_handler,
                       (void *)VELOCITY_TOOL_PIN);
}

void velocity_tool_init(void *p1) {
  float rpm = 0.0f;
  float velocity = 0.0f;
  uint64_t period_instants[2] = {0U};
  int period_diff = 0;
  char payload[20];

  ESP_LOGI(TAG, "Starting the velocity tool thread...");
  RingbufHandle_t *ring_buf = (RingbufHandle_t *)p1;
  set_gpio_velocity_tool();

  velocity_tool_sem = xSemaphoreCreateCounting(PULSES_PER_REVOLUTION, 0);
  if (velocity_tool_sem == NULL) {
    goto error;
  }

  while (true) {
    xSemaphoreTake(velocity_tool_sem, portMAX_DELAY);
    period_instants[0] = pdTICKS_TO_MS(countTicks);
    xSemaphoreTake(velocity_tool_sem, portMAX_DELAY);
    xSemaphoreTake(velocity_tool_sem, portMAX_DELAY);
    xSemaphoreTake(velocity_tool_sem, portMAX_DELAY);
    period_instants[1] = pdTICKS_TO_MS(countTicks);
    period_diff = period_instants[1] - period_instants[0];
    if (period_diff != 0) {
      rpm = (float)(((float)(FREQUENCY_IN_MILLIS)) / period_diff);
      velocity =
          rpm * 0.104719f; /* pi/30(conversão do RPM) * raio da roda * 3.6 */
      sprintf(payload, "velocity: %.5f", velocity);
      ESP_LOGI(TAG, "period %d ms rpm %f velocity %f", period_diff, rpm,
               velocity);
      UBaseType_t rc = xRingbufferSend(*ring_buf, &payload, sizeof(payload),
                                       pdMS_TO_TICKS(1));
      if (rc != pdTRUE) {
        printf("Failed to send item\n");
      }
    } else {
      ESP_LOGE(TAG, "ERROR");
    }
  }

error:
  ESP_LOGI(TAG, "Error creating semaphore");
  return;
}

gpio_config_t velocity_tool_gpio_config = {
    .pin_bit_mask = (1 << VELOCITY_TOOL_PIN),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_ENABLE,
    .intr_type = GPIO_INTR_POSEDGE,
};
