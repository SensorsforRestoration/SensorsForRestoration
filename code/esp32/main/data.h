#pragma once
#include <stdint.h>

typedef struct {
    float depth[360];
    float temperature[1];
    float salinity[2];
} data;

typedef struct {
    uint32_t packet_id;    
    uint64_t timestamp;  
    uint8_t sensor_id[6];
    data payload;
} packet_t;
