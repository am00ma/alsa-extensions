/** @file ring.h
 *  @brief Implementation of float ring buffer preserving concept of areas.
 *
 *  Implementation mostly taken from jack2/common/ringbuffer.c
 *
 *  */
#pragma once

#include "pcm_lfloat.h" // IWYU pragma: keep

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define sndx_ring_to_float   0
#define sndx_ring_from_float 0
#else
#define sndx_ring_to_float   1
#define sndx_ring_from_float 1
#endif

/** @brief Ringbuffer preserving concept of areas to directly write to/from device. */
typedef struct
{
    u32       channels; ///< Audio channels
    uframes_t capacity; ///< Typically, multiple of `period_size * nperiods`, power of two

    area_t* areas; ///< Mapping for data copy operations
    float*  data;  ///< Backing buffer for all channels

    sframes_t read;  ///< Read pointer
    sframes_t write; ///< Write pointer

    int from_int; ///< Conversion function index: Copy int format to int32
    int to_int;   ///< Conversion function index: Copy int32 to int format

} sndx_ring_t;

#define sndx_ring_mask(rb, val) ((val) & (rb->capacity - 1))

/** @brief Dump debugging info. */
void sndx_ring_dump(sndx_ring_t* rb, output_t* output);

/** @brief Initialize areas and backing buffer. */
int sndx_ring_open(sndx_ring_t** rbp, format_t format, u32 channels, uframes_t capacity, output_t* output);

/** @brief Free areas and backing buffer. */
void sndx_ring_close(sndx_ring_t* rb);

/** @brief Available space to read. TODO: If using uframes_t, could mess up if w < r. */
uframes_t sndx_ring_read_avail(sndx_ring_t* rb);

/** @brief Available space to write. */
uframes_t sndx_ring_write_avail(sndx_ring_t* rb);

/** @brief Advance read pointer. */
void sndx_ring_read_advance(sndx_ring_t* rb, uframes_t frames);

/** @brief Advance write pointer. */
void sndx_ring_write_advance(sndx_ring_t* rb, uframes_t frames);

/** @brief Write from channel areas to ringbuffer with proper conversions. */
uframes_t sndx_ring_write_from_areas( //
    sndx_ring_t*  rb,
    const area_t* src_areas,
    uframes_t     src_offset,
    u32           channels,
    uframes_t     frames);

/** @brief Read from ringbuffer to channel areas with proper conversions. */
uframes_t sndx_ring_read_to_areas( //
    sndx_ring_t*  rb,
    const area_t* dst_areas,
    uframes_t     dst_offset,
    u32           channels,
    uframes_t     frames);
