#ifndef PROCESS_H
#define PROCESS_H

#include "pagemap.h"

#include <abi.h>
#include <cpu.h>

#include <drivers/vfs.h>

#include <core/types.h>

#include <stdint.h>

#define FD_MAX 4

struct thread;

enum runstate {
    RS_NEW,
    RS_READY,
    RS_EXITED,
};

struct process {
    struct file execfile;
    char        name[DEBUGSTR_MAX];

    pid_t     pid;
    uintptr_t start_addr;

    /* Stack addresses */
    uintptr_t ustack;
    uintptr_t kstack;

    /** Process's address space (page mapping hierarchy) */
    struct addrspc addrspc;

    /**
     * File descriptors
     *
     * - 0 = stdin
     * - 1 = stdout
     * - 2 = stderr
     */
    struct file *fds[FD_MAX];

    /* Threads in process */
    int              threadct_active; ///< Count of non-exited threads
    struct list_head threads;
};

struct thread {
    pid_t tid;

    /* Connection to process */
    struct process  *process;
    struct list_head process_threads;

    /* TODO: Other fields for thread? */

    /* Run state */
    enum runstate        runstate;
    struct cpu_task_save saved_state;
    int                  exit_status;

    /* Schduling and task switching */
    struct list_head queue;

    /* Stats */
    int yield_ct;
    int preempt_ct;
};

extern struct process *current_process;
extern struct thread *current_thread;

struct process *process_alloc(void);
int  process_load_path(struct process *p, const char *cwd, const char *path);
int  process_start(struct process *p, int argc, char *argv[]);
void process_close(struct process *p);

void process_kill(struct process *p);
_Noreturn void process_exit(int status);
ssize_t        process_write(int fd, const void *src, size_t count);

int thread_create(struct process *p, uintptr_t start_addr, uintptr_t ustack);
int thread_switch(struct thread *outgoing, struct thread *incoming);
_Noreturn void thread_exit(int status);
int            thread_join(pid_t tid);
int thread_yield(void);
int thread_preempt(void);

#endif /* PROCESS_H */
