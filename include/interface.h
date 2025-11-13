/** @file interface.h
 *  @brief Interface for various (wait, read, write [WRW]) loop implementations.
 *
 *  1. open, close: Initialization, setting params, allocation and freeing resources
 *  2. start, stop: Write silence, start/stop wait-read-write loop
 *  3. wait, xrun: Wait for `avail = period_size`, handle xruns
 *  4. read, write: Read/write mmaped to/from device areas from/to buffer areas
 *  5. callbacks: User defined callbacks
 *
 */
#pragma once

#include "duplex.h"
#include "params.h"

/** @brief Open pcm handles, set hardware and software params, allocate structs
 *
 *  Steps:
 *    1. setting of channels
 *    2. setting of formats, rate
 *
 */
typedef int (*sndx_duplex_open_fn)(sndx_duplex_t* d,
                                   const char*    playback_device,
                                   const char*    capture_device,
                                   sndx_params_t* params,
                                   void*          data,
                                   output_t*      output);

/** @brief Free memory and close pcm handles
 *
 *  Guaranteed to succeed, checks for nil before freeing, sets nil after freeing
 */
typedef int (*sndx_duplex_close_fn)(sndx_duplex_t* d, void* data);

/** @brief Write silence and prepare for WRW loop
 *
 *  Steps:
 *    1. Call `snd_pcm_prepare`
 *    2. Zero the buffer / alloc special silence buffer
 *    3. Write to device using mmap
 *    4. Check status changed from PREPARED to RUNNING
 */
typedef int (*sndx_duplex_start_fn)(sndx_duplex_t* d, void* data);

/** @brief Drop pcm handles
 *
 *  Steps:
 *      1. ? Zero the buffer / alloc special silence buffer
 *      2. ? Write to device using mmap
 *      3. Drop handles
 */
typedef int (*sndx_duplex_stop_fn)(sndx_duplex_t* d, void* data);

/** @brief Calls stop and start in succession
 *
 *  Used in xrun_recovery.
 */
typedef int (*sndx_duplex_restart_fn)(sndx_duplex_t* d, void* data);

// TODO: xrun_recovery

/** @brief Wait using poll/snd_pcm_wait/..
 *
 *  Many possible implementations, with various ways to recover from xrun
 *      1. `snd_pcm_wait`
 *      2. `poll`
 *
 *  Questions:
 *      1. In duplex operation, which pcm to wait on? Fabian's talk: both
 *      2. How is device disconnection handled?
 *      3. Is it possible to just use `pcm_recover`?
 */
typedef int (*sndx_duplex_wait_fn)(sndx_duplex_t* d, void* data);

/** @brief Read from device to buffer
 *
 *   Converts from integer format of device to float format for buffer.
 *       Device: interleaved
 *       Buffer: non-interleaved
 */
typedef int (*sndx_duplex_read_fn)( //
    sndx_duplex_t* d,
    uframes_t      cap_avail,
    uframes_t*     frames_in,
    uframes_t*     frames_read,
    uframes_t*     in_max,
    void*          data);

/** @brief Write from buffer to device
 *
 *   Converts from float format for buffer to integer format of device.
 *       Buffer: non-interleaved
 *       Device: interleaved
 */
typedef int (*sndx_duplex_write_fn)( //
    sndx_duplex_t* d,
    uframes_t      play_avail,
    uframes_t*     frames_out,
    uframes_t*     frames_written,
    uframes_t*     out_max,
    void*          data);

/** @brief Audio callback function, with duplex fully exposed for now */
typedef int (*sndx_duplex_callback_fn)(sndx_duplex_t* d, sframes_t offset, sframes_t frames, void* data);

/** @brief Coherent set of duplex operations */
typedef struct sndx_duplex_ops_t
{
    // Open, alloc, close
    sndx_duplex_open_fn  open_fn;
    sndx_duplex_close_fn close_fn;

    // Start, drop
    sndx_duplex_start_fn   start_fn;
    sndx_duplex_stop_fn    stop_fn;
    sndx_duplex_restart_fn restart_fn;

    // Polling
    sndx_duplex_wait_fn wait_fn;

    // IO
    sndx_duplex_read_fn  read_fn;
    sndx_duplex_write_fn write_fn;

    // Actual audio callback
    sndx_duplex_callback_fn callback_fn;

} sndx_duplex_ops_t;
