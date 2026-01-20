#include "esp_now.h"
#include "nvs_flash.h"
#include "data.h"
#include <stdio.h>
#include "display.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"
#include "receiver/gps.h"

nvs_handle_t namespace;

static const char *NAMESPACE = "storage";

#define MOSI_PIN 23
#define MISO_PIN 19
#define SCK_PIN 18
#define CS_PIN 5

#define MOUNT_POINT "/sdcard"

esp_err_t storage_init(void)
{
    esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &namespace);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    sdmmc_card_t *card;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 400; // Needs to be slow for initialization to work.
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_MISO |
                 SPICOMMON_BUSFLAG_MOSI | SPICOMMON_BUSFLAG_SCLK,
    };

    gpio_set_pull_mode(MOSI_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(MISO_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SCK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CS_PIN, GPIO_PULLUP_ONLY);

    err = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (err != ESP_OK)
    {
        return err;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CS_PIN;
    slot_config.host_id = host.slot;

    const char mount_point[] = MOUNT_POINT;
    err = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (err != ESP_OK)
    {
        return err;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

esp_err_t write_packet_file(data_packet_t *packet, char *packet_key)
{
    char *file_name;
    asprintf(&file_name, "%s/%s.pkt", MOUNT_POINT, packet_key);

    FILE *f = fopen(file_name, "w");
    if (f == NULL)
    {
        free(file_name);
        return ESP_FAIL;
    }

    size_t written_items = fwrite(packet, sizeof(data_packet_t), 1, f);
    if (written_items != 1)
    {
        free(file_name);
        return ESP_FAIL;
    }

    fclose(f);

    free(file_name);

    return ESP_OK;
}

uint32_t fnv1a_hash(const void *data_t, size_t len)
{
    uint32_t hash = 0x811C9DC5;
    const uint8_t *bytes = data_t;

    for (size_t i = 0; i < len; i++)
    {
        hash ^= bytes[i];
        hash *= 0x01000193;
    }

    return hash;
}

uint32_t hash_packet(uint8_t *address, uint16_t sequence_id, uint16_t packet_num)
{
    uint32_t h = 0;

    h ^= fnv1a_hash(address, sizeof(address[0]) * 6);
    h ^= fnv1a_hash(&sequence_id, sizeof(sequence_id));
    h ^= fnv1a_hash(&packet_num, sizeof(packet_num));

    return h;
}

uint32_t hash_sequence(uint8_t *address, uint16_t sequence_id)
{
    uint32_t h = 0;

    h ^= fnv1a_hash(address, sizeof(address[0]) * 6);
    h ^= fnv1a_hash(&sequence_id, sizeof(sequence_id));

    return h;
}

esp_err_t store_packet(uint8_t *address, data_packet_t *packet, bool *received_all)
{
    char packet_key[9];
    snprintf(packet_key, sizeof(packet_key), "%08" PRIX32, hash_packet(address, packet->sequence_id, packet->packet_num));

    bool new_packet = false;
    esp_err_t err = nvs_find_key(namespace, packet_key, NULL);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            err = nvs_set_u8(namespace, packet_key, 0);
            if (err != ESP_OK)
            {
                return err;
            }
            new_packet = true;
        }
        else
        {
            return err;
        }
    }

    char sequence_key[9];
    snprintf(sequence_key, sizeof(sequence_key), "%08" PRIX32, hash_sequence(address, packet->sequence_id));

    uint8_t packets_received = 0;
    err = nvs_get_u8(namespace, sequence_key, &packets_received);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        return err;
    }

    if (new_packet)
    {
        err = write_packet_file(packet, packet_key);
        if (err != ESP_OK)
        {
            return err;
        }
        packets_received++;
    }

    if (packets_received == packet->total)
    {
        *received_all = true;
    }

    err = nvs_set_u8(namespace, sequence_key, packets_received);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_commit(namespace);
    if (err != ESP_OK)
    {
        return err;
    }

    receive_message(
        packet->sequence_id,
        packet->packet_num,
        packet->total,
        address,
        packets_received);

    return ESP_OK;
}

void mac_to_key(const uint8_t mac[6], char out[17]) // 12 + null
{
    snprintf(out, 17,
             "pos_%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
}

esp_err_t store_sensor(uint8_t *address)
{
    char sensor_key[17];
    mac_to_key(address, sensor_key);

    char line[128];
    esp_err_t err = gps_read_line(line, sizeof(line));
    if (err != ESP_OK)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }

    err = nvs_set_str(namespace, sensor_key, line);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}