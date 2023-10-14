#include "../include/motor_rpm.h"
#include "../include/utils.h"
#include "driver/gpio.h"
#include "freertos/ringbuf.h"
#include "soc/rtc_wdt.h"

#define TAG "motor_rpm"

#define MOTOR_RPM_PIN 23
#define MOTOR_PULSES_TO_RPM 2

SemaphoreHandle_t motor_rpm_sem;

gpio_config_t motor_rpm_gpio_config;

static TickType_t countTicks = 0;

static uint64_t periods[2] = {0U, 0U};
static int period_diff = 0;
static float rpm = 0.0f;

static void IRAM_ATTR motor_rpm_interrupt_handler(void *args) {
  static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  countTicks = xTaskGetTickCountFromISR();
  xSemaphoreGiveFromISR(motor_rpm_sem, pdFALSE);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

static void set_gpio_motor_rpm() {
  gpio_config(&motor_rpm_gpio_config);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(MOTOR_RPM_PIN, motor_rpm_interrupt_handler,
                       (void *)MOTOR_RPM_PIN);
}

void motor_rpm_init(void *p1) {
  ESP_LOGI(TAG, "Starting the rpm motor thread...");
  RingbufHandle_t *ring_buf = (RingbufHandle_t *)p1;
  set_gpio_motor_rpm();
  char payload[20];

  motor_rpm_sem = xSemaphoreCreateCounting(MOTOR_PULSES_TO_RPM, 0);
  if (motor_rpm_sem == NULL) {
    goto error;
  }

  while (true) {
    xSemaphoreTake(motor_rpm_sem, portMAX_DELAY);
    periods[0] = pdTICKS_TO_MS(countTicks);
    xSemaphoreTake(motor_rpm_sem, portMAX_DELAY);
    periods[1] = pdTICKS_TO_MS(countTicks);
    period_diff = periods[1] - periods[0];
    if (period_diff != 0) {
      rpm = (float)(((float)(FREQUENCY_IN_MILLIS)) / period_diff);
      ESP_LOGI(TAG, "period %d ms rpm %f", period_diff, rpm);
      sprintf(payload, "rpm: %.5f", rpm);
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

gpio_config_t motor_rpm_gpio_config = {
    .pin_bit_mask = (1 << MOTOR_RPM_PIN),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_NEGEDGE,
};
