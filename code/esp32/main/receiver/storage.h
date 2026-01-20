#include "esp_now.h"

esp_err_t storage_init(void);
esp_err_t store_packet(uint8_t *address, data_packet_t *packet, bool *received_all);
esp_err_t store_sensor(uint8_t *address);