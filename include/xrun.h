#pragma once

#include "types.h" // IWYU pragma: keep

int sndx_xrun_recovery_alsalib(snd_pcm_t* pcm, int err, output_t* output);
int sndx_xrun_recovery_jack2(snd_pcm_t* capt, snd_pcm_t* play, float* /* delayed_usecs */, output_t* output);
