#include "types.h"

typedef struct
{
    u32 min;
    u32 max;

} minmax_t;

typedef struct
{
    minmax_t channels;
    minmax_t rate;
    format_t formats[8]; // Max 8 formats
    isize    num_formats;

} device_t;

void dump_device(device_t* d, const char* name, const char* stream)
{
    p_info("%s [ %s ]", name, stream);
    p_info("  channels: %d, %d", d->channels.min, d->channels.max);
    p_info("  rate    : %d, %d", d->rate.min, d->rate.max);
    p_log(" formats  : ");
    if (d->num_formats > 0)
    {
        RANGE(i, d->num_formats - 1) { p_log("%s, ", snd_pcm_format_name(d->formats[i])); }
        p_log("%s", snd_pcm_format_name(d->formats[d->num_formats - 1]));
    }
    else p_log("none");
    p_log("\n");
}

int main()
{
    snd_ctl_card_info_t* info;
    snd_pcm_info_t*      pcminfo_capture;
    snd_pcm_info_t*      pcminfo_playback;

    char card_name[128 - 11];
    char dev_name[128];

    // Allocated on stack, so automatically freed
    snd_ctl_card_info_alloca(&info);
    snd_pcm_info_alloca(&pcminfo_capture);
    snd_pcm_info_alloca(&pcminfo_playback);

    int card_no = -1;
    while (snd_card_next(&card_no) >= 0 && card_no >= 0)
    {
        snd_ctl_t* handle;
        sprintf(card_name, "hw:%d", card_no);

        if (snd_ctl_open(&handle, card_name, 0) >= 0 && snd_ctl_card_info(handle, info) >= 0)
        {
            int device_no = -1;
            sprintf(card_name, "hw:%s", snd_ctl_card_info_get_id(info));

            while (snd_ctl_pcm_next_device(handle, &device_no) >= 0 && device_no != -1)
            {
                sprintf(dev_name, "%s,%d", card_name, device_no);

                int      err;
                device_t playback = {};

                stream_t streams[2] = {SND_PCM_STREAM_PLAYBACK, SND_PCM_STREAM_CAPTURE};
                RANGE(s, 2)
                {
                    snd_pcm_t* pcm;
                    err = snd_pcm_open(&pcm, dev_name, streams[s], 0);
                    if (err == -2) continue; // no such file or directory, i.e. device does not support stream
                    SndFatal(err, "Failed(%d): snd_pcm_open for %s: %s (%s)", //
                             err, snd_pcm_stream_name(streams[s]), dev_name);

                    hw_params_t* hw;
                    snd_pcm_hw_params_alloca(&hw);

                    err = snd_pcm_hw_params_any(pcm, hw);
                    SndFatal(err, "Failed: snd_pcm_hw_params_any: %s");

                    err = snd_pcm_hw_params_get_channels_min(hw, &playback.channels.min);
                    SndFatal(err, "Failed: snd_pcm_hw_params_get_channels_min: %s");

                    err = snd_pcm_hw_params_get_channels_max(hw, &playback.channels.max);
                    SndFatal(err, "Failed: snd_pcm_hw_params_get_channels_min: %s");

                    err = snd_pcm_hw_params_get_rate_min(hw, &playback.rate.min, 0);
                    SndFatal(err, "Failed: snd_pcm_hw_params_get_rate_min: %s");

                    err = snd_pcm_hw_params_get_rate_max(hw, &playback.rate.max, 0);
                    SndFatal(err, "Failed: snd_pcm_hw_params_get_rate_min: %s");

                    snd_pcm_format_mask_t* fmask = nullptr;
                    snd_pcm_format_mask_alloca(&fmask);

                    snd_pcm_hw_params_get_format_mask(hw, fmask);
                    playback.num_formats = 0;
                    RANGE(fmt, SND_PCM_FORMAT_LAST)
                    if (snd_pcm_format_mask_test(fmask, fmt))
                    {
                        playback.formats[playback.num_formats] = fmt;
                        playback.num_formats++;
                    }

                    dump_device(&playback, dev_name, snd_pcm_stream_name(streams[s]));

                    err = snd_pcm_close(pcm);
                    SndFatal(err, "Failed: snd_pcm_close: %s");
                }
            }

            snd_ctl_close(handle);
        }
    }

    return EXIT_SUCCESS;
}
