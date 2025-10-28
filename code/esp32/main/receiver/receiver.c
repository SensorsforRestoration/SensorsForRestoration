#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "data.h"
#include "esp_sntp.h"
#include "sys/time.h"
#include "receiver/storage.h"

static const char *TAG = "RECEIVER";

static esp_now_peer_info_t broadcastPeer =
    {
        .channel = 0,
        .encrypt = false,
        .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} // need to find the mac address of the other arduino
};

int num = 0;

static void recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *d, int len)
{
    if (len != sizeof(packet_t))
    {
        ESP_LOGI(TAG, "Unexpected data size: %d", len);
        return;
    }

    packet_t received;
    memcpy(&received, d, sizeof(received));
    ESP_LOGI(TAG,
             "From " MACSTR " | Packet Num: %lu | Time: %llu | Depth[2]=%.2f | Salinity[0]=%.2f | Temp[1]=%.2f",
             MAC2STR(recv_info->src_addr),
             received.packet_num,
             received.timestamp,
             received.payload.depth[2],
             received.payload.salinity[0],
             received.payload.temperature[1]);

    bool received_all = false;
    ESP_ERROR_CHECK(store_packet(&received, &received_all));
}

static void init_sntp(void)
{
    ESP_LOGI(TAG, "Initialising SNTP...");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2020 - 1900) && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for NTP time sync...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    if (retry == retry_count)
    {
        ESP_LOGW(TAG, "Time sync failed, using local clock");
    }
    else
    {
        ESP_LOGI(TAG, "Time synchronised");
    }
}

static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    ESP_LOGI(TAG, "Time sync send status: %s", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

void time_sync_task(void *pvParam)
{
    while (1)
    {
        time_t now;
        time(&now);

        time_sync_packet_t pkt = {.timestamp = (uint64_t)now};
        esp_err_t result = esp_now_send(broadcastPeer.peer_addr, (uint8_t *)&pkt, sizeof(pkt));

        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "Sent time sync: %llu", (unsigned long long)pkt.timestamp);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to send time sync: %s", esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // every 5 seconds
    }
}

void receiver(void)
{
    // When testing: ESP_ERROR_CHECK(nvs_flash_erase());
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
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));
    ESP_ERROR_CHECK(esp_now_add_peer(&broadcastPeer));

    init_sntp();

    ESP_ERROR_CHECK(storage_init());

    ESP_LOGI(TAG, "ESP-NOW receiver ready");

    xTaskCreate(time_sync_task, "time_sync_task", 4096, NULL, 5, NULL);

    /* static packet_t packet_1 = {
        .sequence_id = 12,
        .packet_num = 1,
        .total = 2,
    };

    static packet_t packet_2 = {
        .sequence_id = 12,
        .packet_num = 2,
        .total = 2,
    };

    bool received_all = false;

    ESP_ERROR_CHECK(store_packet(&packet_1, &received_all));
    ESP_LOGI(TAG, "received all: %s", received_all ? "true" : "false");
    ESP_ERROR_CHECK(store_packet(&packet_2, &received_all));
    ESP_LOGI(TAG, "received all: %s", received_all ? "true" : "false");
    */
}