#include "sndx/callback.h"

void sndx_duplex_copy_capt_to_play( //
    sndx_buffer_t* buf_capt,
    sndx_buffer_t* buf_play,
    sframes_t      len,
    float*         gain)
{
    // Actual realtime check - can just return, but better to flag
    AssertMsg(len >= 0, "Received negative len: %ld", len);

    // Copy capture channels to playback
    isize buf_size = buf_capt->frames; // buf_capt->frames == buf_play->frames Guaranteed by setup
    if (buf_capt->channels == 1)
    {
        RANGE(chn, buf_play->channels)
        RANGE(i, len) { buf_play->bufdata[i + (buf_size * chn)] = buf_capt->bufdata[i] * (*gain); }
    }
    else if (buf_capt->channels == buf_play->channels)
    {
        RANGE(chn, buf_play->channels)
        RANGE(i, len) { buf_play->bufdata[i + (buf_size * chn)] = buf_capt->bufdata[i + (buf_size * chn)] * (*gain); }
    }
}
