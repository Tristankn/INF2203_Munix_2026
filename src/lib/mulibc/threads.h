#ifndef THREADS_H
#define THREADS_H

#include <core/compiler.h>
#include <core/errno.h>
#include <core/types.h>

#include <stdatomic.h>
#include <stdint.h>

/* === Threads === */

struct _thrd {
    pid_t       ktid;
    atomic_flag inuse;
    uintptr_t   ustack_base;
#if __linux__
    volatile uint32_t futex;
#endif
};

enum _thrd_res {
    thrd_success = 0,
    thrd_error   = -EFAULT,
    thrd_busy    = -EBUSY,
    thrd_nomem   = -ENOMEM,
};

typedef struct _thrd *thrd_t; ///< Type for a user-side thread identifier
typedef int (*thrd_start_t)(void *); ///< Type for a thread start function

int            thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
_Noreturn void thrd_exit(int res);
int            thrd_join(thrd_t thr);

void thrd_yield(void);

/* === Mutexes === */

struct _mtx {
    /* TODO: Design Mutex struct */
};

typedef struct _mtx mtx_t; ///< Type for a user-side mutex

/** Mutex type */
enum _mtx_type {
    mtx_plain = 1, ///< Normal mutex
};

int  mtx_init(mtx_t *mutex, int type);
void mtx_destroy(mtx_t *mutex);
int  mtx_trylock(mtx_t *mutex);
int  mtx_lock(mtx_t *mutex);
int  mtx_unlock(mtx_t *mutex);

#endif /* THREADS_H */
