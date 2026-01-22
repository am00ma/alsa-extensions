/** @file duplex_demo.c
 *  @brief Demo of using duplex api
 */
#include "sndx/duplex.h"
#include "sndx/types.h"

// Application
int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stderr, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(              //
        &d,                              //
        "hw:A96,0", "hw:A96,0",          //
        SND_PCM_FORMAT_S32_LE,           //
        48000, 128, 2,                   //
        SND_PCM_ACCESS_MMAP_INTERLEAVED, //
        output);
    SndFatal_(err, "Failed sndx_duplex_open: %s");

    sndx_dump_duplex(d, d->out);

    // PREPARED -> RUNNING
    err = sndx_duplex_start(d);
    SndGoto_(err, __close, "Failed: sndx_duplex_start: %s");

    while (d->timer->frames_capt < d->rate * 10)
    {
        uframes_t avail = 0;

        err = sndx_duplex_wait(d, &avail);
        SndGoto_(err, __close, "Failed: sndx_duplex_wait: %s");

        uframes_t capt_frames = avail;
        uframes_t capt_offset = 0;

        err = sndx_duplex_read(d, &capt_frames, &capt_offset);
        SndGoto_(err, __close, "Failed: sndx_duplex_read: %s");

        // Copy soft buffer
        RANGE(chn, d->ch_play)
        RANGE(i, capt_offset, (isize)(capt_offset + capt_frames))
        {
            isize pos_play = i + chn * d->buf_play->frames;
            isize pos_capt = i;

            d->buf_play->bufdata[pos_play] = d->buf_capt->bufdata[pos_capt];
        }

        uframes_t play_frames = capt_frames;
        uframes_t play_offset = capt_offset;

        err = sndx_duplex_write(d, &play_frames, &play_offset);
        SndGoto_(err, __close, "Failed: sndx_duplex_write: %s");

        d->timer->frames_capt += avail;
    }

__close:

    err = sndx_duplex_stop(d);
    SndFatal_(err, "Failed sndx_duplex_stop: %s");

    err = sndx_duplex_close(d);
    SndFatal(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    return 0;
}
