#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"

#include "receiver/receiver.h"
#include "sensor/sensor.h"

enum Device
{
    SENSOR,
    RECEIVER,
};

const int MODE = RECEIVER;

void app_main(void)
{
    // Init NVS
    ESP_ERROR_CHECK(nvs_flash_init());

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
