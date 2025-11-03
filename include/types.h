#pragma once
// clang-format off

#define DEBUG

/* ---------------------------------------------------------------------------
 * Common includes
 * ------------------------------------------------------------------------- */
#include <stddef.h>         // ptrdiff_t
#include <sys/poll.h>       // Poll fds
#include <alsa/asoundlib.h> // ALSA

typedef ptrdiff_t isize;
typedef size_t    usize;

typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
typedef int64_t  i64;

typedef struct int24_t
{
    uint8_t x[3];
} int24_t;

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
 * Range, i over n
 * ------------------------------------------------------------------------- */
#define RANGE(...) RANGEx(__VA_ARGS__, RANGE4, RANGE3, RANGE2, RANGE1)(__VA_ARGS__)
#define RANGEx(a, b, c, d, e, ...) e

#define RANGE1(i)          for (isize i = 0  ; i < 1  ; i++)
#define RANGE2(i, b)       for (isize i = 0  ; i < (b); i++)
#define RANGE3(i, a, b)    for (isize i = (a); i < (b); i++)
#define RANGE4(i, a, b, c) for (isize i = (a); i < (b); i += (c))

/* ---------------------------------------------------------------------------
 * Pretty output
 * ------------------------------------------------------------------------- */
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_ITALIC  "\033[3m"
#define COLOR_ULINE   "\033[4m"

#define COLOR_BLACK   "\033[0;30m"
#define COLOR_RED     "\033[0;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_YELLOW  "\033[0;33m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_MAGENTA "\033[0;35m"
#define COLOR_CYAN    "\033[0;36m"
#define COLOR_WHITE   "\033[0;37m"

// For printing %.*s
#define pstr(s) (int)s.len, s.buf
#define ppstr(s) (int)s->len, s->buf

// Log, pretty print, with/without Linebreak
#define p_log(...)         fprintf(stderr, __VA_ARGS__)
#define prettyl(color, ...) (p_log(color), p_log(__VA_ARGS__), p_log(COLOR_RESET"\n"))
#define pretty(color, ...)  (p_log(color), p_log(__VA_ARGS__), p_log(COLOR_RESET))

// Example to define style
#define STYLE_PTITLE COLOR_YELLOW COLOR_BOLD COLOR_ITALIC COLOR_ULINE
#define p_title(...) {p_log(STYLE_PTITLE); p_log(__VA_ARGS__); p_log(COLOR_RESET"\n");}
#define p_info(...)  {p_log(__VA_ARGS__);  p_log("\n");}
#define p_error(...) {p_log(COLOR_RED);    p_log(__VA_ARGS__); p_log(COLOR_RESET"\n");}

#ifdef DEBUG
#define p_debug(...)         prettyl(COLOR_WHITE, __VA_ARGS__)
#define pp_debug(color, ...) prettyl(color      , __VA_ARGS__)
#else
#define p_debug( ... )
#define pp_debug(color, ...)
#endif // DEBUG

#define print_(...) printx_(__VA_ARGS__, print4_, print3_, print2_, print1_)(__VA_ARGS__)
#define printx_(a, b, c, d, e, ...) e

#define print1_(x)          p_debug(   "%s: %ld", #x, x);
#define print2_(x, y)       p_debug(   "%s: " y , #x, x);
#define print3_(x, y, z)    p_debug( z "%s: " y , #x, x);
#define print4_(x, y, z, w) p_debug( z "%" #w "s: " y , #x, x);

/* ---------------------------------------------------------------------------
 * Normal Error handling -> prints to stderr
 * ------------------------------------------------------------------------- */
#define p_err(err, behaviour, ...)            \
    if (err) {                                \
        p_error("%s:%d", __FILE__, __LINE__); \
        p_error(__VA_ARGS__);                 \
        behaviour;                            \
    }

#define Fatal( cond,  ...)     if (cond) p_err(cond, exit(err) , __VA_ARGS__);
#define Return(err ,  ...)     if (err)  p_err(err , return err, __VA_ARGS__);
#define RetVal(cond, val, ...) if (cond) p_err(cond, return val, __VA_ARGS__);

#define SndFatal( err, ...)      p_err(err<0, exit(-1)  , __VA_ARGS__, snd_strerror(err));
#define SndReturn(err, ...)      p_err(err<0, return err, __VA_ARGS__, snd_strerror(err));
#define SndRetVal(err, val, ...) p_err(err<0, return val, __VA_ARGS__, snd_strerror(err));

/* ---------------------------------------------------------------------------
 * ALSA Error handling -> prints to snd_output_printf named output
 * ------------------------------------------------------------------------- */
#define a_log(  ...)  snd_output_printf(output, __VA_ARGS__);
#define a_info( ...)  { a_log(__VA_ARGS__);  a_log("\n"); }
#define a_error(...)  { a_log(COLOR_RED);    a_log(__VA_ARGS__); a_log(COLOR_RESET"\n");}
#define a_title(...)  { a_log(STYLE_PTITLE); a_log(__VA_ARGS__); a_log(COLOR_RESET"\n");}

#define a_err(err, behaviour, ...)            \
    if (err) {                                \
        a_error("%s:%d", __FILE__, __LINE__); \
        a_error(__VA_ARGS__);                 \
        behaviour;                            \
    }

// No standard error
#define Fatal_( cond, ...)      a_err(cond, exit(-1)  , __VA_ARGS__);
#define Return_(err , ...)      a_err(err , return err, __VA_ARGS__);
#define RetVal_(cond, val, ...) a_err(cond, return val, __VA_ARGS__);

// Standard system error
#define SysFatal_( cond, ...)      a_err(cond, exit(-1)  , __VA_ARGS__, strerror(err));
#define SysReturn_(err , ...)      a_err(err , return err, __VA_ARGS__, strerror(err));
#define SysRetVal_(cond, val, ...) a_err(cond, return val, __VA_ARGS__, strerror(err));

// Standard alsa error
#define SndFatal_( err, ...)      a_err(err<0, exit(-1)  , __VA_ARGS__, snd_strerror(err));
#define SndReturn_(err, ...)      a_err(err<0, return err, __VA_ARGS__, snd_strerror(err));
#define SndRetVal_(err, val, ...) a_err(err<0, return val, __VA_ARGS__, snd_strerror(err));

/* ---------------------------------------------------------------------------
 *  General asserts
 * ------------------------------------------------------------------------- */
// Debugger friendly assert
// https://nullprogram.com/blog/2022/06/26/
#define Assert(cond)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    if (!(cond)) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
        p_error("%s:%d", __FILE__, __LINE__);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
        p_error("Failed: %s", #cond);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
        __builtin_trap();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    }

#define AssertMsg(cond, ...)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    if (!(cond)) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
        p_error("%s:%d", __FILE__, __LINE__);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
        p_error("Failed: %s", #cond);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
        p_error(__VA_ARGS__);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
        __builtin_trap();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    }

// clang-format on
