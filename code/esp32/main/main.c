<<<<<<< HEAD
/**
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
=======
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
>>>>>>> dbc69586aa8548d476daf9ff4f1c6e61825864b6
#include "esp_log.h"
#include "esp_mac.h"

#include "receiver/receiver.h"
#include "sensor/sensor.h"

enum Device
{
    SENSOR,
    RECEIVER,
};

const static int MODE = RECEIVER;

void app_main(void)
{
    switch (MODE)
    {
    case SENSOR:
        sensor();
        break;
    case RECEIVER:
        receiver();
        break;
    default:
        ESP_LOGE("STARTUP", "Invalid mode selected");
    }
}

**/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"

static const char *TAG = "ESP_NOW_SENDER";

// Replace this MAC address with your receiver's
static uint8_t receiver_mac[] = {0xF0, 0x24, 0xF9, 0x0C, 0xD2, 0x78};

// Structure of data packet
typedef struct __attribute__((packed)) {
    int id;
    float temperature;
    float humidity;
} data_packet_t;

// Callback when data is sent
static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ESP_LOGI(TAG, "Send Status: %s", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    // ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));

    // Add peer
    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, receiver_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    ESP_LOGI(TAG, "ESP-NOW Sender initialized.");

    while (true) {
        data_packet_t packet = {
            .id = 1,
            .temperature = (float)(rand() % 1000) / 10.0f, // 0.0 - 100.0°C
            .humidity = (float)(rand() % 1000) / 10.0f     // 0.0 - 100.0%
        };

        esp_err_t result = esp_now_send(receiver_mac, (uint8_t *)&packet, sizeof(packet));

        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Data sent: ID=%d, Temp=%.1f°C, Hum=%.1f%%", packet.id, packet.temperature, packet.humidity);
        } else {
            ESP_LOGE(TAG, "Send error: %s", esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
