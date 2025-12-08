/** @file types.h
 *  @brief Type aliases and macros to make code readable.
 */
#pragma once
// clang-format off

#define DEBUG

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SND_LITTLE_ENDIAN
#define SNDRV_LITTLE_ENDIAN
#define SNDRV_LITTLE_ENDIAN_BITFIELD
#elif __BYTE_ORDER == __BIG_ENDIAN
#define SND_BIG_ENDIAN
#define SNDRV_BIG_ENDIAN
#define SNDRV_BIG_ENDIAN_BITFIELD
#else
#error "Unsupported endian..."
#endif

/* ---------------------------------------------------------------------------
 * Common includes
 * ------------------------------------------------------------------------- */
#include <common-types.h>   // primitives, range, printing, error handling, asserts
#include <limits.h>         // INT_MAX
#include <sys/poll.h>       // Poll fds
#include <sys/time.h>       // timersub
#include <alsa/asoundlib.h> // ALSA

/* ---------------------------------------------------------------------------
 * Aliases
 * ------------------------------------------------------------------------- */
typedef snd_output_t           output_t;
typedef snd_pcm_format_t       format_t;
typedef snd_pcm_access_t       access_t;
typedef snd_pcm_stream_t       stream_t;
typedef snd_pcm_uframes_t      uframes_t;
typedef snd_pcm_sframes_t      sframes_t;
typedef snd_pcm_channel_area_t area_t;
typedef snd_pcm_status_t       status_t;
typedef snd_pcm_hw_params_t    hw_params_t;
typedef snd_pcm_sw_params_t    sw_params_t;

/* ---------------------------------------------------------------------------
 * Normal Error handling -> prints to stderr
 * ------------------------------------------------------------------------- */
#define SndCheck( err, ...)        p_err(err<0, ;         , __VA_ARGS__, snd_strerror(err));
#define SndFatal( err, ...)        p_err(err<0, exit(-1)  , __VA_ARGS__, snd_strerror(err));
#define SndReturn(err, ...)        p_err(err<0, return err, __VA_ARGS__, snd_strerror(err));
#define SndRetVal(err, val, ...)   p_err(err<0, return val, __VA_ARGS__, snd_strerror(err));
#define SndGoto(  err, label, ...) p_err(err<0, goto label, __VA_ARGS__, snd_strerror(err));

/* ---------------------------------------------------------------------------
 * ALSA Error handling -> prints to snd_output_printf named output
 * ------------------------------------------------------------------------- */
#define a_log(  ...)  snd_output_printf(output, __VA_ARGS__);
#define a_info( ...)  { a_log(__VA_ARGS__);  a_log("\n"); }
#define a_error(...)  { a_log(COLOR_RED);    a_log(__VA_ARGS__); a_log(COLOR_RESET"\n");}
#define a_title(...)  { a_log(STYLE_PTITLE); a_log(__VA_ARGS__); a_log(COLOR_RESET"\n");}

// Temporary
#ifdef DEBUG
#define a_debug(...) { a_log(COLOR_BLUE); a_log(__VA_ARGS__); a_log(COLOR_RESET"\n");}
#else
#define a_debug( ... )
#endif // DEBUG

#define a_err(err, behaviour, ...)            \
    if (err) {                                \
        a_error("%s:%d", __FILE__, __LINE__); \
        a_error(__VA_ARGS__);                 \
        behaviour;                            \
    }

// Usual error handling, but printing to output_t
#define Check_( err , ...)      a_err(err , ;         , __VA_ARGS__);
#define Fatal_( cond, ...)      a_err(cond, exit(-1)  , __VA_ARGS__);
#define Return_(err , ...)      a_err(err , return err, __VA_ARGS__);
#define RetVal_(cond, val, ...) a_err(cond, return val, __VA_ARGS__);
#define Goto_(  err , lbl, ...) a_err(err<0, goto  lbl, __VA_ARGS__);

// Standard system error
#define SysCheck_( err, ...)         a_err(err<0, ;         , __VA_ARGS__, strerror(errno));
#define SysFatal_( cond, ...)        a_err(cond , exit(-1)  , __VA_ARGS__, strerror(errno));
#define SysReturn_(err , ...)        a_err(err  , return err, __VA_ARGS__, strerror(errno));
#define SysRetVal_(err , val,   ...) a_err(err  , return val, __VA_ARGS__, strerror(errno));
#define SysGoto_(   err, label, ...) a_err(err<0, goto label, __VA_ARGS__, strerror(errno));

// TODO: Define in common-types
#define SysGoto(err, label, ...) p_err(err<0, goto label, __VA_ARGS__, strerror(errno));

// Standard alsa error
#define SndCheck_( err, ...)        a_err(err<0, ;         , __VA_ARGS__, snd_strerror(err));
#define SndFatal_( err, ...)        a_err(err<0, exit(-1)  , __VA_ARGS__, snd_strerror(err));
#define SndReturn_(err, ...)        a_err(err<0, return err, __VA_ARGS__, snd_strerror(err));
#define SndRetVal_(err, val, ...)   a_err(err<0, return val, __VA_ARGS__, snd_strerror(err));
#define SndGoto_(  err, label, ...) a_err(err<0, goto label, __VA_ARGS__, snd_strerror(err));

// clang-format on
