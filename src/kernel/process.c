#include "process.h"

#include "kernel.h"

#include <abi.h>
#include <cpu.h>
#include <cpu_pagemap.h>

#include <drivers/fileformat/elf.h>
#include <drivers/log.h>
#include <drivers/vfs.h>

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
    p->kstack = KSTACK_DFLT;

    res = addrspc_init(&p->addrspc);
    if (res < 0) goto error;

        /* Use this address space.
     * We'll need to update the address space as we load the ELF segments. */
        //TODO: Turn this on when ready
        //pm_set_root(p->addrspc.root_entry);

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
        uintptr_t vaddr = ALIGN_DOWN(phdr.p_vaddr, phdr.p_align);
        uintptr_t paddr = vaddr;
        size_t    size  = ALIGN_UP(phdr.p_memsz, phdr.p_align);

        pme_t flags = PME_USER;
        if (phdr.p_flags & PF_W) flags |= PME_W;

            // TODO: set flags appropriately for ELF segment

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

enum start_strategy {
    PSTART_CALL,
    PSTART_LAUNCH,
};

int process_start(struct process *p, int argc, char *argv[])
{
    enum start_strategy start_strat = PSTART_CALL;

    current_process = p;

    switch (start_strat) {
    case PSTART_CALL: {
        /* Start process via simple function call. */
        main_fn *entry = (void *) p->start_addr;
        pr_info("%s: calling to %p to start ...\n", argv[0], entry);
        int res = entry(argc, argv);
        pr_info("%s: returned %d\n", argv[0], res);
        return res;
    }
    };

    return -ENOTSUP;
}

void process_kill(struct process *p)
{
    if (!p) return;
    pr_info("killing process %d (%s)\n", p->pid, p->name);
    process_close(p);
    if (p == current_process) kernel_noreturn();
}

