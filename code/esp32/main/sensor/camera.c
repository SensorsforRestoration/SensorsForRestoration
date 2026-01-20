#include "esp_camera.h"
#include "esp_err.h"
#include "esp_log.h"
#include <string.h>
#include <stdint.h>

static const char *TAG = "CAMERA";

// ==============================
// Camera pin map (XIAO ESP32-S3 Sense)
// ==============================
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   10
#define SIOD_GPIO_NUM   40   // CAM_SDA
#define SIOC_GPIO_NUM   39   // CAM_SCL

#define Y2_GPIO_NUM     15
#define Y3_GPIO_NUM     17
#define Y4_GPIO_NUM     18
#define Y5_GPIO_NUM     16
#define Y6_GPIO_NUM     14
#define Y7_GPIO_NUM     12
#define Y8_GPIO_NUM     11
#define Y9_GPIO_NUM     48

#define VSYNC_GPIO_NUM  38
#define HREF_GPIO_NUM   47
#define PCLK_GPIO_NUM   13

// Many ESP32 camera drivers deliver BGR565; if your reds/blues are swapped, set to 1.
#define RGB565_IS_BGR 1

// Average a (2R+1)x(2R+1) window around the center
#define AVG_RADIUS 3

static bool s_cam_inited = false;

static inline void rgb565_to_rgb888(uint16_t pix, uint8_t *r, uint8_t *g, uint8_t *b)
{
    // unpack 5-6-5
    uint8_t R = (uint8_t)(((pix >> 11) & 0x1F) * 255 / 31);
    uint8_t G = (uint8_t)(((pix >> 5)  & 0x3F) * 255 / 63);
    uint8_t B = (uint8_t)(( pix        & 0x1F) * 255 / 31);

#if RGB565_IS_BGR
    // swap R/B
    *r = B;
    *g = G;
    *b = R;
#else
    *r = R;
    *g = G;
    *b = B;
#endif
}

esp_err_t camera_init(void)
{
    if (s_cam_inited) return ESP_OK;

    camera_config_t config = {0};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;

    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;

    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;

    // We want color metrics, so RGB565 is fine
    config.pixel_format = PIXFORMAT_RGB565;

    // Keep it modest for speed/memory. Color from center doesn’t need VGA.
    config.frame_size   = FRAMESIZE_QVGA; // 320x240
    config.fb_count     = 1;
    config.grab_mode    = CAMERA_GRAB_LATEST;

    // Use PSRAM if present; if your board doesn’t have PSRAM, you may need CAMERA_FB_IN_DRAM
    config.fb_location  = CAMERA_FB_IN_PSRAM;

    ESP_LOGI(TAG, "Initializing camera...");
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_cam_inited = true;
    ESP_LOGI(TAG, "Camera initialized");
    return ESP_OK;
}

esp_err_t camera_capture_color(uint8_t *out_r, uint8_t *out_g, uint8_t *out_b)
{
    if (!out_r || !out_g || !out_b) return ESP_ERR_INVALID_ARG;

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGW(TAG, "Failed to get frame buffer");
        return ESP_FAIL;
    }

    if (fb->format != PIXFORMAT_RGB565) {
        ESP_LOGW(TAG, "Unexpected format: %d", fb->format);
        esp_camera_fb_return(fb);
        return ESP_FAIL;
    }

    const uint16_t w = fb->width;
    const uint16_t h = fb->height;
    const uint16_t cx = w / 2;
    const uint16_t cy = h / 2;

    // Frame buffer is bytes; interpret pixels carefully
    const uint8_t *buf8 = (const uint8_t *)fb->buf;

    // Compute stride bytes (some drivers pad rows)
    size_t stride_bytes = (fb->height > 0) ? (fb->len / fb->height) : (size_t)w * 2;
    if (stride_bytes < (size_t)w * 2) stride_bytes = (size_t)w * 2;

    uint32_t sumR = 0, sumG = 0, sumB = 0;
    uint32_t count = 0;

    const int R = AVG_RADIUS;
    for (int dy = -R; dy <= R; dy++) {
        int y = (int)cy + dy;
        if (y < 0 || y >= (int)h) continue;

        size_t row_off = (size_t)y * stride_bytes;
        for (int dx = -R; dx <= R; dx++) {
            int x = (int)cx + dx;
            if (x < 0 || x >= (int)w) continue;

            size_t pix_off = row_off + (size_t)x * 2;
            if (pix_off + 1 >= fb->len) continue;

            // little-endian 16-bit
            uint16_t pix = (uint16_t)buf8[pix_off] | ((uint16_t)buf8[pix_off + 1] << 8);

            uint8_t r,g,b;
            rgb565_to_rgb888(pix, &r, &g, &b);

            sumR += r; sumG += g; sumB += b;
            count++;
        }
    }

    esp_camera_fb_return(fb);

    if (count == 0) {
        *out_r = *out_g = *out_b = 0;
        return ESP_FAIL;
    }

    *out_r = (uint8_t)(sumR / count);
    *out_g = (uint8_t)(sumG / count);
    *out_b = (uint8_t)(sumB / count);

    return ESP_OK;
}
