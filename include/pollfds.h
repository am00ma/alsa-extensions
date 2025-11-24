/** @file pollfds.h
 *  @brief Manager for all measurements and memory related to polling.
 *
 *  Manages:
 *      1. Poll addresses, counts
 *      2. Poll timing
 *      3. Xrun and delays
 */
#pragma once

#include "types.h"

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

    pfd_t* addr;
    u32    play_nfds;
    u32    capt_nfds;

    u64 poll_next;
    u64 poll_last;
    u64 poll_late;
    u64 poll_timeout;

    u64 period_usecs;
    u64 delayed_usecs;
    u64 last_wait_ust;
    u32 xrun_count;
    u32 retry_count;

} sndx_pollfds_t;

/** @brief Allocate memory and init stats for struct pollfds */
int sndx_pollfds_open(sndx_pollfds_t** pfdsp, snd_pcm_t* play, snd_pcm_t* capt, snd_output_t* output);

/** @brief Free memory from struct pollfds, free p itself */
void sndx_pollfds_close(sndx_pollfds_t* p);

/** @brief Only reallocate poll fds memory, keeping stats the same, recounting poll descriptors */
int sndx_pollfds_reset(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, snd_output_t* output);

typedef enum sndx_pollfds_poll_error_t
{
    POLLFD_SUCCESS = 0,
    POLLFD_FATAL,
    POLLFD_NEEDS_RESTART,

} sndx_pollfds_poll_error_t;

/** @brief Do the polling and handle the errors
 *
 *  Problem: Needs `sndx_pollfds_xrun`, but that depends on `duplex_start`, `duplex_stop`
 *
 *  Example usage:
 *
 *  ```c
 *    int       err   = 0;
 *    uframes_t avail = 0;
 *
 *    __retry:
 *
 *        err = sndx_pollfds_poll(p, play, capt, output);
 *        SndReturn_(err, "Failed: sndx_pollfds_poll: %s");
 *
 *        err = sndx_pollfds_avail(p, play, capt, &avail, output);
 *        SndReturn_(err, "Failed: sndx_pollfds_avail: %s");
 *
 *        Assert(avail != 0);
 *
 *        // Unexpected but not error
 *        if (avail != period_size)
 *            a_info("avail != period_size (%d != %d)", avail, period_size);
 *
 *        // All good here on to read (mmap_begin)
 *  ```
 *
 * */
int sndx_pollfds_wait(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output);

/** @brief Get minimum of available read/write space */
int sndx_pollfds_avail(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output);

/** @brief Handle xrun, including stopping and starting again
 *
 *  Problem: We need duplex_start and duplex_stop here
 *
 * */
int sndx_pollfds_xrun(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output);
