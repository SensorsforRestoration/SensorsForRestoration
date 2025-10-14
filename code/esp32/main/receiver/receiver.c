#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"

static const char *TAG = "RECEIVER";

int num = 0;

static void recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    int received_value;
    memcpy(&received_value, data, sizeof(int));
    ESP_LOGI(TAG, "Packet num %d data len %d from " MACSTR,
             num,
             len,
             MAC2STR(recv_info->src_addr));
    num++;
}

void receiver(void)
{

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