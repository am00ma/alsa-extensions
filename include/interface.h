/*! \file interface.h
    \brief Interface for various (wait, read, write [WRW]) loop implementations

    Details.
*/
#pragma once

#include "params.h"

typedef struct sndx_duplex_t sndx_duplex_t;

/*! \fn sndx_duplex_callback_fn
    \brief Audio callback function, with duplex fully exposed for now

    Details.
*/
typedef int (*sndx_duplex_callback_fn)(sndx_duplex_t* d, sframes_t offset, sframes_t frames, void* data);

/*! \fn sndx_duplex_open_fn
    \brief Open pcm handles, set hardware and software params, allocate structs

    Details.
*/
typedef int (*sndx_duplex_open_fn)(sndx_duplex_t* d,
                                   const char*    playback_device,
                                   const char*    capture_device,
                                   sndx_params_t* params,
                                   void*          data,
                                   output_t*      output);

/*! \fn sndx_duplex_close_fn
    \brief Free memory and close pcm handles

    Details.
*/
typedef int (*sndx_duplex_close_fn)(sndx_duplex_t* d, void* data);

/*! \fn sndx_duplex_start_fn
    \brief Write silence and prepare for WRW loop

    Details.
*/
typedef int (*sndx_duplex_start_fn)(sndx_duplex_t* d, void* data);

/*! \fn sndx_duplex_stop_fn
    \brief Drop pcm handles

    Details.
*/
typedef int (*sndx_duplex_stop_fn)(sndx_duplex_t* d, void* data);

/*! \fn sndx_duplex_restart_fn
    \brief Basically stop and start again

    Used in xrun_recovery.
*/
typedef int (*sndx_duplex_restart_fn)(sndx_duplex_t* d, void* data);

/*! \fn sndx_duplex_wait_fn
    \brief Wait using poll/snd_pcm_wait/..

    Details.
*/
typedef int (*sndx_duplex_wait_fn)(sndx_duplex_t* d, void* data);

/*! \fn sndx_duplex_read_fn
    \brief Read from device to buffer

    Converts from integer format of device to float format for buffer.
        Device: interleaved
        Buffer: non-interleaved
*/
typedef int (*sndx_duplex_read_fn)( //
    sndx_duplex_t* d,
    uframes_t      cap_avail,
    uframes_t*     frames_in,
    uframes_t*     frames_read,
    uframes_t*     in_max,
    void*          data);

/*! \fn sndx_duplex_write_fn
    \brief Audio callback function, with duplex fully exposed for now

    Converts from float format for buffer to integer format of device.
        Buffer: non-interleaved
        Device: interleaved
*/
typedef int (*sndx_duplex_write_fn)( //
    sndx_duplex_t* d,
    uframes_t      play_avail,
    uframes_t*     frames_out,
    uframes_t*     frames_written,
    uframes_t*     out_max,
    void*          data);

// TODO: xrun_recovery

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
