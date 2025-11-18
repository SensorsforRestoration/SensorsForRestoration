#include "esp_sleep.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "data.h"
#include <sys/time.h>
#include <string.h>
#include "driver/gpio.h"

#define TRIG_PIN 5
#define ECHO_PIN 18

static const char *TAG = "SENSOR";

RTC_SLOW_ATTR data current_data;
RTC_SLOW_ATTR int count = 0;
RTC_SLOW_ATTR uint32_t packet_num = 0;

const int wakeup_time_sec = 1;
uint8_t receiver_mac[] = {0x34, 0x5F, 0x45, 0x37, 0x8C, 0xA4};

void setup_gpio()
{
    esp_rom_gpio_pad_select_gpio(TRIG_PIN);
    gpio_set_direction(TRIG_PIN, GPIO_MODE_OUTPUT);
    esp_rom_gpio_pad_select_gpio(ECHO_PIN);
    gpio_set_direction(ECHO_PIN, GPIO_MODE_INPUT);
}

static void recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *d, int len)
{
    if (len == sizeof(time_sync_packet_t))
    {
        time_sync_packet_t pkt;
        memcpy(&pkt, d, sizeof(pkt));
        struct timeval tv = {.tv_sec = pkt.timestamp, .tv_usec = 0};
        settimeofday(&tv, NULL);
        ESP_LOGI("SENSOR", "Clock synchronised to %llu", pkt.timestamp);
        return;
    }
}

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
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, receiver_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

static void send_packet(void)
{
    init_esp_now();

    packet_t packet = {0};

    packet.packet_num = packet_num++;
    // esp_read_mac(packet.sensor_id, 123);
    memcpy(&packet.payload, &current_data, sizeof(data));

    ESP_LOGI(TAG, "Sending full packet...");

    esp_err_t result = esp_now_send(receiver_mac, (uint8_t *)&packet, sizeof(packet));
    if (result == ESP_OK)
    {
        ESP_LOGI(TAG, "ESP-NOW send success (packet ID %lu)", packet.packet_num);
    }
    else
    {
        ESP_LOGI(TAG, "ESP-NOW send failed: %s", esp_err_to_name(result));
    }

    vTaskDelay(pdMS_TO_TICKS(100));
}

// check and change this code
float readDistance()
{
    gpio_set_level(TRIG_PIN, 0);
    // esp_timer_delete(2); //debug this part
    gpio_set_level(TRIG_PIN, 1);
    // esp_timer_delete(10); //debug this part
    gpio_set_level(TRIG_PIN, 0);

    uint64_t pulse_start = 0;
    uint64_t pulse_end = 0;

    while (gpio_get_level(ECHO_PIN) == 0)
    {
        // Do nothing or handle timeout
    }

    pulse_start = esp_timer_get_time();

    while (gpio_get_level(ECHO_PIN) == 1)
    {
        // Do nothing or handle timeout
    }

    pulse_end = esp_timer_get_time();

    uint64_t duration = pulse_end - pulse_start;

    float distance = duration * 0.0344 / 2; // 0.0344 cm/µs for speed of sound in air
    return distance;
}

void sensor(void)
{
    setup_gpio();

    gpio_reset_pin(TRIG_PIN);
    gpio_set_direction(TRIG_PIN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(ECHO_PIN);
    gpio_set_direction(ECHO_PIN, GPIO_MODE_INPUT);

    count++;
    // current_data.depth[count] = readDistance();

    if (count % 5 == 0)
    {
        current_data.temperature[(count / 5) - 1] = count;
    }

    if (count % 10 == 0)
    {
        current_data.salinity[count] = count;
        send_packet();
        count = 0;
    }

    time_t now;
    time(&now);
    ESP_LOGI(TAG, "Current time: %s", ctime(&now));

    ESP_LOGI(TAG, "Entering deep sleep for %d seconds...", wakeup_time_sec);
    esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000ULL);

    // Deep sleep — CPU powers off, app_main() will run on wake
    esp_deep_sleep_start();
}