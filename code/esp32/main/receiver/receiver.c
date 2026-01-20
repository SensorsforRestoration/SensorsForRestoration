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
#include "receiver/display.h"
#include "receiver/gps.h"

static const char *TAG = "RECEIVER";

#define I2C_PORT I2C_NUM_0
#define SDA_PIN 21
#define SCL_PIN 22

static esp_now_peer_info_t broadcastPeer =
    {
        .channel = 0,
        .encrypt = false,
        .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

esp_err_t must_peer(const uint8_t *address)
{
    if (esp_now_is_peer_exist(address))
    {
        return ESP_OK;
    }

    esp_now_peer_info_t peerInfo = {
        .channel = 0,
        .encrypt = false,
    };
    memcpy(peerInfo.peer_addr, address, ESP_NOW_ETH_ALEN);
    return esp_now_add_peer(&peerInfo);
}

static void recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *d, int len)
{
    switch (len)
    {
    case sizeof(data_packet_t):
        data_packet_t packet;
        memcpy(&packet, d, sizeof(packet));

        ESP_LOGI(TAG,
                 "From " MACSTR " | Packet Num: %lu | Time: %llu | Depth[2]=%.2f | Salinity[0]=%.2f | Temp[1]=%.2f",
                 MAC2STR(recv_info->src_addr),
                 packet.packet_num,
                 packet.timestamp,
                 packet.data.depth[2],
                 packet.data.salinity[0],
                 packet.data.temperature[1]);

        bool received_all = false;
        ESP_ERROR_CHECK(store_packet(recv_info->src_addr, &packet, &received_all));
        if (received_all)
        {
            ESP_LOGI(TAG, "All packets for sequence %u received", packet.sequence_id);
        }

        break;

    case sizeof(broadcast_packet_t):
        broadcast_packet_t broadcast;
        memcpy(&broadcast, d, sizeof(broadcast));

        if (broadcast.broadcast_type == BROADCAST_TYPE_NEW_SENSOR)
        {
            ESP_LOGI(TAG, "New sensor detected");

            esp_err_t err = store_sensor(recv_info->src_addr);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to store sensor: %s", esp_err_to_name(err));
            }

            time_t now;
            time(&now);
            sensor_start_packet_t start_pkt = {.timestamp = (uint64_t)time(NULL)};

            err = must_peer(recv_info->src_addr);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to add peer: %s", esp_err_to_name(err));
                return;
            }

            err = esp_now_send(recv_info->src_addr, (uint8_t *)&start_pkt, sizeof(start_pkt));
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to send start packet: %s", esp_err_to_name(err));
                return;
            }

            new_sensor_message(recv_info->src_addr);
            // TODO: maybe we should have some confirmation signal?
        }
        else
        {
            ESP_LOGE(TAG, "Bad broadcast type: %d", broadcast.broadcast_type);
        }

        break;
    default:
        ESP_LOGE(TAG, "Unexpected data size: %d", len);
    }
}

static void init_sntp(void)
{
    ESP_LOGI(TAG, "Initialising SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

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

static void upload_request_task(void *pv)
{
    while (1)
    {
        upload_request_t req = {.magic = UPLOAD_MAGIC};
        esp_now_send(broadcastPeer.peer_addr, (uint8_t *)&req, sizeof(req));
        vTaskDelay(pdMS_TO_TICKS(2000)); // every 2s while boat is nearby
    }
}

static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    ESP_LOGI(TAG, "Message send status: %s", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

void receiver(void)
{
    // Uncomment for testing:
    ESP_ERROR_CHECK(nvs_flash_erase());
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

    xTaskCreate(upload_request_task, "upload_request_task", 2048, NULL, 5, NULL);

    ESP_ERROR_CHECK(storage_init());

    display_init();

    ESP_ERROR_CHECK(gps_init());

    init_sntp();

    ESP_LOGI(TAG, "ESP-NOW receiver ready");
}