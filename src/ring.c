/** @file ring.c
 *  @brief Implementation of float ring buffer preserving concept of areas.
 *
 *  Implementation mostly taken from jack2/common/ringbuffer.c
 *
 *  */
#include "ring.h"

void sndx_ring_dump(sndx_ring_t* rb, output_t* output)
{
    a_info("Ring:");
    a_info("  capacity, data: %ld, %p\n"
           "     read, write: %ld, %ld",
           rb->capacity, rb->data, rb->read, rb->write);
    RANGE(chn, rb->channels)
    {
        a_info("  area %ld:", chn);
        a_info("    addr, first, step : %p, %d, %d", //
               rb->areas[chn].addr, rb->areas[chn].first, rb->areas[chn].step);
        float* buf = &((float*)rb->areas[chn].addr)[rb->areas[chn].first / rb->areas[chn].step];
        print_range(buf, rb->capacity, "%.4f ");
    }
}

/** @brief Initialize areas and backing buffer. */
int sndx_ring_open(sndx_ring_t** rbp, format_t format, u32 channels, uframes_t capacity, output_t* output)
{
    int err;

    sndx_ring_t* rb;
    rb = calloc(1, sizeof(*rb));
    RetVal_(!rb, -ENOMEM, "Failed calloc buffer_t* b");

    rb->channels = channels;
    rb->capacity = capacity;

    rb->from_int = snd_pcm_linear_get_index(format, SND_PCM_FORMAT_S32);
    rb->to_int   = snd_pcm_linear_put_index(SND_PCM_FORMAT_S32, format);

    rb->data = calloc(channels * capacity, sizeof(float));
    err      = -(!rb->data);
    Goto_(err, __close, "Failed calloc float* rb->data");

    rb->areas = calloc(channels, sizeof(area_t));
    err       = -(!rb->areas);
    Goto_(err, __close, "Failed calloc area_t* rb->areas");

    RANGE(chn, rb->channels)
    {
        rb->areas[chn].addr  = rb->data;
        rb->areas[chn].first = chn * rb->capacity * sizeof(float) * 8;
        rb->areas[chn].step  = sizeof(float) * 8;
    }

    *rbp = rb;

    return 0;

__close:
    sndx_ring_close(rb);
    *rbp = nullptr;

    return err;
}

void sndx_ring_close(sndx_ring_t* rb)
{
    if (!rb) return;

    Free(rb->areas);
    Free(rb->data);
    Free(rb);
}

/** @brief Available space to read. TODO: If using uframes_t, could mess up if w < r. */
uframes_t sndx_ring_read_avail(sndx_ring_t* rb)
{
    sframes_t w, r;

    w = rb->write;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    r = rb->read;

    return sndx_ring_mask(rb, w - r);
}

/** @brief Available space to write. */
uframes_t sndx_ring_write_avail(sndx_ring_t* rb)
{

    sframes_t w, r;

    w = rb->write;
    r = rb->read;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);

    return sndx_ring_mask(rb, r - w - 1); // NOTE: Removing -1
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
uframes_t sndx_ring_write_from_areas( //
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
        rb->from_int, sndx_ring_to_float);
    __atomic_thread_fence(__ATOMIC_RELEASE); /* ensure pointer increment happens after copy */
    rb->write = sndx_ring_mask(rb, w + frames1);

    if (frames2)
    {
        // dst_offset should always equal 0?
        w          = rb->write;
        dst_offset = sndx_ring_mask(rb, w);   // Using old rb->write to keep consistent
        snd_pcm_lfloat_convert_integer_float( //
            dst_areas, dst_offset,            //
            src_areas, src_offset,            //
            channels, frames2,                //
            rb->from_int, sndx_ring_to_float);
        __atomic_thread_fence(__ATOMIC_RELEASE); /* ensure pointer increment happens after copy */
        rb->write = sndx_ring_mask(rb, w + frames2);
    }

    return frames;
}

/** @brief Read from ringbuffer to channel areas with proper conversions. */
uframes_t sndx_ring_read_to_areas( //
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
        rb->to_int, sndx_ring_from_float);
    __atomic_thread_fence(__ATOMIC_RELEASE); /* ensure pointer increment happens after copy */
    rb->read = sndx_ring_mask(rb, r + frames1);

    if (frames2)
    {
        // Shouldn't src_offset always equal 0?
        r          = rb->read;
        src_offset = sndx_ring_mask(rb, r);   // Using old rb->read to keep consistent
        snd_pcm_lfloat_convert_float_integer( //
            dst_areas, dst_offset,            //
            src_areas, src_offset,            //
            channels, frames2,                //
            rb->to_int, sndx_ring_from_float);
        __atomic_thread_fence(__ATOMIC_RELEASE); /* ensure pointer increment happens after copy */
        rb->read = sndx_ring_mask(rb, r + frames2);
    }

    return frames;
}
