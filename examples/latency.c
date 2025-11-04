#include "duplex.h"
#include "types.h"
#include <alsa/asoundlib.h>

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

    char*     play_buf;
    char*     capt_buf;
    uframes_t loop_limit = d->rate;

    err = sndx_duplex_start(d, &play_buf, &capt_buf, loop_limit);
    SndCheck_(err, "Failed sndx_duplex_start: %s");

    err = sndx_duplex_stop(d, play_buf, capt_buf);
    SndCheck_(err, "Failed sndx_duplex_stop: %s");

    err = sndx_duplex_close(d);
    SndCheck_(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    return 0;
}
