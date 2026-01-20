#include "ssd1306.h"
#include "data.h"
#include "string.h"
#include "esp_mac.h"

#define I2C_PORT I2C_NUM_0
#define SDA_PIN 21
#define SCL_PIN 22

SSD1306_t dev;

void display_init(void)
{
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&dev, 128, 64);
    ssd1306_clear_screen(&dev, false);
}

void display_text(int page, bool invert, char *text)
{
    ssd1306_display_text(&dev, page, text, strlen(text), invert);
}

void display_textf(int page, bool invert, const char *format, ...)
{
    char line[20];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    display_text(page, invert, line);
}

void receive_message(uint16_t sequence_id, uint8_t packet_num, uint16_t total, uint8_t *sensor_address, uint8_t packets_received)
{
    ssd1306_clear_screen(&dev, false);

    display_text(0, true, "PACKET RECEIVED");
    display_textf(1, false, "Sensor ID: " MACSTR, MAC2STR(sensor_address));
    display_textf(2, false, "Sequence ID: %d", sequence_id);
    display_textf(3, false, "Packet Number: %d", packet_num);
    display_text(4, false, "Packets Received");
    display_textf(5, false, "%d/%d", packets_received, total);
}

void new_sensor_message(uint8_t *sensor_address)
{
    ssd1306_clear_screen(&dev, false);

    display_text(0, true, "NEW SENSOR STARTED");
    display_textf(1, false, "Sensor ID: " MACSTR, MAC2STR(sensor_address));
}