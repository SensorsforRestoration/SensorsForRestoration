#include "esp_now.h"

void display_init(void);
void receive_message(uint16_t sequence_id, uint8_t packet_num, uint16_t total, uint16_t sensor_id, uint8_t packets_received);
void display_text(int page, bool invert, char *text);