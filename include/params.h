/** @file params.h
 *  @brief Setting software and hardware params for single pcm device.
 *
 *  'Vendoring' of snd_spcm_set_params(...).
 *
 *  Necessary changes:
 *  1. Set period_size and buffer_size instead of times
 *  2. Allow different playback and capture channels
 *  3. Customization of start and stop thresholds
 *  4. Ensuring `buffer_size / period_size = periods` is integer
 *  5. Enabling of timerstamping in sw
 *  6. Ensuring `avail_min == period_size` in sw
 *  7. Better error handling
 *
 */
#pragma once

#include "types.h"

/** @brief Hardware config common to playback and capture (except for channels).

    Hold variables frequently used in other functions.
*/
typedef struct
{
    u32       channels;
    format_t  format;
    access_t  access;
    u32       rate;
    u32       periods;
    uframes_t period_size;

} sndx_params_t;

/** @brief Dump params to output. */
void sndx_dump_params(sndx_params_t* params, output_t* output);

/** @brief Set buffer size specifically based on typical latency requirements specified in `spcm` */
int sndx_set_buffer_size(snd_spcm_latency_t latency, uframes_t* buffer_size);

/** @brief Set config for hardware params.
 *
 *  Param `channels` is the only one passed by ref, as that is set by this func.
 *  If strict_channels, it can be used to set expectations.
 *
 */
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
    output_t*    output);

/** @brief Set config for software params. Main things to note are min_avail, start_threshold, stop_threshold */
int sndx_set_sw_params(       //
    snd_pcm_t*   pcm,         //
    sw_params_t* sw_params,   //
    uframes_t    period_size, //
    u32          periods,     //
    output_t*    output);

/** @brief Set config for both hardware and software params
 *
 * All params are set and reflected as they are passed by reference
 *
 * */
int sndx_set_params(            //
    snd_pcm_t* pcm,             //
    u32*       channels,        //
    format_t*  format,          //
    u32*       rate,            //
    uframes_t* period_size,     //
    u32*       periods,         //
    access_t   _access,         //
    bool       strict_channels, //
    output_t*  output);
