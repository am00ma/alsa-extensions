/** @file ring.h
 *  @brief Implementation of float ring buffer preserving concept of areas.
 *
 *  Implementation mostly taken from jack2/common/ringbuffer.c
 *
 *  */
#pragma once

#include "pcm_lfloat.h"
#include "types.h"

/** @brief Ringbuffer preserving concept of areas. */
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

#ifdef SND_LITTLE_ENDIAN
#define sndx_ring_to_float   0
#define sndx_ring_from_float 0
#else
#define sndx_ring_to_float   1
#define sndx_ring_from_float 1
#endif

/** @brief Available space to read. TODO: If using uframes_t, could mess up if w < r. */
uframes_t sndx_ring_read_avail(sndx_ring_t* rb)
{
    sframes_t w, r;

    w = rb->write;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    r = rb->read;

    return w - r;
}

/** @brief Available space to write. */
uframes_t sndx_ring_write_avail(sndx_ring_t* rb)
{

    sframes_t w, r;

    w = rb->write;
    r = rb->read;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);

    return (r - w - 1) & (rb->capacity - 1);
}

/** @brief Advance read pointer. */
void sndx_ring_read_advance(sndx_ring_t* rb, uframes_t frames)
{
    sframes_t dst = sndx_ring_mask(rb, rb->read + frames);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    rb->read = dst;
}

/** @brief Advance write pointer. */
void sndx_ring_write_advance(sndx_ring_t* rb, uframes_t frames)
{
    sframes_t dst = sndx_ring_mask(rb, rb->write + frames);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    rb->write = dst;
}

/** @brief Write from channel areas to ringbuffer with proper conversions. */
uframes_t sndx_ring_write( //
    sndx_ring_t*  rb,
    const area_t* src_areas,
    uframes_t     src_offset,
    u32           channels,
    uframes_t     frames)
{
    // Check available
    sframes_t w     = rb->write;
    uframes_t avail = sndx_ring_write_avail(rb);

    // Limit to available
    frames = frames > avail ? avail : frames;

    sframes_t frames1 = frames;
    sframes_t frames2 = 0;
    if ((w + frames) > rb->capacity)
    {
        frames1 = rb->capacity - sndx_ring_mask(rb, w);
        frames2 = sndx_ring_mask(rb, (w + frames));
    }
    Assert((sframes_t)frames == (frames1 + frames2));

    // Copy first segment and advance
    const area_t* dst_areas  = rb->areas;
    uframes_t     dst_offset = sndx_ring_mask(rb, w);
    snd_pcm_lfloat_convert_integer_float( // !! --- Integer -> Float ---
        dst_areas, dst_offset,            //
        src_areas, src_offset,            //
        channels, frames1,                //
        rb->to_int, sndx_ring_from_float);
    __atomic_thread_fence(__ATOMIC_RELEASE); /* ensure pointer increment happens after copy */
    rb->write = sndx_ring_mask(rb, rb->write + frames1);

    if (frames2)
    {
        // Should always equal 0?
        dst_offset = sndx_ring_mask(rb, w + frames1); // Using old rb->write to keep consistent
        snd_pcm_lfloat_convert_float_integer(         //
            dst_areas, dst_offset,                    //
            src_areas, src_offset,                    //
            channels, frames2,                        //
            rb->to_int, sndx_ring_from_float);
        __atomic_thread_fence(__ATOMIC_RELEASE); /* ensure pointer increment happens after copy */
        rb->write = sndx_ring_mask(rb, rb->write + frames2);
    }

    // Return frames written
    return frames;
}

/** @brief Read from ringbuffer to channel areas with proper conversions. */
uframes_t sndx_ring_read( //
    sndx_ring_t*  rb,
    const area_t* dst_areas,
    uframes_t     dst_offset,
    u32           channels,
    uframes_t     frames)
{
    // Check available
    sframes_t r     = rb->read;
    uframes_t avail = sndx_ring_read_avail(rb);

    // Limit to available
    frames = frames > avail ? avail : frames;

    sframes_t frames1 = frames;
    sframes_t frames2 = 0;
    if ((r + frames) > rb->capacity)
    {
        frames1 = rb->capacity - sndx_ring_mask(rb, r);
        frames2 = sndx_ring_mask(rb, (r + frames));
    }
    Assert((sframes_t)frames == (frames1 + frames2));

    // Copy first segment and advance
    const area_t* src_areas  = rb->areas;
    uframes_t     src_offset = sndx_ring_mask(rb, r);
    snd_pcm_lfloat_convert_float_integer( // !!  --- Float -> Integer ---
        dst_areas, dst_offset,            //
        src_areas, src_offset,            //
        channels, frames1,                //
        rb->from_int, sndx_ring_to_float);
    __atomic_thread_fence(__ATOMIC_RELEASE); /* ensure pointer increment happens after copy */
    rb->read = sndx_ring_mask(rb, rb->read + frames1);

    if (frames2)
    {
        // Should always equal 0?
        src_offset = sndx_ring_mask(rb, r + frames1); // Using old rb->read to keep consistent
        snd_pcm_lfloat_convert_float_integer(         //
            dst_areas, dst_offset,                    //
            src_areas, src_offset,                    //
            channels, frames2,                        //
            rb->from_int, sndx_ring_to_float);
        __atomic_thread_fence(__ATOMIC_RELEASE); /* ensure pointer increment happens after copy */
        rb->read = sndx_ring_mask(rb, rb->read + frames2);
    }

    // Return frames read
    return frames;
}
