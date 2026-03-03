/**
 * @file
 * Simple logging framework.
 *
 * Inspired by the Linux kernel's printk system.
 *
 * See:
 * - <https://docs.kernel.org/core-api/printk-basics.html>
 * - <https://elixir.bootlin.com/linux/v6.10/source/include/linux/printk.h>
 */
#ifndef LOG_H
#define LOG_H

#include <core/compiler.h>
#include <core/string.h>

#include <stdatomic.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>

/**
 * Options for log verbosity
 *
 * These are in order of increasing verbosity. Setting the log level (@ref
 * LOG_LEVEL) to LOG_ERROR will only print error-level log messages. Setting
 * the level to LOG_WARN will print error- and warning- level messages.
 * And so on.
 */
enum log_level {
    LOG_ERROR, ///< error: serious problem that might crash the program
    LOG_WARN,  ///< warning: irregular state that might lead to errors
    LOG_INFO,  ///< info: normal status update
    LOG_DEBUG, ///< debug: verbose information that might spam the output
    LOG_TRACE, ///< trace: extra verbose output
};

/**
 * @def LOG_LEVEL
 * Log level to use for the current compilation unit
 *
 * To set log level, define the LOG_LEVEL macro before including the header
 * file. This will set the log level for the current compilation unit (the C
 * file that is including this header).
 *
 * ```c
 * // Set the level first, then include the header.
 * #define LOG_LEVEL LOG_WARN
 * #include <drivers/log.h>
 *
 * void foo(int bar) {
 *      pr_info("foo-ing...\n");    // Will not print
 *      if (bar < 3) {
 *          pr_warn("low bar level %d\n", bar); // Will print
 *      }
 * }
 * ```
 *
 * If the module is not set when the header is included, then the header
 * will set it to a default level.
 *
 * - When compiling for our OS, the default is @ref LOG_INFO.
 * - When compiling for another OS, such as Linux, the default level is
 *      @ref LOG_ERROR. This is to reduce log output noise during unit testing.
 */
#if !defined(LOG_LEVEL) && !__HOSTED__
#define LOG_LEVEL LOG_INFO
#endif
#if !defined(LOG_LEVEL) && __HOSTED__
#define LOG_LEVEL LOG_ERROR
#endif

#if !defined(LOG_PREFIX) && defined(__FILE_NAME__)
#define LOG_PREFIX __FILE_NAME__
#elif !defined(LOG_PREFIX)
#define LOG_PREFIX ""
#endif

struct file;

int log_set_file(struct file *file);

struct _log_extra {
    const int  *result;
    const char *prefix;
    const char *postfix;
    const char *valname;
    unsigned    tblhw;
    const char *valdecode;
};

ATTR_PRINTFLIKE(4, 5)
int _logf(
        enum log_level           lvl,
        const char              *prefix,
        const struct _log_extra *req,
        const char              *fmt,
        ...
);

#define logf(LVL, FMT, ...) \
    do { \
        if (LOG_LEVEL >= LVL) \
            _logf(LVL, LOG_PREFIX, NULL, FMT, ##__VA_ARGS__); \
    } while (0)

#define pr_error(FMT, ...)   logf(LOG_ERROR, FMT, ##__VA_ARGS__)
#define pr_warning(FMT, ...) logf(LOG_WARN, FMT, ##__VA_ARGS__)
#define pr_info(FMT, ...)    logf(LOG_INFO, FMT, ##__VA_ARGS__)
#define pr_debug(FMT, ...)   logf(LOG_DEBUG, FMT, ##__VA_ARGS__)
#define pr_trace(FMT, ...)   logf(LOG_TRACE, FMT, ##__VA_ARGS__)

#define log_result(RES, FMT, ...) \
    do { \
        if ((RES < 0 && LOG_LEVEL >= LOG_WARN) \
            || (RES >= 0 && LOG_LEVEL >= LOG_INFO)) { \
            enum log_level    _lvl  = RES < 0 ? LOG_WARN : LOG_INFO; \
            int               _res  = RES; \
            struct _log_extra extra = {.result = &_res}; \
            _logf(_lvl, LOG_PREFIX, &extra, FMT, ##__VA_ARGS__); \
        } \
    } while (0)

#define debug_result(RES, FMT, ...) \
    do { \
        if ((RES < 0 && LOG_LEVEL >= LOG_DEBUG) \
            || (RES >= 0 && LOG_LEVEL >= LOG_DEBUG)) { \
            enum log_level    _lvl  = RES < 0 ? LOG_DEBUG : LOG_DEBUG; \
            int               _res  = RES; \
            struct _log_extra extra = {.result = &_res}; \
            _logf(_lvl, LOG_PREFIX, &extra, FMT, ##__VA_ARGS__); \
        } \
    } while (0)

#ifndef LOG_TABLE_HEAD_WIDTH
#define LOG_TABLE_HEAD_WIDTH 16
#endif

#define log_val(LVL, VAL, fmt) \
    do { \
        if (LOG_LEVEL >= LVL) { \
            struct _log_extra extra = { \
                    .valname = #VAL, \
                    .tblhw   = LOG_TABLE_HEAD_WIDTH, \
                    .postfix = "\n", \
            }; \
            _logf(LVL, LOG_PREFIX, &extra, fmt, VAL); \
        } \
    } while (0)

#define log_val_decode(LVL, VAL, fmt, decode) \
    do { \
        if (LOG_LEVEL >= LVL) { \
            struct _log_extra extra = { \
                    .valname   = #VAL, \
                    .tblhw     = LOG_TABLE_HEAD_WIDTH, \
                    .valdecode = decode, \
                    .postfix   = "\n", \
            }; \
            _logf(LVL, LOG_PREFIX, &extra, fmt, VAL); \
        } \
    } while (0)

/**
 * Print a log entry, but only once.
 */
#define logf_once(lvl, fmt, ...) \
    do { \
        static atomic_flag _msg_printed = ATOMIC_FLAG_INIT; \
        if (!atomic_flag_test_and_set(&_msg_printed)) \
            logf(lvl, fmt, ##__VA_ARGS__); \
    } while (0)

/**
 * Print a todo message.
 */
#define TODO() \
    logf_once(LOG_WARN, "%s:%d: TODO in %s\n", __FILE__, __LINE__, __func__)

char *
flagstr(char         *dest,
        unsigned long flags,
        size_t        count,
        const char   *src1,
        const char   *src0);

#endif /* LOG_H */
