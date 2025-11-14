/** @file duplex.h
 *  @brief Fat analogue of `snd_pcm_t` which holds 2 handles, one playback and one capture.
 *
 *  Manages:
 *      1. PCM handles
 *      2. Buffers
 *      3. Polling
 *      4. Loop timing
 */
#pragma once

#include "buffer.h"

/** @brief Analogue of `snd_pcm_t` that manages pcm handles, buffers, polling, timing.
 *
 *  Provided in audio callback.
 *
 *  Manages:
 *      1. PCM handles
 *      2. Buffers
 *      3. Polling
 *      4. Loop timing
 *
 *  Usage in loop:
 *      1. open
 *      2. close
 *      3. start
 *      4. stop
 *      5. restart
 *      6. wait
 *      7. read
 *      8. write
 *      9. xrun
 */
typedef struct sndx_duplex_t
{
    snd_pcm_t* play; ///< Playback pcm handle
    snd_pcm_t* capt; ///< Capture pcm handle

    u32       ch_play;     ///< Set to min channels for playback pcm
    u32       ch_capt;     ///< Set to min channels for capture pcm
    format_t  format;      ///< Currently assuming same
    u32       rate;        ///< Aslo must match
    uframes_t period_size; ///< Must match
    u32       periods;     ///< Must match, buffer_size = period_size * periods
    bool      linked;      ///< May not be possible based on play, capt

    sndx_buffer_t* buf_play; ///< Connects float buffer to playback device area, used by write
    sndx_buffer_t* buf_capt; ///< Connects float buffer to capture device area, used by read

    output_t* out; ///< Alsa's builtin message buffer

} sndx_duplex_t;

/** @brief Dump duplex params to output. */
void sndx_dump_duplex(sndx_duplex_t* d, snd_output_t* output);

/** @brief Dump duplex status to output. */
void sndx_dump_duplex_status(sndx_duplex_t* d, output_t* output);

/** @brief Open a pair of playback and capture devices and link them.
 *
 *  Process:
 *      1. Open pcm handles
 *      2. Set HW, SW params
 *      3. Initialize buffers
 *      4. Initialize pollfds
 *      5. Initialize timer
 *
 *  Errors:
 *      1. Could not open pcm
 *      2. Could not set hw/sw params
 */
int sndx_duplex_open(                //
    sndx_duplex_t** duplexp,         //
    const char*     playback_device, //
    const char*     capture_device,  //
    format_t        format,          //
    u32             rate,            //
    uframes_t       period_size,     //
    u32             periods,         //
    access_t        _access,         //
    snd_output_t*   output);

/** @brief Close playback and capture handles.
 *
 *  Process:
 *      1. Free buffers
 *      2. Free pollfds
 *      3. Free timer
 *      4. Drop pcm handles
 *      5. Free hardware
 *      6. Free self
 *
 *  Free cannot fail, as it checks for null before freeing and sets nullptr after.
 *
 *  Errors:
 *      1. Passed on from `snd`
 */
int sndx_duplex_close(sndx_duplex_t* d);

/** @brief Read from device to buffer
 *
 * Read from device to buffer, converting to float format
 */
sframes_t sndx_duplex_read( //
    sndx_duplex_t* d,
    uframes_t*     offset,
    uframes_t*     frames);

/** @brief Write from buffer to device
 *
 *
 * Write from buffer to device, converting from float to device format
 */
sframes_t sndx_duplex_write( //
    sndx_duplex_t* d,
    uframes_t*     offset,
    uframes_t*     frames);

/** @brief Write initial silence to get the playback device started.
 *
 * Usually set to `period_size * nperiods`.
 */
int sndx_duplex_write_initial_silence(sndx_duplex_t* d);
int sndx_duplex_write_initial_silence_direct(sndx_duplex_t* d);
