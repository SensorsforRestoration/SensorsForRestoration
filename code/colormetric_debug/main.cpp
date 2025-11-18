#include <Arduino.h>
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "WiFi.h"
#include "esp_http_server.h"
#include "esp_log.h"

// ==============================
// Camera pin map (XIAO ESP32-S3 Sense)
// ==============================
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   10
#define SIOD_GPIO_NUM   40   // CAM_SDA
#define SIOC_GPIO_NUM   39   // CAM_SCL

#define Y2_GPIO_NUM     15   // DVP_Y2
#define Y3_GPIO_NUM     17   // DVP_Y3
#define Y4_GPIO_NUM     18   // DVP_Y4
#define Y5_GPIO_NUM     16   // DVP_Y5
#define Y6_GPIO_NUM     14   // DVP_Y6
#define Y7_GPIO_NUM     12   // DVP_Y7
#define Y8_GPIO_NUM     11   // DVP_Y8
#define Y9_GPIO_NUM     48   // DVP_Y9

#define VSYNC_GPIO_NUM  38
#define HREF_GPIO_NUM   47
#define PCLK_GPIO_NUM   13

// ==============================
// Tunables
// ==============================

// Many ESP32 camera drivers deliver BGR565, not RGB565.
// If your reds/blues look swapped, change the compile-time default below.
// You can also override per-request with the ?swap=0|1 query parameter on /frame.ppm
#define RGB565_IS_BGR 1

// runtime flag (initialized from compile-time default) — can be overridden per-request
static bool rgb565_is_bgr = (RGB565_IS_BGR != 0);

// Average a (2*R+1)x(2*R+1) window around the center.
// 3 => 7x7 window. Increase if you want even smoother readings.
#define AVG_RADIUS 3

// Optionally freeze auto exposure/white-balance after warmup to
// stabilize color (milliseconds). Set to 0 to keep auto controls on.
#define AUTO_FREEZE_AFTER_MS 1500

// ==============================
// Color naming helpers
// ==============================
struct NamedColor {
  const char* name;
  uint8_t r, g, b;
};

// ~18 distinct, easy-to-recognize colors
static const NamedColor PALETTE[] = {
  {"Black",     0,   0,   0},
  {"White",   255, 255, 255},
  {"Gray",    128, 128, 128},
  {"Red",     220,  20,  60},
  {"Green",    34, 139,  34},
  {"Blue",     30, 144, 255},
  {"Yellow",  255, 215,   0},
  {"Cyan",      0, 255, 255},
  {"Magenta", 255,   0, 255},
  {"Orange",  255, 140,   0},
  {"Pink",    255, 105, 180},
  {"Purple",  147, 112, 219},
  {"Brown",   139,  69,  19},
  {"Lime",    191, 255,   0},
  {"Navy",      0,   0, 128},
  {"Teal",      0, 128, 128},
  {"Olive",   128, 128,   0},
  {"Maroon",  128,   0,   0}
};
static const size_t PALETTE_LEN = sizeof(PALETTE)/sizeof(PALETTE[0]);

// Test different pixel format interpretations
enum PixelFormat {
  FMT_RGB565_LE = 0,    // RGB565 little-endian (normal)
  FMT_BGR565_LE = 1,    // BGR565 little-endian (R/B swapped)
  FMT_RGB565_BE = 2,    // RGB565 big-endian (byte swapped)
  FMT_BGR565_BE = 3,    // BGR565 big-endian (byte + R/B swapped)
  FMT_COUNT = 4
};

static PixelFormat detected_format = FMT_BGR565_LE; // default

static inline void rgb565_to_rgb888_format(uint16_t pix, uint8_t &r, uint8_t &g, uint8_t &b, PixelFormat fmt) {
  uint16_t actual_pix = pix;
  
  // Handle byte swapping for big-endian formats
  if (fmt == FMT_RGB565_BE || fmt == FMT_BGR565_BE) {
    actual_pix = ((pix & 0xFF) << 8) | ((pix >> 8) & 0xFF);
  }
  
  // unpack 5-6-5
  uint8_t R = ((actual_pix >> 11) & 0x1F) * 255 / 31;
  uint8_t G = ((actual_pix >> 5)  & 0x3F) * 255 / 63;
  uint8_t B = ( actual_pix        & 0x1F) * 255 / 31;
  
  // Handle R/B swapping for BGR formats
  if (fmt == FMT_BGR565_LE || fmt == FMT_BGR565_BE) {
    r = B; g = G; b = R;
  } else {
    r = R; g = G; b = B;
  }
}

static inline void rgb565_to_rgb888(uint16_t pix, uint8_t &r, uint8_t &g, uint8_t &b, bool swap_rb) {
  // Legacy function - use detected format or fallback to swap_rb
  PixelFormat fmt = swap_rb ? FMT_BGR565_LE : FMT_RGB565_LE;
  rgb565_to_rgb888_format(pix, r, g, b, fmt);
}

// Return nearest color name by squared distance in RGB space
const char* nearest_color_name(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t best = 0xFFFFFFFF;
  const char* best_name = "Unknown";
  for (size_t i = 0; i < PALETTE_LEN; ++i) {
    int32_t dr = int32_t(r) - PALETTE[i].r;
    int32_t dg = int32_t(g) - PALETTE[i].g;
    int32_t db = int32_t(b) - PALETTE[i].b;
    uint32_t d2 = uint32_t(dr*dr + dg*dg + db*db);
    if (d2 < best) {
      best = d2;
      best_name = PALETTE[i].name;
    }
  }
  return best_name;
}

// Auto-detect the correct pixel format by testing all variants
PixelFormat detect_pixel_format(camera_fb_t *fb) {
  if (!fb || fb->format != PIXFORMAT_RGB565) return FMT_BGR565_LE;
  
  const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(fb->buf);
  const uint16_t w = fb->width;
  const uint16_t h = fb->height;
  size_t stride_bytes = (fb->height > 0) ? fb->len / fb->height : size_t(w) * 2;
  
  // Sample center region
  const uint16_t cx = w / 2;
  const uint16_t cy = h / 2;
  const int sample_radius = min(AVG_RADIUS, min(int(cx), int(cy)));
  
  PixelFormat best_format = FMT_BGR565_LE;
  float best_score = -1.0f;
  
  for (int fmt_idx = 0; fmt_idx < FMT_COUNT; fmt_idx++) {
    PixelFormat fmt = (PixelFormat)fmt_idx;
    
    uint32_t sumR = 0, sumG = 0, sumB = 0;
    uint16_t count = 0;
    uint32_t color_variance = 0;
    
    // Sample pixels in center region
    for (int dy = -sample_radius; dy <= sample_radius; dy++) {
      int y = cy + dy;
      if (y < 0 || y >= h) continue;
      
      size_t row_byte = size_t(y) * stride_bytes;
      for (int dx = -sample_radius; dx <= sample_radius; dx++) {
        int x = cx + dx;
        if (x < 0 || x >= w) continue;
        
        size_t pix_idx = row_byte + size_t(x) * 2;
        if (pix_idx + 1 >= fb->len) continue;
        
        uint16_t pix = uint16_t(buf8[pix_idx]) | (uint16_t(buf8[pix_idx + 1]) << 8);
        uint8_t r, g, b;
        rgb565_to_rgb888_format(pix, r, g, b, fmt);
        
        sumR += r; sumG += g; sumB += b; count++;
        
        // Calculate color variance (how much R,G,B differ - lower is better for neutral colors)
        uint32_t avg = (uint32_t(r) + g + b) / 3;
        color_variance += abs(int(r) - int(avg)) + abs(int(g) - int(avg)) + abs(int(b) - int(avg));
      }
    }
    
    if (count == 0) continue;
    
    uint8_t avgR = sumR / count;
    uint8_t avgG = sumG / count;
    uint8_t avgB = sumB / count;
    float avg_variance = float(color_variance) / count;
    
    // Score based on:
    // 1. Lower color variance (neutral colors should have similar R,G,B)
    // 2. Reasonable brightness range (not too dark or oversaturated)
    uint8_t brightness = (avgR + avgG + avgB) / 3;
    float brightness_score = (brightness > 20 && brightness < 235) ? 1.0f : 0.5f;
    float variance_score = 1.0f / (1.0f + avg_variance / 50.0f); // Normalize variance
    
    float total_score = brightness_score * variance_score;
    
    Serial.printf("Format %d: RGB=(%u,%u,%u) variance=%.1f score=%.3f\n", 
                  fmt_idx, avgR, avgG, avgB, avg_variance, total_score);
    
    if (total_score > best_score) {
      best_score = total_score;
      best_format = fmt;
    }
  }
  
  const char* format_names[] = {"RGB565_LE", "BGR565_LE", "RGB565_BE", "BGR565_BE"};
  Serial.printf("Auto-detected format: %s (score=%.3f)\n", format_names[best_format], best_score);
  
  return best_format;
}

void print_memory_info(const char* tag) {
  size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  size_t dram_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[%s] PSRAM free: %u (largest %u)  DRAM free: %u\n",
                tag, (unsigned)psram_free, (unsigned)psram_largest, (unsigned)dram_free);
}

// ==============================
// Globals
// ==============================
static uint32_t boot_ms = 0;
static bool auto_frozen = false;

// WiFi AP config
const char* AP_SSID = "ESP_CAM_DEBUG";
const char* AP_PASS = "esp32cam"; // must be >=8 for WPA2

static const char *TAG = "camserver";

// Simple HTTP server handle
static httpd_handle_t server = NULL;

// forward declarations
static esp_err_t frame_handler(httpd_req_t *req);
static esp_err_t index_handler(httpd_req_t *req);
static void register_uri_handlers();

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println("\n=== XIAO ESP32-S3 Sense camera (RGB565/BGR565 + color name) ===");
  Serial.printf("PSRAM detected: %s\n", psramFound() ? "YES" : "NO");
  print_memory_info("boot");

  camera_config_t config = {};
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

  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = FRAMESIZE_VGA;      // 640x480 for clearer images
  config.xclk_freq_hz = 20000000;           // try 16000000 if flaky
  config.fb_count     = 1;
  config.jpeg_quality = 10;                 // Lower = better quality (0-63)
  config.grab_mode    = CAMERA_GRAB_LATEST; // Get latest frame for smoother capture

  // Critical for S3 Sense
  config.fb_location  = CAMERA_FB_IN_PSRAM;

  Serial.println("Initializing camera...");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Init failed (0x%X). Retrying with 16 MHz XCLK...\n", (unsigned)err);
    print_memory_info("before-retry");
    config.xclk_freq_hz = 16000000;
    err = esp_camera_init(&config);
  }
  if (err != ESP_OK) {
    Serial.printf("FATAL: Camera init failed (0x%X).\n", (unsigned)err);
    while (true) delay(1000);
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    // Start with auto controls on; we can freeze them later
    s->set_gain_ctrl(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);          // Auto white balance gain
    
    // Image quality improvements
    s->set_brightness(s, 0);        // -2 to 2
    s->set_contrast(s, 1);          // -2 to 2 (slight boost)
    s->set_saturation(s, 0);        // -2 to 2
    s->set_sharpness(s, 1);         // -2 to 2 (slight sharpening)
    s->set_denoise(s, 1);           // Enable noise reduction
    
    // Advanced settings for clarity
    s->set_ae_level(s, 0);          // Auto exposure level
    s->set_aec_value(s, 300);       // Auto exposure value
    s->set_agc_gain(s, 0);          // Auto gain control
    
    Serial.println("Camera quality settings applied");
  }

  print_memory_info("after-init");
  Serial.println("Camera ready.");
  
  // Auto-detect pixel format with first frame
  Serial.println("Auto-detecting pixel format...");
  camera_fb_t *test_fb = esp_camera_fb_get();
  if (test_fb) {
    detected_format = detect_pixel_format(test_fb);
    esp_camera_fb_return(test_fb);
  } else {
    Serial.println("Failed to get test frame for format detection");
  }
  
  boot_ms = millis();

  // Start WiFi AP
  WiFi.mode(WIFI_AP);
  bool apok = WiFi.softAP(AP_SSID, AP_PASS);
  if (apok) {
    Serial.printf("WiFi AP started: %s (IP %s)\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("Failed to start AP");
  }

  // Start HTTP server
  httpd_config_t config_http = HTTPD_DEFAULT_CONFIG();
  config_http.server_port = 80;
  if (httpd_start(&server, &config_http) == ESP_OK) {
    register_uri_handlers();
    ESP_LOGI(TAG, "HTTP server started");
  } else {
    Serial.println("Failed to start HTTP server");
    server = NULL;
  }
}

void maybe_freeze_auto() {
#if AUTO_FREEZE_AFTER_MS > 0
  if (!auto_frozen && millis() - boot_ms > AUTO_FREEZE_AFTER_MS) {
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
      s->set_exposure_ctrl(s, 0);
      s->set_gain_ctrl(s, 0);
      s->set_whitebal(s, 0);
      auto_frozen = true;
      Serial.println("Auto exposure/gain/WB frozen for stability.");
    }
  }
#endif
}

void loop() {
  maybe_freeze_auto();

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to get frame buffer");
    delay(200);
    return;
  }

  if (fb->format != PIXFORMAT_RGB565) {
    Serial.println("Unexpected format! Need RGB565.");
    esp_camera_fb_return(fb);
    delay(500);
    return;
  }

  const uint16_t w = fb->width;
  const uint16_t h = fb->height;
  const uint16_t cx = w / 2;
  const uint16_t cy = h / 2;
  const uint16_t* buf16 = reinterpret_cast<const uint16_t*>(fb->buf);

  // Average a window around the center to stabilize reading
  const int R = AVG_RADIUS;
  uint32_t sumR = 0, sumG = 0, sumB = 0;
  uint16_t count = 0;

  for (int dy = -R; dy <= R; ++dy) {
    int y = int(cy) + dy; if (y < 0 || y >= h) continue;
    uint32_t row = uint32_t(y) * w;
    for (int dx = -R; dx <= R; ++dx) {
      int x = int(cx) + dx; if (x < 0 || x >= w) continue;
  uint16_t pix565 = buf16[row + x];
  uint8_t r, g, b;
  rgb565_to_rgb888_format(pix565, r, g, b, detected_format);
      sumR += r; sumG += g; sumB += b; ++count;
    }
  }

  uint8_t r = (count ? sumR / count : 0);
  uint8_t g = (count ? sumG / count : 0);
  uint8_t b = (count ? sumB / count : 0);
  const char* name = nearest_color_name(r, g, b);

  Serial.printf("Center~%dx%d avg RGB=(%3u,%3u,%3u)  WxH=%ux%u  Mode=%s  Color≈%s\n",
                (2*R+1), (2*R+1), r, g, b, w, h,
#if RGB565_IS_BGR
                "BGR565->RGB",
#else
                "RGB565",
#endif
                name);

  esp_camera_fb_return(fb);
  delay(250);
}

// HTTP server handler: serve frame as PPM (binary P6).
static esp_err_t frame_handler(httpd_req_t *req) {
  Serial.printf("Frame request: %s\n", req->uri ? req->uri : "NULL");
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to get frame buffer");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  if (fb->format != PIXFORMAT_RGB565) {
    esp_camera_fb_return(fb);
    httpd_resp_set_status(req, "415 Unsupported Media Type");
    httpd_resp_send(req, "Need RGB565 frame", HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
  }

  const uint16_t w = fb->width;
  const uint16_t h = fb->height;
  // send PPM header
  char hdr[64];
  int hdr_len = snprintf(hdr, sizeof(hdr), "P6\n%u %u\n255\n", (unsigned)w, (unsigned)h);
  httpd_resp_set_type(req, "image/x-portable-pixmap");
  // send header as first chunk
  if (httpd_resp_send_chunk(req, hdr, hdr_len) != ESP_OK) {
    esp_camera_fb_return(fb);
    return ESP_FAIL;
  }

  const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(fb->buf);
  // Many camera drivers provide a stride (padding) per row. Compute bytes-per-row from fb->len
  size_t stride_bytes = 0;
  if (fb->height > 0) stride_bytes = fb->len / fb->height; // bytes per row (may include padding)
  if (stride_bytes == 0) stride_bytes = size_t(w) * 2;

  const size_t row_buf_size = size_t(w) * 3;
  uint8_t *row_buf = (uint8_t*)malloc(row_buf_size);
  if (!row_buf) {
    esp_camera_fb_return(fb);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  // pixels_per_row derived from stride; we will clamp to fb->width
  size_t pixels_per_row = stride_bytes / 2;
  if (pixels_per_row < w) {
    // stride smaller than width: we'll still iterate upto pixels_per_row to avoid OOB
    // but write only up to w to the row buffer (remaining pixels will be black)
  }

  // Use auto-detected format, with optional query override
  PixelFormat use_format = detected_format;
  if (req->uri && req->uri[0]) {
    // parse query for format override: ?fmt=0,1,2,3
    char *q = strchr(req->uri, '?');
    if (q) {
      char *p = strstr(q, "fmt=");
      if (p) {
        int v = atoi(p + 4);
        if (v >= 0 && v < FMT_COUNT) {
          use_format = (PixelFormat)v;
        }
      }
    }
  }

  for (uint16_t y = 0; y < h; ++y) {
    size_t row_byte = size_t(y) * stride_bytes;
    // clear row buffer to black to avoid remnants if stride < w
    memset(row_buf, 0, row_buf_size);
    for (uint16_t x = 0; x < w; ++x) {
      size_t pix_idx = row_byte + size_t(x) * 2;
      if (pix_idx + 1 >= fb->len) {
        // out of bounds, leave black
        continue;
      }
      // assemble little-endian 16-bit pixel (low byte first)
      uint16_t pix = uint16_t(buf8[pix_idx]) | (uint16_t(buf8[pix_idx + 1]) << 8);
      uint8_t R, G, B;
      rgb565_to_rgb888_format(pix, R, G, B, use_format);
      row_buf[x*3 + 0] = R;
      row_buf[x*3 + 1] = G;
      row_buf[x*3 + 2] = B;
    }
    if (httpd_resp_send_chunk(req, (const char*)row_buf, row_buf_size) != ESP_OK) {
      break;
    }
  }

  free(row_buf);
  esp_camera_fb_return(fb);
  httpd_resp_send_chunk(req, NULL, 0); // end of response
  return ESP_OK;
}

// Camera settings handler
static esp_err_t settings_handler(httpd_req_t *req) {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  // Parse query parameters for settings
  if (req->uri && req->uri[0]) {
    char *q = strchr(req->uri, '?');
    if (q) {
      // brightness=-1&contrast=1&saturation=0&sharpness=1
      char *p;
      if ((p = strstr(q, "brightness="))) {
        int v = atoi(p + 11);
        s->set_brightness(s, constrain(v, -2, 2));
      }
      if ((p = strstr(q, "contrast="))) {
        int v = atoi(p + 9);
        s->set_contrast(s, constrain(v, -2, 2));
      }
      if ((p = strstr(q, "saturation="))) {
        int v = atoi(p + 11);
        s->set_saturation(s, constrain(v, -2, 2));
      }
      if ((p = strstr(q, "sharpness="))) {
        int v = atoi(p + 10);
        s->set_sharpness(s, constrain(v, -2, 2));
      }
    }
  }
  
  char buf[256];
  int n = snprintf(buf, sizeof(buf), 
    "{\"status\":\"ok\",\"resolution\":\"%ux%u\",\"message\":\"Settings updated\"}", 640, 480);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, buf, n);
  return ESP_OK;
}

// simple index handler
static esp_err_t color_handler(httpd_req_t *req) {
  // capture a frame and compute center average RGB
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { httpd_resp_send_500(req); return ESP_FAIL; }
  if (fb->format != PIXFORMAT_RGB565) { esp_camera_fb_return(fb); httpd_resp_send_500(req); return ESP_FAIL; }

  const uint16_t w = fb->width;
  const uint16_t h = fb->height;
  const uint16_t cx = w / 2;
  const uint16_t cy = h / 2;
  const int R = AVG_RADIUS;
  const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(fb->buf);
  size_t stride_bytes = (fb->height > 0) ? fb->len / fb->height : size_t(w) * 2;

  uint32_t sumR=0,sumG=0,sumB=0; uint16_t count=0;
  for (int dy=-R; dy<=R; ++dy) {
    int y = int(cy) + dy; if (y<0||y>=h) continue;
    size_t row_byte = size_t(y) * stride_bytes;
    for (int dx=-R; dx<=R; ++dx) {
      int x = int(cx) + dx; if (x<0||x>=w) continue;
      size_t pix_idx = row_byte + size_t(x)*2;
      if (pix_idx+1 >= fb->len) continue;
      uint16_t pix = uint16_t(buf8[pix_idx]) | (uint16_t(buf8[pix_idx+1])<<8);
      uint8_t rr,gg,bb; rgb565_to_rgb888_format(pix, rr,gg,bb, detected_format);
      sumR += rr; sumG += gg; sumB += bb; ++count;
    }
  }
  uint8_t ar = (count? sumR/count:0);
  uint8_t ag = (count? sumG/count:0);
  uint8_t ab = (count? sumB/count:0);
  const char* name = nearest_color_name(ar,ag,ab);
  esp_camera_fb_return(fb);

  char buf[128];
  int n = snprintf(buf, sizeof(buf), "{\"r\":%u,\"g\":%u,\"b\":%u,\"name\":\"%s\"}", (unsigned)ar, (unsigned)ag, (unsigned)ab, name);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, buf, n);
  return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req) {
  const char* html = 
    "<html><head><title>ESP Camera Debug</title></head><body>"
    "<h3>ESP Camera Debug (VGA 640x480)</h3>"
    "<p>Use Python viewer: <code>python tools/view_cam.py</code></p>"
    "<p>Endpoints:</p>"
    "<ul>"
    "<li><a href=\"/frame.ppm\">/frame.ppm</a> - Current frame</li>"
    "<li><a href=\"/color.json\">/color.json</a> - Color analysis</li>"
    "<li><a href=\"/settings\">/settings</a> - Camera settings</li>"
    "</ul>"
    "</body></html>";
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// register handlers after server start
static void register_uri_handlers() {
  if (!server) return;
  httpd_uri_t idx_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(server, &idx_uri);

  httpd_uri_t frame_uri = {
    .uri = "/frame.ppm",
    .method = HTTP_GET,
    .handler = frame_handler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(server, &frame_uri);
  httpd_uri_t color_uri = {
    .uri = "/color.json",
    .method = HTTP_GET,
    .handler = color_handler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(server, &color_uri);
  
  httpd_uri_t settings_uri = {
    .uri = "/settings",
    .method = HTTP_GET,
    .handler = settings_handler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(server, &settings_uri);
}