#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

static int thread_main(void *arg)
{
    printf("%s called with arg %p -> %d\n", __func__, arg, *(int *) arg);
    *(int *) arg *= 2;
    return 0;
}

static void show_help(const char *progname)
{
    printf("%s - basic thread create/join test\n"
           "   -n N    set number of threads (default 1)\n"
           "   -h      display help\n"
           "\nexample\n"
           "   %s -n 4\n",
           progname, progname);
}

static void print_int_arr(int argc, int args[])
{
    printf("%p -> {", args);
    for (int i = 0; i < argc; i++) printf("%s%d", i ? ", " : "", args[i]);
    printf("}\n");
}

int main(int argc, char *argv[])
{
    size_t nthreads = 1;
    int    helpmode = 0;

    /* Process command line arguments. */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'n': nthreads = atoi(argv[i + 1]), i++; break;
            case 'h':
            default: helpmode = 1;
            }
        } else helpmode = 1;
    }

    if (helpmode) {
        show_help(argv[0]);
        return 0;
    }

    int    res;
    thrd_t utids[nthreads];
    int    results[nthreads];

    /* Fill arguments array. */
    for (size_t i = 0; i < nthreads; i++) results[i] = i + 1;
    printf("threads result array: ");
    print_int_arr(nthreads, results);

    /* Launch threads. */
    for (size_t i = 0; i < nthreads; i++) {
        res = thrd_create(&utids[i], thread_main, &results[i]);
        if (res < 0)
            printf("[%zu] thrd_create error: %s\n", i, strerror(-res));
        else printf("[%zu] thrd_create OK: utid=%p\n", i, utids[i]);
    }

    /* Join threads. */
    for (size_t i = 0; i < nthreads; i++) {
        printf("[%zu] joining thread %p...\n", i, utids[i]);
        res = thrd_join(utids[i]);
        if (res < 0) printf("[%zu] thrd_join error: %s\n", i, strerror(-res));
        else printf("[%zu] thrd_join OK\n", i);
    }

    /* Print array. */
    printf("threads result array: ");
    print_int_arr(nthreads, results);
    return 0;
}
