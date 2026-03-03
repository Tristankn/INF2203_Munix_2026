#include "log.h"

#include <drivers/vfs.h>

#include <core/compiler.h>
#include <core/errno.h>
#include <core/macros.h>
#include <core/sprintf.h>

#define DEFAULT_BUFSZ 256

static struct file *log_file = NULL;

static const char *LVLSTRS[] = {
        [LOG_ERROR] = "error", [LOG_WARN] = "warning", [LOG_INFO] = "info",
        [LOG_DEBUG] = "debug", [LOG_TRACE] = "trace",
};

int log_set_file(struct file *file)
{
    int res  = 0;
    log_file = file;
    char debugstrbuf[DEBUGSTR_MAX];
    log_result(
            res, "set log file to %s\n",
            (file_debugstr(debugstrbuf, DEBUGSTR_MAX, file), debugstrbuf)
    );
    return res;
}

static int log_vsnprintf(
        char                    *buf,
        size_t                   n,
        enum log_level           lvl,
        const char              *prefix,
        const struct _log_extra *x,
        const char              *fmt,
        va_list                  va
)
{
    int   res;
    char *pos = buf, *end = buf + n;

    /* Format level string. */
    pos += res = snprintf(pos, BUFREM(pos, end), "%-7s: ", LVLSTRS[lvl]);
    if (res < 0) return res;

    /* Format result. */
    if (x && x->result) {
        if (*x->result == 0) {
            pos += res = snprintf(pos, BUFREM(pos, end), "[OK] ");
        } else if (*x->result <= 0) {
            char errbuf[DEFAULT_BUFSZ];
            strerror_s(errbuf, DEFAULT_BUFSZ, -*x->result);
            pos += res = snprintf(pos, BUFREM(pos, end), "[-%s] ", errbuf);
        } else {
            pos += res = snprintf(pos, BUFREM(pos, end), "[%d] ", *x->result);
        }
        if (res < 0) return res;
    }

    /* Format prefix. */
    pos += res = snprintf(pos, BUFREM(pos, end), "%s: ", prefix);
    if (res < 0) return res;

    /* Format value name. */
    if (x && x->valname) {
        pos += res = snprintf(
                pos, BUFREM(pos, end), "%-*s: ", x->tblhw, x->valname
        );
        if (res < 0) return res;
    }

    /* Format message. */
    pos += res = vsnprintf(pos, BUFREM(pos, end), fmt, va);
    if (res < 0) return res;

    /* Format decoded value. */
    if (x && x->valdecode) {
        pos += res = snprintf(pos, BUFREM(pos, end), " (%s)", x->valdecode);
        if (res < 0) return res;
    }

    /* Format postfix. */
    if (x && x->postfix) {
        pos += res = snprintf(pos, BUFREM(pos, end), "%s", x->postfix);
        if (res < 0) return res;
    }

    return pos - buf;
}

int _vlogf(
        enum log_level           lvl,
        const char              *prefix,
        const struct _log_extra *x,
        const char              *fmt,
        va_list                  va
)
{
    if (lvl < 0) return -EINVAL;
    if (!log_file) return -EBADF;

    int bufsz = DEFAULT_BUFSZ;
try_format : {
    char buf[bufsz];

    /* Format message. */
    va_list va_tmp;
    va_copy(va_tmp, va);
    int res = log_vsnprintf(buf, bufsz, lvl, prefix, x, fmt, va);
    va_end(va_tmp);
    if (res < 0) return res;

    /* If the formatted output was too big for the buffer,
     * try again with a buffer that is the correct size. */
    if (res >= bufsz) {
        bufsz = res + 1;
        goto try_format;
    }

    /* If formatting was successful, write to log file. */
    res = file_write(log_file, buf, res);
    return res;
}
}

ATTR_PRINTFLIKE(4, 5)
int _logf(
        enum log_level           lvl,
        const char              *prefix,
        const struct _log_extra *req,
        const char              *fmt,
        ...
)
{
    va_list va;
    va_start(va, fmt);
    int res = _vlogf(lvl, prefix, req, fmt, va);
    va_end(va);
    return res;
}

char *
flagstr(char         *dest,
        unsigned long flags,
        size_t        count,
        const char   *src1,
        const char   *src0)
{
    dest[count] = '\0';
    for (size_t i = 0; i < count; i++) {
        int    bit = (flags >> i) & 0x1;
        size_t pos = count - 1 - i;
        dest[pos]  = bit ? src1[pos] : src0 ? src0[pos] : '-';
    }
    return dest;
}
