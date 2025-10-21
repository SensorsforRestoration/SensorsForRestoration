#include "esp_sleep.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "data.h"

static const char *TAG = "SENSOR";

RTC_SLOW_ATTR data current_data;
RTC_SLOW_ATTR int count = 0;
RTC_SLOW_ATTR uint32_t packet_id = 0;

const int wakeup_time_sec = 1;
uint8_t receiver_mac[] = {0x3C, 0xE9, 0x0E, 0x72, 0x0A, 0xFC};

static void init_esp_now(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, receiver_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

static void send_packet(void)
{
    packet_t packet = {0};

    packet.packet_id = packet_id++;
    packet.timestamp = esp_timer_get_time();
    esp_read_mac(packet.sensor_id, ESP_MAC_WIFI_STA);

    memcpy(&packet.payload, &current_data, sizeof(data));

    ESP_LOGI(TAG, "Initialising ESP-NOW and sending full packet...");
    init_esp_now();

    esp_err_t result = esp_now_send(receiver_mac, (uint8_t *)&packet, sizeof(packet));
    if (result == ESP_OK)
    {
        ESP_LOGI(TAG, "ESP-NOW send success (packet ID %lu)", packet.packet_id);
    }
    else
    {
        ESP_LOGI(TAG, "ESP-NOW send failed: %s", esp_err_to_name(result));
    }

    vTaskDelay(pdMS_TO_TICKS(100));
}

void sensor(void)
{
    // Increment count in RTC memory
    count++;
    current_data.depth[count] = count;

    if (count % 5 == 0)
    {
        current_data.temperature[(count / 5) - 1] = count;
    }

    if (count % 10 == 0)
    {
        current_data.salinity[0] = count;
        send_packet();
        count = 0;
    }

    ESP_LOGI(TAG, "Entering deep sleep for %d seconds...", wakeup_time_sec);
    esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000ULL);

    // Deep sleep — CPU powers off, app_main() will run on wake
    esp_deep_sleep_start();
}
