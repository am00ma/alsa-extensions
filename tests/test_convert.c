#include "buffer.h"
#include "types.h"
#include <alsa/asoundlib.h>

int main()
{

    output_t* OUT;
    int       err = snd_output_stdio_attach(&OUT, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    const format_t formats[3] = {SND_PCM_FORMAT_S32_LE, SND_PCM_FORMAT_S24_3LE, SND_PCM_FORMAT_S16_LE};
    // const format_t formats[3] = {SND_PCM_FORMAT_S24_3LE, SND_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S32_LE};
    // const format_t formats[3] = {SND_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S32_LE, SND_PCM_FORMAT_S24_3LE};

    RANGE(format, 3)
    {
        RANGE(interleaved, 2)
        {
            int err;

            p_title("%s: %s", snd_pcm_format_name(formats[format]),
                    interleaved > 0 ? "interleaved" : "non-interleaved");

            sndx_buffer_t* buf    = nullptr;
            snd_output_t*  output = OUT;

            err = sndx_buffer_open( //
                &buf,               //
                2,                  // channels
                formats[format],    // format
                10,                 // frames
                interleaved > 0,    // interleaved
                output);
            SndFatal_(err, "Failed sndx_buffer_open: %s");

            char* samples = nullptr;

            err = sndx_buffer_samples_alloc(buf, &samples, output);
            SndFatal_(err, "Failed sndx_buffer_samples_alloc: %s");

            sndx_dump_buffer(buf, output);

            // Load data to buffer
            if (interleaved > 0)
            {
                RANGE(chn, buf->channels)
                RANGE(i, (isize)buf->frames) buf->buf.data[(i * buf->channels) + chn] = (float)i / buf->frames;
            }
            else {
                RANGE(chn, buf->channels)
                RANGE(i, (isize)buf->frames) buf->buf.data[(chn * buf->frames) + i] = (float)i / buf->frames;
            }
            print_range(buf->buf.data, buf->channels * (isize)buf->frames, "%+2.2f | ");

            // Convert to format
            sndx_buffer_to_area_from_buf(buf, 0, buf->frames, formats[format]);

            // Reset buffer
            RANGE(chn, buf->channels)
            RANGE(i, (isize)buf->frames) buf->buf.data[(i * buf->channels) + chn] = 0.0;
            print_range(buf->buf.data, buf->channels * (isize)buf->frames, "%+2.2f | ");

            // Convert from format to float
            sndx_buffer_to_buf_from_area(buf, 0, buf->frames, formats[format]);
            print_range(buf->buf.data, buf->channels * (isize)buf->frames, "%+2.2f | ");

            err = sndx_buffer_samples_free(samples);
            SndFatal_(err, "Failed sndx_buffer_samples_free: %s");

            err = sndx_buffer_close(buf);
            SndFatal_(err, "Failed sndx_buffer_close: %s");
        }
    }

    return 0;
}
