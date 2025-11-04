#include "memops.h"

int main()
{

    constexpr int frames  = 10;
    constexpr int skip_16 = 4;
    constexpr int skip_24 = 6;
    constexpr int skip_32 = 8;

    float irange[frames * 2] = {};
    RANGE(i, frames) { irange[i * 2] = (float)i / 10; }

    char  idst_16_i[frames * skip_16 * 2] = {};
    float idst_16_f[frames * 2]           = {};

    RANGE(chn, 2)
    {
        sample_move_d16_sS(idst_16_i, irange, skip_16, 2, frames);
        sample_move_dS_s16(idst_16_f, idst_16_i, 2, skip_16, frames);
    }
    print_range(idst_16_f, frames, "%+2.2f | ");

    char  idst_24_i[frames * skip_24 * 2] = {};
    float idst_24_f[frames * 2]           = {};
    sample_move_d24_sS(idst_24_i, irange, skip_24, 2, frames);
    sample_move_dS_s24(idst_24_f, idst_24_i, 2, skip_24, frames);
    print_range(idst_24_f, frames, "%+2.2f | ");

    char  idst_32_i[frames * skip_32 * 2] = {};
    float idst_32_f[frames * 2]           = {};
    sample_move_d32_sS(idst_32_i, irange, skip_32, 2, frames);
    sample_move_dS_s32(idst_32_f, idst_32_i, 2, skip_32, frames);
    print_range(idst_32_f, frames, "%+2.2f | ");

    return EXIT_SUCCESS;
}
