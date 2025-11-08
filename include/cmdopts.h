#pragma once

#include "types.h"

typedef struct sndx_cmdopts_t
{
    const char*          s_opts;
    const struct option* l_opts;
    u32                  l_opts_count;

} sndx_cmdopts_t;

void cmdopts_help(sndx_cmdopts_t* opts);
int  cmdopts_parse_opt(sndx_cmdopts_t* opts, int key, const char* optarg);
int  cmdopts_validate_opts(sndx_cmdopts_t* opts);
