#include <time.h>

struct header
{
    unsigned int num;
    unsigned int total;
    time_t time;
};

struct data
{
    unsigned int depth[360];
    float salinity[1];
    float temperature[2];
};

struct packet
{
    struct header header;
    struct data data;
};