#include "duplex.h"
#include "params.h"
#include "types.h"
#include <alsa/asoundlib.h>

static sndx_params_t default_params = {
    .channels    = 2,
    .format      = SND_PCM_FORMAT_S16,
    .access      = SND_PCM_ACCESS_MMAP_INTERLEAVED,
    .rate        = 48000,
    .nperiods    = 2,
    .period_size = 128,
    .buffer_size = 256,
};

void sndx_dump_duplex(sndx_duplex_t* d, snd_output_t* output)
{
    a_title("duplex:");
    a_info("  ch_play    : %d", d->ch_play);
    a_info("  ch_capt    : %d", d->ch_capt);
    a_info("  format     : %s", snd_pcm_format_name(d->format));
    a_info("  rate       : %d", d->rate);
    a_info("  period_size: %d", d->period_size);
    a_info("  buffer_size: %d", d->period_size * d->nperiods);
    a_info("  nperiods   : %d", d->nperiods);
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
    uframes_t       buffer_size,     //
    uframes_t       period_size,     //
    access_t        _access,         //
    output_t*       output)
{
    int err;

    sndx_duplex_t* d;

    d = (sndx_duplex_t*)calloc(1, sizeof(*d));
    if (!d) return -ENOMEM;

    d->out = output;

    err = snd_pcm_open(&d->play, playback_device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    SndReturn_(err, "Failed: snd_pcm_open: %s");

    err = snd_pcm_open(&d->capt, capture_device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    SndReturn_(err, "Failed: snd_pcm_open: %s");

    sndx_params_t play_params = default_params;
    sndx_params_t capt_params = default_params;
    play_params.format = capt_params.format = format;
    play_params.access = capt_params.access = _access;
    play_params.rate = capt_params.rate = rate;
    play_params.buffer_size = capt_params.buffer_size = buffer_size;
    play_params.period_size = capt_params.period_size = period_size;
    play_params.nperiods = capt_params.nperiods = buffer_size / period_size;

    err = sndx_set_params(        //
        d->play,                  //
        &play_params.channels,    //
        &play_params.format,      //
        &play_params.rate,        //
        &play_params.buffer_size, //
        &play_params.period_size, //
        play_params.access,       //
        false,                    //
        d->out);                  //
    SndReturn_(err, "Failed: sndx_set_params: %s");

    err = !((rate == play_params.rate) &&               //
            (buffer_size == play_params.buffer_size) && //
            (period_size == play_params.period_size));
    Return_(err, "Could not set params");

    // err = snd_pcm_nonblock(d->play, 0);
    // SndReturn_(err, "Failed: snd_pcm_nonblock: %s");

    err = sndx_set_params(        //
        d->capt,                  //
        &capt_params.channels,    //
        &capt_params.format,      //
        &capt_params.rate,        //
        &capt_params.buffer_size, //
        &capt_params.period_size, //
        capt_params.access,       //
        false,                    //
        d->out);                  //
    SndReturn_(err, "Failed: sndx_set_params: %s");

    err = !((rate == capt_params.rate) &&               //
            (buffer_size == capt_params.buffer_size) && //
            (period_size == capt_params.period_size));
    Return_(err, "Could not set params");

    err = play_params.format != capt_params.format;
    Return_(err, "play_params.format != capt_params.format");

    err = snd_pcm_nonblock(d->capt, SND_PCM_NONBLOCK);
    SndReturn_(err, "Failed: snd_pcm_nonblock: %s");

    d->format      = play_params.format;
    d->rate        = rate;
    d->period_size = period_size;
    d->nperiods    = buffer_size / period_size;

    d->ch_play = play_params.channels;
    d->ch_capt = capt_params.channels;

    err = snd_pcm_link(d->capt, d->play);
    SndReturn_(err, "Failed: snd_pcm_link: %s");

    d->linked = true;

    // Allocate buffers
    err = sndx_buffer_open(&d->buf_play, d->format, d->ch_play, buffer_size, output);
    SndReturn_(err, "Failed: sndx_buffer_open(play): %s");

    err = sndx_buffer_open(&d->buf_capt, d->format, d->ch_capt, buffer_size, output);
    SndReturn_(err, "Failed: sndx_buffer_open(capt): %s");

    *duplexp = d;

    return 0;
}

int sndx_duplex_close(sndx_duplex_t* d)
{
    int err;

    snd_output_t* output = d->out;

    err = snd_pcm_close(d->capt);
    SndReturn_(err, "Failed: snd_pcm_close: %s");

    err = snd_pcm_close(d->play);
    SndReturn_(err, "Failed: snd_pcm_close: %s");

    sndx_buffer_close(d->buf_capt);
    sndx_buffer_close(d->buf_play);

    free(d);

    return 0;
}

sframes_t sndx_duplex_readbuf(sndx_duplex_t* d, char* buf, long len, uframes_t* frames, uframes_t* max)
{
    long r;
    do { r = snd_pcm_mmap_readi(d->capt, buf, len); } while (r == -EAGAIN);
    if (r > 0)
    {
        *frames += r;
        if ((long)*max < r) *max = r;
    }
    else {
        sndx_dump_duplex_status(d, d->out);
    }
    // showstat(handle, 0);
    return r;
}

sframes_t sndx_duplex_writebuf(sndx_duplex_t* d, char* buf, long len, size_t* frames)
{
    snd_output_t* output = d->out;

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
    }
    return 0;
}

int sndx_duplex_write_initial_silence(sndx_duplex_t* d, char* play_buf, uframes_t* frames_silence)
{
    int err;

    err = snd_pcm_format_set_silence(d->format, play_buf, d->period_size * d->ch_play);
    SndGoto(err, __error, "Failed snd_pcm_format_set_silence: %s");

    RANGE(i, 2)
    {
        err = sndx_duplex_writebuf(d, play_buf, d->period_size, frames_silence);
        Goto(err < 0, __error, "Failed writebuf");
    }

    return 0;

__error:
    sndx_dump_duplex_status(d, d->out);
    return err;
}

int sndx_duplex_start(sndx_duplex_t* d, char** play_bufp, char** capt_bufp, uframes_t loop_limit)
{
    int err;

    output_t* output = d->out;

    // Allocate samples temporarily
    isize capt_size = d->buf_capt->frames * d->buf_capt->channels * d->buf_capt->bytes;
    char* capt_buf  = calloc(capt_size, sizeof(char));
    *capt_bufp      = capt_buf;

    isize play_size = d->buf_play->frames * d->buf_play->channels * d->buf_play->bytes;
    char* play_buf  = calloc(play_size, sizeof(char));
    *play_bufp      = play_buf;

    sndx_buffer_map_dev_to_samples(d->buf_play, play_buf);
    sndx_buffer_map_dev_to_samples(d->buf_capt, capt_buf);

    // Set silence
    uframes_t frames_silence = 0;

    err = sndx_duplex_write_initial_silence(d, play_buf, &frames_silence);
    SndFatal(err, "Failed sndx_duplex_write_initial_silence: %s");

    sndx_dump_duplex(d, output);

    // Start prep for loop -> on error goto __error
    uframes_t frames_out = frames_silence;
    uframes_t frames_in  = 0;
    uframes_t in_max     = 0;

    // Start of loop
    ssize_t r, cap_avail;

    while (frames_in < loop_limit)
    {
        cap_avail = d->period_size;

        snd_pcm_wait(d->capt, 1000);

        r = sndx_duplex_readbuf(d, capt_buf, cap_avail, &frames_in, &in_max);
        SndReturn_(r, "Failed readbuf: %s");

        err = sndx_duplex_writebuf(d, play_buf, r, &frames_out);
        SndReturn_(r, "Failed writebuf: %s");
    }

    sndx_dump_duplex(d, output);

    return 0;
}

int sndx_duplex_stop(sndx_duplex_t* d, char* play_buf, char* capt_buf)
{
    snd_pcm_drop(d->capt);
    snd_pcm_drain(d->play);
    snd_pcm_nonblock(d->play, SND_PCM_NONBLOCK);

    snd_pcm_unlink(d->capt);
    snd_pcm_hw_free(d->play);
    snd_pcm_hw_free(d->capt);

    free(capt_buf);
    free(play_buf);

    sndx_dump_duplex_status(d, d->out);

    return 0;
}
