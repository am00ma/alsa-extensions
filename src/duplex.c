#include "duplex.h"
#include "params.h"
#include "types.h"

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

/** \example latency.c
 * This is an example of how to use the Example_Test class.
 * More details about this example.
 */
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
    SndGoto_(err, __close, "Failed: snd_pcm_open: %s");

    err = snd_pcm_open(&d->capt, capture_device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    SndGoto_(err, __close, "Failed: snd_pcm_open: %s");

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
    SndGoto_(err, __close, "Failed: sndx_set_params: %s");

    err = !((rate == play_params.rate) &&               //
            (buffer_size == play_params.buffer_size) && //
            (period_size == play_params.period_size));
    Goto_(err, __close, "Failed: params check");

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
    SndGoto_(err, __close, "Failed: sndx_set_params: %s");

    err = !((rate == capt_params.rate) &&               //
            (buffer_size == capt_params.buffer_size) && //
            (period_size == capt_params.period_size));
    Return_(err, "Could not set params");

    err = play_params.format != capt_params.format;
    Return_(err, "play_params.format != capt_params.format");

    err = snd_pcm_nonblock(d->capt, SND_PCM_NONBLOCK);
    SndGoto_(err, __close, "Failed: snd_pcm_nonblock: %s");

    d->format      = play_params.format;
    d->rate        = rate;
    d->period_size = period_size;
    d->nperiods    = buffer_size / period_size;

    d->ch_play = play_params.channels;
    d->ch_capt = capt_params.channels;

    err = snd_pcm_link(d->capt, d->play);
    SndGoto_(err, __close, "Failed: snd_pcm_link: %s");

    d->linked = true;

    // Allocate buffers
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

sframes_t sndx_duplex_readbuf(sndx_duplex_t* d, char* buf, long len, uframes_t* frames, uframes_t* max)
{
    long r;
    do { r = snd_pcm_mmap_readi(d->capt, buf, len); } while (r == -EAGAIN);
    if (r > 0)
    {
        *frames += r;
        if ((long)*max < r) *max = r;

        // After read, copy to soft buffer as float
        sndx_buffer_dev_to_buf(d->buf_capt, 0, r);
    }
    else {
        sndx_dump_duplex_status(d, d->out);
    }

    // showstat(handle, 0);
    return r;
}

void sndx_duplex_copy_capt_to_play(sndx_buffer_t* buf_capt, sndx_buffer_t* buf_play, sframes_t len, void* data)
{
    float* gain = data;

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

sframes_t sndx_duplex_writebuf(sndx_duplex_t* d, char* buf, long len, size_t* frames)
{
    snd_output_t* output = d->out;

    // Write from soft buffer to device as int
    sndx_buffer_buf_to_dev(d->buf_play, 0, len);

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

void sndx_duplex_timer_start(sndx_duplex_t* d)
{
    timestamp_now(&d->timer.start);
    timestamp_get(d->play, &d->timer.play);
    timestamp_get(d->capt, &d->timer.capt);
}

void sndx_duplex_timer_stop(sndx_duplex_t* d, uframes_t frames_in, output_t* output)
{
    a_info("Timer status:");

    if (d->timer.play.tv_sec == d->timer.capt.tv_sec && //
        d->timer.play.tv_usec == d->timer.capt.tv_usec)
        a_info("  Hardware sync");

    i64 diff  = timestamp_diff_now(&d->timer.start);
    i64 mtime = frames_to_micro(frames_in, d->rate);
    a_info("  Elapsed real  : %ld us", diff);
    a_info("  Elapsed device: %ld us", mtime);
    a_info("  Diff (device - real): %ld us", mtime - diff);
    a_info("  Playback = %li.%i", (long)d->timer.play.tv_sec, (int)d->timer.play.tv_usec);
    a_info("  Capture  = %li.%i", (long)d->timer.capt.tv_sec, (int)d->timer.capt.tv_usec);
    a_info("  Diff     = %li", timestamp_diff(d->timer.play, d->timer.capt));
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

    return 0;
}
