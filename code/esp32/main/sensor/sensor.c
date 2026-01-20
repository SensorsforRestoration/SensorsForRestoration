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
#include <time.h>
#include "rom/ets_sys.h"

#include "camera.h"
#include "logger.h"

#define TRIG_PIN 5  // need to check these pins
#define ECHO_PIN 18 // need to check these pins

static const char *TAG = "SENSOR";

// Persist across deep sleep
RTC_SLOW_ATTR uint16_t s_sequence_id = 1;
RTC_SLOW_ATTR uint32_t s_minutes = 0; // increments each wake
RTC_SLOW_ATTR uint16_t s_packet_num = 1;

esp_err_t camera_init(void);
esp_err_t camera_capture_color(uint8_t *r, uint8_t *g, uint8_t *b);

static volatile bool s_upload_requested = false;

const int wakeup_time_sec = 60;
uint8_t receiver_mac[] = {0x34, 0x5F, 0x45, 0x37, 0x8C, 0xA4}; // need to fill this in correctly for each sensor

static void setup_gpio(void)
{
    gpio_reset_pin(TRIG_PIN);
    gpio_set_direction(TRIG_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(TRIG_PIN, 0);

    gpio_reset_pin(ECHO_PIN);
    gpio_set_direction(ECHO_PIN, GPIO_MODE_INPUT);
}

static void recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *d, int len)
{
    (void)recv_info;

    if (len == sizeof(time_sync_packet_t))
    {
        time_sync_packet_t pkt;
        memcpy(&pkt, d, sizeof(pkt));
        struct timeval tv = {.tv_sec = pkt.timestamp, .tv_usec = 0};
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "Time synced to %lu", (unsigned long)pkt.timestamp);
        return;
    }

    if (len == sizeof(upload_request_t))
    {
        upload_request_t req;
        memcpy(&req, d, sizeof(req));
        if (req.magic == UPLOAD_MAGIC)
        {
            ESP_LOGI(TAG, "Upload requested by boat!");
            s_upload_requested = true;
        }
        return;
    }
}

static void init_esp_now(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    else
    {
        ESP_ERROR_CHECK(err);
    }

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

    err = esp_now_add_peer(&peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST)
        ESP_ERROR_CHECK(err);
}

static void send_one_record_as_packet(const log_record_t *rec, uint16_t idx, uint16_t total)
{
    packet_t pkt = {0};
    pkt.sequence_id = s_sequence_id;
    pkt.packet_num = idx;
    pkt.total = total;
    pkt.timestamp = rec->unix_s;
    pkt.sensor_id = 1;
    pkt.payload.depth_mm = rec->depth_mm;
    pkt.payload.r = rec->r;
    pkt.payload.g = rec->g;
    pkt.payload.b = rec->b;

    esp_err_t r = esp_now_send(receiver_mac, (uint8_t *)&pkt, sizeof(pkt));
    if (r != ESP_OK)
    {
        ESP_LOGW(TAG, "Send failed idx=%u: %s", idx, esp_err_to_name(r));
    }

    vTaskDelay(pdMS_TO_TICKS(15)); // small pacing
}

static bool time_is_valid(void)
{
    time_t now;
    time(&now);
    return (now > 1700000000); // ~late 2023; simple sanity
}

// is this the old code that needs to be replaced??
static int16_t readDepthMm(void)
{
    gpio_set_level(TRIG_PIN, 0);
    ets_delay_us(2);
    gpio_set_level(TRIG_PIN, 1);
    ets_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    const int64_t t0 = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 0)
    {
        if (esp_timer_get_time() - t0 > 30000)
            return -1;
    }

    const int64_t pulse_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 1)
    {
        if (esp_timer_get_time() - pulse_start > 30000)
            return -2;
    }
    const int64_t pulse_end = esp_timer_get_time();
    const int64_t dur_us = pulse_end - pulse_start;

    float mm = (float)dur_us * 0.343f / 2.0f;
    return (int16_t)mm;
}

void sensor(void)
{
    setup_gpio();
    init_esp_now();
    ESP_ERROR_CHECK(logger_init());

    // Measure depth
    int16_t depth_mm = readDepthMm();

    // Decide whether to do daily color (simple: once every 1440 minutes)
    // bool do_color_today = (s_minutes % 1440u) == 0; // roughly once/day
    bool do_color_today = true;
    uint8_t r = 0, g = 0, b = 0;
    uint8_t flags = 0;
    if (time_is_valid())
        flags |= 0x01;

    if (do_color_today)
    {
        if (camera_init() == ESP_OK && camera_capture_color(&r, &g, &b) == ESP_OK)
        {
            flags |= 0x02; // color_valid
        }
    }

    // Log record to flash
    log_record_t rec = {
        .unix_s = (uint32_t)(time_is_valid() ? time(NULL) : 0),
        .depth_mm = depth_mm,
        .r = r,
        .g = g,
        .b = b,
        .flags = flags};

    ESP_ERROR_CHECK(logger_append(&rec));
    s_minutes++;

    // If boat asked, upload everything and then clear
    if (s_upload_requested)
    {
        uint32_t count = 0;
        logger_count(&count);
        ESP_LOGI(TAG, "Uploading %lu records...", (unsigned long)count);

        // Send all records as packet_t sequence the receiver already understands
        logger_read_all_and_send(send_one_record_as_packet);

        // Clear log after upload
        logger_clear();

        // Start a new sequence/session
        s_sequence_id++;
        s_upload_requested = false;
    }

    ESP_LOGI(TAG, "Sleeping %d sec", wakeup_time_sec);
    esp_sleep_enable_timer_wakeup((uint64_t)wakeup_time_sec * 1000000ULL);
    esp_deep_sleep_start();
}