#include "sndx/types.h"
#include <signal.h>
#include <sys/timerfd.h>

#define NSEC_PER_SEC 1000000000L

static int       stop = 0;
static clockid_t cid  = CLOCK_REALTIME;

static int timeout               = 5;
static int ignore_clock          = 0;
static int ignore_active_sensing = 0;
static int do_print_timestamp    = 1;

static void sig_handler(int sig ATTRIBUTE_UNUSED) { stop = 1; }

/*
 * prints MIDI commands, formatting them nicely
 */
static void print_byte(unsigned char byte, struct timespec* ts)
{
    static enum {
        STATE_UNKNOWN,
        STATE_1PARAM,
        STATE_1PARAM_CONTINUE,
        STATE_2PARAM_1,
        STATE_2PARAM_2,
        STATE_2PARAM_1_CONTINUE,
        STATE_SYSEX
    } state     = STATE_UNKNOWN;
    int newline = 0;

    if (byte >= 0xf8) newline = 1;
    else if (byte >= 0xf0)
    {
        newline = 1;
        switch (byte)
        {
        case 0xf0: state = STATE_SYSEX; break;
        case 0xf1:
        case 0xf3: state = STATE_1PARAM; break;
        case 0xf2: state = STATE_2PARAM_1; break;
        case 0xf4:
        case 0xf5:
        case 0xf6: state = STATE_UNKNOWN; break;
        case 0xf7:
            newline = state != STATE_SYSEX;
            state   = STATE_UNKNOWN;
            break;
        }
    }
    else if (byte >= 0x80)
    {
        newline = 1;
        if (byte >= 0xc0 && byte <= 0xdf) state = STATE_1PARAM;
        else state = STATE_2PARAM_1;
    }
    else /* b < 0x80 */
    {
        int running_status = 0;
        newline            = state == STATE_UNKNOWN;
        switch (state)
        {
        case STATE_1PARAM: state = STATE_1PARAM_CONTINUE; break;
        case STATE_1PARAM_CONTINUE: running_status = 1; break;
        case STATE_2PARAM_1: state = STATE_2PARAM_2; break;
        case STATE_2PARAM_2: state = STATE_2PARAM_1_CONTINUE; break;
        case STATE_2PARAM_1_CONTINUE:
            running_status = 1;
            state          = STATE_2PARAM_2;
            break;
        default: break;
        }
        if (running_status) fputs("\n  ", stdout);
    }

    putchar(newline ? '\n' : ' ');
    if (newline && do_print_timestamp)
    {
        /* Nanoseconds does not make a lot of sense for serial MIDI (the
         * 31250 bps one) but I'm not sure about MIDI over USB.
         */
        printf("%lld.%.9ld) ", (long long)ts->tv_sec, ts->tv_nsec);
    }

    printf("%02X", byte);
}

int main()
{
    int err;

    snd_rawmidi_t *input, *output;
    err = snd_rawmidi_open(&input, &output, "hw:1,0,0", SND_RAWMIDI_NONBLOCK);
    SndFatal(err, "Failed: snd_rawmidi_open: %s");

    // Trigger reading
    snd_rawmidi_read(input, NULL, 0);

    int               read = 0;
    int               npfds;
    struct pollfd*    pfds;
    struct itimerspec itimerspec = {.it_interval = {0, 0}};

    npfds = 1 + snd_rawmidi_poll_descriptors_count(input);
    pfds  = alloca(npfds * sizeof(struct pollfd));

    if (timeout > 0)
    {
        pfds[0].fd = timerfd_create(CLOCK_MONOTONIC, 0);
        err        = (pfds[0].fd == -1);
        SysGoto(err, __close, "Failed: timerfd_create (timeout) :%s");

        pfds[0].events = POLLIN;
    }
    else
    {
        pfds[0].fd = -1;
    }

    // NOTE: Not checking error?
    snd_rawmidi_poll_descriptors(input, &pfds[1], npfds - 1);

    // NOTE: Why here and not on top?
    signal(SIGINT, sig_handler);

    if (timeout > 0)
    {
        float timeout_int;

        itimerspec.it_value.tv_nsec = modff(timeout, &timeout_int) * NSEC_PER_SEC;
        itimerspec.it_value.tv_sec  = timeout_int;
        err                         = timerfd_settime(pfds[0].fd, 0, &itimerspec, NULL);
        SysGoto(err, __close, "Failed: timerfd_settime (timeout) :%s");
    }

    for (;;)
    {
        unsigned char   buf[256];
        int             i, length;
        unsigned short  revents;
        struct timespec ts;

        err = poll(pfds, npfds, -1);
        if (stop || (err < 0 && errno == EINTR)) break;
        SysGoto(err, __close, "Failed: poll:%s");

        err = clock_gettime(cid, &ts);
        SysGoto(err, __close, "Failed: clock_gettime (cid=%d): %s", cid);

        err = snd_rawmidi_poll_descriptors_revents(input, &pfds[1], npfds - 1, &revents);
        SndGoto(err, __close, "Failed: snd_rawmidi_poll_descriptors_revents: %s");

        if (revents & (POLLERR | POLLHUP)) break;
        if (!(revents & POLLIN))
        {
            if (pfds[0].revents & POLLIN) break; // Got signal from timeout, so exit loop
            continue;                            // Did not get POLLIN, so poll again
        }

        err = snd_rawmidi_read(input, buf, sizeof(buf));
        if (err == -EAGAIN) continue;
        SndGoto(err, __close, "Failed: snd_rawmidi_read: %s");

        length = 0;
        for (i = 0; i < err; ++i)
            if ((buf[i] != MIDI_CMD_COMMON_CLOCK && buf[i] != MIDI_CMD_COMMON_SENSING) ||
                (buf[i] == MIDI_CMD_COMMON_CLOCK && !ignore_clock) ||
                (buf[i] == MIDI_CMD_COMMON_SENSING && !ignore_active_sensing))
                buf[length++] = buf[i];
        if (length == 0) continue;
        read += length;

        RANGE(i, length) { print_byte(buf[i], &ts); }

        fflush(stdout);

        // Reset timer as we received event
        if (timeout > 0)
        {
            err = timerfd_settime(pfds[0].fd, 0, &itimerspec, NULL);
            SysGoto(err, __close, "Failed: timerfd_settime (timeout): %s");
        }
    }

    printf("\n%d bytes read\n", read);

__close:
    snd_rawmidi_close(input);
    snd_rawmidi_close(output);
    return 0;
}
