#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include <stdatomic.h>

static size_t nthreads = 2;

/* Type for shared value */
typedef unsigned long shr_t;
#define FMT_SHR "%10lu"

/* Count parameters */
static shr_t       count = 1000 * 1000;
static const shr_t step  = 1;

/* Different shared variables */
static shr_t         shared_split   = 0; ///< Will be read then updated
static shr_t         shared_oneline = 0; ///< Will be updated with +=
static _Atomic shr_t shared_atomic  = 0; ///< Will be updated atomically

/* Shared variables that will be protected by a mutex */
static mtx_t mutex;
static shr_t shared_split_mtx   = 0; ///< Will be read then updated
static shr_t shared_oneline_mtx = 0; ///< Will be updated with +=

/** Count-up thread */
static int thread_main(void *arg)
{
    (void) arg; // Unused

    for (shr_t i = 0; i < count; i++) {
        /* Explicitly read-and-then-update:
         * Here we do some work betwen the read and the update to
         * increase the chances that the thread will be interrupted
         * at exactly that point.
         * When running in QEMU, the branch is a point where QEMU will
         * pause its highly-optimized emulation and check for interrupts.
         * Otherwise QEU would never interrupt here. */
        shr_t tmp = shared_split;
        if (tmp % 2 == 0) asm volatile(""); // QEMU hack
        shared_split = tmp + step;

        /* Read-and-update in one line:
         * This one line of C might compile to more than one CPU op.
         * Even if it is a single op, there might still be a data race
         * in multi-core systems. */
        shared_oneline += step;

        /* Atomic read-and-update:
         * This should compile to a hardware-supported atomic
         * operation that will not be affected by interrupts or
         * other CPUs. */
        shared_atomic += step;

        /* Updates protected by a mutex */
        mtx_lock(&mutex);
        {
            /* Explicit read-and-then-update, protected this time */
            tmp = shared_split_mtx;
            if (tmp % 13 == 0) asm volatile(""); // QEMU hack
            shared_split_mtx = tmp + step;

            /* Read-and-update in one line, protected this time */
            shared_oneline_mtx += step;
        }
        mtx_unlock(&mutex);
    }

    return 0;
}

static void show_help(const char *progname)
{
    printf("%s - data race demonstration\n"
           "   -n N    set number of threads (default 2)\n"
           "   -c C    set count (default 1 million)\n"
           "   -p P    set count to 2^P\n"
           "   -h      display help\n"
           "\nexample\n"
           "   %s -n 4\n",
           progname, progname);
}

int main(int argc, char *argv[])
{
    int helpmode = 0;

    /* Process command line arguments. */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'n': nthreads = atoi(argv[i + 1]), i++; break;
            case 'c': count = atoi(argv[i + 1]), i++; break;
            case 'p': count = 1 << atoi(argv[i + 1]), i++; break;
            case 'h':
            default: helpmode = 1;
            }
        } else helpmode = 1;
    }

    if (helpmode) {
        show_help(argv[0]);
        return 0;
    }

    printf("Spawning %zu threads to each add " FMT_SHR " to result.\n",
           nthreads, count);

    int    res;
    thrd_t utids[nthreads];
    mtx_init(&mutex, mtx_plain);

    /* Launch threads. */
    for (size_t i = 0; i < nthreads; i++) {
        res = thrd_create(&utids[i], thread_main, NULL);
        if (res < 0)
            printf("[%zu] thrd_create error: %s\n", i, strerror(-res));
    }

    /* Join threads. */
    for (size_t i = 0; i < nthreads; i++) {
        res = thrd_join(utids[i]);
        if (res < 0) printf("[%zu] thrd_join error: %s\n", i, strerror(-res));
    }

    printf("Expected result:    " FMT_SHR "\n", nthreads * count * step);
    printf("shared_split:       " FMT_SHR "\n", shared_split);
    printf("shared_oneline:     " FMT_SHR "\n", shared_oneline);
    printf("shared_atomic:      " FMT_SHR "\n", shared_atomic);
    printf("shared_split_mtx:   " FMT_SHR "\n", shared_split_mtx);
    printf("shared_oneline_mtx: " FMT_SHR "\n", shared_oneline_mtx);

    mtx_destroy(&mutex);
    return 0;
}

