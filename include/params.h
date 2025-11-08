/*! \file params.h
    \brief A Documented file.

    Details.
*/

#pragma once

#include "types.h"

/*! \class sndx_params_t
        \brief Hardware config common to playback and capture (except for channels).

    Hold variables frequently used in other functions.
*/
typedef struct
{
    u32       channels;
    format_t  format;
    access_t  access;
    u32       rate;
    u32       nperiods;
    uframes_t period_size;
    uframes_t buffer_size;

} sndx_params_t;

void sndx_dump_params(sndx_params_t* params, snd_output_t* output);

int sndx_set_buffer_size(snd_spcm_latency_t latency, uframes_t* buffer_size);

/*! \fn sndx_set_hw_params
        \brief Set config for hardware params.

   @param strict_channels Whether to honor requested channels, or make best effort
   @param buffer_size     Equals (period_size * nperiods)
 */
int sndx_set_hw_params(           //
    snd_pcm_t*   pcm,             //
    hw_params_t* hw_params,       //
    u32          rate,            //
    u32*         channels,        //
    format_t     format,          //
    uframes_t    buffer_size,     //
    uframes_t    period_size,     //
    access_t     access,          //
    bool         strict_channels, //
    output_t*    output);

int sndx_set_sw_params(       //
    snd_pcm_t*   pcm,         //
    sw_params_t* sw_params,   //
    uframes_t    buffer_size, //
    uframes_t    period_size, //
    output_t*    output);

int sndx_set_params(            //
    snd_pcm_t* pcm,             //
    u32*       channels,        //
    format_t*  format,          //
    u32*       rate,            //
    uframes_t* buffer_size,     //
    uframes_t* period_size,     //
    access_t   _access,         //
    bool       strict_channels, //
    output_t*  output);
