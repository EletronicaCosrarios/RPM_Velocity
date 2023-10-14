#include "../include/esp_now_tool.h"
#include "../include/utils.h"
#include "../include/wifi_init.h"
#include "esp_crc.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TAG "esp_now_sender"

#define ESPNOW_MAXDELAY 512

static xQueueHandle esp_now_queue;

static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF,
                                                  0xFF, 0xFF, 0xFF};

static void esp_now_send_callback(const uint8_t *mac_addr,
                                  esp_now_send_status_t status) {
  esp_now_event_t evt;
  esp_now_event_send_cb_t *send_cb = &evt.info.send_cb;

  if (mac_addr == NULL) {
    ESP_LOGE(TAG, "Send cb arg error");
    return;
  }

  memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
  send_cb->status = status;
  if (xQueueSend(esp_now_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
    ESP_LOGW(TAG, "Send send queue fail");
  }
}

/* Prepare ESPNOW data to be sent. */
void prepare_data_to_send_esp_now(esp_now_send_param_t *send_param,
                                  char *payload) {
  esp_now_data_t *buf = (esp_now_data_t *)send_param->buffer;

  assert(send_param->len >= sizeof(esp_now_data_t));

  buf->state = send_param->state;
  buf->crc = 0;
  buf->magic = send_param->magic;
  strcpy(buf->payload, payload);
  buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
}

static void set_peer_esp_now(esp_now_peer_info_t *peer) {
  memset(peer, 0, sizeof(esp_now_peer_info_t));
  peer->channel = CONFIG_ESPNOW_CHANNEL;
  peer->ifidx = ESPNOW_WIFI_IF;
  peer->encrypt = false;
  memcpy(peer->peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
  ESP_ERROR_CHECK(esp_now_add_peer(peer));
  free(peer);
}

static void set_parameters_esp_now(esp_now_send_param_t *parameters) {
  memset(parameters, 0, sizeof(esp_now_send_param_t));
  parameters->unicast = false;
  parameters->broadcast = true;
  parameters->state = 0;
  parameters->magic = esp_random();
  parameters->count = CONFIG_ESPNOW_SEND_COUNT;
  parameters->delay = CONFIG_ESPNOW_SEND_DELAY;
  parameters->len = sizeof(esp_now_data_t);
  parameters->buffer = malloc(sizeof(esp_now_data_t));
  memcpy(parameters->dest_mac, broadcast_mac, ESP_NOW_ETH_ALEN);
}

static esp_err_t esp_now_initialize() {
  esp_now_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(esp_now_event_t));
  if (esp_now_queue == NULL) {
    ESP_LOGE(TAG, "Create mutex fail");
    return ESP_FAIL;
  }

  ESP_ERROR_CHECK(esp_now_init());
  ESP_ERROR_CHECK(esp_now_register_send_cb(esp_now_send_callback));
  ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

  return ESP_OK;
}

void send_message_esp_now(esp_now_send_param_t *send_param) {
  if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) !=
      ESP_OK) {
    ESP_LOGE(TAG, "Send error");
    free(send_param->buffer);
    free(send_param);
    vSemaphoreDelete(esp_now_queue);
    esp_now_deinit();
    ;
  }
}

void esp_now_sender_init(void *p1) {
  esp_now_send_param_t *send_param = malloc(sizeof(esp_now_send_param_t));
  esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
  RingbufHandle_t *ring_buffer = (RingbufHandle_t *)p1;
  size_t item_size;
  esp_now_event_t evt;
  char *payload;

  wifi_init();
  esp_now_initialize();
  set_peer_esp_now(peer);
  set_parameters_esp_now(send_param);

  ESP_LOGI(TAG, "Start sending broadcast data");
  send_message_esp_now(send_param);

  while (xQueueReceive(esp_now_queue, &evt, portMAX_DELAY) == pdTRUE) {
    esp_now_event_send_cb_t *send_cb = &evt.info.send_cb;
    memcpy(send_param->dest_mac, send_cb->mac_addr, ESP_NOW_ETH_ALEN);
    payload =
        (char *)xRingbufferReceive(*ring_buffer, &item_size, portMAX_DELAY);
    ESP_LOGI(TAG, "send data '%s' to " MACSTR "", payload,
             MAC2STR(send_cb->mac_addr));
    prepare_data_to_send_esp_now(send_param, payload);
    send_message_esp_now(send_param);
    vRingbufferReturnItem(*ring_buffer, (void *)payload);
  }
  vTaskDelay(portMAX_DELAY);
}
