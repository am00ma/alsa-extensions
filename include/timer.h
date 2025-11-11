/** @file timer.h
 *  @brief Conversions, timestamping.
 */
#pragma once

#include "types.h"

typedef struct timespec tspec_t;

#define frames_to_micro(frames, rate) (u64)((frames * 1000000LL) + (rate / 2)) / rate;
#define timestamp_now(tstamp)                                                                                          \
    if (clock_gettime(CLOCK_MONOTONIC_RAW, tstamp)) printf("clock_gettime() failed\n");

// TODO: Sort out snd time and sys/time timestamps

/** @brief TODO: what does this do */
u64 timestamp_diff_now(tspec_t* tstamp);

/** @brief TODO: what does this do */
long timestamp_diff(snd_timestamp_t t1, snd_timestamp_t t2);

/** @brief TODO: what does this do */
void timestamp_get(snd_pcm_t* handle, snd_timestamp_t* timestamp);

/** @brief Get clock time in microseconds */
u64 get_microseconds();

/** @brief Captures snapshot of play and capt for timing metrics. */
typedef struct
{

    snd_timestamp_t play;
    snd_timestamp_t capt;
    tspec_t         start;

} sndx_timer_t;

/** @brief Capture snapshot using snd timer and system timer */
void sndx_duplex_timer_start(sndx_timer_t* t, snd_pcm_t* play, snd_pcm_t* capt);

/** @brief Capture snapshot of end and print difference in sys and snd time */
void sndx_duplex_timer_stop(sndx_timer_t* t, uframes_t frames_in, u32 rate, output_t* output);
