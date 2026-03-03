#include "kshell.h"

#include "kernel.h"
#include "process.h"

#include <drivers/devices.h>
#include <drivers/fileformat/ascii.h>
#include <drivers/log.h>
#include <drivers/vfs.h>

#include <core/compiler.h>
#include <core/ctype.h>
#include <core/errno.h>
#include <core/list.h>
#include <core/macros.h>
#include <core/sprintf.h>
#include <core/string.h>

#include <stdarg.h>
#include <stdbool.h>

#define SH_PREFIX    "kshell: "
#define SH_LINEBUFSZ 256
#define SH_ARGVSZ    16
#define SH_TTYFLAGS  (TTY_ECHO | TTY_COOKED)

typedef int shcmd_fn(struct kshell *sh, int argc, char *argv[]);

struct shcmd {
    const char *name;
    shcmd_fn   *fn;
};

static const struct shcmd KSH_CMDS[];
static const char        *BIN_PATHS[] = {"/sbin", "/bin"};

/** @name Shell utilities */
///@{

#define reporterr(sh, res, fmt, ...) \
    if (res < 0) \
    file_printf( \
            sh->err, SH_PREFIX "[%s] %s: " fmt, strerror(-res), __func__, \
            ##__VA_ARGS__ \
    )

/**
 * Breaks a command line into separate arguments
 *
 * N.B. This is a destructive operation. Whitespace in the command line
 * parameter will be replaced with nulls to break it into separate argument
 * strings. If you want to preserve the original command line, you should make
 * a copy of it to pass in here.
 *
 * @param   cmdline [input/output] command line to parse. The storage in
 *                      this array will be reused for the parsed arguments.
 * @param   argv    [output] desination array for separate argument pointers
 * @param   argn    [input]  size of argv array
 *
 * @returns argc, the number of parsed arguments. Returns -E2BIG if there are
 *          too many arguments for the destination array.
 */
static int sh_break_cmdline(char *cmdline, char *argv[], size_t argn)
{
    size_t argc   = 0;
    bool   inword = false;

    for (char ch = *cmdline; ch; ch = *++cmdline) {
        /* Replace spaces with null terminators to break up command line
         * string into separate argument strings. */
        if (isspace(ch)) *cmdline = '\0', inword = false;

        if (isgraph(ch)) {
            /* If we encounter the start of a new word,
             * record it in the argv array. */
            if (!inword) {
                if (argc < argn - 1) argv[argc++] = cmdline;
                else return -E2BIG; // Unless we're out of slots.
            }
            /* Update the in-word state. */
            inword = true;
        }
    }

    argv[argc] = NULL;
    return argc;
}

static const char *ftype_marker(enum dirtype dtype)
{
    switch (dtype) {
    case DT_CHR: return "*";
    case DT_DIR: return "/";
    case DT_REG: return "";
    case DT_UNKNOWN: return "?";
    }
    return "?";
}

static int print_cmds(struct file *f, const struct shcmd cmds[])
{
    int   res;
    char  buf[SH_LINEBUFSZ];
    char *pos = buf, *end = buf + SH_LINEBUFSZ;
    for (const struct shcmd *cmd = cmds; cmd->name && pos < end; cmd++) {
        if (pos != buf) pos += snprintf(pos, BUFREM(pos, end), ", ");
        pos += snprintf(pos, BUFREM(pos, end), "%s", cmd->name);
    }

    res = file_printf(f, SH_PREFIX "built-in commands: %s\n", buf);
    if (res < 0) return res;
    return 0;
}

///@}

/** @name Shell command functions */
///@{

static int cmd_inputtest(struct kshell *sh, int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    int res;

    /* Save previous TTY flags */
    unsigned savedflags;
    res = file_ioctl(sh->in, TTY_GETFLAGS, (uintptr_t) &savedflags);
    reporterr(sh, res, "could not get TTY flags\n");
    if (res < 0) return res;

    /* Set TTY to echo control chars */
    unsigned testflags = (savedflags & ~TTY_COOKED) | TTY_ECHO | TTY_ECHOCTL;
    res                = file_ioctl(sh->in, TTY_SETFLAGS, testflags);
    reporterr(sh, res, "could not set TTY test mode\n");
    if (res < 0) return res;

    /* Read and echo characters. */
    char descbuf[SH_LINEBUFSZ];
    file_debugstr(descbuf, SH_LINEBUFSZ, sh->in);
    file_printf(sh->out, "Reading from %s. Press CTRL-D to stop.\n", descbuf);
    int testres;
    for (;;) {
        char ch;
        do testres = file_read(sh->in, &ch, 1);
        while (testres == -EAGAIN);
        reporterr(sh, testres, "error while reading characters\n");
        if (testres < 0) break;
        if (testres == 0 || ch == CTRL_D) break;
    }
    file_printf(sh->in, "\n");

    /* Restore flags */
    res = file_ioctl(sh->in, TTY_SETFLAGS, savedflags);
    reporterr(sh, res, "could not restore TTY flags\n");
    if (res < 0) return res;
    return 0;
}

static int cmd_mount(struct kshell *sh, int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    struct superblock *sb;

    int maxpathlen = 0;
    list_for_each_entry(sb, &vfs_mount_list, s_mount_list)
    {
        maxpathlen = MAX(maxpathlen, (int) strlen(sb->s_mountpath));
    }

    list_for_each_entry(sb, &vfs_mount_list, s_mount_list)
    {
        file_printf(
                sh->out, "%-*s = %s (type %s)\n", maxpathlen, sb->s_mountpath,
                sb->s_name, sb->s_op->name
        );
    }
    return 0;
}

static int cmd_pwd(struct kshell *sh, int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    file_printf(sh->out, "%s\n", sh->cwd);
    return 0;
}

static int cmd_ls(struct kshell *sh, int argc, char *argv[])
{
    int res, dir_isopen = 0;

    const char *dirpath;
    if (argc >= 2) dirpath = argv[1];
    else dirpath = NULL;

    struct file dir;
    res = file_open_path(&dir, sh->cwd, dirpath);
    if (res < 0) goto exit;
    dir_isopen = 1;

    struct dirent de;
    for (;;) {
        res = file_readdir(&dir, &de);
        if (res < 0) goto exit;
        if (res == 0) break;
        file_printf(sh->out, "%s%s\n", de.d_name, ftype_marker(de.d_type));
    }

exit:
    if (dir_isopen) file_close(&dir);
    return res;
}

static int cmd_stat(struct kshell *sh, int argc, char *argv[])
{
    int res;

    if (argc < 2) {
        file_printf(sh->err, "usage: %s FILE\n", argv[0]);
        return 1;
    }

    const char  *filepath = argv[1];
    struct fstat fstat;
    res = file_stat(&fstat, sh->cwd, filepath);
    reporterr(sh, res, "file not found\n");
    if (res < 0) return res;

    file_printf(sh->out, "  File: %s\n", filepath);
    file_printf(sh->out, "  Size: %lu\n", fstat.f_size);
    file_printf(sh->out, " Inode: %u\n", fstat.f_ino);
    return 0;
}

static int cmd_xhead(struct kshell *sh, int argc, char *argv[])
{
    int res, f_isopen = 0;

    if (argc < 2) {
        file_printf(sh->err, "usage: %s FILE\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];
    struct file f;
    res = file_open_path(&f, sh->cwd, filepath);
    if (res < 0) goto exit;
    f_isopen = 1;

    const int rowbytes = 16;
    const int rows     = 10;
    loff_t    off      = 0;
    for (int i = 0; i < rows; i++) {
        /* Read one row worth of bytes. */
        unsigned char rowbuf[rowbytes];
        res = file_read(&f, rowbuf, rowbytes);
        if (res < 0) goto exit;
        if (res == 0) break;

        file_printf(sh->out, "%.8lx:", off);
        off += res;

        /* Print row bytes. */
        for (int j = 0; j < rowbytes; j++) {
            if (j % 2 == 0) file_printf(sh->out, " ");
            if (j < res) file_printf(sh->out, "%.2x", rowbuf[j]);
            else file_printf(sh->out, "%s", "  ");
        }

        file_printf(sh->out, "  ");

        /* Print row ASCII. */
        for (int j = 0; j < rowbytes; j++) {
            char c = j < res && isprint(rowbuf[j]) ? rowbuf[j] : '.';
            file_printf(sh->out, "%c", c);
        }

        file_printf(sh->out, "\n");
    }

    res = 0;

exit:
    if (f_isopen) file_close(&f);
    return res;
}

#define GREY_ON_BLACK 0x07
#define ED_SCREEN     2

/**
 * Clear screen and reset TTY color via ANSI escape codes
 *
 * @see
 * - <https://en.wikipedia.org/wiki/ANSI_escape_code>
 */
static int cmd_reset(struct kshell *sh, int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    file_printf(sh->out, "\033[38;5;%dm", GREY_ON_BLACK);
    file_printf(sh->out, "\033[%dJ", ED_SCREEN);
    return 0;
}

static int cmd_help(struct kshell *sh, int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    return print_cmds(sh->out, KSH_CMDS);
}

static const struct shcmd KSH_CMDS[] = {
        {"help", cmd_help},
        {"inputtest", cmd_inputtest},
        {"mount", cmd_mount},
        {"pwd", cmd_pwd},
        {"ls", cmd_ls},
        {"stat", cmd_stat},
        {"xhead", cmd_xhead},
        {"reset", cmd_reset},
        {},
};

///@}

/** @name Shell operation */
///@{

static shcmd_fn *kshell_search_builtins(const struct shcmd cmds[], char *arg0)
{
    for (const struct shcmd *cmd = cmds; cmd->fn; cmd++)
        if (!cmd->name || strcmp(arg0, cmd->name) == 0) return cmd->fn;
    return NULL;
}

static const char *kshell_search_bin(struct kshell *ksh, const char *execname)
{
    int res;
    for (size_t i = 0; i < ARRAY_SIZE(BIN_PATHS); i++) {
        const char *binpath = BIN_PATHS[i];

        struct fstat fstat;
        res = file_stat(&fstat, binpath, execname);
        if (res == -ENOENT) continue;
        reporterr(ksh, res, "error looking for %s/%s\n", binpath, execname);
        if (res < 0) return NULL;
        if (fstat.f_type == DT_REG) return binpath;
    }
    return NULL;
}

int kshell_init_tty(struct kshell *sh, struct file *tty)
{
    int res;
    *sh = (struct kshell){.in = tty, .out = tty, .err = tty};
    snprintf(sh->cwd, PATH_MAX, "%s", "/");

    char buf[SH_LINEBUFSZ];
    file_printf(
            sh->out, KERNEL_NAME " " KERNEL_VERSION " kshell %s\n",
            (file_debugstr(buf, SH_LINEBUFSZ, tty), buf)
    );

    res = file_ioctl(sh->in, TTY_SETFLAGS, TTY_ECHO | TTY_COOKED);
    reporterr(sh, res, "could not set TTY flags\n");
    return res;
}

int kshell_read_exec(struct kshell *sh)
{
    int res;

    if (!sh->waiting_for_input) {
        /* Show prompt. */
        file_printf(sh->out, "> ");
        sh->waiting_for_input = 1;
    }

    /* Read line. */
    char linebuf[SH_LINEBUFSZ];
    res = file_readstr(sh->in, linebuf, sizeof(linebuf));
    if (res == -EAGAIN) return res;
    reporterr(sh, res, "could not read command line\n");
    if (res < 0) return res;
    if (res == 0) return 0; // End of file.

    sh->waiting_for_input = 0;

    /* Parse line into arguments. */
    char *argv[SH_ARGVSZ];
    int   argc = sh_break_cmdline(linebuf, argv, SH_ARGVSZ);
    reporterr(sh, res, "could not parse command line\n");
    if (argc < 0) return argc;
    if (argc == 0) return -EAGAIN;

    /* Search for builtin command. */
    shcmd_fn *cmd = kshell_search_builtins(KSH_CMDS, argv[0]);
    if (cmd) {
        res = cmd(sh, argc, argv);
        reporterr(sh, res, "%s exited with code %d\n", argv[0], res);
        return -EAGAIN;
    }

    /* Search for executable. */
    const char *bindir = kshell_search_bin(sh, argv[0]);
    if (bindir) {
        struct process *p = process_alloc();
        res               = process_load_path(p, bindir, argv[0]);
        reporterr(sh, res, "could not load %s\n", argv[0]);

        /* Link process's stdio files to shell's. */
        p->fds[0] = sh->in;  // FD 0 = stdin
        p->fds[1] = sh->out; // FD 1 = stdout
        p->fds[2] = sh->err; // FD 2 = stderr

        res = process_start(p, argc, argv);
        reporterr(sh, res, "%s exited with code %d\n", argv[0], res);
        process_close(p);
        return -EAGAIN;
    }

    /* Not found. */
    file_printf(sh->err, SH_PREFIX "unknown or program: %s\n", argv[0]);
    print_cmds(sh->err, KSH_CMDS);
    return -EAGAIN;
}

int kshell_run(struct kshell *sh)
{
    for (;;) {
        int res = kshell_read_exec(sh);
        if (res == -EAGAIN) continue;
        if (res < 0) return res;
        if (res == 0) return 0; // End of file.
    }
}

int kshell_init_run(void)
{
    int res;

    struct file   tty1;
    struct kshell ksh;

    /* Open a TTY for user interaction. */
    res = file_open_dev(&tty1, MAKEDEV(MAJ_TTY, 1));
    if (res < 0) return res;

    /* Start the in-kernel shell. */
    res = kshell_init_tty(&ksh, &tty1);
    if (res < 0) return res;

    res = kshell_run(&ksh);
    if (res < 0) return res;

    return 0;
}

///@}
