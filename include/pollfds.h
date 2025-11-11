/** @file pollfds.h
 *  @brief Manager for all measurements and memory related to polling.
 *
 *  Manages:
 *      1. Poll addresses
 *      2. Counts of descriptors
 *      3. Poll timing
 *      4. Xrun and delays
 */
#pragma once

#include "types.h"

typedef struct pollfd pfd_t;

/** @brief Manager for all measurements and memory related to polling.
 *
 *  Manages:
 *      1. Poll addresses
 *      2. Counts of descriptors
 *      3. Poll timing
 *      4. Xrun and delays
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
    u64 xrun_count;

} sndx_pollfds_t;

/** @brief Allocate memory for struct pollfds. */
int sndx_pollfds_open(sndx_pollfds_t** pfdsp, snd_pcm_t* play, snd_pcm_t* capt, snd_output_t* output);

/** @brief Free memory from struct pollfds. */
void sndx_pollfds_close(sndx_pollfds_t* pfdsp);

typedef enum sndx_pollfds_poll_error_t
{
    POLLFD_SUCCESS = 0,
    POLLFD_FATAL,
    POLLFD_RECOVERABLE,
    POLLFD_NEEDS_RESTART,

} sndx_pollfds_poll_error_t;

/** @brief Do the polling and handle the errors
 *
 *  Return value is indicated in both status as well as the return value
 *
 *  Example usage:
 *
 *  ```c
 *    int       err   = 0;
 *    uframes_t avail = 0;
 *
 *    __retry:
 *
 *        err = sndx_pollfds_poll(p, &avail, output);
 *
 *        switch (err) {
 *        case POLLFD_NEEDS_RESTART: { restart(); };
 *        case POLLFD_RECOVERABLE: { notify_xrun(); goto __retry; };
 *        case POLLFD_SUCCESS: break;
 *        }
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
 *
 *
 * */
sndx_pollfds_poll_error_t sndx_pollfds_poll(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output);
