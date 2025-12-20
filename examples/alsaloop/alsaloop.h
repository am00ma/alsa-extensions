#pragma once

#include "sndx/types.h"

enum
{
    SRC_SINC_BEST_QUALITY   = 0,
    SRC_SINC_MEDIUM_QUALITY = 1,
    SRC_SINC_FASTEST        = 2,
    SRC_ZERO_ORDER_HOLD     = 3,
    SRC_LINEAR              = 4
};

typedef enum _sync_type
{
    SYNC_TYPE_NONE = 0,
    SYNC_TYPE_SIMPLE, /* add or remove samples */
    SYNC_TYPE_CAPTRATESHIFT,
    SYNC_TYPE_PLAYRATESHIFT,
    SYNC_TYPE_SAMPLERATE,
    SYNC_TYPE_AUTO, /* order: CAPTRATESHIFT, PLAYRATESHIFT, */
                    /*        SAMPLERATE, SIMPLE */
    SYNC_TYPE_LAST = SYNC_TYPE_AUTO
} sync_type_t;

typedef enum _slave_type
{
    SLAVE_TYPE_AUTO = 0,
    SLAVE_TYPE_ON   = 1,
    SLAVE_TYPE_OFF  = 2,
    SLAVE_TYPE_LAST = SLAVE_TYPE_OFF
} slave_type_t;

struct loopback_handle
{
    struct loopback* loopback;

    char*      device;
    char*      id;
    int        card_number;
    snd_pcm_t* handle;

    access_t  access;
    format_t  format;
    u32       rate;
    u32       rate_req;
    u32       channels;
    u32       buffer_size;
    u32       period_size;
    uframes_t avail_min;

    u32 buffer_size_req;
    u32 period_size_req;
    u32 frame_size;

    u32 resample : 1; /* do resample */
    u32 nblock : 1;   /* do block (period size) transfers */
    u32 xrun_pending : 1;
    u32 pollfd_count;

    /* I/O job */
    char*     buf;       /* I/O buffer */
    uframes_t buf_pos;   /* I/O position */
    uframes_t buf_count; /* filled samples */
    uframes_t buf_size;  /* buffer size in frames */
    uframes_t buf_over;  /* capture buffer overflow */
    int       stall;

    /* statistics */
    uframes_t max;
    u64       counter;
    u64       sync_point; /* in samples */
    sframes_t last_delay;
    f64       pitch;
    uframes_t total_queued;
};

struct loopback
{
    char* id;

    struct loopback_handle* capt;
    struct loopback_handle* play;

    uframes_t latency;         /* final latency in frames */
    u32       latency_req;     /* in frames */
    u32       latency_reqtime; /* in us */
    u64       loop_time;       /* ~0 = unlimited (in seconds) */
    u64       loop_limit;      /* ~0 = unlimited (in frames) */

    snd_output_t* output;
    snd_output_t* state;

    int pollfd_count;
    int active_pollfd_count;

    u32 linked : 1; /* linked streams */
    u32 reinit : 1;
    u32 running : 1;
    u32 stop_pending : 1;

    uframes_t    stop_count;
    sync_type_t  sync; /* type of sync */
    slave_type_t slave;
    int          thread; /* thread number */
    u32          wake;

    /* statistics */
    f64       pitch;
    f64       pitch_delta;
    sframes_t pitch_diff;
    sframes_t pitch_diff_min;
    sframes_t pitch_diff_max;
    u32       total_queued_count;
    tstamp_t  tstamp_start;
    tstamp_t  tstamp_end;

    /* xrun profiling */
    u32       xrun : 1; /* xrun profiling */
    tstamp_t  xrun_last_update;
    tstamp_t  xrun_last_wake0;
    tstamp_t  xrun_last_wake;
    tstamp_t  xrun_last_check0;
    tstamp_t  xrun_last_check;
    sframes_t xrun_last_pdelay;
    sframes_t xrun_last_cdelay;
    uframes_t xrun_buf_pcount;
    uframes_t xrun_buf_ccount;
    u32       xrun_out_frames;
    i64       xrun_max_proctime;
    f64       xrun_max_missing;

    /* sample rate */
    u32 use_samplerate : 1;
};

int  create_loopback_handle( //
    struct loopback_handle** _handle,
    const char*              device,
    const char*              ctldev,
    const char*              id);
int  create_loopback( //
    struct loopback**       _handle,
    struct loopback_handle* play,
    struct loopback_handle* capt,
    snd_output_t*           output);
void set_loop_time(struct loopback* loop, unsigned long loop_time);
void add_loop(struct loopback* loop);

int  pcmjob_init(struct loopback* loop);
int  pcmjob_done(struct loopback* loop);
int  pcmjob_start(struct loopback* loop);
int  pcmjob_stop(struct loopback* loop);
int  pcmjob_pollfds_init(struct loopback* loop, struct pollfd* fds);
int  pcmjob_pollfds_handle(struct loopback* loop, struct pollfd* fds);
void pcmjob_state(struct loopback* loop);
