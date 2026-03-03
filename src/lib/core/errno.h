/* clang-format off */

#if !__munix__
/*
 * Defer to system's errno.h to ensure we get matching error numbers.
 *
 * Note: It is important to keep this #include_next directive outside
 * of the whole-file #ifndef guards. That way, we always follow to the
 * next <errno.h>, even if the next <errno.h> is a copy of this one.
 * We want to follow through the copy and then on to the system's <errno.h>.
 */
#include_next <errno.h>
#endif

#ifndef KERRNO_H
#define KERRNO_H
/**
 * @file
 *
 * System error numbers <errno.h>
 *
 * The C standard defines only three error codes: EDOM, EILSEQ, and ERANGE.
 * See: <https://en.cppreference.com/w/c/error/errno_macros>
 *
 * The POSIX standard defines many more.
 * See: <https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/errno.h.html>
 *
 * For more detailed explanations of how the different codes are typically
 * used, the GNU libc documentation for Error Codes is a good resource.
 * <https://sourceware.org/glibc/manual/latest/html_node/Error-Codes.html>
 *
 * Error codes that we do not use are commented out.
 * You can uncomment them to use them.
 *
 * (Note to staff: this file can be generated from a list of error constants
 *  using tools in the staff repository.)
 */

#if __munix__

/** @name Required by C Standard (C99) */
///@{
#define EDOM              1 ///< Mathematics argument out of domain of function.
#define EILSEQ            2 ///< Illegal byte sequence.
#define ERANGE            3 ///< Result too large.
///@}

/** @name POSIX: Memory/Value */
///@{
#define EFAULT            4 ///< Bad address.
#define ENOMEM            5 ///< Not enough space.
#define EOVERFLOW         6 ///< Value too large to be stored in data type.
///@}

/** @name POSIX: Missing functionality */
///@{
#define ENOSYS            7 ///< Syscall not supported  .
                            ///< This value is especially used when a requested syscall is not implemented.

#define ENOTSUP           8 ///< Not supported (may be the same value as [EOPNOTSUPP]).
                            ///< In contrast to ENOSYS, here the syscall itself is implemented, but the requested subset of functionality is not.

///@}

/** @name POSIX: Bad arguments */
///@{
#define EINVAL            9 ///< Invalid argument.
#define E2BIG            10 ///< Argument list too long.
#define EPERM            11 ///< Operation not permitted.
#define EBUSY            12 ///< Device or resource busy.
#define EDEADLK          13 ///< Resource deadlock would occur.
#define EINTR            14 ///< Interrupted function.
///@}

/** @name POSIX: Non-Blocking Op */
///@{
#define EAGAIN           15 ///< Resource unavailable, try again (may be the same value as [EWOULDBLOCK]).
//#define EALREADY         16 ///< Operation already in progress.
//#define EWOULDBLOCK      17 ///< Operation would block (may be the same value as [EAGAIN]).
///@}

/** @name POSIX: Process Control */
///@{
#define ECHILD           18 ///< No child processes.
#define ESRCH            19 ///< No such process.
///@}

/** @name POSIX: I/O: General */
///@{
#define EIO              20 ///< I/O error.
#define ENOBUFS          21 ///< No buffer space available.
//#define ENOSPC           22 ///< No space left on device.
///@}

/** @name POSIX: I/O: File Descriptors */
///@{
#define EBADF            23 ///< Bad file descriptor.
#define EMFILE           24 ///< File descriptor value too large.
#define ENFILE           25 ///< Too many files open in system.
///@}

/** @name POSIX: I/O: Devices */
///@{
#define ENODEV           26 ///< No such device.
//#define ENXIO            27 ///< No such device or address.
///@}

/** @name POSIX: I/O: Filesystem */
///@{
//#define EACCES           28 ///< Permission denied.
//#define EEXIST           29 ///< File exists.
//#define EFBIG            30 ///< File too large.
#define EISDIR           31 ///< Is a directory.
//#define ENAMETOOLONG     32 ///< Filename too long.
#define ENOENT           33 ///< No such file or directory.
//#define ENOLCK           34 ///< No locks available.
#define ENOTDIR          35 ///< Not a directory or a symbolic link to a directory.
//#define ENOTEMPTY        36 ///< Directory not empty.
//#define EROFS            37 ///< Read-only file system.
///@}

/** @name POSIX: I/O: Executables */
///@{
#define ENOEXEC          41 ///< Executable file format error.
//#define ETXTBSY          42 ///< Text file busy.
///@}

/** @name POSIX: I/O: Pipes */
///@{
//#define EPIPE            43 ///< Broken pipe.
//#define ESPIPE           44 ///< Invalid seek.
///@}

/** @name POSIX: I/O: Terminals */
///@{
#define ENOTTY           45 ///< Inappropriate I/O control operation.
///@}
#endif /* __munix__ */

#endif /* KERRNO_H */
/* clang-format on */
