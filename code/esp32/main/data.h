#pragma once
#include <stdint.h>

// Sensor readings taken at predetermined intervals.
typedef __attribute__((packed)) struct
{
    int16_t  depth_mm;       // depth mm
    int16_t salinity;
    uint8_t r;
    uint8_t g;
    uint8_t b;
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


//////////////////////////// Chat code i think this adds a part for the reciever board to send a signal to request the files

// NEW: boat → sensor message to request upload
typedef __attribute__((packed)) struct
{
    uint32_t magic;   // 0xB0A7CAFE
} upload_request_t;

#define UPLOAD_MAGIC 0xB0A7CAFEu
