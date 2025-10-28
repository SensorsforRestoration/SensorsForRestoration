#pragma once
#include <stdint.h>

typedef struct
{
    float depth[360];
    float temperature[1];
    float salinity[2];
} data;

typedef struct
{
    uint16_t sequence_id;
    uint8_t packet_num;
    uint16_t total;
    uint64_t timestamp;
    uint16_t sensor_id;
    data payload;
} packet_t;

typedef struct {
    uint64_t timestamp;
} time_sync_packet_t;