/** @file timer.h
 *  @brief Conversions, timestamping.
 *
 *  tspec_t  : system timestamp
 *  tstamp_t : snd trigger timestamp
 */
#pragma once

#include "types.h"

/** @brief Time spec (nanoseconds) from sys/time.h `{tv_secs, tv_nsecs}` */
typedef struct timespec tspec_t;

/** @brief Time spec (microseconds) from alsa `{tv_sec, tv_usec}` */
typedef snd_timestamp_t tstamp_t;

/** @brief High resolution time spec (nanoseconds) from alsa `{tv_sec, tv_nsec}` */
typedef snd_htimestamp_t htstamp_t;

/** @brief Convert time in frames to time in microseconds */
#define frames_to_micro(frames, rate) (u64)((frames * 1000000LL) + (rate / 2)) / rate;

/** @brief Get system time (prob same as get_microseconds) */
#define timestamp_now(tstamp)                                                                                          \
    if (clock_gettime(CLOCK_MONOTONIC_RAW, tstamp)) printf("clock_gettime() failed\n");

/** @brief Get difference from given system time in microseconds */
u64 timespec_diff_now(tspec_t* tstamp);

/** @brief Difference in trigger timestamps in microseconds */
i64 timestamp_diff(tstamp_t t1, tstamp_t t2);

/** @brief Wrapper for `snd_pcm_status_get_trigger_tstamp` */
void timestamp_get(snd_pcm_t* handle, tstamp_t* timestamp);

/** @brief Get clock time in microseconds */
u64 get_microseconds();

/** @brief Keeps track of timing using sys, snd, frames */
typedef struct
{

    tspec_t start; ///< System time before while loop
    tspec_t end;   ///< System time after while loop

    tstamp_t stamp_play; ///< Alsa trigger stamp for playback
    tstamp_t stamp_capt; ///< Alsa trigger stamp for capture

    uframes_t frames_play; ///< Frames written by playback
    uframes_t frames_capt; ///< Frames read by capture

} sndx_timer_t;

/** @brief Capture snapshot using snd timer and system timer */
void sndx_duplex_timer_start(sndx_timer_t* t, snd_pcm_t* play, snd_pcm_t* capt);

/** @brief Capture snapshot of end and print difference in sys and snd time */
void sndx_duplex_timer_stop(sndx_timer_t* t, uframes_t frames_in, u32 rate, output_t* output);
