#include "unistd.h"

#include <sys/syscall.h>

_Noreturn void _exit(int status)
{
#if __linux__
    syscall(__NR_exit_group, status);
#elif __munix__
    syscall(SYS_exit, status);
#endif

    /* If the syscall fails, attempt to cause an exception. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiv-by-zero"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
    static int bad_val = 0;
    bad_val            = 10 / 0;
#pragma GCC diagnostic pop

    /* If the exception fails, go into an infinite loop to avoid returning. */
    for (;;)
        ;
}

ssize_t write(int fd, const void *src, size_t count)
{
    return syscall(SYS_write, fd, src, count);
}

