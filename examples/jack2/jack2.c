#include "duplex.h"
#include "pollfds.h"

typedef struct
{

    sndx_duplex_t*  d; ///< play, capture handles; buffers; read/write functions; timing; msgbuffer
    sndx_pollfds_t* p; ///< poll fd addresses and stats

} jack_t;

int  jack_open(jack_t** jackp, output_t* output);
void jack_close(jack_t* j);

int jack_start(jack_t* j);
int jack_stop(jack_t* j);

int jack_xrun(jack_t* j);
int jack_wait(jack_t* j, sframes_t* avail);

int jack_read(jack_t* j, uframes_t frames);
int jack_write(jack_t* j, uframes_t frames);

int jack_open(jack_t** jackp, output_t* output)
{
    int err;

    jack_t* j;
    j = calloc(1, sizeof(*j));
    RetVal_(!j, -ENOMEM, "Failed calloc jack_t* j");

    err = sndx_duplex_open(              //
        &j->d,                           //
        "hw:FC1,0", "hw:FC1,0",          //
        SND_PCM_FORMAT_S16_LE,           //
        48000, 128, 2,                   //
        SND_PCM_ACCESS_MMAP_INTERLEAVED, //
        output);
    SndGoto_(err, __close, "Failed sndx_duplex_open: %s");

    err = sndx_pollfds_open(&j->p, j->d->play, j->d->capt, j->d->rate, j->d->period_size, j->d->out);
    SndGoto_(err, __close, "Failed sndx_pollfds_open: %s");

    *jackp = j;

    return 0;

__close:
    jack_close(j);

    return err;
}

void jack_close(jack_t* j)
{
    sndx_duplex_close(j->d);
    sndx_pollfds_close(j->p);
    Free(j);
}

int jack_start(jack_t* j)
{
    int       err;
    output_t* output = j->d->out;

    j->p->poll_last = 0;
    j->p->poll_next = 0;

    // Since we use mmap, we need to start
    err = snd_pcm_prepare(j->d->play);
    SndReturn_(err, "Failed snd_pcm_prepare (play): %s");

    if (!j->d->linked)
    {
        err = snd_pcm_prepare(j->d->capt);
        SndReturn_(err, "Failed snd_pcm_prepare (capt): %s");
    }

    // Reallocate pfds
    err = sndx_pollfds_reset(j->p, j->d->play, j->d->capt, j->d->out);
    SndReturn_(err, "Failed sndx_pollfds_reset: %s");

    // Fill silence and pcm_start playback (if not linked, also capture)
    // Also starts timer, but maybe we should have a toggle for that
    err = sndx_duplex_start(j->d);
    SndReturn_(err, "Failed sndx_duplex_start: %s");

    return 0;
}

int jack_stop(jack_t* j)
{
    int       err;
    output_t* output = j->d->out;

    // TODO: Clear output: Can i not do that with drain silence (thought there's something like that)?
    // err = snd_pcm_drain(j->d->play);
    // SndReturn_(err, "Failed snd_pcm_drain (play): %s");

    // Drop play, if linked, also drop capture
    // Also stops timer, but maybe we should have a toggle for that
    err = sndx_duplex_stop(j->d);
    SndReturn_(err, "Failed sndx_duplex_stop: %s");

    return 0;
}

/** @brief Reimplementation of alsa_driver_xrun_recovery */
int jack_xrun(jack_t* j)
{
    int err;

    sndx_duplex_t* d      = j->d;
    output_t*      output = d->out;

    a_error("Starting xrun recovery");

    // Cannot return error
    sndx_pollfds_xrun(j->p, j->d->play, j->d->capt, j->d->out);

    // Jack calls it restart
    err = jack_stop(j);
    SndReturn_(err, "Failed: jack_stop: %s");

    err = jack_start(j);
    SndReturn_(err, "Failed: jack_start: %s");

    return 0;
}

int jack_wait(jack_t* j, sframes_t* avail)
{
    int       err;
    output_t* output = j->d->out;

    sndx_pollfds_error_t perr;

    perr = sndx_pollfds_wait(j->p, j->d->play, j->d->capt, j->d->out);
    switch (perr)
    {
    case POLLFD_SUCCESS: break;
    case POLLFD_FATAL: RetVal_(perr, -POLLFD_FATAL, "Fatal: Poll failed"); break;
    case POLLFD_NEEDS_RESTART:
        err = jack_xrun(j);
        SndReturn_(err, "Failed xrun recovery: %s");
        break;
    }

    // Stores value in argument avail
    perr = sndx_pollfds_avail(j->p, j->d->play, j->d->capt, avail, j->d->out);
    switch (perr)
    {
    case POLLFD_SUCCESS: break;
    case POLLFD_FATAL: RetVal_(perr, -POLLFD_FATAL, "Fatal: Avail failed"); break;
    case POLLFD_NEEDS_RESTART:
        err = jack_xrun(j);
        SndReturn_(err, "Failed xrun recovery: %s");
        break;
    }

    return 0;
}

int jack_read(jack_t* j, uframes_t nframes)
{
    int       err;
    output_t* output = j->d->out;

    err = -(nframes > j->p->period_size);
    Return_(err, "Failed: jack_read: nframes > j->p->period_size : %ld > %ld", nframes, j->p->period_size);

    uframes_t offset       = 0;
    uframes_t contiguous   = 0;
    sframes_t nread        = 0;
    sframes_t orig_nframes = nframes;

    const area_t* areas = j->d->buf_capt->dev;

    while (nframes)
    {
        contiguous = nframes;

        // Get address from alsa
        err = snd_pcm_mmap_begin(j->d->capt, &areas, &offset, &contiguous);
        SndReturn_(err, "Failed: snd_pcm_mmap_begin %s");

        // Map to device areas
        sndx_buffer_mmap_dev_areas(j->d->buf_capt, areas);

        // Copy to float buffer
        sndx_buffer_dev_to_buf(j->d->buf_capt, offset, contiguous);

        // Commit to move to next batch
        err = snd_pcm_mmap_commit(j->d->capt, offset, contiguous);
        SndReturn_(err, "Failed: snd_pcm_mmap_commit %s");

        nframes -= contiguous;
        nread   += contiguous;
    }

    err = -(nread != orig_nframes);
    Return_(err, "Failed: jack_read: nframes > j->p->period_size : %ld > %ld", nframes, j->p->period_size);

    return 0;
}

int jack_write(jack_t* j, uframes_t nframes)
{
    int       err;
    output_t* output = j->d->out;

    err = nframes > j->p->period_size;
    Return_(err, "Failed: jack_write: nframes > j->p->period_size : %ld > %ld", nframes, j->p->period_size);

    uframes_t offset       = 0;
    uframes_t contiguous   = 0;
    sframes_t nwritten     = 0;
    sframes_t orig_nframes = nframes;

    const area_t* areas = j->d->buf_play->dev;

    while (nframes)
    {
        contiguous = nframes;

        // Get address from alsa
        err = snd_pcm_mmap_begin(j->d->play, &areas, &offset, &contiguous);
        SndReturn_(err, "Failed: snd_pcm_mmap_begin %s");

        // Map to device areas
        sndx_buffer_mmap_dev_areas(j->d->buf_play, areas);

        // Copy to float buffer
        sndx_buffer_buf_to_dev(j->d->buf_play, offset, contiguous);

        // TODO: silence untouched

        // Commit to move to next batch
        err = snd_pcm_mmap_commit(j->d->play, offset, contiguous);
        SndReturn_(err, "Failed: snd_pcm_mmap_commit %s");

        nframes  -= contiguous;
        nwritten += contiguous;
    }

    err = -(nwritten != orig_nframes);
    Return_(err, "Failed: jack_read: nframes > j->p->period_size : %ld > %ld", nframes, j->p->period_size);

    return 0;
}

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stderr, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    jack_t* j;
    err = jack_open(&j, output);
    SndFatal_(err, "Failed jack_open: %s");

    err = jack_start(j);
    SndFatal_(err, "Failed jack_start: %s");

    sframes_t frames = 0;
    sframes_t tries  = 0;
    while (frames < j->d->rate && tries < 10)
    {
        sframes_t avail = 0;

        err = jack_wait(j, &avail);
        SndFatal_(err, "Failed jack_wait: %s");

        frames += avail;

        j->d->timer->frames_capt += avail;

        err = jack_read(j, avail);
        SndFatal_(err, "Failed jack_read: %s");

        // Callback

        err = jack_write(j, avail);
        SndFatal_(err, "Failed jack_write: %s");

        j->d->timer->frames_play += avail;

        tries += 1;
    }

    err = jack_stop(j);
    SndFatal_(err, "Failed jack_stop: %s");

    jack_close(j);

    snd_output_close(output);

    return 0;
}
