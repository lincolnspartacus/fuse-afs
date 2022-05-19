#ifndef TIMING_H
#define TIMING_H

#include <time.h>

static inline long double time_monotonic()
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (time.tv_sec * 1000000000.0L ) + time.tv_nsec;
}

#endif