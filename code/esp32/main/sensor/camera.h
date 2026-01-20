#pragma once

static inline void rgb565_to_rgb888(uint16_t pix, uint8_t *r, uint8_t *g, uint8_t *b);

esp_err_t camera_init(void);

esp_err_t camera_capture_color(uint8_t *out_r, uint8_t *out_g, uint8_t *out_b);