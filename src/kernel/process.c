#include "process.h"

#include "kernel.h"
#include "scheduler.h"

#include <abi.h>
#include <cpu.h>
#include <cpu_pagemap.h>

#include <drivers/fileformat/elf.h>
#include <drivers/log.h>
#include <drivers/vfs.h>

#include <core/compiler.h>
#include <core/errno.h>
#include <core/inttypes.h>
#include <core/macros.h>
#include <core/path.h>
#include <core/sprintf.h>
#include <core/string.h>

#define PROCESS_MAX 8
static struct process pcb[PROCESS_MAX];
static pid_t          next_pid = 1;
struct process *current_process;

#define THREAD_MAX 16
static struct thread tcb[THREAD_MAX];
static pid_t         next_tid = 1;
struct thread       *current_thread;

#define KSTACKSZ 4096
unsigned char kstacks[THREAD_MAX][KSTACKSZ] ATTR_ALIGNED(KSTACKSZ);

/* === Thread Allocation/Initialization === */

static struct thread *thread_alloc(void)
{
    for (int i = 0; i < THREAD_MAX; i++)
        if (!tcb[i].tid) return &tcb[i];
    return NULL;
}

static void thread_close(struct thread *t)
{
    pr_debug("destroying thread %d (%s)\n", t->tid, t->process->name);

    /* Remove from lists. */
    list_del(&t->process_threads);
    list_del(&t->queue);

    /* Clear struct. */
    *t = (struct thread){};
}

/* === Process Allocation/Initialization === */

struct process *process_alloc(void)
{
    for (int i = 0; i < PROCESS_MAX; i++)
        if (!pcb[i].pid) return &pcb[i];
    return NULL;
}

int process_load_path(struct process *p, const char *cwd, const char *path)
{
    int res, file_isopen = 0;

    /* Reset struct. */
    *p = (struct process){.pid = next_pid++};
    path_basename(p->name, DEBUGSTR_MAX, path);

    /* Set stack addresses. */
    p->ustack = USTACK_DFLT;
    /* Allocate matching kernel stack. */
    ptrdiff_t i = p - pcb;
    p->kstack   = (uintptr_t) kstacks[i] + KSTACKSZ;

    res = addrspc_init(&p->addrspc);
    if (res < 0) goto error;

        /* Use this address space.
     * We'll need to update the address space as we load the ELF segments. */
    pm_set_root(p->addrspc.root_entry);

    /* Make user stack writeable. */
    size_t    ustack_sz = PAGESZ;
    uintptr_t ustack_low =
            STACK_DIR == STACK_DOWN ? p->ustack - ustack_sz : p->ustack;
    res = addrspc_map(
            &p->addrspc, (void *) ustack_low, ustack_low, ustack_sz,
            PME_USER | PME_W
    );
    if (res < 0) goto error;

    /* Open file. */
    res = file_open_path(&p->execfile, cwd, path);
    if (res < 0) goto error;
    file_isopen = 1;

    /* Read ELF header. */
    Elf32_Ehdr ehdr;
    res = elf_read_ehdr32(&p->execfile, &ehdr);
    if (res < 0) goto error;
    p->start_addr = ehdr.e_entry;

    /* Load segments. */
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr phdr;
        res = elf_read_phdr32(&p->execfile, &ehdr, i, &phdr);
        if (res < 0) goto error;

        /* Skip non-load segments. */
        if (phdr.p_type != PT_LOAD) continue;

        /* Set permissions for segment pages. */
        uintptr_t vaddr = ALIGN_DOWN(phdr.p_vaddr, PAGESZ);
        uintptr_t paddr = vaddr;
        size_t    size = ALIGN_UP(phdr.p_vaddr + phdr.p_memsz, PAGESZ) - vaddr;

        pme_t flags = PME_USER;
        if (phdr.p_flags & PF_W) flags |= PME_W;

        res = addrspc_map(&p->addrspc, (void *) vaddr, paddr, size, flags);
        if (res < 0) goto error;

            /* Load. */
        res = elf_load_seg32(&p->execfile, &phdr);
        if (res < 0) goto error;
    }

    return 0;

error:
    addrspc_cleanup(&p->addrspc);
    if (file_isopen) file_close(&p->execfile);
    return res;
}

void process_close(struct process *p)
{
    if (p == current_process) current_process = NULL;
    pm_set_root(kernel_addrspc.root_entry);
    addrspc_cleanup(&p->addrspc);
    file_close(&p->execfile);
    *p = (struct process){};
}

/* === Process Management === */

typedef int main_fn(int argc, char *argv[]);

/**
 * Push a string onto a stack
 *
 * Adjusts the stack pointer and returns a pointer to the start of the string.
 */
static char *push_str(ureg_t **stack, char *str)
{
    if (!str) return NULL;

    int   slots = ALIGN_UP(strlen(str) + 1, sizeof(ureg_t)) / sizeof(ureg_t);
    char *dst;

    switch (STACK_DIR) {
    case STACK_DOWN:
        *stack -= slots;       // Make room for string.
        dst = (char *) *stack; // Start of string is current position.
        break;
    case STACK_UP:
        dst = (char *) (*stack + 1); // Start of string is next slot.
        *stack += slots;             // Adjust stack to end of string.
        break;
    }

    strcpy(dst, str);
    return dst;
}

static void push_args(ureg_t **sp, int argc, char *argv[])
{
    switch (STACK_DIR) {
    case STACK_DOWN: {
        /* Push string data onto stack. */
        char *new_argv[argc];
        for (int i = 0; i < argc; i++) new_argv[i] = push_str(sp, argv[i]);

        /* Push a separating zero. */
        PUSH(*sp, 0);

        /* Push new argv[] pointers. */
        for (int i = argc - 1; i >= 0; i--) PUSH(*sp, (ureg_t) new_argv[i]);

        /* Push argc. */
        PUSH(*sp, argc);
        break;
    }

    default: {
        TODO();
        kernel_noreturn();
    }
    };
}

enum start_strategy {
    PSTART_CALL,
    PSTART_LAUNCH,
    PSTART_THREAD,
};

int process_start(struct process *p, int argc, char *argv[])
{
    enum start_strategy start_strat = PSTART_LAUNCH;

    current_process = p;

    switch (start_strat) {
    case PSTART_CALL: {
        /* Start process via simple function call. */
        main_fn *entry = (void *) p->start_addr;
        pr_debug("%s: calling to %p to start ...\n", argv[0], entry);
        int res = entry(argc, argv);
        pr_debug("%s: returned %d\n", argv[0], res);
        return res;
    }
    case PSTART_LAUNCH: {
        /* Launch process by switching to user stack and user privilege. */
        push_args((ureg_t **) &p->ustack, argc, argv);
        cpu_user_kstack_set(p->kstack);
        cpu_user_start(p->start_addr, p->ustack);
        /* Will not return. */
    }
    };

    return -ENOTSUP;
}

void process_kill(struct process *p)
{
    if (!p) return;
    pr_debug("killing process %d (%s)\n", p->pid, p->name);
    process_close(p);
    if (p == current_process) kernel_noreturn();
}

_Noreturn void process_exit(int status)
{
    pr_info("process %d (%s) exited with status %d\n", current_process->pid,
            current_process->name, status);
    process_close(current_process);
    kernel_noreturn();
}

ssize_t process_write(int fd, const void *src, size_t count)
{
    if (fd < 0 || fd >= FD_MAX) return -EBADF;
    if (!current_process || !current_process->fds[fd]) return -EBADF;
    return file_write(current_process->fds[fd], src, count);
}

/* === Thread Management === */

int thread_switch(struct thread *outgoing, struct thread *incoming)
{
    if (!incoming) return -ESRCH;

    pr_debug(
            "switching threads: %d (%s) -> %d (%s)\n",
            outgoing ? outgoing->tid : 0,
            outgoing ? outgoing->process->name : "none", incoming->tid,
            incoming->process->name
    );

    /* Set current thread and set kstack on interrupt. */
    current_thread  = incoming;
    current_process = incoming->process;
    /* TODO: Set target kernel stack for incoming thread. */

    /* Low-level save/restore. */
    if (outgoing && cpu_task_save(&outgoing->saved_state) != 0) {
        /* Non-zero return value indicates that we are resuming
         * a saved thread. Return from here and follow saved thread's
         * return path to where it yielded or was interrupted. */
        pr_trace("%d (%s) resumed\n", outgoing->tid, outgoing->process->name);
        return 0;
    } else if (outgoing) {
        /* Zero return value indicates that the outgoing thread was
         * saved successfully. Continue to restoring incoming thread. */
        pr_trace("%d (%s) saved\n", outgoing->tid, outgoing->process->name);
    }

    /* No outgoing thread or successful save of outgoing thread.
     * Now switch to incoming thread. */
    switch (incoming->runstate) {
    case RS_NEW:
        /* TODO: update run state and launch thread */
        return -ENOSYS;

    case RS_READY:
        cpu_task_restore(&incoming->saved_state, 1);
        /* No return. */

    default:
        pr_error(
                "%s: incoming thread %d (%s) has non-run state %d\n", __func__,
                incoming->tid, incoming->process->name, incoming->runstate
        );
    }

    kernel_noreturn();
}

int thread_create(struct process *p, uintptr_t start_addr, uintptr_t ustack)
{
    TODO();
    UNUSED(p), UNUSED(start_addr), UNUSED(ustack);
    return -ENOSYS;
}

_Noreturn void thread_exit(int status)
{
    TODO();
    UNUSED(status);
    kernel_noreturn();
}

int thread_join(pid_t tid)
{
    TODO();
    UNUSED(tid);
    return -ENOSYS;
}

int thread_yield(void)
{
    TODO();
    return -ENOSYS;
}

int thread_preempt(void)
{
    TODO();
    return -ENOSYS;
}
