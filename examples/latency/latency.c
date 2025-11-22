/** @file latency.c
 *  @brief Implementation of `latency.c` with sndx_duplex
 *
 *  Seems like simplest possible implementation.
 *
 *  1. No checking xruns, directly exits, still works, never got an xrun
 *  2. Not even error checking `snd_pcm_wait`
 *  3. Direct readi, writei
 *  4. Use of mmap is useless however `write_initial_silence` uses mmap
 *
 */
#include "duplex.h"

// Application
int main()
{
    int err; ///< Brief description after the member

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
        i64 r;

        // Wait -> no error checking, still works somehow
        snd_pcm_wait(d->capt, 1000);

        // NOTE: We are copying twice by not using mmap_begin, mmap_commit
        //       thus rendering the mmap quite useless

        // Read - prob period_size as that is guaranteed in avail_min?
        uframes_t len = d->period_size;
        do { r = snd_pcm_mmap_readi(d->capt, d->buf_capt->devdata, len); } while (r == -EAGAIN);
        if (r > 0) { d->timer->frames_capt += r; }

        // To soft buffer -> reads from devdata
        sndx_buffer_dev_to_buf(d->buf_capt, 0, r);

        // Copy soft buffer
        isize pos_play, pos_capt;
        RANGE(chn, d->ch_play)
        RANGE(i, r)
        {
            pos_play = i + chn * d->buf_play->frames;
            pos_capt = i;

            d->buf_play->bufdata[pos_play] = d->buf_capt->bufdata[pos_capt];
        }

        // To write buffer -> writes to devdata
        sndx_buffer_buf_to_dev(d->buf_play, 0, r);

        // Write
        len               = r;
        isize pos_devdata = 0;
        while (len > 0)
        {
            r = snd_pcm_mmap_writei(d->play, &d->buf_play->devdata[pos_devdata], len);
            if (r == -EAGAIN) continue;
            SndGoto_(r, __close, "Failed: snd_pcm_mmap_writei: %s");

            pos_devdata += r * d->buf_play->channels * d->buf_play->bytes;

            len -= r;

            d->timer->frames_play += r;
        }
    }

__close:

    err = sndx_duplex_stop(d);
    SndFatal_(err, "Failed sndx_duplex_stop: %s");

    err = sndx_duplex_close(d);
    SndFatal(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    return 0;
}
