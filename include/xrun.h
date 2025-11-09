/** @file xrun.h
 *  @brief Various implementations of handling xruns
 */
#pragma once

#include "types.h" // IWYU pragma: keep

int sndx_xrun_recovery_alsalib(snd_pcm_t* pcm, int err, output_t* output);
