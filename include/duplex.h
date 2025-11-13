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
 */
typedef struct sndx_duplex_t
{
    snd_pcm_t* play;
    snd_pcm_t* capt;

    format_t format;
    u32      rate;
    u32      period_size;
    u32      periods;
    u32      ch_play;
    u32      ch_capt;
    bool     linked;

    sndx_buffer_t* buf_play;
    sndx_buffer_t* buf_capt;

    output_t* out;

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
    uframes_t       buffer_size,     //
    uframes_t       period_size,     //
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

/** @brief Read from buffer
 *
 * TODO: Is this device buffer?
 */
sframes_t sndx_duplex_readbuf( //
    sndx_duplex_t* d,
    char*          buf,
    i64            len,
    uframes_t      offset,
    uframes_t*     frames,
    uframes_t*     max);

/** @brief Write to buffer
 *
 * TODO: Is this device buffer?
 */
sframes_t sndx_duplex_writebuf( //
    sndx_duplex_t* d,
    char*          buf,
    i64            len,
    uframes_t      offset,
    uframes_t*     frames,
    uframes_t*     max);

/** @brief Write initial silence to get the playback device started.
 *
 * Usually set to `period_size * nperiods`.
 */
int sndx_duplex_write_initial_silence( //
    sndx_duplex_t* d,
    char*          play_buf,
    uframes_t*     frames_silence);

/** @fn sndx_duplex_copy_capt_to_play(sndx_buffer_t* buf_capt, sndx_buffer_t* buf_play, sframes_t len, void* data)
 *  @brief Helper to copy first channel of capture to all channels of playback (for mono -> stereo)
 *
 * Most cheap USB audio dongles have 2 playback and 1 capture channel.
 */
void sndx_duplex_copy_capt_to_play( //
    sndx_buffer_t* buf_capt,
    sndx_buffer_t* buf_play,
    sframes_t      len,
    void*          data);
