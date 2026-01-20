#include "logger.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"

static const char *TAG = "LOGGER";
static wl_handle_t s_wl = WL_INVALID_HANDLE;

#define MOUNT_POINT "/storage"
#define LOG_PATH    MOUNT_POINT "/depthlog.bin"

esp_err_t logger_init(void)
{
    if (s_wl != WL_INVALID_HANDLE) return ESP_OK;

    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true,  // first run will format the partition
        .allocation_unit_size = 4096
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_POINT, "storage", &mount_config, &s_wl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Mount FATFS failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Mounted flash FAT at %s", MOUNT_POINT);
    return ESP_OK;
}

esp_err_t logger_append(const log_record_t *rec)
{
    if (!rec) return ESP_ERR_INVALID_ARG;
    esp_err_t err = logger_init();
    if (err != ESP_OK) return err;

    FILE *f = fopen(LOG_PATH, "ab");
    if (!f) return ESP_FAIL;

    size_t w = fwrite(rec, sizeof(*rec), 1, f);
    fclose(f);
    return (w == 1) ? ESP_OK : ESP_FAIL;
}

esp_err_t logger_count(uint32_t *out_count)
{
    if (!out_count) return ESP_ERR_INVALID_ARG;
    *out_count = 0;

    esp_err_t err = logger_init();
    if (err != ESP_OK) return err;

    FILE *f = fopen(LOG_PATH, "rb");
    if (!f) return ESP_OK; // no file yet

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);

    if (sz <= 0) return ESP_OK;
    *out_count = (uint32_t)(sz / (long)sizeof(log_record_t));
    return ESP_OK;
}

esp_err_t logger_read_all_and_send(void (*send_fn)(const log_record_t *rec, uint16_t idx, uint16_t total))
{
    if (!send_fn) return ESP_ERR_INVALID_ARG;

    esp_err_t err = logger_init();
    if (err != ESP_OK) return err;

    uint32_t count = 0;
    ESP_ERROR_CHECK(logger_count(&count));
    if (count == 0) {
        ESP_LOGI(TAG, "No records to upload.");
        return ESP_OK;
    }

    FILE *f = fopen(LOG_PATH, "rb");
    if (!f) return ESP_FAIL;

    log_record_t rec;
    for (uint32_t i = 0; i < count; i++) {
        size_t r = fread(&rec, sizeof(rec), 1, f);
        if (r != 1) break;
        send_fn(&rec, (uint16_t)(i + 1), (uint16_t)count);
    }

    fclose(f);
    return ESP_OK;
}

esp_err_t logger_clear(void)
{
    esp_err_t err = logger_init();
    if (err != ESP_OK) return err;

    // Truncate by reopening in write mode
    FILE *f = fopen(LOG_PATH, "wb");
    if (!f) return ESP_FAIL;
    fclose(f);
    ESP_LOGI(TAG, "Log cleared.");
    return ESP_OK;
}
