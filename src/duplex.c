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

    // // Allocate poll fds
    // d->pfds.play_nfds     = snd_pcm_poll_descriptors_count(d->play);
    // d->pfds.capt_nfds     = snd_pcm_poll_descriptors_count(d->capt);
    // d->pfds.nfds          = d->pfds.play_nfds + d->pfds.capt_nfds;
    // d->pfds.addr          = calloc(d->pfds.nfds, sizeof(pfd_t));
    // d->pfds.poll_timeout  = 1000;
    // d->pfds.poll_next     = 0;
    // d->pfds.poll_late     = 0;
    // d->pfds.period_usecs  = (u64)floor((((float)d->period_size) / d->rate) * 1000000.0f);
    // d->pfds.xrun_count    = 0;
    // d->pfds.process_count = 0;

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

    // if (d->pfds.addr)
    // {
    //     free(d->pfds.addr);
    //     d->pfds.addr = nullptr;
    // }

    free(d);
    d = nullptr;

    return 0;
}

int sndx_duplex_write_initial_silence(sndx_duplex_t* d, char* play_buf, uframes_t* frames_silence)
{
    int err;

    err = snd_pcm_format_set_silence(d->format, play_buf, d->period_size * d->ch_play);
    SndGoto(err, __error, "Failed snd_pcm_format_set_silence: %s");

    uframes_t silence_max = 0;
    RANGE(i, 2)
    {
        err = sndx_duplex_writebuf(d, play_buf, d->period_size, 0, frames_silence, &silence_max);
        Goto(err < 0, __error, "Failed writebuf");
    }

    return 0;

__error:
    sndx_dump_duplex_status(d, d->out);
    return err;
}

sframes_t sndx_duplex_readbuf(sndx_duplex_t* d, char* buf, i64 len, uframes_t offset, uframes_t* frames, uframes_t* max)
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

sframes_t
sndx_duplex_writebuf(sndx_duplex_t* d, char* buf, i64 len, uframes_t offset, uframes_t* frames, uframes_t* max)
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
