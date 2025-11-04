#include "xrun.h"

int sndx_xrun_recovery_alsalib(snd_pcm_t* pcm, int err, snd_output_t* output)
{
    snd_output_printf(output, "xrun recovery\n");
    if (err == -EPIPE)
    {
        err = snd_pcm_prepare(pcm);
        SndReturn_(err, "Failed: XRUN recovery: snd_pcm_prepare: %s");
        return 0;
    }
    else if (err == -ESTRPIPE)
    {
        while ((err = snd_pcm_resume(pcm)) == -EAGAIN)
        {
            snd_output_printf(output, "sleeping\n");
            usleep(1000); /* wait until the suspend flag is released */
        }
        if (err < 0)
        {
            err = snd_pcm_prepare(pcm);
            SndReturn_(err, "Failed: SUSPEND recovery: snd_pcm_prepare: %s");
        }
        return 0;
    }

    return err;
}

int sndx_xrun_recovery_jack2(snd_pcm_t* capt, snd_pcm_t* play, float* /* delayed_usecs */, snd_output_t* output)
{
    int err;

    snd_pcm_status_t* status;
    snd_pcm_status_alloca(&status);

    if (capt)
    {
        err = snd_pcm_status(capt, status);
        SndReturn_(err, "[capt] Failed: snd_pcm_status: %s");
    }
    else {
        err = snd_pcm_status(play, status);
        SndReturn_(err, "[play] Failed: snd_pcm_status: %s");
    }

    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_SUSPENDED)
    {
        snd_output_printf(output, "SND_PCM_STATE_SUSPENDED: resuming...\n");
        if (capt)
        {
            err = snd_pcm_prepare(capt);
            SndReturn_(err, "[capt] Failed: SUSPEND recovery: snd_pcm_prepare: %s");
        }
        if (play)
        {
            err = snd_pcm_prepare(play);
            SndReturn_(err, "[play] Failed: SUSPEND recovery: snd_pcm_prepare: %s");
        }
    }

    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN)
    {
        snd_output_printf(output, "SND_PCM_STATE_XRUN: resuming...\n");
        if (capt)
        {
            err = snd_pcm_prepare(capt);
            SndReturn_(err, "[capt] Failed: XRUN recovery: snd_pcm_prepare: %s");
        }
        if (play)
        {
            err = snd_pcm_prepare(play);
            SndReturn_(err, "[play] Failed: XRUN recovery: snd_pcm_prepare: %s");
        }
    }

    // TODO: Restart driver

    return 0;
}
