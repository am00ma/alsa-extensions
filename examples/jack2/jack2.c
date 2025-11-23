#include "duplex.h"
#include "pollfds.h"
#include "types.h"

typedef struct
{

    sndx_duplex_t*  d; ///< play, capture handles; buffers; read/write functions; timing; msgbuffer
    sndx_pollfds_t* p; ///< poll fd addresses and stats

} jack_t;

int  jack_open(jack_t** jackp, output_t* output);
void jack_close(jack_t* j);

// int jack_start(jack_t* j);
// int jack_stop(jack_t* j);
// int jack_wait(jack_t* j, int extra_fd, uframes_t* avail, float* delayed_usecs);
// int jack_xrun(jack_t* j);
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

    err = sndx_pollfds_open(&j->p, j->d->play, j->d->capt, j->d->out);
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
    err = sndx_pollfds_open(&j->p, j->d->play, j->d->capt, j->d->out);
    SndReturn_(err, "Failed sndx_pollfds_open: %s");

    // Fill silence and pcm_start playback (if not linked, also capture)
    err = sndx_duplex_start(j->d);
    SndReturn_(err, "Failed sndx_duplex_start: %s");

    return 0;
}

int jack_stop(jack_t* j)
{
    int       err;
    output_t* output = j->d->out;

    err = snd_pcm_drop(j->d->play);
    SndReturn_(err, "Failed snd_pcm_drop: %s");

    if (!j->d->linked)
    {
        err = snd_pcm_drop(j->d->capt);
        SndReturn_(err, "Failed snd_pcm_drop (capt): %s");
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
