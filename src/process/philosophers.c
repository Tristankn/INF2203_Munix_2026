#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#define PHL_MAX 8

/** Move cursor up via ANSI escape code */
static int fprintf_cursor_up(FILE *f, int lines)
{
    return fprintf(f, "\033[%dA", lines);
}

/** Move cursor down via ANSI escape code */
static int fprintf_cursor_down(FILE *f, int lines)
{
    return fprintf(f, "\033[%dB", lines);
}

/** Move cursor to a specific column via ANSI escape code */
static int fprintf_cursor_horiz(FILE *f, int c)
{
    return fprintf(f, "\033[%dG", c + 1);
}

static void delay_loop(size_t slowdown)
{
    unsigned long long delay_loops = 1ull << slowdown;
    for (unsigned long long i = 0; i < delay_loops; i++) {
        /* Do nothing, but trick the compiler into thinking that we are doing
         * something. If the compiler can tell that this loop is useless then
         * it will simply remove it during optimization. */
        asm volatile("");
    }
}

/* Dining philosphers threads. */

static size_t nthreads            = 3;
static size_t phl_loops           = 100;
static size_t slowdown            = 24;
static int    exit_first_finished = 0;
static mtx_t  print_mtx;

enum phl_state {
    PHL_THINKING,
    PHL_HUNGRY,
    PHL_EATING,
    PHL_FULL,
};

static const int PHL_ART_W = 5;

static const char *PHL_ART[] = {
        [PHL_THINKING] = "(-.-)",
        [PHL_HUNGRY]   = "('/')",
        [PHL_EATING]   = "(^O^)",
        [PHL_FULL]     = "(^.^)",
};

static const char *PHL_TXT[] = {
        [PHL_THINKING] = "think",
        [PHL_HUNGRY]   = "hngry",
        [PHL_EATING]   = " eat ",
        [PHL_FULL]     = "full",
};

struct philosopher {
    size_t phil_id;
    int    left_handed; ///< Pick up left fork first

    mtx_t *fork_right;
    mtx_t *fork_left;
    int    have_right;
    int    have_left;

    enum phl_state state;
    unsigned int   meal_ct; ///< Count of meals eaten
};

static int phl_print(struct philosopher *phl)
{
    const int FORK_W = 4;
    int       hpos   = phl->phil_id * (PHL_ART_W + FORK_W) + 1;
    mtx_lock(&print_mtx);
    {
        /* Print meal count above philosopher's head. */
        fprintf_cursor_up(stdout, 4);
        fprintf_cursor_horiz(stdout, hpos);
        fprintf(stdout, "  %*u  ", PHL_ART_W, phl->meal_ct);

        /* Print philosopher state as ASCII art. */
        fprintf_cursor_down(stdout, 1);
        fprintf_cursor_horiz(stdout, hpos);
        fprintf(stdout, "%c %s %c", phl->have_right ? '>' : '_',
                PHL_ART[phl->state], phl->have_left ? '<' : '_');

        fprintf_cursor_down(stdout, 1);
        fprintf_cursor_horiz(stdout, hpos);
        fprintf(stdout, "  %*.*s  ", PHL_ART_W, PHL_ART_W,
                PHL_TXT[phl->state]);

        fprintf_cursor_down(stdout, 2);
        fprintf_cursor_horiz(stdout, 0);
    }
    mtx_unlock(&print_mtx);
    return 0;
}

static int phl_main(void *arg)
{
    struct philosopher *phl = arg;

    /* Choose order in which to pick up forks. */
    mtx_t *forka = phl->left_handed ? phl->fork_left : phl->fork_right;
    mtx_t *forkb = phl->left_handed ? phl->fork_right : phl->fork_left;
    int   *has_a = phl->left_handed ? &phl->have_left : &phl->have_right;
    int   *has_b = phl->left_handed ? &phl->have_right : &phl->have_left;

    for (size_t i = 0; i < phl_loops; i++) {
        /* Think */
        phl->state = PHL_THINKING;
        phl_print(phl);
        delay_loop(slowdown + 2);

        /* Get hungry */
        phl->state = PHL_HUNGRY;
        phl_print(phl);
        delay_loop(slowdown);

        /* Pick up fork A */
        mtx_lock(forka);
        *has_a = 1;
        phl_print(phl);
        delay_loop(slowdown);

        /* Pick up fork B */
        mtx_lock(forkb);
        *has_b = 1;
        phl_print(phl);
        //delay_loop(slowdown);

        /* Eat */
        phl->state = PHL_EATING;
        phl->meal_ct++;
        phl_print(phl);
        delay_loop(slowdown + 1);

        phl->state = PHL_FULL;
        phl_print(phl);

        /* Put down fork B */
        mtx_unlock(forkb);
        *has_b = 0;
        phl_print(phl);
        //delay_loop(slowdown);

        /* Put down fork A */
        mtx_unlock(forka);
        *has_a = 0;
        phl_print(phl);
        delay_loop(slowdown);
    }

    if (exit_first_finished) {
        mtx_lock(&print_mtx);
        _exit(0);
        mtx_unlock(&print_mtx);
    }
    return 0;
}

static void show_help(const char *progname)
{
    printf("%s - the dining philosophers\n"
           "    -n N    set number of philosophers (default %zu)\n"
           "    -c C    set loop count (default %zu)\n"
           "    -s S    set slowdown (busy wait 2^S loops, default %zu)\n"
           "    -l      make one philosopher left-handed\n"
           "    -e      exit when the first philosopher finishes\n"
           "    -h      display help\n"
           "\nexample\n"
           "   %s -n 5\n",
           progname, nthreads, phl_loops, slowdown, progname);
}

int main(int argc, char *argv[])
{
    int helpmode    = 0;
    int left_handed = 0;

    /* Process command line arguments. */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'n': nthreads = atoi(argv[i + 1]), i++; break;
            case 'c': phl_loops = atoi(argv[i + 1]), i++; break;
            case 's': slowdown = atoi(argv[i + 1]), i++; break;
            case 'l': left_handed = 1; break;
            case 'e': exit_first_finished = 1; break;
            case 'h':
            default: helpmode = 1;
            }
        } else helpmode = 1;
    }

    if (helpmode) {
        show_help(argv[0]);
        return 0;
    }

    /* Set up philosophers */
    int                res;
    struct philosopher philosophers[nthreads];
    mtx_t              forks[nthreads];
    thrd_t             utids[nthreads];

    for (size_t i = 0; i < nthreads; i++) {
        mtx_init(&forks[i], mtx_plain);
        philosophers[i] = (struct philosopher){
                .phil_id    = i,
                .fork_right = &forks[i],
                .fork_left  = &forks[(i + 1) % nthreads],
        };
    }
    if (left_handed) philosophers[nthreads - 1].left_handed = 1;

    mtx_init(&print_mtx, mtx_plain);
    fprintf(stdout, "\n\n\n\n\n"); // Make room for ASCII art

    /* Launch threads. */
    for (size_t i = 0; i < nthreads; i++) {
        res = thrd_create(&utids[i], phl_main, &philosophers[i]);
        if (res < 0)
            printf("[%zu] thrd_create error: %s\n", i, strerror(-res));
    }

    /* Join threads. */
    for (size_t i = 0; i < nthreads; i++) {
        res = thrd_join(utids[i]);
        if (res < 0) printf("[%zu] thrd_join error: %s\n", i, strerror(-res));
    }

    /* Cleanup */
    for (size_t i = 0; i < nthreads; i++) {
        mtx_destroy(&forks[i]);
    }
    mtx_destroy(&print_mtx);
    fprintf(stdout, "\n");

    return 0;
}

