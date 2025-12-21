#include "sndx/duplex.h"
#include "sndx/buffer.h"
#include "sndx/params.h"
#include "sndx/types.h"
#include <sched.h>

static sndx_params_t default_params = {
    .channels    = 2,
    .format      = SND_PCM_FORMAT_S16,
    .access      = SND_PCM_ACCESS_MMAP_INTERLEAVED,
    .rate        = 48000,
    .periods     = 2,
    .period_size = 128,
};

void sndx_dump_duplex(sndx_duplex_t* d, output_t* output)
{
    a_title("duplex:");
    a_info("  ch_play    : %d", d->ch_play);
    a_info("  ch_capt    : %d", d->ch_capt);
    a_info("  format     : %s", snd_pcm_format_name(d->format));
    a_info("  rate       : %d", d->rate);
    a_info("  period_size: %ld", d->period_size);
    a_info("  buffer_size: %ld", d->period_size * d->periods);
    a_info("  nperiods   : %d", d->periods);
    a_info("  linked     : %d", d->linked);

    a_title("Play:");
    snd_pcm_dump(d->play, d->out);

    sndx_dump_buffer(d->buf_play, output);

    a_title("Capture:");
    snd_pcm_dump(d->capt, d->out);

    sndx_dump_buffer(d->buf_capt, output);

    sndx_dump_duplex_status(d, output);
}

void sndx_dump_duplex_status(sndx_duplex_t* d, output_t* output)
{
    int err;

    snd_pcm_status_t* status;
    snd_pcm_status_alloca(&status);

    a_info("Capture status:");
    err = snd_pcm_status(d->capt, status);
    SndFatal(err, "Stream status error: %s");
    snd_pcm_status_dump(status, output);

    a_info("Playback status:");
    err = snd_pcm_status(d->play, status);
    SndFatal(err, "Stream status error: %s");
    snd_pcm_status_dump(status, output);
}

int sndx_duplex_open(                //
    sndx_duplex_t** duplexp,         //
    const char*     playback_device, //
    const char*     capture_device,  //
    format_t        format,          //
    u32             rate,            //
    uframes_t       period_size,     //
    u32             periods,         //
    access_t        _access,         //
    output_t*       output)
{
    int err;

    sndx_duplex_t* d;

    d = calloc(1, sizeof(*d));
    RetVal_(!d, -ENOMEM, "Failed calloc duplex_t* b");

    d->out = output;

    err = snd_pcm_open(&d->play, playback_device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    SndGoto_(err, __close, "Failed: snd_pcm_open: %s");

    err = snd_pcm_open(&d->capt, capture_device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    SndGoto_(err, __close, "Failed: snd_pcm_open: %s");

    sndx_params_t play_params = default_params;
    sndx_params_t capt_params = default_params;
    play_params.format = capt_params.format = format;
    play_params.access = capt_params.access = _access;
    play_params.rate = capt_params.rate = rate;
    play_params.period_size = capt_params.period_size = period_size;
    play_params.periods = capt_params.periods = periods;

    err = sndx_set_params(        //
        d->play,                  //
        &play_params.channels,    //
        &play_params.format,      //
        &play_params.rate,        //
        &play_params.period_size, //
        &play_params.periods,     //
        play_params.access,       //
        false,                    //
        d->out);                  //
    SndGoto_(err, __close, "Failed: sndx_set_params: %s");

    err = !((rate == play_params.rate) &&       //
            (periods == play_params.periods) && //
            (period_size == play_params.period_size));
    Goto_(err, __close, "Failed: params check");

    err = snd_pcm_nonblock(d->play, 0);
    SndReturn_(err, "Failed: snd_pcm_nonblock (play): %s");

    err = snd_pcm_nonblock(d->capt, 0);
    SndReturn_(err, "Failed: snd_pcm_nonblock (capt): %s");

    err = sndx_set_params(        //
        d->capt,                  //
        &capt_params.channels,    //
        &capt_params.format,      //
        &capt_params.rate,        //
        &capt_params.period_size, //
        &capt_params.periods,     //
        capt_params.access,       //
        false,                    //
        d->out);                  //
    SndGoto_(err, __close, "Failed: sndx_set_params: %s");

    err = !((rate == capt_params.rate) &&       //
            (periods == capt_params.periods) && //
            (period_size == capt_params.period_size));
    Return_(err, "Could not set params");

    err = play_params.format != capt_params.format;
    Return_(err, "play_params.format != capt_params.format");

    err = snd_pcm_nonblock(d->capt, SND_PCM_NONBLOCK);
    SndGoto_(err, __close, "Failed: snd_pcm_nonblock: %s");

    d->format      = play_params.format;
    d->access      = play_params.access;
    d->rate        = rate;
    d->period_size = period_size;
    d->periods     = periods;

    d->ch_play = play_params.channels;
    d->ch_capt = capt_params.channels;

    err = snd_pcm_link(d->play, d->capt);
    SndGoto_(err, __close, "Failed: snd_pcm_link: %s");

    d->linked = true;

    // Allocate buffers
    uframes_t buffer_size = period_size * periods;

    err = sndx_buffer_open(&d->buf_play, d->format, d->ch_play, buffer_size, output);
    SndGoto_(err, __close, "Failed: sndx_buffer_open: %s"); // can only fail cause of memory

    err = sndx_buffer_open(&d->buf_capt, d->format, d->ch_capt, buffer_size, output);
    SndGoto_(err, __close, "Failed: sndx_buffer_open: %s"); // can only fail cause of memory

    d->timer = calloc(1, sizeof(sndx_timer_t));
    err      = -(!d->timer);
    Goto_(err, __close, "Failed: calloc(timer)"); // can only fail cause of memory

    err = sndx_pollfds_open(&d->pfd, d->play, d->capt, d->rate, d->period_size, d->out);
    SndGoto_(err, __close, "Failed sndx_pollfds_open: %s");

    *duplexp = d;

    return 0;

__close:
    sndx_duplex_close(d);
    *duplexp = nullptr;

    return err;
}

int sndx_duplex_close(sndx_duplex_t* d)
{
    if (!d) return 0;

    int       err;
    output_t* output = d->out;

    if (d->capt)
    {
        err = snd_pcm_close(d->capt);
        SndReturn_(err, "Failed: snd_pcm_close: %s");
    }

    if (d->play)
    {
        err = snd_pcm_close(d->play);
        SndReturn_(err, "Failed: snd_pcm_close: %s");
    }

    sndx_pollfds_close(d->pfd);
    sndx_buffer_close(d->buf_capt);
    sndx_buffer_close(d->buf_play);

    Free(d->timer);
    Free(d);

    return 0;
}

int sndx_duplex_write_rw_initial_silence(sndx_duplex_t* d)
{
    int       err;
    output_t* output = d->out;

    char* buf[4096];

    err = snd_pcm_format_set_silence(d->format, buf, d->period_size);
    SndGoto_(err, __error, "Failed: snd_pcm_format_set_silence: %s");

    RANGE(i, d->periods)
    {
        err = snd_pcm_writei(d->play, buf, d->period_size);
        SndGoto_(err, __error, "Failed: snd_pcm_writei: %s");
    }

    // a_info("Wrote silence: %ld frames (period_size=%ld, periods=%d)", //
    //        d->period_size * d->periods, d->period_size, d->periods);

    return 0;

__error:
    sndx_dump_duplex_status(d, d->out);
    return err;
}

int sndx_duplex_write_mmap_initial_silence_direct(sndx_duplex_t* d)
{
    int       err;
    output_t* output = d->out;

    char* buf[4096];

    err = snd_pcm_format_set_silence(d->format, buf, d->period_size);
    SndGoto_(err, __error, "Failed: snd_pcm_format_set_silence: %s");

    sframes_t nleft    = d->period_size * d->periods;
    sframes_t nwritten = 0;
    uframes_t avail    = 0;

    while (nleft)
    {
        // Restrict to nleft
        avail = nleft;

        // Restrict to period_size
        avail = avail > d->period_size ? d->period_size : avail;

        err = snd_pcm_mmap_writei(d->play, buf, avail);
        SndGoto_(err, __error, "Failed: snd_pcm_mmap_begin: %s");

        nleft    -= avail;
        nwritten += avail;
    }
    a_info("Wrote silence: %ld frames (period_size=%ld, periods=%d)", nwritten, d->period_size, d->periods);

    return 0;

__error:
    sndx_dump_duplex_status(d, d->out);
    return err;
}

int sndx_duplex_write_mmap_initial_silence(sndx_duplex_t* d)
{
    int       err;
    output_t* output = d->out;

    // Prepare the device area
    const area_t* areas;

    sframes_t nleft    = d->period_size * d->periods;
    sframes_t nwritten = 0;
    uframes_t offset   = 0;
    uframes_t avail    = 0;

    while (nleft)
    {
        err = snd_pcm_avail_update(d->play);
        SndGoto_(err, __error, "Failed: snd_pcm_avail_update: %s");

        // Restrict to nleft
        avail = err > nleft ? nleft : err;

        // Restrict to period_size
        avail = avail > d->period_size ? d->period_size : avail;

        err = snd_pcm_mmap_begin(d->play, &areas, &offset, &avail);
        SndGoto_(err, __error, "Failed: snd_pcm_mmap_begin: %s");

        err = snd_pcm_areas_silence(areas, offset, d->ch_play, avail, d->format);
        SndGoto_(err, __error, "Failed: snd_pcm_area_silence: %s");

        err = snd_pcm_mmap_commit(d->play, offset, avail);
        SndGoto_(err, __error, "Failed: snd_pcm_mmap_commit: %s");

        nleft    -= avail;
        nwritten += avail;
    }
    a_info("Wrote silence: %ld frames (period_size=%ld, periods=%d)", nwritten, d->period_size, d->periods);

    return 0;

__error:
    sndx_dump_duplex_status(d, d->out);
    return err;
}

int sndx_duplex_start(sndx_duplex_t* d)
{
    int       err;
    output_t* output = d->out;

    switch (d->access)
    {
    case SND_PCM_ACCESS_MMAP_INTERLEAVED:
        err = sndx_duplex_write_mmap_initial_silence(d);
        SndCheck_(err, "Failed sndx_duplex_write_mmap_initial_silence: %s");

        // Apparently, we start manually only in this case
        err = snd_pcm_start(d->play);
        SndReturn_(err, "Failed snd_pcm_start play: %s");

        break;
    case SND_PCM_ACCESS_RW_INTERLEAVED:
        err = sndx_duplex_write_rw_initial_silence(d);
        SndCheck_(err, "Failed sndx_duplex_write_rw_initial_silence: %s");
        break;

    default: RetVal(-1, -1, "Unknown access: %s", snd_pcm_access_name(d->access));
    }

    if (!d->linked)
    {
        err = snd_pcm_start(d->capt);
        SndCheck_(err, "Failed snd_pcm_start capt: %s");
    }

    // RUNNING
    // sndx_dump_duplex_status(d, output);

    // Start the timer (TODO: provide option to check if in xrun)
    sndx_timer_start(d->timer, d->rate, d->play, d->capt);

    return 0;
}

int sndx_duplex_stop(sndx_duplex_t* d)
{
    int       err;
    output_t* output = d->out;

    err = snd_pcm_drop(d->play);
    SndCheck_(err, "Failed snd_pcm_drop play: %s");

    if (!d->linked)
    {
        err = snd_pcm_drop(d->capt);
        SndCheck_(err, "Failed snd_pcm_drop capt: %s");
    }

    // Stop the timer (TODO: provide option to check if in xrun)
    sndx_timer_stop(d->timer, d->play, d->capt);

    // sndx_dump_timer(d->timer, d->out);

    return 0;
}

int sndx_duplex_read(sndx_duplex_t* d, uframes_t* frames, uframes_t* offset)
{
    int       err;
    output_t* output = d->out;

    err = -(*frames > d->period_size);
    Return_(err, "Failed: sndx_duplex_read: frames > period_size : %ld > %ld", *frames, d->period_size);

    uframes_t dev_offset = 0;
    uframes_t dev_frames = *frames;

    uframes_t buf_offset = *offset;

    uframes_t contiguous   = 0;
    sframes_t nread        = 0;
    sframes_t orig_nframes = *frames;

    const area_t* areas = d->buf_capt->dev;

    while (dev_frames)
    {
        contiguous = dev_frames;

        // Get address from alsa
        err = snd_pcm_mmap_begin(d->capt, &areas, &dev_offset, &contiguous);
        SndReturn_(err, "Failed: snd_pcm_mmap_begin %s");

        // Map to device areas
        sndx_buffer_mmap_dev_areas(d->buf_capt, areas);

        // Copy from device areas to float buffer
        // NOTE: Soft buffer will also wrap, so how to present in callback?
        sndx_buffer_dev_to_buf_skew(d->buf_capt, contiguous, dev_offset, buf_offset);

        // Commit to move to next batch
        err = snd_pcm_mmap_commit(d->capt, dev_offset, contiguous);
        SndReturn_(err, "Failed: snd_pcm_mmap_commit %s");

        dev_frames -= contiguous;
        buf_offset += contiguous;
        nread      += contiguous;
    }

    *frames = nread;

    err = -(nread != orig_nframes);
    Return_(err, "Failed: sndx_duplex_read: nread != orig_nframes : %ld != %ld", nread, orig_nframes);

    return 0;
}

int sndx_duplex_write(sndx_duplex_t* d, uframes_t* frames, uframes_t* offset)
{
    int       err;
    output_t* output = d->out;

    err = *frames > d->period_size;
    Return_(err, "Failed: sndx_duplex_write: nframes > period_size : %ld > %ld", *frames, d->period_size);

    uframes_t dev_offset = 0;
    uframes_t dev_frames = *frames;

    uframes_t buf_offset = *offset;

    uframes_t contiguous   = 0;
    sframes_t nwritten     = 0;
    sframes_t orig_nframes = *frames;

    const area_t* areas = d->buf_play->dev;

    while (dev_frames)
    {
        contiguous = dev_frames;

        // Get address from alsa
        err = snd_pcm_mmap_begin(d->play, &areas, &dev_offset, &contiguous);
        SndReturn_(err, "Failed: snd_pcm_mmap_begin %s");

        // Map to device areas
        sndx_buffer_mmap_dev_areas(d->buf_play, areas);

        // Copy from float buffer to device areas
        sndx_buffer_buf_to_dev_skew(d->buf_play, contiguous, buf_offset, dev_offset);

        // TODO: silence untouched - should be automatic with soft buffer

        // Commit to move to next batch
        err = snd_pcm_mmap_commit(d->play, dev_offset, contiguous);
        SndReturn_(err, "Failed: snd_pcm_mmap_commit %s");

        dev_frames -= contiguous;
        nwritten   += contiguous;
    }

    err = -(nwritten != orig_nframes);
    Return_(err, "Failed: sndx_duplex_write: nwritten != orig_nframes : %ld != %ld", nwritten, orig_nframes);

    return 0;
}

int sndx_duplex_set_schduler(output_t* output)
{
    int err;

    struct sched_param sched_param;
    int                policy  = SCHED_FIFO;
    const char*        spolicy = "FIFO";

    err = sched_getparam(0, &sched_param);
    SysReturn_(err, "Failed: sched_getparam: %s");

    sched_param.sched_priority = sched_get_priority_max(policy);

    err = sched_setscheduler(0, policy, &sched_param);
    RetVal_(err, -1,                                           //
            "Failed: sched_setscheduler: %s with priority %i", //
            spolicy, sched_param.sched_priority);

    a_info("Scheduler set to %s with priority %i...", //
           spolicy, sched_param.sched_priority);

    return 0;
}

int sndx_duplex_wait(sndx_duplex_t* d, uframes_t* avail)
{

    int       err;
    output_t* output = d->out;

    // Wait -> TODO: error checking
    snd_pcm_wait(d->capt, 1000);

    // NOTE: No longer copying twice
    sframes_t capt_avail = snd_pcm_avail_update(d->capt);
    SndReturn_(capt_avail, "Failed: snd_pcm_avail_update (capt): %s");

    sframes_t play_avail = snd_pcm_avail_update(d->play);
    SndReturn_(play_avail, "Failed: snd_pcm_avail_update (play): %s");

    err = -(play_avail < (i64)d->period_size);
    Return_(err, "Failed: play_avail < d->period_size: %ld < %ld", play_avail, d->period_size);

    err = -(capt_avail < (i64)d->period_size);
    Return_(err, "Failed: capt_avail < d->period_size: %ld < %ld", capt_avail, d->period_size);

    sframes_t nframes = d->period_size;

    *avail = capt_avail < play_avail ? capt_avail : play_avail;
    *avail = nframes < play_avail ? nframes : play_avail;

    return 0;
}
