#include <oss/elf.h>

#include <drivers/log.h>
#include <drivers/vfs.h>

#include <core/errno.h>
#include <core/inttypes.h>
#include <core/macros.h>
#include <core/sprintf.h>

static int
e_ident_tostr(char *dst, size_t n, const unsigned char e_ident[EI_NIDENT])
{
    char *pos = dst, *end = dst + n;

    const char *class;
    switch (e_ident[EI_CLASS]) {
    case ELFCLASSNONE: class = "none"; break;
    case ELFCLASS32: class = "32-bit"; break;
    case ELFCLASS64: class = "64-bit"; break;
    default: class = "invalid"; break;
    }
    pos += snprintf(
            pos, BUFREM(pos, end), "class=%u(%s)", e_ident[EI_CLASS], class
    );

    const char *data;
    switch (e_ident[EI_DATA]) {
    case ELFDATANONE: data = "none"; break;
    case ELFDATA2LSB: data = "LE"; break;
    case ELFDATA2MSB: data = "BE"; break;
    default: data = "invalid"; break;
    }
    pos += snprintf(
            pos, BUFREM(pos, end), " data=%u(%s)", e_ident[EI_DATA], data
    );

    pos += snprintf(pos, BUFREM(pos, end), " v%u", e_ident[EI_VERSION]);

    const char *abi;
    switch (e_ident[EI_OSABI]) {
    case ELFOSABI_SYSV: abi = "sysv"; break;
    case ELFOSABI_LINUX: abi = "linux"; break;
    default: abi = "other"; break;
    }
    pos += snprintf(
            pos, BUFREM(pos, end), " abi=%u(%s),v%u", e_ident[EI_OSABI], abi,
            e_ident[EI_ABIVERSION]
    );

    return pos - dst;
}

#define FMT_PH32 "%#" PRIx32

static int elf_phdr32_tostr(char *dst, size_t n, Elf32_Phdr *phdr)
{
    char *pos = dst, *end = dst + n;

    const char *type;
    switch (phdr->p_type) {
    case PT_NULL: type = "NULL"; break;
    case PT_LOAD: type = "LOAD"; break;
    case PT_DYNAMIC: type = "DYNAMIC"; break;
    case PT_INTERP: type = "INTERP"; break;
    case PT_NOTE: type = "NOTE"; break;
    case PT_SHLIB: type = "SHLIB"; break;
    case PT_PHDR: type = "PHDR"; break;
    case PT_TLS: type = "TLS"; break;
    default: type = "other"; break;
    };

    pos += snprintf(
            pos, BUFREM(pos, end), "type=%" PRIu32 "(%s)", phdr->p_type, type
    );

#define printval(LABEL, VAL) \
    snprintf(pos, BUFREM(pos, end), " " LABEL "=%#" PRIx32, VAL)

    pos += printval("offset", phdr->p_offset);
    pos += printval("vaddr", phdr->p_vaddr);
    pos += printval("filesz", phdr->p_filesz);
    pos += printval("memsz", phdr->p_memsz);
    pos += printval("flags", phdr->p_flags);
    pos += printval("align", phdr->p_align);

    return pos - dst;
}

int elf_read_ehdr32(struct file *f, Elf32_Ehdr *ehdr)
{
    int  res;
    char debugbuf[PATH_MAX];

    res = file_read(f, ehdr, sizeof(Elf32_Ehdr));
    if (res < 0) return res;

    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) return -EINVAL;
    pr_debug(
            "ELF %s\n",
            (e_ident_tostr(debugbuf, PATH_MAX, ehdr->e_ident), debugbuf)
    );

    res = ehdr->e_ident[EI_CLASS] == ELFCLASS32 ? 0 : -ENOTSUP;
    if (res < 0) return res;
    res = ehdr->e_type == ET_EXEC ? 0 : -ENOTSUP;
    if (res < 0) return res;

    pr_debug("entry point %#8zx\n", ehdr->e_entry);
    pr_debug(
            "%u segments, entry size %u bytes\n", ehdr->e_phnum,
            ehdr->e_phentsize
    );

    return 0;
}

int elf_read_phdr32(
        struct file *f, Elf32_Ehdr *ehdr, size_t i, Elf32_Phdr *phdr
)
{
    int  res;
    char debugbuf[PATH_MAX];

    res = file_lseek(f, ehdr->e_phoff + i * sizeof(Elf32_Phdr), SEEK_SET);
    if (res < 0) return res;
    res = file_read(f, phdr, sizeof(Elf32_Phdr));
    if (res < 0) return res;

    pr_debug(
            "seg %zu: %s\n", i,
            (elf_phdr32_tostr(debugbuf, PATH_MAX, phdr), debugbuf)
    );

    return 0;
}

int elf_load_seg32(struct file *f, Elf32_Phdr *phdr)
{
    int res;

    res = file_lseek(f, phdr->p_offset, SEEK_SET);
    if (res < 0) return res;

    unsigned char *vaddr = (void *) phdr->p_vaddr;
    unsigned char *vpos  = vaddr;
    unsigned char *fend  = vaddr + phdr->p_filesz;
    unsigned char *mend  = vaddr + phdr->p_memsz;

    /* Copy from file. */
    while (vpos < fend) {
        vpos += res = file_read(f, vpos, fend - vpos);
        if (res < 0) return res;
    }

    /* Zero fill to 'p_memsz'. */
    memset(vpos, 0, mend - vpos);

    return 0;
}

