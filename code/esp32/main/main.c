#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
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
