#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef struct __attribute__((packed)) {
    uint32_t unix_s;     // 0 if not synced
    int16_t  depth_mm;   // depth in mm
    uint8_t  r, g, b;    // 0 unless daily color sample
    uint8_t  flags;      // bit0=time_valid, bit1=color_valid
} log_record_t;

esp_err_t logger_init(void);
esp_err_t logger_append(const log_record_t *rec);
esp_err_t logger_read_all_and_send(void (*send_fn)(const log_record_t *rec, uint16_t idx, uint16_t total));
esp_err_t logger_clear(void);
esp_err_t logger_count(uint32_t *out_count);
