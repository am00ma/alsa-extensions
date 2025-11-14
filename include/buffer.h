/** @file buffer.h
 *  @brief Implementation of backing buffer for device, in non-interleaved float format.
 *
 *  While it is easy to just use a random buffer for each channel for backing,
 *  we try to stay within ALSA framework and define them as channel areas.
 *
 *  This could potentially help us use convert functions from ALSA,
 *  in case we want the backing buffer to be interleaved but maybe
 *  that is not necessary after all.
 *
 *  @image html channel-areas.svg
 *
 *  Used in the following functions:
 *
 *  @see sndx_duplex_open
 */
#pragma once

#include "types.h"

/** @brief Backing buffer for device, in non-interleaved float format.
 *
 *  Details:
 *      - size of buffer: period_size * periods
 *      - allowed formats of device: SND_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S24_3LE, SND_PCM_FORMAT_S32_LE
 *      - format of buffer: float, non-interleaved
 *      - how areas are defined: @see buffer.h
 */
typedef struct
{
    format_t  format;   ///< SND_PCM_FORMAT_S16_LE / SND_PCM_FORMAT_S24_3LE / SND_PCM_FORMAT_S32_LE
    u32       bytes;    ///< S16_LE = 2 ; S24_3LE = 3 ; S32_LE = 4
    u32       channels; ///< Audio channels
    uframes_t frames;   ///< Typically, `period_size * nperiods`

    area_t* dev;  ///< Interleaved, backed by device
    area_t* buf;  ///< Non-interleaved, backed by `float* data`
    float*  data; ///< Backing for buf [frames * channels]

} sndx_buffer_t;

/** @brief Dump buffer params to output. */
void sndx_dump_buffer(sndx_buffer_t* b, snd_output_t* output);

/** @brief Dump buffer areas to output. */
void sndx_dump_buffer_areas(sndx_buffer_t* b, uframes_t offset, uframes_t frames, snd_output_t* output);

/** @brief Allocate memory for the backing buffer and set up channel areas. */
int sndx_buffer_open(sndx_buffer_t** bufp, format_t format, u32 channels, uframes_t frames, snd_output_t* output);

/** @brief Free memory from the backing buffer. */
void sndx_buffer_close(sndx_buffer_t* b);

/** @brief Convert float samples to device format and copy to device. */
void sndx_buffer_buf_to_dev(sndx_buffer_t* b, uframes_t offset, uframes_t frames);

/** @brief Convert device format samples to float format and copy to buffer. */
void sndx_buffer_dev_to_buf(sndx_buffer_t* b, uframes_t offset, uframes_t frames);

/** @brief Helper for testing.
 *
 *  Maps a given char* buffer to device areas.
 *  TODO: Draw figure for sndx_buffer_map_dev_to_samples
 */
void sndx_buffer_map_dev_to_samples(sndx_buffer_t* b, char* samples);
