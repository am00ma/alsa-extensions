# timers

All the various choices available:

```c
/** PCM timestamp type */
typedef enum _snd_pcm_tstamp_type {
    SND_PCM_TSTAMP_TYPE_GETTIMEOFDAY = 0, /**< gettimeofday equivalent */
    SND_PCM_TSTAMP_TYPE_MONOTONIC,        /**< posix_clock_monotonic equivalent */
    SND_PCM_TSTAMP_TYPE_MONOTONIC_RAW,    /**< monotonic_raw (no NTP) */
    SND_PCM_TSTAMP_TYPE_LAST = SND_PCM_TSTAMP_TYPE_MONOTONIC_RAW,
} snd_pcm_tstamp_type_t;

/** PCM audio timestamp type */
typedef enum _snd_pcm_audio_tstamp_type {
    /**
     * first definition for backwards compatibility only,
     * maps to wallclock/link time for HDAudio playback and DEFAULT/DMA time for everything else
     */
    SND_PCM_AUDIO_TSTAMP_TYPE_COMPAT = 0,
    SND_PCM_AUDIO_TSTAMP_TYPE_DEFAULT = 1,           /**< DMA time, reported as per hw_ptr */
    SND_PCM_AUDIO_TSTAMP_TYPE_LINK = 2,              /**< link time reported by sample or wallclock counter, reset on startup */
    SND_PCM_AUDIO_TSTAMP_TYPE_LINK_ABSOLUTE = 3,     /**< link time reported by sample or wallclock counter, not reset on startup */
    SND_PCM_AUDIO_TSTAMP_TYPE_LINK_ESTIMATED = 4,    /**< link time estimated indirectly */
    SND_PCM_AUDIO_TSTAMP_TYPE_LINK_SYNCHRONIZED = 5, /**< link time synchronized with system time */
    SND_PCM_AUDIO_TSTAMP_TYPE_LAST = SND_PCM_AUDIO_TSTAMP_TYPE_LINK_SYNCHRONIZED
} snd_pcm_audio_tstamp_type_t;

/** PCM audio timestamp config */
typedef struct _snd_pcm_audio_tstamp_config {
    /* 5 of max 16 bits used */
    unsigned int type_requested:4; /**< requested audio tstamp type   */
    unsigned int report_delay:1;   /**< add total delay to A/D or D/A */
} snd_pcm_audio_tstamp_config_t;

/** PCM audio timestamp report */
typedef struct _snd_pcm_audio_tstamp_report {
    /* 6 of max 16 bits used for bit-fields */

    unsigned int valid:1;       /**< for backwards compatibility */
    unsigned int actual_type:4; /**< actual type if hardware could not support requested timestamp */

    unsigned int accuracy_report:1; /**< 0 if accuracy unknown, 1 if accuracy field is valid */
    unsigned int accuracy;          /**< up to 4.29s in ns units, will be packed in separate field  */
} snd_pcm_audio_tstamp_report_t;
```
