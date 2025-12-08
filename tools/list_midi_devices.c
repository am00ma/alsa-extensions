#include "sndx/types.h"

typedef struct
{
    int card;
    int device;
    int sub;

    bool has_input;
    bool has_output;

    char hw_name[32];
    char dev_name[128];

} sndx_midi_device_t;

int device_list(void)
{
    int card, err;

    card = -1;

    err = snd_card_next(&card);
    SndReturn(err, "Failed: snd_card_next: %s");

    err = (card < 0);
    Return(err, "Failed: no sound card found (card < 0)");

    p_info("Dir Device    Name");
    do
    {
        snd_ctl_t* ctl;
        char       name[32];
        int        device;

        sprintf(name, "hw:%d", card);
        err = snd_ctl_open(&ctl, name, 0);
        SndReturn(err, "Failed: snd_ctl_open (card=%d): %s", card);

        device = -1;
        err    = snd_ctl_rawmidi_next_device(ctl, &device);
        SndReturn(err, "Failed: snd_ctl_rawmidi_next_device (card=%d): %s", card);

        while (device >= 0)
        {
            const char*         name;
            const char*         sub_name;
            int                 subs, subs_in, subs_out;
            snd_rawmidi_info_t* info;

            snd_rawmidi_info_alloca(&info);
            snd_rawmidi_info_set_device(info, device);

            snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
            err = snd_ctl_rawmidi_info(ctl, info);
            if (err >= 0) subs_in = snd_rawmidi_info_get_subdevices_count(info);
            else subs_in = 0;

            snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
            err = snd_ctl_rawmidi_info(ctl, info);
            if (err >= 0) subs_out = snd_rawmidi_info_get_subdevices_count(info);
            else subs_out = 0;

            subs = subs_in > subs_out ? subs_in : subs_out;
            Goto(!subs, __next_device, "Failed: subs == 0 (card=%d, device=%d)", card, device);

            RANGE(sub, subs)
            {
                //
                snd_rawmidi_info_set_stream(info, sub < subs_in ? SND_RAWMIDI_STREAM_INPUT : SND_RAWMIDI_STREAM_OUTPUT);
                snd_rawmidi_info_set_subdevice(info, sub);
                err = snd_ctl_rawmidi_info(ctl, info);
                SndGoto(err, __next_device, "Failed: snd_ctl_rawmidi_info (card=%d,device=%d) : %s", card, device);

                if (snd_rawmidi_info_get_flags(info) & SNDRV_RAWMIDI_INFO_STREAM_INACTIVE)
                {
                    p_info("Inactive stream");
                }
                name     = snd_rawmidi_info_get_name(info);
                sub_name = snd_rawmidi_info_get_subdevice_name(info);
                if (sub == 0 && sub_name[0] == '\0')
                {
                    // Never enters this loop, cause sub_name.len always >= 0
                    sndx_midi_device_t m = {
                        .card       = card,
                        .device     = device,
                        .sub        = sub,
                        .has_input  = sub < subs_in,
                        .has_output = sub < subs_out,
                    };
                    memcpy(m.dev_name, name, strlen(name));
                    snprintf(m.hw_name, 32, "hw:%d,%d", m.card, m.device);
                    p_info("%c%c  %s  %s (%d subdevices)", //
                           m.has_input ? 'I' : ' ',        //
                           m.has_output ? 'O' : ' ',       //
                           m.hw_name, m.dev_name, subs);   //
                    break;
                }
                else
                {
                    sndx_midi_device_t m = {
                        .card       = card,
                        .device     = device,
                        .sub        = sub,
                        .has_input  = sub < subs_in,
                        .has_output = sub < subs_out,
                    };
                    memcpy(m.dev_name, sub_name, strlen(sub_name));
                    snprintf(m.hw_name, 32, "hw:%d,%d,%d", m.card, m.device, m.sub);
                    p_info("%c%c  %s  %s",           //
                           m.has_input ? 'I' : ' ',  //
                           m.has_output ? 'O' : ' ', //
                           m.hw_name, m.dev_name);   //
                }
            }

        __next_device:
            err = snd_ctl_rawmidi_next_device(ctl, &device);
            SndReturn(err, "Failed: snd_ctl_rawmidi_next_device (card=%d): %s", card);
        }

        snd_ctl_close(ctl);

        err = snd_card_next(&card);
        Return(err, "Failed: cannot determine card number (card < 0)");

    } while (card >= 0);

    return 0;
}

int main()
{
    device_list();
    return 0;
}
