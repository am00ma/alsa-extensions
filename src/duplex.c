#include "duplex.h"
#include "buffer.h"
#include "params.h"
#include "types.h"
#include <alsa/asoundlib.h>

static sndx_params_t default_params = {
    .channels    = 2,
    .format      = SND_PCM_FORMAT_S16,
    .access      = SND_PCM_ACCESS_MMAP_INTERLEAVED,
    .rate        = 48000,
    .periods     = 2,
    .period_size = 128,
};

void sndx_dump_duplex(sndx_duplex_t* d, snd_output_t* output)
{
    a_title("duplex:");
    a_info("  ch_play    : %d", d->ch_play);
    a_info("  ch_capt    : %d", d->ch_capt);
    a_info("  format     : %s", snd_pcm_format_name(d->format));
    a_info("  rate       : %d", d->rate);
    a_info("  period_size: %d", d->period_size);
    a_info("  buffer_size: %d", d->period_size * d->periods);
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
    if (!d) return -ENOMEM;

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

    // err = snd_pcm_nonblock(d->play, 0);
    // SndReturn_(err, "Failed: snd_pcm_nonblock: %s");

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
    d->rate        = rate;
    d->period_size = period_size;
    d->periods     = periods;

    d->ch_play = play_params.channels;
    d->ch_capt = capt_params.channels;

    err = snd_pcm_link(d->capt, d->play);
    SndGoto_(err, __close, "Failed: snd_pcm_link: %s");

    d->linked = true;

    // Allocate buffers
    uframes_t buffer_size = period_size * periods;

    err = sndx_buffer_open(&d->buf_play, d->format, d->ch_play, buffer_size, output);
    SndGoto_(err, __close, "Failed: sndx_buffer_open: %s"); // can only fail cause of memory

    err = sndx_buffer_open(&d->buf_capt, d->format, d->ch_capt, buffer_size, output);
    SndGoto_(err, __close, "Failed: sndx_buffer_open: %s"); // can only fail cause of memory

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

    int err;

    snd_output_t* output = d->out;

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

    sndx_buffer_close(d->buf_capt);
    sndx_buffer_close(d->buf_play);

    free(d);
    d = nullptr;

    return 0;
}

int sndx_duplex_write_initial_silence(sndx_duplex_t* d)
{
    int       err;
    output_t* output = d->out;

    // Prepare the device area
    const area_t* areas;

    sframes_t nleft  = d->period_size * d->periods;
    uframes_t offset = 0;
    uframes_t avail  = 0;

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

        nleft -= avail;
    }

    return 0;

__error:
    sndx_dump_duplex_status(d, d->out);
    return err;
}

sframes_t sndx_duplex_readbuf( //
    sndx_duplex_t* d,
    char*          buf,
    i64            len,
    uframes_t      offset,
    uframes_t*     frames,
    uframes_t*     max)
{
    i64 r;
    do { r = snd_pcm_mmap_readi(d->capt, buf, len); } while (r == -EAGAIN);
    if (r > 0)
    {
        *frames += r;
        if ((long)*max < r) *max = r;

        // After read, copy to soft buffer as float
        sndx_buffer_dev_to_buf(d->buf_capt, offset, r);
    }
    else {
        sndx_dump_duplex_status(d, d->out);
    }

    // showstat(handle, 0);
    return r;
}

sframes_t sndx_duplex_writebuf( //
    sndx_duplex_t* d,
    char*          buf,
    i64            len,
    uframes_t      offset,
    uframes_t*     frames,
    uframes_t*     max)
{
    snd_output_t* output = d->out;

    // Write from soft buffer to device as int
    sndx_buffer_buf_to_dev(d->buf_play, offset, len);

    long r;
    int  frame_bytes = (snd_pcm_format_physical_width(d->format) / 8) * d->ch_play;

    while (len > 0)
    {
        r = snd_pcm_mmap_writei(d->play, buf, len);
        if (r == -EAGAIN) continue;
        else if (r == -EINVAL) { SysReturn_(-EINVAL, "Invalid args, check access (MMAP / RW): %s"); }
        else if (r < 0) SndReturn_(r, "Failed: snd_pcm_mmap_writei: %s");
        buf     += r * frame_bytes;
        len     -= r;
        *frames += r;
        if ((long)*max < r) *max = r;
    }

    return 0;
}

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
        RANGE(i, len) { buf_play->data[i + (buf_size * chn)] = buf_capt->data[i] * (*gain); }
    }
    else if (buf_capt->channels == buf_play->channels)
    {
        RANGE(chn, buf_play->channels)
        RANGE(i, len) { buf_play->data[i + (buf_size * chn)] = buf_capt->data[i + (buf_size * chn)] * (*gain); }
    }
}
