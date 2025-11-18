#pragma once
#include <stdint.h>

// Sensor readings taken at predetermined intervals.
typedef __attribute__((packed)) struct
{
    float depth[360];
    float temperature[1];
    float salinity[2];
} data;

typedef __attribute__((packed)) struct
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

    // sensor_id is the sensor this packet is coming from.
    uint16_t sensor_id;

    // payload is the data.
    data payload;
} packet_t;

typedef __attribute__((packed)) struct
{
    uint64_t timestamp;
} time_sync_packet_t;