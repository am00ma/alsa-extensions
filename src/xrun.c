#include "xrun.h"

int sndx_xrun_recovery_alsalib(snd_pcm_t* pcm, int err, output_t* output)
{
    a_error("xrun recovery");
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
            a_error("sleeping");
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
