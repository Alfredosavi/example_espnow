// RECEIVER

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "espnow_example.h"


static const char *TAG = "ESP_NOW";

StaticQueue_t xStaticQueue;
uint8_t ucQueueStorageArea[BUFFER_QUEUE_LENGTH * BUFFER_QUEUE_SIZE];
QueueHandle_t xQueueBufferReceiver;

volatile payload_t payload = { 0 };

static uint8_t MAC_ADDR_BROADCAST[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; 
static uint8_t MAC_ADDR_UNICAST[ESP_NOW_ETH_ALEN] = { 0x3c, 0x61, 0x05, 0x11, 0xd0, 0x04 }; // MAC address of the transmitter

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
  ESP_LOGI(TAG, "[+CALLBACK] Receive data from: " MACSTR ", len: %d", MAC2STR(recv_info->src_addr), len);

  ESP_LOGI(TAG, "-------------------------------------------------");
  uint8_t *srcMacAddr = recv_info->src_addr;
  uint8_t *desMacAddr = recv_info->des_addr;

  ESP_LOGI(TAG, "srcMacAddr: " MACSTR ", desMacAddr: " MACSTR, MAC2STR(srcMacAddr), MAC2STR(desMacAddr));
  ESP_LOGI(TAG, "RSSI: %d, Channel: %d, TimeStamp: %d", recv_info->rx_ctrl->rssi, recv_info->rx_ctrl->channel, recv_info->rx_ctrl->timestamp);

  char bufferzin[len * 4];
  int index = 0;
  for (int i = 0; i < len; i++)
    index += sprintf(bufferzin + index, "%02X ", data[i]);
  ESP_LOGI(TAG, "PAYLOAD: %s", bufferzin);
  ESP_LOGI(TAG, "-------------------------------------------------");

  if (len == sizeof(payload_t)){
    if (xQueueSend(xQueueBufferReceiver, data, 10) != pdTRUE){
      ESP_LOGW(TAG, "Send receive queue fail");
    }
  }
}


static void espnow_task(void *pvParameter){

  while (xQueueReceive(xQueueBufferReceiver, &payload, portMAX_DELAY) == pdTRUE){
    ESP_LOGI(TAG, "[+TASK] ID: %d, N_BYTES: %d, PAYLOAD: %s", payload.id, payload.n_bytes, payload.payload);

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

static esp_err_t espnow_init(void)
{
    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    /* Add broadcast peer information to peer list. */
    // esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    // if (peer == NULL) {
    //     ESP_LOGE(TAG, "Malloc peer information fail");
    //     return ESP_FAIL;
    // }

    // memset(peer, 0, sizeof(esp_now_peer_info_t));
    // peer->channel = CONFIG_ESPNOW_CHANNEL;
    // peer->ifidx = ESPNOW_WIFI_IF;
    // peer->encrypt = false;
    // memcpy(peer->peer_addr, MAC_ADDR_BROADCAST, ESP_NOW_ETH_ALEN);
    // ESP_ERROR_CHECK(esp_now_add_peer(peer));
    // free(peer);

    esp_now_peer_info_t peer;

    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    peer.channel = CONFIG_ESPNOW_CHANNEL;
    peer.ifidx = ESPNOW_WIFI_IF;
    peer.encrypt = false;
    memcpy(peer.peer_addr, MAC_ADDR_BROADCAST, ESP_NOW_ETH_ALEN);

    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    return ESP_OK;
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();
    espnow_init();

    uint8_t mac[6] = { 0 };
    esp_wifi_get_mac(ESPNOW_WIFI_MODE, mac);
    ESP_LOGI(TAG, "MAC_ADDR: "MACSTR"", MAC2STR(mac));

    xQueueBufferReceiver = xQueueCreateStatic(BUFFER_QUEUE_LENGTH, BUFFER_QUEUE_SIZE, ucQueueStorageArea, &xStaticQueue);
    xTaskCreate(espnow_task, "espnow_task", 2048, NULL, 4, NULL);
}
