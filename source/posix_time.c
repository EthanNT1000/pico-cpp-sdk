// Custom POSIX time functions for micro-ROS on Raspberry Pi Pico
// Based on Pico SDK time_us_64() for high-resolution timestamps

#include <time.h>  // For struct timespec
#include "pico/time.h"  // For time_us_64()

// Custom implementation of clock_gettime using Pico's microsecond timer
// This provides monotonic time since boot (suitable for relative measurements in micro-ROS)
int clock_gettime(clockid_t clk_id, struct timespec *tp) {
    // Ignore clk_id for simplicity; use monotonic time
    // (micro-ROS primarily needs consistent, high-res timestamps)
    uint64_t us = time_us_64();
    tp->tv_sec = us / 1000000;
    tp->tv_nsec = (us % 1000000) * 1000;
    return 0;  // Success
}

// Optional: Stub for clock_settime (not typically called by micro-ROS, but for completeness)
int clock_settime(clockid_t clk_id, const struct timespec *tp) {
    // Not implemented; micro-ROS does not require absolute time setting
    // (time synchronization occurs via the agent)
    return 0;
}

// Optional: usleep stub (maps to Pico sleep)
void usleep(uint64_t us) {
    sleep_us(us);
}
