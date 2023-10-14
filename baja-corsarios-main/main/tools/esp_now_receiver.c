#include "../include/esp_now_tool.h"
#include "../include/wifi_init.h"
#include "esp_crc.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TAG "esp_now_receiver"

#define ESPNOW_MAXDELAY 512

static xQueueHandle esp_now_queue;

static void esp_now_receive_callback(const uint8_t *mac_addr,
                                     const uint8_t *data, int len) {
  esp_now_event_t evt;
  esp_now_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

  if (mac_addr == NULL || data == NULL || len <= 0) {
    ESP_LOGE(TAG, "Receive cb arg error");
    return;
  }

  memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
  recv_cb->data = malloc(len);
  if (recv_cb->data == NULL) {
    ESP_LOGE(TAG, "Malloc receive data fail");
    return;
  }
  memcpy(recv_cb->data, data, len);
  recv_cb->data_len = len;
  if (xQueueSend(esp_now_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
    ESP_LOGW(TAG, "Send receive queue fail");
    free(recv_cb->data);
  }
}

int esp_now_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state,
                       char *payload, int *magic) {
  esp_now_data_t *buf = (esp_now_data_t *)data;
  uint16_t crc, crc_cal = 0;

  if (data_len < sizeof(esp_now_data_t)) {
    ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
    return -1;
  }

  *state = buf->state;
  *magic = buf->magic;
  strcpy(payload, buf->payload);
  crc = buf->crc;
  buf->crc = 0;
  crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

  if (crc_cal == crc) {
    return 0;
  }

  return -1;
}

static void set_peer_esp_now(esp_now_peer_info_t *peer,
                             esp_now_event_recv_cb_t *recv_cb) {
  memset(peer, 0, sizeof(esp_now_peer_info_t));
  peer->channel = CONFIG_ESPNOW_CHANNEL;
  peer->ifidx = ESPNOW_WIFI_IF;
  peer->encrypt = true;
  memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
  memcpy(peer->peer_addr, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
  ESP_ERROR_CHECK(esp_now_add_peer(peer));
  free(peer);
}

static esp_err_t esp_now_initialize() {
  esp_now_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(esp_now_event_t));
  if (esp_now_queue == NULL) {
    ESP_LOGE(TAG, "Create mutex fail");
    return ESP_FAIL;
  }

  ESP_ERROR_CHECK(esp_now_init());
  ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_receive_callback));
  ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

  return ESP_OK;
}

void esp_now_receiver_init(void *p1) {
  ARGUNSED(p1);
  esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
  esp_now_event_t evt;

  char payload[20];
  uint8_t recv_state = 0U;
  int recv_magic = 0;

  wifi_init();
  esp_now_initialize();
  ESP_LOGI(TAG, "Start receiving broadcast data");

  while (xQueueReceive(esp_now_queue, &evt, portMAX_DELAY) == pdTRUE) {
    esp_now_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    esp_now_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, payload,
                       &recv_magic);
    free(recv_cb->data);
    ESP_LOGI(TAG, "Receive '%s' from: " MACSTR ", len: %d", payload,
             MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

    if (esp_now_is_peer_exist(recv_cb->mac_addr) == false) {
      set_peer_esp_now(peer, recv_cb);
    }
  }
  vTaskDelay(portMAX_DELAY);
}
