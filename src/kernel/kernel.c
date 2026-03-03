#include "kernel.h"

#include "kshell.h"
#include "pagemap.h"

#include <abi.h>
#include <boot.h>
#include <cpu.h>

#include <drivers/devices.h>
#include <drivers/fileformat/ascii.h>
#include <drivers/log.h>
#include <drivers/vfs.h>

#include <core/errno.h>
#include <core/string.h>
#include <core/types.h>

#include <stdalign.h>

static struct file      serial1;
static struct boot_info boot_info;

static int init_log(void)
{
    int res;

    res = file_open_dev(&serial1, MAKEDEV(MAJ_SERIAL, 1));
    if (res < 0) return res;
    log_set_file(&serial1);

    res = file_ioctl(&serial1, SRL_SETFLAGS, SRL_ICRNL | SRL_OCRNL);
    log_result(res, "turn on serial newline fixes\n");

    return 0;
}

static int mount_initrd(void)
{
    int res;

    res = boot_info.initrd_addr ? 0 : -ENODEV;
    log_result(res, "get initrd info provided by bootloader\n");
    if (res < 0) return res;

    unsigned rd_minor = res = ramdisk_create(
            boot_info.initrd_addr, boot_info.initrd_size, "initrd"
    );
    if (res < 0) return res;

    res = fs_mountdev(MAKEDEV(MAJ_RAMDISK, rd_minor), FS_CPIO, "/");
    if (res < 0) return res;

    return 0;
}

static _Noreturn void kernel_noreturn_inner(void)
{
    pr_info("control reached a dead end in kernel, dropping to shell\n");
    for (;;) {
        int res = kshell_init_run();
        log_result(res, "kshell returned\n");
    }
}

_Noreturn void kernel_noreturn(void)
{
    /* Restart with a fresh shell on fresh stack. */
    cpu_fresh_stack(kernel_noreturn_inner, KSTACK_DFLT);
}

int init_int_controller(void);
int init_timer_interrupt(void);

int kernel_main(void)
{
    int res;

    /* Set up essential I/O and logging. */
    res = init_driver_serial();
    if (res < 0) return res;
    init_log();
    read_boot_info(&boot_info);

    /* Init CPU and memory. */
    init_cpu();
    init_pm();

    // TODO: Initialize timer interrupt
    //init_timer_interrupt();

    /* Init more essential drivers. */
    init_driver_ramdisk();
    init_driver_tty();
    init_driver_cpiofs();

    /* Mount init ramdisk. */
    mount_initrd();

    /* Start shell. */
    kshell_init_run();

    kernel_noreturn();
}
