#if __linux__
/* Defer to Linux's <syscall.h> to get Linux's syscall numbers. */
#include_next <sys/syscall.h>
#endif

#ifndef SYSCALL_H
#define SYSCALL_H

#if __munix__
enum syscall_nr {
    SYS_NULL = 0,
    SYS_exit,
    SYS_write,
    SYS_thrd_create,
    SYS_thrd_exit,
    SYS_thrd_join,
    SYS_thrd_yield,
    SYS_MAX
};
#endif /* __munix__ */

long syscall(long number, ...);
#if __linux__
int syscall_clone_linux(
        int (*fn)(void *), void *stack, int flags, void *arg, ...
        /* pid_t *parent_tid, void *tls, pid_t *child_tid */
);
#elif __munix__
int syscall_thrd_create_munix(int (*fn)(void *), void *stack, void *arg);
#endif

#endif /* SYSCALL_H */
