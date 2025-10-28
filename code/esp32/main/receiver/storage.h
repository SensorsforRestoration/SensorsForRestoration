#include "esp_now.h"

esp_err_t storage_init(void);
esp_err_t store_packet(packet_t *packet, bool *received_all);