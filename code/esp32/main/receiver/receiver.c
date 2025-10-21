#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "data.h"

static const char *TAG = "RECEIVER";

int num = 0;

static void recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *d, int len)
{
    if (len != sizeof(packet_t))
    {
        ESP_LOGW(TAG, "Unexpected data size: %d", len);
        return;
    }

    packet_t received;
    memcpy(&received, d, sizeof(received));
    ESP_LOGI(TAG, 
            "From " MACSTR " | Packet ID: %lu | Time: %llu | Depth[2]=%.2f | Salinity[0]=%.2f | Temp[1]=%.2f",
             MAC2STR(recv_info->src_addr),
             received.packet_id,
             recieved.timestamp,
             recieved.payload.depth[2],
             recieved.payload.salinity[0]
             recieved.payload.temperature[1],
             recieved.payload.ph[0]);
    num++;
}

void receiver(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    // Init Wi-Fi in STA mode
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Init ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));

    ESP_LOGI(TAG, "ESP-NOW receiver ready");
}