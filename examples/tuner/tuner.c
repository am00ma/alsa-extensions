/** @file tuner.c
 *  @brief Computing fft for each period to estimate pitch
 */
#include "sndx/types.h" // IWYU pragma: keep
#include <sndfile.h>

#define max(a, b)                                                                                                      \
    ({                                                                                                                 \
        __typeof__(a) _a = (a);                                                                                        \
        __typeof__(b) _b = (b);                                                                                        \
        _a > _b ? _a : _b;                                                                                             \
    })

// Application
int main(int argc, char* argv[])
{
    int err;

    err = -(argc < 2);
    Fatal(err, "USAGE: tuner PATH");

    const char* path = argv[1];

    SNDFILE* f;
    SF_INFO  info;
    f = sf_open(path, SFM_READ, &info);

    float* samples = calloc(info.frames * info.channels, sizeof(float));
    err            = -(!samples);
    Fatal(err, "Failed: calloc(info.frames * info.channels, sizeof(float))");

    isize count = sf_readf_float(f, samples, info.frames);
    err         = -(count != info.frames);
    Fatal(err, "Failed: sf_readf_float: count != info.frames: %ld != %ld", count, info.frames);

    print_(info.frames);

    float max_left  = 0;
    float max_right = 0;

    RANGE(i, count)
    {
        max_left  = max(max_left, samples[2 * i]);
        max_right = max(max_right, samples[2 * i + 1]);
    }
    p_info("max_left, max_right: %f, %f", max_left, max_right);

    free(samples);
    sf_close(f);

    return 0;
}
