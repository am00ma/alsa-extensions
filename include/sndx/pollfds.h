/** @file pollfds.h
 *  @brief Manager for all measurements and memory related to polling.
 *
 *  Manages:
 *      1. Poll addresses, counts
 *      2. Poll timing
 *      3. Xrun and delays
 */
#pragma once

#include "sndx/types.h"

typedef struct pollfd pfd_t;

/** @brief Struct to handle polling and poll timings
 *
 *  Initialized in duplex_start
 *
 *  Details:
 *      1. Poll addresses, counts
 *      2. Poll timing
 *      3. Xrun and delays
 */
typedef struct sndx_pollfds_t
{

    pfd_t* addr;      ///< Allocated on open and reallocated on restart
    u32    play_nfds; ///< Number of playback fds
    u32    capt_nfds; ///< Number of capture fds

    u32 rate;        ///< Necessary for period_usecs calculation
    u64 period_size; ///< Necessary for period_usecs, avail calculation

    u64 poll_next;    ///< Expected time of next poll
    u64 poll_last;    ///< Recorded time of last poll
    u64 poll_late;    ///< Was it late?
    u64 poll_timeout; ///< Used in poll(..., poll_timeout)

    u64 period_usecs;  ///< period in usecs
    u64 delayed_usecs; ///< delay in usecs
    u64 last_wait_ust; ///< timestamp of last wait in usecs
    u32 xrun_count;    ///< Number of xruns
    u32 retry_count;   ///< Number of poll(...) retries if poll_ret == 0

} sndx_pollfds_t;

/** @brief Allocate memory and init stats for struct pollfds */
int sndx_pollfds_open( //
    sndx_pollfds_t** pfdsp,
    snd_pcm_t*       play,
    snd_pcm_t*       capt,
    u32              rate,
    u64              period_size,
    output_t*        output);

/** @brief Free memory from struct pollfds, free p itself */
void sndx_pollfds_close(sndx_pollfds_t* p);

/** @brief Only reallocate poll fds memory, keeping stats the same, recounting poll descriptors */
int sndx_pollfds_reset(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output);

typedef enum sndx_pollfds_poll_error_t
{
    POLLFD_SUCCESS = 0,
    POLLFD_FATAL,
    POLLFD_NEEDS_RESTART,

} sndx_pollfds_error_t;

/** @brief Do the polling and handle the errors
 *
 *  Problem: Needs `sndx_pollfds_xrun`, but that depends on `duplex_start`, `duplex_stop`
 *
 *  TODO: example usage
 *
 *  Returns one of 3 values:
 *      - POLLFD_SUCCESS
 *      - POLLFD_FATAL
 *      - POLLFD_NEEDS_RESTART
 *
 *  Based on that, caller can start xrun recovery and restart
 *
 * */
sndx_pollfds_error_t sndx_pollfds_wait(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output);

/** @brief Get minimum of available read/write space
 *
 *  Returns one of 3 values:
 *      - POLLFD_SUCCESS
 *      - POLLFD_FATAL
 *      - POLLFD_NEEDS_RESTART
 * */
sndx_pollfds_error_t sndx_pollfds_avail( //
    sndx_pollfds_t* p,
    snd_pcm_t*      play,
    snd_pcm_t*      capt,
    sframes_t*      avail,
    output_t*       output);

/** @brief Handle xrun, (drop, prepare)
 *
 *  Problem: We need duplex_start and duplex_stop here
 *           even if we can drop, prepare again,
 *           to fill silence we need all the params including format, channels, ...
 *
 *  NOTE: user's responsibility to stop and start (restart) the duplex after calling this function
 *
 * */
void sndx_pollfds_xrun(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output);
