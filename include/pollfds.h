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
    u32    nfds;
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
