#include "esp_now.h"
#include "driver/uart.h"
#include "gps.h"
#include "esp_timer.h"

#define GPS_UART_NUM UART_NUM_2
#define GPS_UART_TX_PIN 17
#define GPS_UART_RX_PIN 16
#define GPS_UART_BAUDRATE 9600

esp_err_t gps_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = GPS_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    esp_err_t err = uart_param_config(GPS_UART_NUM, &uart_config);
    if (err != ESP_OK)
    {
        return err;
    }

    err = uart_set_pin(GPS_UART_NUM,
                       GPS_UART_TX_PIN, // TX → to GPS RX (optional)
                       GPS_UART_RX_PIN, // RX → from GPS TX
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
    {
        return err;
    }

    return uart_driver_install(GPS_UART_NUM, 2048, 0, 0, NULL, 0);
}

esp_err_t gps_read_line(char *out, size_t max_len)
{
    size_t idx = 0;
    uint8_t ch;
    int64_t start = esp_timer_get_time();

    while (1)
    {
        int len = uart_read_bytes(GPS_UART_NUM, &ch, 1, pdMS_TO_TICKS(20));

        if (len > 0)
        {
            if (ch == '\n')
            { // end of NMEA sentence
                if (idx < max_len - 1)
                {
                    out[idx] = 0;
                    return ESP_OK;
                }
                return ESP_ERR_INVALID_SIZE; // buffer too small
            }

            if (idx < max_len - 1)
            {
                out[idx++] = ch;
            }
            else
            {
                return ESP_ERR_ESPNOW_FULL; // buffer overflow
            }
        }

        // manual timeout in microseconds
        if ((esp_timer_get_time() - start) > (pdMS_TO_TICKS(1500) * (1000000 / configTICK_RATE_HZ)))
        {
            return ESP_ERR_TIMEOUT;
        }
    }
}