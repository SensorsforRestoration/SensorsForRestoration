#include "esp_now.h"

void display_init(void);
void receive_message(uint16_t sequence_id, uint8_t packet_num, uint16_t total, uint8_t *sensor_address, uint8_t packets_received);
void display_text(int page, bool invert, char *text);
void new_sensor_message(uint8_t *sensor_address);