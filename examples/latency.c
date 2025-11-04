#include "duplex.h"
#include <alsa/asoundlib.h>

void showstat(snd_pcm_t* handle, size_t frames, output_t* output)
{
    int               err;
    snd_pcm_status_t* status;

    snd_pcm_status_alloca(&status);
    err = snd_pcm_status(handle, status);
    SndFatal(err, "Stream status error: %s");

    printf("*** frames = %li ***\n", (long)frames);
    snd_pcm_status_dump(status, output);
}

long readbuf(snd_pcm_t* handle, char* buf, long len, size_t* frames, size_t* max)
{
    long r;
    do { r = snd_pcm_mmap_readi(handle, buf, len); } while (r == -EAGAIN);
    if (r > 0)
    {
        *frames += r;
        if ((long)*max < r) *max = r;
    }
    // showstat(handle, 0);
    return r;
}

long writebuf(snd_pcm_t* handle, char* buf, long len, size_t* frames, format_t format, u32 channels, output_t* output)
{
    long r;
    int  frame_bytes = (snd_pcm_format_physical_width(format) / 8) * channels;

    while (len > 0)
    {
        r = snd_pcm_mmap_writei(handle, buf, len);
        if (r == -EAGAIN) continue;
        else if (r == -EINVAL) { SysReturn_(-EINVAL, "Invalid args, check access (MMAP / RW): %s"); }
        else if (r < 0) return r;
        buf     += r * frame_bytes;
        len     -= r;
        *frames += r;
    }
    return 0;
}

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(              //
        &d,                              //
        "hw:FC1,0", "hw:FC1_1,0",        //
        SND_PCM_FORMAT_S16_LE,           //
        48000, 256, 128,                 //
        SND_PCM_ACCESS_MMAP_INTERLEAVED, //
        output);
    SndFatal(err, "Failed sndx_duplex_open: %s");

    isize capt_size = d->buf_capt->frames * d->buf_capt->channels * d->buf_capt->bytes;
    char* capt_buf  = calloc(capt_size, sizeof(char));

    isize play_size = d->buf_play->frames * d->buf_play->channels * d->buf_play->bytes;
    char* play_buf  = calloc(play_size, sizeof(char));

    sndx_buffer_map_dev_to_samples(d->buf_play, play_buf);
    sndx_buffer_map_dev_to_samples(d->buf_capt, capt_buf);

    sndx_dump_duplex(d, output);

    // Start prep for loop -> on error goto __error
    uframes_t frames_out = 0;
    uframes_t frames_in  = 0;
    uframes_t in_max     = 0;
    uframes_t loop_limit = d->rate;
    uframes_t latency    = d->period_size;

    // Set silence
    err = snd_pcm_format_set_silence(d->format, play_buf, d->period_size * d->ch_play);
    SndGoto(err, __error, "Failed snd_pcm_format_set_silence: %s");

    RANGE(i, 2)
    {
        err = writebuf(d->play, play_buf, d->period_size, &frames_out, d->format, d->ch_play, d->out);
        Goto(err < 0, __error, "Failed writebuf");
    }

    showstat(d->play, 0, output);
    showstat(d->capt, 0, output);

    // Start of loop
    bool    ok = true;
    ssize_t r, cap_avail;

    while (ok && frames_in < loop_limit)
    {
        cap_avail = latency;

        snd_pcm_wait(d->capt, 1000);

        r = readbuf(d->capt, capt_buf, cap_avail, &frames_in, &in_max);
        Goto(r < 0, __error, "Failed readbuf");

        err = writebuf(d->play, play_buf, r, &frames_out, d->format, d->ch_play, output);
        Goto(r < 0, __error, "Failed writebuf");
    }

__error:

    snd_pcm_drop(d->capt);
    snd_pcm_drain(d->play);
    snd_pcm_nonblock(d->play, SND_PCM_NONBLOCK);

    snd_pcm_unlink(d->capt);
    snd_pcm_hw_free(d->play);
    snd_pcm_hw_free(d->capt);

    showstat(d->play, 0, output);
    showstat(d->capt, 0, output);

    err = sndx_duplex_close(d);
    SndFatal(err, "Failed sndx_duplex_close: %s");

    free(capt_buf);
    free(play_buf);

    snd_output_close(output);

    return 0;
}
