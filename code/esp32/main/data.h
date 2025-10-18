#include <time.h>

typedef struct __attribute__((packed))
{
    unsigned int num;
    unsigned int total;
    time_t time;
} header;

typedef struct __attribute__((packed))
{
    unsigned int depth[360];
    float salinity[1];
    float temperature[2];
} data;

typedef struct __attribute__((packed))
{
    header header;
    data data;
} packet;