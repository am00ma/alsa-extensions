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
#include "types.h"

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
        // Wait -> no error checking, still works somehow
        snd_pcm_wait(d->capt, 1000);

        // NOTE: No longer copying twice
        sframes_t capt_avail = snd_pcm_avail_update(d->capt);
        SndGoto_(capt_avail, __close, "Failed: snd_pcm_avail_update (capt): %s");

        sframes_t play_avail = snd_pcm_avail_update(d->play);
        SndGoto_(play_avail, __close, "Failed: snd_pcm_avail_update (play): %s");

        err = -(play_avail < (i64)d->period_size);
        Goto_(err, __close, "Failed: play_avail < (i64)d->period_size (play): %ld < %ld", play_avail, d->period_size);

        err = -(capt_avail < (i64)d->period_size);
        Goto_(err, __close, "Failed: capt_avail < (i64)d->period_size (capt): %ld < %ld", capt_avail, d->period_size);

        const area_t* capt_areas = d->buf_capt->dev;

        sframes_t nframes = d->period_size;

        uframes_t offset     = 0;
        uframes_t contiguous = 0;

        while (nframes)
        {
            contiguous = nframes;

            // Get address from alsa
            err = snd_pcm_mmap_begin(d->capt, &capt_areas, &offset, &contiguous);
            SndReturn_(err, "Failed: snd_pcm_mmap_begin %s");

            // Map to device areas
            sndx_buffer_mmap_dev_areas(d->buf_capt, capt_areas);

            // Copy from device areas to float buffer
            sndx_buffer_dev_to_buf(d->buf_capt, offset, contiguous);

            // Commit to move to next batch
            err = snd_pcm_mmap_commit(d->capt, offset, contiguous);
            SndReturn_(err, "Failed: snd_pcm_mmap_commit %s");

            d->timer->frames_capt += contiguous;
            nframes               -= contiguous;
        }

        // Copy soft buffer
        isize pos_play, pos_capt;
        RANGE(chn, d->ch_play)
        RANGE(i, offset, (isize)(offset + contiguous))
        {
            pos_play = i + chn * d->buf_play->frames;
            pos_capt = i;

            d->buf_play->bufdata[pos_play] = d->buf_capt->bufdata[pos_capt];
        }

        const area_t* play_areas = d->buf_play->dev;

        // Write
        offset     = 0;
        contiguous = 0;
        nframes    = d->period_size;

        while (nframes)
        {
            contiguous = nframes;

            // Get address from alsa
            err = snd_pcm_mmap_begin(d->play, &play_areas, &offset, &contiguous);
            SndReturn_(err, "Failed: snd_pcm_mmap_begin %s");

            // Map to device areas
            sndx_buffer_mmap_dev_areas(d->buf_play, play_areas);

            // Copy from float buffer to device areas
            sndx_buffer_buf_to_dev(d->buf_play, offset, contiguous);

            // TODO: silence untouched

            // Commit to move to next batch
            err = snd_pcm_mmap_commit(d->play, offset, contiguous);
            SndReturn_(err, "Failed: snd_pcm_mmap_commit %s");

            d->timer->frames_play += contiguous;
            nframes               -= contiguous;
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
