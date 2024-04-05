// TRANSMITTER

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
                                                            
static uint8_t MAC_ADDR_BROADCAST[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t MAC_ADDR_UNICAST[ESP_NOW_ETH_ALEN] = { 0x24, 0xdc, 0xc3, 0x4a, 0xfe, 0x99 }; // MAC address of the receiver

volatile payload_t payload = {
    .id = 0x01,
    .function = 0xF2,
    .n_bytes = 12,
    .payload = "Hello World"
  };


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
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif
}


static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  if(status == ESP_NOW_SEND_SUCCESS)
    ESP_LOGI(TAG, "[+CALLBACK] Send success");
  else
    ESP_LOGE(TAG, "[+CALLBACK] Send error");
}

static void espnow_task(void *pvParameter)
{
    while(true){   
      if (esp_now_send(MAC_ADDR_BROADCAST, (uint8_t *)&payload, sizeof(payload)) == ESP_OK){
        ESP_LOGI(TAG, "[+TASK] Send success. Payload Size: %d", sizeof(payload));
        payload.id++;
      }else{
        ESP_LOGE(TAG, "[+TASK] Send error");
      }

      vTaskDelay(8000 / portTICK_PERIOD_MS);
    }
}

static esp_err_t espnow_init(void)
{
    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
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
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    wifi_init();
    espnow_init();

    uint8_t mac[6] = { 0 };
    esp_wifi_get_mac(ESPNOW_WIFI_MODE, mac);
    ESP_LOGI(TAG, "MAC_ADDR: "MACSTR"", MAC2STR(mac));

    xTaskCreate(espnow_task, "espnow_task", 2048, NULL, 4, NULL);
}
