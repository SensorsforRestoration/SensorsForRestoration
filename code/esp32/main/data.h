#pragma once
#include <stdint.h>

// Sensor readings taken at predetermined intervals.
typedef struct
{
    float depth[360];
    float temperature[1];
    float salinity[2];

    // depth_mm is the depth in millimeters.
    int16_t depth_mm;

    int16_t r;
    int16_t g;
    int16_t b;
} data_t;

typedef struct
{
    // sequence_id is the sequence of packets this packet belong to.
    // Each sequence is all the data for a period of time, probably
    // a day.
    uint16_t sequence_id;

    // packet_num is the number of this packet in the sequence.
    uint8_t packet_num;

    // total is the total number of packets in the sequence.
    uint16_t total;

    // timestamp is the time of the start of the data in this packet.
    uint32_t timestamp;

    // payload is the data.
    data_t data;
} data_packet_t;

typedef struct
{
    uint64_t timestamp;
} sensor_start_packet_t;

typedef enum
{
    BROADCAST_TYPE_NEW_SENSOR = 0,
    BROADCAST_TYPE_RECEIVER
} broadcast_type_t;

typedef struct
{
    broadcast_type_t broadcast_type;
} broadcast_packet_t;
