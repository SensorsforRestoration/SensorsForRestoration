#include "esp_now.h"

#define ERR_BUFFER_OVERFLOW 0x110;

esp_err_t gps_init(void);
esp_err_t gps_read_line(char *out, size_t max_len);