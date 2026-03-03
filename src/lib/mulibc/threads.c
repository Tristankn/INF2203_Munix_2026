#include "threads.h"

#include <sys/syscall.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

#include <core/compiler.h>
#include <core/errno.h>

#if __linux__
#define __USE_GNU
#include <linux/futex.h> // Imort FUTEX_ constants
#include <sched.h>       // Import CLONE_ constants
#endif

/* === Threads === */

#define THRD_MAX     8
#define THRD_STACKSZ 4096

static struct _thrd uthrds[THRD_MAX] = {};
unsigned char       ustacks[THRD_MAX][THRD_STACKSZ] ATTR_ALIGNED(THRD_STACKSZ);

static int thrd_ok(thrd_t thrd)
{
    if (uthrds <= thrd && thrd < uthrds + THRD_MAX) return 1;

    fprintf(stderr, "invalid thrd_t %p\n", thrd);
    return 0;
}

static struct _thrd *thrd_alloc(void)
{
    for (size_t i = 0; i < THRD_MAX; i++)
        if (!atomic_flag_test_and_set(&uthrds[i].inuse)) return &uthrds[i];

    return NULL;
}

static void thrd_free(thrd_t thrd)
{
    if (!thrd_ok(thrd)) return;
    if (thrd) atomic_flag_clear(&thrd->inuse);
}

/**
 * Create a new thread
 *
 * @return  On success, returns 0. On failure, returns a negative errno.
 *          Note: This differs from the C11 standard, which has a special
 *          enum for thread return values.
 */
int thrd_create(thrd_t *thrd, thrd_start_t func, void *arg)
{
    struct _thrd *ut = thrd_alloc();
    if (!ut) return -ENOMEM;

    ptrdiff_t thr_i = ut - uthrds;
    ut->ustack_base = (uintptr_t) ustacks[thr_i];

    int   res      = -ENOSYS;
    void *stackptr = (void *) ut->ustack_base + THRD_STACKSZ;

#if __linux__
    ut->futex      = 1;
    unsigned flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND
                     | CLONE_THREAD | CLONE_SYSVSEM | CLONE_CHILD_CLEARTID;
    res = syscall_clone_linux(
            func, stackptr, flags, arg, &ut->ktid, NULL, &ut->futex
    );
#elif __munix__
    res = syscall_thrd_create_munix(func, stackptr, arg);
#endif

    /* Error: call returned negative errno */
    if (res < 0) goto error;

    /* Success: call returned child thread ID */
    if (res > 0) {
        ut->ktid = res;
        *thrd    = ut;
        return 0;
    }

error:
    thrd_free(ut);
    return res;
}

_Noreturn void thrd_exit(int res)
{
#if __linux__
    syscall(__NR_exit, res);
#elif __munix__
    syscall(SYS_thrd_exit, res);
#endif

    /* No return */
    fprintf(stderr, "error: Returned to %s\n", __func__);
    _exit(res);
}

/**
 * Wait for a thread to finish
 *
 * @return  On success, returns 0. On failure, returns a negative errno.
 *          Note: This differs from the C11 standard, which has a special
 *          enum for thread return values.
 */
int thrd_join(thrd_t thrd)
{
    int res;
    if (!thrd_ok(thrd)) return -ESRCH;
    struct _thrd *ut = thrd;
#if __linux__
    res = syscall(SYS_futex, &ut->futex, FUTEX_WAIT, 1, NULL);
    if (res == -EAGAIN && ut->futex == 0) res = 0;
#elif __munix__
    res = syscall(SYS_thrd_join, ut->ktid);
#endif

    /* Error: call returned negative errno */
    if (res < 0) return res;

    thrd_free(ut);
    return 0;
}

void thrd_yield(void)
{
#if __linux__
    syscall(__NR_sched_yield);
#elif __munix__
    syscall(SYS_thrd_yield);
#endif
}

/* === Mutexes === */

int mtx_init(mtx_t *mutex, int type)
{
    /* TODO */
    (void) mutex;
    (void) type;
    return thrd_success;
}

void mtx_destroy(mtx_t *mutex)
{
    /* TODO */
    (void) mutex;
    return;
}

int mtx_trylock(mtx_t *mutex)
{
    /* TODO */
    (void) mutex;
    return thrd_success;
}

int mtx_lock(mtx_t *mutex)
{
    /* TODO */
    (void) mutex;
    return thrd_success;
}

int mtx_unlock(mtx_t *mutex)
{
    /* TODO */
    (void) mutex;
    return thrd_success;
}

