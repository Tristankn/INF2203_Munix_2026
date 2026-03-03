/* clang-format off */
/**
 * @file
 * Strings describing different error codes
 *
 * Error codes that we do not use are commented out.
 * Uncomment them to use them.
 *
 * Note that more strings will increase executable size.
 *
 * (Note to staff: this file can be generated from a list of error constants
 *  using tools in the staff repository.)
 */
#include "string.h"

#include <core/errno.h>
#include <core/macros.h>
#include <core/sprintf.h>

static const char *known_constant(int errnum)
{
    switch (errnum) {
    /* --- Required by C Standard (C99) --- */

    case EDOM:            return "EDOM";
    case EILSEQ:          return "EILSEQ";
    case ERANGE:          return "ERANGE";

    /* --- POSIX: Memory/Value --- */

    case EFAULT:          return "EFAULT";
    case ENOMEM:          return "ENOMEM";
    case EOVERFLOW:       return "EOVERFLOW";

    /* --- POSIX: Missing functionality --- */

    case ENOSYS:          return "ENOSYS";
    case ENOTSUP:         return "ENOTSUP";

    /* --- POSIX: Bad arguments --- */

    case EINVAL:          return "EINVAL";
    case E2BIG:           return "E2BIG";
    case EPERM:           return "EPERM";
    case EBUSY:           return "EBUSY";
    case EDEADLK:         return "EDEADLK";
    case EINTR:           return "EINTR";

    /* --- POSIX: Non-Blocking Op --- */

    case EAGAIN:          return "EAGAIN";
    //case EALREADY:        return "EALREADY";
    //case EWOULDBLOCK:     return "EWOULDBLOCK";

    /* --- POSIX: Process Control --- */

    case ECHILD:          return "ECHILD";
    case ESRCH:           return "ESRCH";

    /* --- POSIX: I/O: General --- */

    case EIO:             return "EIO";
    case ENOBUFS:         return "ENOBUFS";
    //case ENOSPC:          return "ENOSPC";

    /* --- POSIX: I/O: File Descriptors --- */

    case EBADF:           return "EBADF";
    case EMFILE:          return "EMFILE";
    case ENFILE:          return "ENFILE";

    /* --- POSIX: I/O: Devices --- */

    case ENODEV:          return "ENODEV";
    //case ENXIO:           return "ENXIO";

    /* --- POSIX: I/O: Filesystem --- */

    //case EACCES:          return "EACCES";
    //case EEXIST:          return "EEXIST";
    //case EFBIG:           return "EFBIG";
    case EISDIR:          return "EISDIR";
    //case ENAMETOOLONG:    return "ENAMETOOLONG";
    case ENOENT:          return "ENOENT";
    //case ENOLCK:          return "ENOLCK";
    case ENOTDIR:         return "ENOTDIR";
    //case ENOTEMPTY:       return "ENOTEMPTY";
    //case EROFS:           return "EROFS";

    /* --- POSIX: I/O: Executables --- */

    case ENOEXEC:         return "ENOEXEC";
    //case ETXTBSY:         return "ETXTBSY";

    /* --- POSIX: I/O: Pipes --- */

    //case EPIPE:           return "EPIPE";
    //case ESPIPE:          return "ESPIPE";

    /* --- POSIX: I/O: Terminals --- */

    case ENOTTY:          return "ENOTTY";

    /* --- Default --- */

    default: return NULL;
    }
}

static char err_buf[] = "E00000000";

static int format_unknown(char *dst, size_t n, int errnum)
{
    return snprintf(dst, n, "E%d", errnum);
}

int strerror_s(char *dst, size_t n, int errnum)
{
    const char *known = known_constant(errnum);
    if (known) return snprintf(dst, n, "%s", known);
    else return format_unknown(dst, n, errnum);
}

/**
 * Get a string representation of an error code.
 *
 * @param errnum    Error code. Likely an error number constant from @ref
 *                  errno.h, but could be any integer.
 *
 * @returns A string representation of that error code.
 *          The buffer for the returned string is owned by this function.
 *          It may be overwritten the next time this function is called.
 *          Extra care must be taken when multiple threads might call
 *          this function concurrently.
 *
 * This is a very simple implementation.
 * If the error code is a known error code constant from @ref errno.h,
 * it returns that costant's name, e.g. "EIO".
 * Otherwise it formats the number as a string, like "[errno 123]".
 *
 * @note
 *      Every error name string listed here takes up space in your executable.
 *      It is advisable to only include strings errors that can be expected
 *      to occur on this system.
 */
char *strerror(int errnum)
{
    const char *known = known_constant(errnum);
    if (known) return (char*) known;

    format_unknown(err_buf, ARRAY_SIZE(err_buf), errnum);
    return err_buf;
}
/* clang-format on */
