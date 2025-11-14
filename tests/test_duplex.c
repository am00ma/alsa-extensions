/** @file test_duplex.c
 *  @brief Checking the basic process of wait-read-write
 *
 *  Checklist:
 *      1. Capture, playback are non-block (jack makes playback blocked)
 *      2. Interleaved mmap access
 *
 */
#include "buffer.h"
#include "duplex.h"
#include "timer.h"
#include "types.h"

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(              //
        &d,                              //
        "hw:FC1,0", "hw:FC1,0",          //
        SND_PCM_FORMAT_S16_LE,           //
        48000, 128, 2,                   //
        SND_PCM_ACCESS_MMAP_INTERLEAVED, //
        output);
    SndFatal(err, "Failed sndx_duplex_open: %s");

    // This should error with broken pipe
    err = snd_pcm_start(d->play);
    SndCheck(err, "Failed snd_pcm_start: %s");

    // PREPARED
    sndx_dump_duplex_status(d, output);

    // Timer
    sndx_timer_t timer = {};

    err = sndx_duplex_write_initial_silence(d);
    SndCheck(err, "Failed sndx_duplex_write_initial_silence: %s");

    err = snd_pcm_start(d->play);
    SndCheck(err, "Failed snd_pcm_start: %s");

    // RUNNING
    sndx_dump_duplex_status(d, output);

    // Start prep for loop -> on error goto __error
    sndx_timer_start(&timer, d->rate, d->play, d->capt);

    // Read write loop
    while (timer.frames_capt < timer.rate * 10)
    {

        const area_t* areas_capt;
        const area_t* areas_play;

        sframes_t nleft_period = d->period_size;
        uframes_t offset       = 0;
        uframes_t avail_capt   = 0;
        uframes_t avail_play   = 0;

        // We need a ring buffer :/
        sframes_t buf_offset_read  = 0;
        sframes_t buf_offset_write = 0;
        sframes_t buf_avail_write  = 0;

        while (nleft_period)
        {
            err = snd_pcm_wait(d->capt, 1000);
            SndGoto_(err, __close, "Failed: snd_pcm_wait: %s");

            err = snd_pcm_avail(d->capt);
            SndGoto_(err, __close, "Failed: snd_pcm_avail_update: %s");

            // Restrict to nleft
            avail_capt = err > nleft_period ? nleft_period : err;

            // Restrict to period_size
            avail_capt = avail_capt > d->period_size ? d->period_size : avail_capt;

            // Get capture buffer
            offset = 0;
            err    = snd_pcm_mmap_begin(d->capt, &areas_capt, &offset, &avail_capt);
            SndGoto_(err, __close, "Failed: snd_pcm_mmap_begin: %s");

            // Map to our device
            RANGE(chn, d->ch_capt) { d->buf_capt->dev[chn] = areas_capt[chn]; }

            // Register frames read
            timer.frames_capt += avail_capt;

            // Read to float buffer
            // - offset -> 0, 128
            sndx_buffer_dev_to_buf(d->buf_capt, buf_offset_read, avail_capt);

            // Commit read
            err = snd_pcm_mmap_commit(d->capt, offset, avail_capt);
            SndGoto_(err, __close, "Failed: snd_pcm_mmap_commit: %s");

            // We can now pass it off to the callback
            buf_avail_write += avail_capt;
            buf_offset_read += avail_capt;

            // Mono to stereo ( copy float data )
            RANGE(chn, d->ch_play)
            RANGE(i, offset, (isize)(offset + avail_capt))
            {
                d->buf_play->bufdata[(chn * d->buf_play->frames) + i] = d->buf_capt->bufdata[i];
                // d->buf_play->bufdata[(chn * d->buf_play->frames) + i] = 0;
            }

            // Check how much is available (necessary, @see snd_pcm_mmap_begin)
            err = snd_pcm_avail(d->play);
            SndGoto_(err, __close, "Failed: snd_pcm_avail_update: %s");

            // Restrict avail_play based on what we have in buffer
            avail_play = (isize)buf_avail_write > err ? err : buf_avail_write;

            // Map areas
            offset = 0;
            err    = snd_pcm_mmap_begin(d->play, &areas_play, &offset, &avail_play);
            SndGoto_(err, __close, "Failed: snd_pcm_mmap_begin: %s");

            RANGE(chn, d->ch_play) { d->buf_play->dev[chn] = areas_play[chn]; }

            // Write from float buffer
            sndx_buffer_buf_to_dev(d->buf_play, buf_offset_read, avail_play);

            // Decrement buffer
            buf_offset_read += avail_play;
            buf_avail_write -= avail_play;

            // // Need to completely write, as we could be in middle of device buffer
            // sframes_t nleft_play;
            // sframes_t nwritten_play;
            // uframes_t to_write;
            // // uframes_t buf_offset;
            //
            // // buf_offset    = 0;
            // nwritten_play = 0;
            // nleft_play    = avail_play;
            //
            // // NOTE: This creates an artificial spin loop
            // // Instead, keep track of buffer read, written
            // isize waited_count = 0;
            // while (nleft_play)
            // {
            //     // Check how much is available (necessary, @see snd_pcm_mmap_begin)
            //     err = snd_pcm_avail(d->play);
            //     SndGoto_(err, __close, "Failed: snd_pcm_avail_update: %s");
            //
            //     // NOTE: This happens quite often
            //     if (err == 0)
            //     {
            //         waited_count++;
            //         continue;
            //     }
            //
            //     // Restrict nleft_play (already less than period_size)
            //     to_write = err > nleft_play ? nleft_play : err;
            //
            //     // a_debug("  snd_pcm_avail = %d\n"   //
            //     //         "     avail_play = %ld\n"  //
            //     //         "     nleft_play = %ld\n"  //
            //     //         "       to_write = %ld\n"  //
            //     //         "         offset = %ld\n", //
            //     //         err, avail_play, nleft_play, to_write, offset);
            //
            //     // Get the play buffer
            //     print_(offset, "1. %ld");
            //     err = snd_pcm_mmap_begin(d->play, &areas_play, &offset, &to_write);
            //     print_(offset, "2. %ld");
            //     SndGoto_(err, __close, "Failed: snd_pcm_mmap_begin: %s");
            //
            //     RANGE(chn, d->ch_play) { d->buf_play->dev[chn] = areas_play[chn]; }
            //
            //     // Write from float buffer
            //     sndx_buffer_buf_to_dev(d->buf_play, offset, to_write);
            //
            //     // Commit write
            //     err = snd_pcm_mmap_commit(d->play, offset, to_write);
            //     SndGoto_(err, __close, "Failed: snd_pcm_mmap_commit: %s");
            //
            //     nleft_play    -= to_write;
            //     nwritten_play += to_write;
            //     // buf_offset    += to_write;
            // }
            // print_(waited_count);

            // err = nwritten_play != (sframes_t)avail_play;
            // Goto_(-err, __close,                       //
            //       "Could not write as much as read:\n" //
            //       "  nwritten_play = %ld\n"            //
            //       "     avail_play = %ld\n"            //
            //       "         offset = %ld\n",           //
            //       nwritten_play, avail_play, offset);

            // Register frames to written
            timer.frames_play += avail_play;

            nleft_period -= avail_capt;
        }
    }

    sndx_timer_stop(&timer, d->play, d->capt);

    sndx_dump_timer(&timer, d->out);

__close:

    err = sndx_duplex_close(d);
    SndFatal(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    return 0;
}
