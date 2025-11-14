#include "duplex.h"
#include "timer.h"

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(              //
        &d,                              //
        "hw:PCH,0", "hw:PCH,0",          //
        SND_PCM_FORMAT_S16_LE,           //
        48000, 128, 2,                   //
        SND_PCM_ACCESS_MMAP_INTERLEAVED, //
        output);
    SndFatal(err, "Failed sndx_duplex_open: %s");

    // This should error with broken pipe
    err = snd_pcm_start(d->play);
    SndCheck(err, "Failed snd_pcm_start: %s");

    // PREPARED
    sndx_dump_duplex_status(d, output);

    // Timer
    sndx_timer_t timer = {};

    err = sndx_duplex_write_initial_silence(d);
    SndCheck(err, "Failed sndx_duplex_write_initial_silence: %s");

    err = snd_pcm_start(d->play);
    SndCheck(err, "Failed snd_pcm_start: %s");

    // RUNNING
    sndx_dump_duplex_status(d, output);

    // Start prep for loop -> on error goto __error
    sndx_timer_start(&timer, d->rate, d->play, d->capt);

    // Read write loop
    while (timer.frames_capt < timer.rate)
    {

        const area_t* areas_capt;
        const area_t* areas_play;

        sframes_t nleft      = d->period_size;
        uframes_t offset     = 0;
        uframes_t avail_capt = 0;
        uframes_t avail_play = 0;

        while (nleft)
        {
            err = snd_pcm_wait(d->capt, 1000);
            SndGoto_(err, __close, "Failed: snd_pcm_wait: %s");

            err = snd_pcm_avail_update(d->capt);
            SndGoto_(err, __close, "Failed: snd_pcm_avail_update: %s");

            // Restrict to nleft
            avail_capt = err > nleft ? nleft : err;

            // Restrict to period_size
            avail_capt = avail_capt > d->period_size ? d->period_size : avail_capt;

            // Get capture buffer
            offset = 0;
            err    = snd_pcm_mmap_begin(d->capt, &areas_capt, &offset, &avail_capt);
            SndGoto_(err, __close, "Failed: snd_pcm_mmap_begin: %s");

            // Register frames read
            timer.frames_capt += avail_capt;

            // Read to float buffer

            // Commit read
            err = snd_pcm_mmap_commit(d->capt, offset, avail_capt);
            SndGoto_(err, __close, "Failed: snd_pcm_mmap_commit: %s");

            // We can now pass it off to the callback
            avail_play = avail_capt;

            // Get the play buffer
            offset = 0;
            err    = snd_pcm_mmap_begin(d->play, &areas_play, &offset, &avail_play);
            SndGoto_(err, __close, "Failed: snd_pcm_mmap_begin: %s");

            AssertMsg((avail_capt == avail_play),              //
                      "Could not write as much as read: "      //
                      "avail_capt == avail_play (%ld != %ld)", //
                      avail_capt, avail_play);

            // Write from float buffer

            // Commit write
            err = snd_pcm_mmap_commit(d->play, offset, avail_play);
            SndGoto_(err, __close, "Failed: snd_pcm_mmap_commit: %s");

            // Register frames to written
            timer.frames_play += avail_play;

            nleft -= avail_capt;
        }
    }

    sndx_timer_stop(&timer, d->play, d->capt);

    sndx_dump_timer(&timer, d->out);

__close:

    err = sndx_duplex_close(d);
    SndFatal(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    return 0;
}
