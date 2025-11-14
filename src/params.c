#include "params.h"
#include "types.h"

void sndx_dump_params(sndx_params_t* params, output_t* output)
{
    a_info("  channels    = %d", params->channels);
    a_info("  format      = %s", snd_pcm_format_name(params->format));
    a_info("  access      = %s", snd_pcm_access_name(params->access));
    a_info("  rate        = %d", params->rate);
    a_info("  period_size = %lu", params->period_size);
    a_info("  periods     = %d", params->periods);
    a_info("  buffer_size = %lu (params->period_size * params->periods)", params->period_size * params->periods);
    a_info("  latency     ="); // TODO: Check computations in jack, juce, rtaudio
}

int sndx_set_buffer_size(snd_spcm_latency_t latency, uframes_t* buffer_size)
{
    switch (latency)
    {
    case SND_SPCM_LATENCY_STANDARD: *buffer_size = 22040; break;
    case SND_SPCM_LATENCY_MEDIUM: *buffer_size = 1024; break;
    case SND_SPCM_LATENCY_REALTIME: *buffer_size = 256; break;
    default: return -EINVAL;
    }
    return 0;
}

int sndx_set_hw_params(           //
    snd_pcm_t*   pcm,             //
    hw_params_t* hw_params,       //
    u32          rate,            //
    u32*         channels,        //
    format_t     format,          //
    uframes_t    period_size,     //
    u32          periods,         //
    access_t     access,          //
    bool         strict_channels, //
    output_t*    output)
{
    int err;

    /*
     * hardware parameters
     */
    err = snd_pcm_hw_params_any(pcm, hw_params);
    SndReturn_(err, "Failed: snd_pcm_hw_params_any: %s");

    err = snd_pcm_hw_params_set_periods_integer(pcm, hw_params);
    SndReturn_(err, "Failed: snd_pcm_hw_params_set_periods_integer: %s");

    err = snd_pcm_hw_params_set_access(pcm, hw_params, access);
    SndReturn_(err, "Failed: snd_pcm_hw_params_set_access: %s");

    err = snd_pcm_hw_params_set_format(pcm, hw_params, format);
    SndReturn_(err, "Failed: snd_pcm_hw_params_set_format: %s");

    err = snd_pcm_hw_params_set_channels(pcm, hw_params, *channels);
    if (err < 0)
    {
        if (!strict_channels)
        {
            u32 rchannels = *channels;

            err = snd_pcm_hw_params_get_channels_min(hw_params, &rchannels);
            SndReturn_(err, "Failed: snd_pcm_hw_params_get_channels_min: %s");

            err = snd_pcm_hw_params_set_channels(pcm, hw_params, rchannels);
            SndReturn_(err, "Failed: snd_pcm_hw_params_set_channels: %s");

            *channels = rchannels;
        }
        else {
            return err;
        }
    }

    err = snd_pcm_hw_params_set_rate(pcm, hw_params, rate, 0);
    SndReturn_(err, "Failed: snd_pcm_hw_params_set_rate: %s");

    err = snd_pcm_hw_params_set_buffer_size(pcm, hw_params, period_size * periods);
    SndReturn_(err, "Failed: snd_pcm_hw_params_set_buffer_size: %s");

    err = snd_pcm_hw_params_set_period_size(pcm, hw_params, period_size, 0);
    SndReturn_(err, "Failed: snd_pcm_hw_params_set_period_size: %s");

    u32 xperiods;
    err = snd_pcm_hw_params_get_periods(hw_params, &xperiods, 0);
    SndReturn_(err, "Failed: snd_pcm_hw_params_get_periods: %s");

    err = xperiods != periods;
    Return_(-err, "Failed: xperiods != periods: %d != %d", xperiods, periods);

    err = snd_pcm_hw_params(pcm, hw_params);
    SndReturn_(err, "Failed: snd_pcm_hw_params: %s");

    return 0;
}

int sndx_set_sw_params(       //
    snd_pcm_t*   pcm,         //
    sw_params_t* sw_params,   //
    uframes_t    period_size, //
    u32          periods,     //
    output_t*    output)
{
    int err;

    err = snd_pcm_sw_params_current(pcm, sw_params);
    SndReturn_(err, "Failed: snd_pcm_sw_params_current: %s");

    err = snd_pcm_sw_params_set_start_threshold(pcm, sw_params, period_size * periods);
    SndReturn_(err, "Failed: snd_pcm_sw_params_set_start_threshold: %s");

    err = snd_pcm_sw_params_set_stop_threshold(pcm, sw_params, period_size * periods);
    SndReturn_(err, "Failed: snd_pcm_sw_params_set_stop_threshold: %s");

    err = snd_pcm_sw_params_set_avail_min(pcm, sw_params, period_size);
    SndReturn_(err, "Failed: snd_pcm_sw_params_set_avail_min: %s");

    err = snd_pcm_sw_params_set_tstamp_mode(pcm, sw_params, SND_PCM_TSTAMP_ENABLE);
    SndReturn_(err, "Failed: snd_pcm_sw_params_set_tstamp_mode: %s");

    err = snd_pcm_sw_params_set_tstamp_type(pcm, sw_params, SND_PCM_TSTAMP_TYPE_MONOTONIC);
    SndReturn_(err, "Failed: snd_pcm_sw_params_set_tstamp_type: %s");

    err = snd_pcm_sw_params(pcm, sw_params);
    SndReturn_(err, "Failed: snd_pcm_sw_params: %s");

    return 0;
}

int sndx_set_params(            //
    snd_pcm_t* pcm,             //
    u32*       channels,        //
    format_t*  format,          //
    u32*       rate,            //
    uframes_t* period_size,     //
    u32*       periods,         //
    access_t   _access,         //
    bool       strict_channels, //
    output_t*  output)
{

    int err;

    hw_params_t* hw_params;
    sw_params_t* sw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_sw_params_alloca(&sw_params);

    assert(pcm);
    assert(*rate >= 5000 && *rate <= 786000);
    assert(*channels >= 1 && *channels <= 512);

    err = sndx_set_hw_params( //
        pcm, hw_params,       //
        *rate, channels, *format, *period_size, *periods, _access, strict_channels, output);
    SndReturn_(err, "Failed: set_hw_params: %s");

    err = sndx_set_sw_params(pcm, sw_params, *period_size, *periods, output);
    SndReturn_(err, "Failed: set_sw_params: %s");

    uframes_t buffer_size;
    err = snd_spcm_init_get_params(pcm, rate, &buffer_size, period_size);
    SndReturn_(err, "Failed: snd_spcm_init_get_params: %s");

    err = buffer_size != (*period_size) * (*periods);
    Return_(-err, "Failed: buffer_size != period_size * periods: %ld != %ld * %d", //
            buffer_size, *period_size, *periods);

    return 0;
}
