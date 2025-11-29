#pragma once

#include "sndx/buffer.h"

/** @fn sndx_duplex_copy_capt_to_play(sndx_buffer_t* buf_capt, sndx_buffer_t* buf_play, sframes_t len, void* data)
 *  @brief Helper to copy first channel of capture to all channels of playback (for mono -> stereo)
 *
 * Most cheap USB audio dongles have 2 playback and 1 capture channel.
 * Here we use float* gain as data
 */
void sndx_duplex_copy_capt_to_play( //
    sndx_buffer_t* buf_capt,
    sndx_buffer_t* buf_play,
    sframes_t      len,
    float*         gain);
