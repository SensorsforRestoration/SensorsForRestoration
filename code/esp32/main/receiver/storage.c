#include "esp_now.h"
#include "nvs_flash.h"
#include "data.h"
#include <stdio.h>

nvs_handle_t namespace;

static const char *NAMESPACE = "storage";

esp_err_t storage_init(void)
{
    return nvs_open(NAMESPACE, NVS_READWRITE, &namespace);
}

esp_err_t store_packet(packet_t *packet, bool *received_all)
{
    char packet_key[16];
    snprintf(packet_key, sizeof(packet_key),
             "p%02x%04x%02x",
             packet->sensor_id & 0xFF,
             packet->sequence_id & 0xFFFF,
             packet->packet_num & 0xFF);

    bool new_packet = false;
    esp_err_t err = nvs_find_key(namespace, packet_key, NULL);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            err = nvs_set_blob(namespace, packet_key, packet, sizeof(*packet));
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

    char sequence_key[16];
    snprintf(sequence_key, sizeof(sequence_key),
             "s%02x%04x", packet->sensor_id & 0xFF, packet->sequence_id & 0xFFFF);

    uint8_t packets_received = 0;
    err = nvs_get_u8(namespace, sequence_key, &packets_received);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        return err;
    }

    if (new_packet)
    {
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

    return nvs_commit(namespace);
}