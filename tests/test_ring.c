#include "sndx/ring.h"

int main()
{
    int err;

    constexpr format_t format   = SND_PCM_FORMAT_S32_LE;
    constexpr u32      channels = 4;
    constexpr u32      capacity = 8;
    constexpr u32      bytes    = sizeof(i32); // S32_LE

    i32*      capt_buf   = nullptr;
    uframes_t src_rate   = 3;
    uframes_t src_offset = 0;
    uframes_t src_frames = 7; // This is not a ring, so max is capacity

    i32*      play_buf   = nullptr;
    uframes_t dst_rate   = 3;
    uframes_t dst_offset = 0;
    uframes_t dst_frames = 7; // This is not a ring, so max is capacity

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_ring_t* rb;
    err = sndx_ring_open(&rb, format, channels, capacity, output);
    SndFatal_(err, "Failed sndx_ring_open: %s");

    // Buffers for devices
    capt_buf = calloc(channels * capacity, sizeof(i32));
    err      = -(!capt_buf);
    Goto(err, __close, "Failed calloc capt_buf");

    // Interleaved
    RANGE(chn, channels)
    RANGE(i, capacity) { capt_buf[(i * channels) + chn] = INT_MAX / (chn + 1); }

    play_buf = calloc(channels * capacity, sizeof(i32));
    err      = -(!play_buf);
    Goto(err, __close, "Failed calloc play_buf");

    // Corresponding interleaved areas
    const area_t capt_areas[channels] = {
        {.addr = capt_buf, .step = channels * bytes * 8, .first = 0},
        {.addr = capt_buf, .step = channels * bytes * 8, .first = 1 * bytes * 8},
        {.addr = capt_buf, .step = channels * bytes * 8, .first = 2 * bytes * 8},
        {.addr = capt_buf, .step = channels * bytes * 8, .first = 3 * bytes * 8},
    };
    const area_t play_areas[channels] = {
        {.addr = play_buf, .step = channels * bytes * 8, .first = 0},
        {.addr = play_buf, .step = channels * bytes * 8, .first = 1 * bytes * 8},
        {.addr = capt_buf, .step = channels * bytes * 8, .first = 2 * bytes * 8},
        {.addr = capt_buf, .step = channels * bytes * 8, .first = 3 * bytes * 8},
    };

    uframes_t frames   = 0;
    uframes_t nwritten = 0;
    uframes_t nread    = 0;
    uframes_t to_write = 0;
    uframes_t to_read  = 0;
    bool      done     = false;
    isize     iter     = 0;
    while (!done)
    {
        p_title("%ld: nwritten, nread: %ld, %ld", iter, nwritten, nread);

        // Device -> Ring
        to_write = src_rate > src_frames - nwritten ? src_frames - nwritten : src_rate;
        if (to_write)
        {
            frames    = sndx_ring_write_from_areas(rb, capt_areas, src_offset + nwritten, channels, to_write);
            nwritten += frames;
            print_(frames);
        }

        p_info("Write: iter %ld: nwritten: %ld, nread: %ld (to_write: %ld, to_read: %ld)", //
               iter, nwritten, nread, to_write, to_read);
        sndx_ring_dump(rb, output);

        // Ring -> Device
        to_read = dst_rate > dst_frames - nread ? dst_frames - nread : dst_rate;
        if (to_read)
        {
            frames  = sndx_ring_read_to_areas(rb, play_areas, dst_offset + nread, channels, to_read);
            nread  += frames;
        }

        p_info("Read: iter %ld: nwritten: %ld, nread: %ld (to_write: %ld, to_read: %ld)", //
               iter, nwritten, nread, to_write, to_read);
        sndx_ring_dump(rb, output);

        done = (nwritten == src_frames) && (nread == dst_frames);
        iter++;
    }

__close:
    Free(capt_buf);
    Free(play_buf);
    sndx_ring_close(rb);
    snd_output_close(output);

    return 0;
}
