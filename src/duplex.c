#include "duplex.h"
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

    a_title("Capture:");
    snd_pcm_dump(d->capt, d->out);
}

int sndx_duplex_open(                //
    sndx_duplex_t** duplexp,         //
    const char*     playback_device, //
    const char*     capture_device,  //
    format_t        format,          //
    u32             rate,            //
    uframes_t       buffer_size,     //
    uframes_t       period_size,     //
    access_t        _access)
{
    int err;

    sndx_duplex_t* d;

    d = (sndx_duplex_t*)calloc(1, sizeof(*d));
    if (!d) return -ENOMEM;

    snd_output_t* output = output;

    err = snd_output_stdio_attach(&output, stdout, 0);
    SndReturn(err, "Failed: snd_output_stdio_attach: %s");

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

    err = snd_pcm_nonblock(d->play, 0);
    SndReturn_(err, "Failed: snd_pcm_nonblock: %s");

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

    free(d);

    return 0;
}
