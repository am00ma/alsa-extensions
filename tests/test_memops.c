#include "memops.h"

int main()
{

    constexpr int frames  = 10;
    constexpr int skip_16 = 2;
    constexpr int skip_24 = 3;
    constexpr int skip_32 = 4;

    float range[frames];
    RANGE(i, frames) { range[i] = (float)i / 10; }

    print_range_header(range, frames, "%5ld | ");
    print_range(range, frames, "%+2.2f | ");

    char  dst_16_i[frames * skip_16];
    float dst_16_f[frames];
    sample_move_d16_sS(dst_16_i, range, skip_16, 1, frames);
    sample_move_dS_s16(dst_16_f, dst_16_i, 1, skip_16, frames);

    print_range(dst_16_f, frames, "%+2.2f | ");

    char  dst_24_i[frames * skip_24];
    float dst_24_f[frames];
    sample_move_d24_sS(dst_24_i, range, skip_24, 1, frames);
    sample_move_dS_s24(dst_24_f, dst_24_i, 1, skip_24, frames);
    print_range(dst_24_f, frames, "%+2.2f | ");

    char  dst_32_i[frames * skip_32];
    float dst_32_f[frames];
    sample_move_d32_sS(dst_32_i, range, skip_32, 1, frames);
    sample_move_dS_s32(dst_32_f, dst_32_i, 1, skip_32, frames);
    print_range(dst_32_f, frames, "%+2.2f | ");

    return EXIT_SUCCESS;
}
