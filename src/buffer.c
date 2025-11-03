#include "buffer.h"
#include "types.h"

void sndx_dump_buffer(sndx_buffer_t* buffer, output_t* output)
{
    a_info("  frames      = %ld", buffer->frames);
    a_info("  channels    = %d", buffer->channels);
    a_info("  bytes       = %d", buffer->bytes);
    a_info("  interleaved = %d", buffer->interleaved);

    RANGE(chn, buffer->channels)
    {
        a_info("    chn %3ld : [ %p | %3d | %3d || %p | %3d ]",                                 //
               chn, buffer->areas[chn].addr, buffer->areas[chn].first, buffer->areas[chn].step, //
               buffer->addrs[chn], buffer->skips[chn]);
    }
}

int sndx_buffer_open(            //
    sndx_buffer_t** bufferp,     //
    u32             channels,    //
    format_t        format,      //
    uframes_t       maxframes,   //
    bool            interleaved, //
    output_t*       output)
{
    sndx_buffer_t* a;
    a = (sndx_buffer_t*)calloc(1, sizeof(*a));
    if (!a) return -ENOMEM;

    a->channels    = channels;
    a->bytes       = snd_pcm_format_width(format) / 8;
    a->interleaved = interleaved;
    a->frames      = maxframes;

    // Map to areas of hardware buffer

    a->areas = (snd_pcm_channel_area_t*)calloc(channels, sizeof(snd_pcm_channel_area_t));
    RetVal_(!a->areas, -ENOMEM, "Failed calloc: snd_pcm_channel_area_t* areas");

    a->addrs = (char**)calloc(channels, sizeof(char*));
    RetVal_(!a->addrs, -ENOMEM, "Failed calloc: char** addrs");

    a->skips = (u32*)calloc(channels, sizeof(u32));
    RetVal_(!a->skips, -ENOMEM, "Failed calloc: u32* skips");

    // Map to areas of soft float buffer (sndx_buffer_format_t = SND_PCM_FORMAT_FLOAT_LE)

    a->buf.bytes = snd_pcm_format_width(sndx_buffer_format_t) / 8;

    a->buf.areas = (snd_pcm_channel_area_t*)calloc(channels, sizeof(snd_pcm_channel_area_t));
    RetVal_(!a->buf.areas, -ENOMEM, "Failed calloc: snd_pcm_channel_area_t* areas");

    a->buf.addrs = (char**)calloc(channels, sizeof(char*));
    RetVal_(!a->buf.addrs, -ENOMEM, "Failed calloc: char** addrs");

    a->buf.skips = (u32*)calloc(channels, sizeof(u32));
    RetVal_(!a->buf.skips, -ENOMEM, "Failed calloc: u32* skips");

    a->buf.data = (char*)calloc(a->channels * a->buf.bytes * a->frames, sizeof(char));
    RetVal_(!a->buf.data, -ENOMEM, "Failed calloc: char* data");

    RANGE(chn, a->channels)
    {
        snd_pcm_channel_area_t* area = &a->areas[chn];

        area->addr = a->buf.data;
        if (interleaved)
        {
            area->first = (a->buf.bytes * 8) * chn;
            area->step  = a->buf.bytes * 8 * a->channels;
        }
        else {
            area->first = (a->frames * a->buf.bytes * 8) * chn;
            area->step  = a->buf.bytes * 8;
        }

        a->addrs[chn] = (char*)snd_pcm_channel_area_addr(area, 0);
        a->skips[chn] = snd_pcm_channel_area_step(area);
    }

    *bufferp = a;
    return 0;
}

int sndx_buffer_map_areas(sndx_buffer_t* a, uframes_t* offset)
{

    RANGE(chn, a->channels)
    {
        snd_pcm_channel_area_t* area = &a->areas[chn];

        a->addrs[chn] = (char*)snd_pcm_channel_area_addr(area, *offset);
        a->skips[chn] = snd_pcm_channel_area_step(area);
    }

    return 0;
}

int sndx_buffer_close(sndx_buffer_t* a)
{
    free(a->areas);
    free(a->addrs);
    free(a->skips);
    free(a->buf.data);
    free(a);
    return 0;
}
