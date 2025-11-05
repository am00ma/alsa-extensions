#include "playback.h"
#include "params.h"

static sndx_params_t default_params = {
    .channels    = 2,
    .format      = SND_PCM_FORMAT_S16,
    .access      = SND_PCM_ACCESS_MMAP_INTERLEAVED,
    .rate        = 48000,
    .nperiods    = 2,
    .period_size = 128,
    .buffer_size = 256,
};

void sndx_dump_playback(sndx_playback_t* d, snd_output_t* output)
{
    a_title("playback:");
    a_info("  ch_play    : %d", d->ch_play);
    a_info("  format     : %s", snd_pcm_format_name(d->format));
    a_info("  rate       : %d", d->rate);
    a_info("  period_size: %d", d->period_size);
    a_info("  buffer_size: %d", d->period_size * d->nperiods);
    a_info("  nperiods   : %d", d->nperiods);

    a_title("Play:");
    snd_pcm_dump(d->play, d->out);

    sndx_dump_buffer(d->buf_play, output);

    sndx_dump_playback_status(d, output);
}

void sndx_dump_playback_status(sndx_playback_t* d, output_t* output)
{
    int err;

    snd_pcm_status_t* status;
    snd_pcm_status_alloca(&status);

    a_info("Playback status:");
    err = snd_pcm_status(d->play, status);
    SndFatal(err, "Stream status error: %s");
    snd_pcm_status_dump(status, output);
}

int sndx_playback_open(                //
    sndx_playback_t** playbackp,       //
    const char*       playback_device, //
    format_t          format,          //
    u32               rate,            //
    uframes_t         buffer_size,     //
    uframes_t         period_size,     //
    access_t          _access,         //
    output_t*         output)
{
    int err;

    sndx_playback_t* d;

    d = (sndx_playback_t*)calloc(1, sizeof(*d));
    if (!d) return -ENOMEM;

    d->out = output;

    err = snd_pcm_open(&d->play, playback_device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    SndReturn_(err, "Failed: snd_pcm_open: %s");

    sndx_params_t play_params = default_params;
    play_params.format        = format;
    play_params.access        = _access;
    play_params.rate          = rate;
    play_params.buffer_size   = buffer_size;
    play_params.period_size   = period_size;
    play_params.nperiods      = buffer_size / period_size;

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

    d->format      = play_params.format;
    d->rate        = rate;
    d->period_size = period_size;
    d->nperiods    = buffer_size / period_size;

    d->ch_play = play_params.channels;

    // Allocate buffers
    err = sndx_buffer_open(&d->buf_play, d->format, d->ch_play, buffer_size, output);
    SndReturn_(err, "Failed: sndx_buffer_open(play): %s");

    *playbackp = d;

    return 0;
}

int sndx_playback_close(sndx_playback_t* d)
{
    int err;

    snd_output_t* output = d->out;

    err = snd_pcm_close(d->play);
    SndReturn_(err, "Failed: snd_pcm_close: %s");

    sndx_buffer_close(d->buf_play);

    free(d);

    return 0;
}

void sndx_playback_callback(sndx_buffer_t* buf_play, sframes_t len, void* data)
{
    float* gain = data;

    // Actual realtime check - can just return, but better to flag
    AssertMsg(len >= 0, "Received negative len: %ld", len);

    // Just silencing for now
    isize buf_size = buf_play->frames;
    RANGE(chn, buf_play->channels)
    RANGE(i, len) { buf_play->data[i + (buf_size * chn)] = 0.0 * (*gain); }
}

sframes_t sndx_playback_writebuf(sndx_playback_t* d, char* buf, long len, size_t* frames)
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

int sndx_playback_write_initial_silence(sndx_playback_t* d, char* play_buf, uframes_t* frames_silence)
{
    int err;

    err = snd_pcm_format_set_silence(d->format, play_buf, d->period_size * d->ch_play);
    SndGoto(err, __error, "Failed snd_pcm_format_set_silence: %s");

    RANGE(i, 2)
    {
        err = sndx_playback_writebuf(d, play_buf, d->period_size, frames_silence);
        Goto(err < 0, __error, "Failed writebuf");
    }

    return 0;

__error:
    sndx_dump_playback_status(d, d->out);
    return err;
}

void sndx_playback_timer_start(sndx_playback_t* d)
{
    timestamp_now(&d->timer.start);
    timestamp_get(d->play, &d->timer.play);
}

void sndx_playback_timer_stop(sndx_playback_t* d, uframes_t frames_out, output_t* output)
{
    a_info("Timer status:");

    i64 diff  = timestamp_diff_now(&d->timer.start);
    i64 mtime = frames_to_micro(frames_out, d->rate);
    a_info("  Elapsed real  : %ld us", diff);
    a_info("  Elapsed device: %ld us", mtime);
    a_info("  Diff (device - real): %ld us", mtime - diff);
}

int sndx_playback_start(sndx_playback_t* d, char** play_bufp, uframes_t loop_limit)
{
    int err;

    output_t* output = d->out;

    // Allocate samples temporarily
    isize play_size = d->buf_play->frames * d->buf_play->channels * d->buf_play->bytes;
    char* play_buf  = calloc(play_size, sizeof(char));
    *play_bufp      = play_buf;

    sndx_buffer_map_dev_to_samples(d->buf_play, play_buf);

    // Set silence
    uframes_t frames_silence = 0;

    err = sndx_playback_write_initial_silence(d, play_buf, &frames_silence);
    SndFatal(err, "Failed sndx_playback_write_initial_silence: %s");

    // After silence, stream is already started (start threshold is 0x7fffffff in latency, 2*period_size for playback)
    sndx_playback_timer_start(d);

    // Debugging
    sndx_dump_playback(d, output);

    // Start prep for loop -> on error goto __error
    uframes_t frames_out = frames_silence;

    // Read write loop
    sframes_t play_avail;
    float     gain = 0.5;
    while (frames_out < loop_limit)
    {
        play_avail = d->period_size;
        snd_pcm_wait(d->play, 1000);

        // Copy capture channels to playback
        sndx_playback_callback(d->buf_play, play_avail, &gain);

        err = sndx_playback_writebuf(d, play_buf, play_avail, &frames_out);
        SndReturn_(err, "Failed writebuf: %s");
    }

    // Report final timings
    sndx_playback_timer_stop(d, frames_out, output);

    return 0;
}

int sndx_playback_stop(sndx_playback_t* d, char* play_buf)
{
    snd_pcm_drain(d->play);
    snd_pcm_nonblock(d->play, SND_PCM_NONBLOCK);
    snd_pcm_hw_free(d->play);

    free(play_buf);

    return 0;
}
