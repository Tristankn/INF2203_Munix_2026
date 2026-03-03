//#define LOG_LEVEL LOG_DEBUG

#include "scheduler.h"

#include "kernel.h"

#include <drivers/log.h>

#include <core/list.h>
#include <core/sprintf.h>

struct list_head ready_queue;

static int tqueue_tostr(char *buf, size_t n, struct list_head *queue)
{
    char *pos = buf, *end = buf + n;

    struct thread *t;
    list_for_each_entry(t, queue, queue)
    {
        pos += snprintf(pos, BUFREM(pos, end), "%u -> ", t->tid);
    }
    pos += snprintf(pos, BUFREM(pos, end), "(end)");
    return pos - buf;
}

void sched_add(struct thread *t)
{
    /* Add thread to ready queue. */
    list_add_tail(&t->queue, &ready_queue);
}

void sched_remove(struct thread *t)
{
    /* Remove thread from scheduling. */
    list_del(&t->queue);
}

static struct thread *choose_next_thread(void)
{
    for (;;) {
        const size_t DBGSZ = 256;
        char         dbgbuf[DBGSZ];
        pr_debug(
                "ready_queue: %s\n",
                (tqueue_tostr(dbgbuf, DBGSZ, &ready_queue), dbgbuf)
        );

        struct thread *t =
                list_first_entry(&ready_queue, struct thread, queue);

        if (!t) {
            pr_error("ready_queue empty; no threads to run\n");
            kernel_noreturn();
        }

        switch (t->runstate) {
        case RS_NEW:
        case RS_READY: return t;
        default:
            pr_debug(
                    "skipping thread %d (%s) with non-run state %d\n", t->tid,
                    t->process->name, t->runstate
            );
            list_rotate_left(&ready_queue);
            break;
        };
    }
}

void schedule(void)
{
    /* If there is a current thread running, put it back in the ready queue. */
    if (current_thread && current_thread->runstate == RS_READY)
        list_add_tail(&current_thread->queue, &ready_queue);

    /* Choose next thread and switch to it. */
    struct thread *next = choose_next_thread();
    sched_remove(next);
    thread_switch(current_thread, next);
}

