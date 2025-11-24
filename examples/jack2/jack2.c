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
int jack_wait(jack_t* j);

// int jack_read(jack_t* j, uframes_t frames);
// int jack_write(jack_t* j, uframes_t frames);

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

    j->p->period_usecs = (u64)floor((((float)j->d->period_size) / j->d->rate) * 1000000.0f);
    j->p->poll_timeout = (int)floor(1.5f * j->p->period_usecs);
    j->p->poll_next    = 0;
    j->p->poll_last    = 0;

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

    err = snd_pcm_prepare(j->d->play);
    SndReturn_(err, "Failed snd_pcm_prepare (play): %s");

    if (!j->d->linked)
    {
        err = snd_pcm_prepare(j->d->capt);
        SndReturn_(err, "Failed snd_pcm_prepare (capt): %s");
    }

    // Reallocate pfds
    sndx_pollfds_close(j->p);
    err = sndx_pollfds_open(&j->p, j->d->play, j->d->capt, j->d->rate, j->d->period_size, j->d->out);
    SndReturn_(err, "Failed sndx_pollfds_open: %s");

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

    status_t* status;
    snd_pcm_status_alloca(&status);

    err = snd_pcm_status(d->capt, status);
    SndCheck_(err, "status error: %s");

    // Jack only checks capt
    // err = snd_pcm_status(d->play, status);
    // SndCheck_(err, "status error: %s");

    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_SUSPENDED)
    {
        a_info("**** alsa_pcm: pcm in suspended state, resuming it");

        err = snd_pcm_prepare(d->capt);
        SndCheck_(err, "error preparing capture after suspend: %s");

        err = snd_pcm_prepare(d->play);
        SndCheck_(err, "error preparing playback after suspend: %s");
    }

    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN)
    {
        j->p->xrun_count++;

        struct timeval now, diff, tstamp;
        snd_pcm_status_get_tstamp(status, &now);
        snd_pcm_status_get_trigger_tstamp(status, &tstamp);

        timersub(&now, &tstamp, &diff);
        j->p->delayed_usecs = diff.tv_sec * 1000000.0 + diff.tv_usec;
        a_info("**** alsa_pcm: xrun of at least %.3f msecs", j->p->delayed_usecs / 1000.0);

        a_info("Repreparing capture");
        err = snd_pcm_prepare(d->capt);
        SndCheck_(err, "error preparing capture after xrun: %s");

        a_info("Repreparing playback");
        err = snd_pcm_prepare(d->play);
        SndCheck_(err, "error preparing playback after xrun: %s");
    }

    // Jack calls it restart

    err = jack_stop(j);
    SndReturn_(err, "Failed: jack_stop: %s");

    err = jack_start(j);
    SndReturn_(err, "Failed: jack_start: %s");

    return 0;
}

int jack_wait(jack_t* j)
{
    sndx_pollfds_poll_error_t perr;

    perr = sndx_pollfds_wait(j->p, j->d->play, j->d->capt, j->d->out);

    switch (perr)
    {
    case POLLFD_SUCCESS: break;
    case POLLFD_FATAL: break;
    case POLLFD_NEEDS_RESTART: break;
    }
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

    err = jack_stop(j);
    SndFatal_(err, "Failed jack_stop: %s");

    jack_close(j);

    snd_output_close(output);
}
