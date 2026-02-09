#include "boot.h"

#include <oss/multiboot2.h>

#include <drivers/log.h>

#include <core/errno.h>
#include <core/inttypes.h>
#include <core/macros.h>

#include <stdint.h>
#include <stdnoreturn.h>

struct mb2_boot_info {
    uint32_t total_size;
    uint32_t _reserved;
};

static uint32_t              mb2_magic;
static struct mb2_boot_info *mb2_boot_info;

void _start_mb2(uint32_t magic, struct mb2_boot_info *boot_info)
{
    mb2_magic     = magic;
    mb2_boot_info = boot_info;
    kernel_main();
}

static const char *mmap_typestr(unsigned mmap_type)
{
    // clang-format off
    switch (mmap_type) {
    case MULTIBOOT_MEMORY_AVAILABLE:        return "AVAILABLE";
    case MULTIBOOT_MEMORY_RESERVED:         return "RESERVED";
    case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: return "ACPI_RECLAIMABLE";
    case MULTIBOOT_MEMORY_NVS:              return "NVS";
    case MULTIBOOT_MEMORY_BADRAM:           return "BAD_RAM";
    default:                                return "unknown";
    }
    // clang-format on
}

/**
 * Read Multiboot2 tags and copy relevant info
 *
 * @see
 *
 * - For details of available tags and their layouts,
 *   see the Multiboot2 Specification:
 *   [Boot Information Format](https://www.gnu.org/software/grub/manual/multiboot2/html_node/Boot-information-format.html)
 *
 * - For predefined constants and structs, see the header @ref multiboot2.h
 */
int read_boot_info(struct boot_info *b)
{
    int res;

    /* Check magic number. */
    res = (mb2_magic == MULTIBOOT2_BOOTLOADER_MAGIC) ? 0 : -EINVAL;
    log_result(res, "got mb2 magic number %#" PRIx32 "\n", mb2_magic);
    if (res < 0) return 0;

    pr_info("reading Multiboo2 boot info...\n");

    /* Parse tags. */
    const char *pos = (char *) (mb2_boot_info + 1);
    const char *end = (char *) mb2_boot_info + mb2_boot_info->total_size;

    while (pos < end) {
        struct multiboot_tag *tag = (void *) pos;
        switch (tag->type) {
        case MULTIBOOT_TAG_TYPE_END: pr_info("tag: end\n"); break;

        case MULTIBOOT_TAG_TYPE_CMDLINE: {
            struct multiboot_tag_string *strtag = (void *) tag;
            pr_info("tag: cmdline = \"%s\"\n", strtag->string);
            break;
        }

        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME: {
            struct multiboot_tag_string *strtag = (void *) tag;
            pr_info("tag: bootloader name = \"%s\"\n", strtag->string);
            break;
        }

        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: {
            struct multiboot_tag_basic_meminfo *meminfo = (void *) tag;
            pr_info("tag: mem info: lower=%uk, upper=%uk\n",
                    meminfo->mem_lower, meminfo->mem_upper);
            break;
        }

        case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR: {
            struct multiboot_tag_load_base_addr *addr = (void *) tag;
            pr_info("tag: load base addr = %p\n",
                    (void *) (uintptr_t) addr->load_base_addr);
            b->kernel_location = (void *) (uintptr_t) addr->load_base_addr;
            break;
        }

        case MULTIBOOT_TAG_TYPE_MMAP: {
            struct multiboot_tag_mmap *mmap = (void *) tag;
            pr_info("tag: memory map: entry version %u (entry size %ub)\n",
                    mmap->entry_version, mmap->entry_size);

            /* Read each entry */
            size_t entryct = (mmap->size - sizeof(struct multiboot_tag_mmap))
                             / mmap->entry_size;
            for (size_t i = 0; i < entryct; i++) {
                struct multiboot_mmap_entry *e = &mmap->entries[i];
                pr_info("\tentry: %#10llx: %#10llx bytes type %u %s\n",
                        e->addr, e->len, e->type, mmap_typestr(e->type));
            }
            break;
        }

        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
            struct multiboot_tag_framebuffer *fb = (void *) tag;
            pr_info("tag: framebuffer: "
                    "addr=%#llx, pitch=%u, width=%u, height=%u, bpp=%u, "
                    "type=%u\n",
                    fb->common.framebuffer_addr, fb->common.framebuffer_pitch,
                    fb->common.framebuffer_width,
                    fb->common.framebuffer_height, fb->common.framebuffer_bpp,
                    fb->common.framebuffer_type);

            if (fb->common.framebuffer_type
                == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT) {
                b->text_fb_addr =
                        (void *) (uintptr_t) fb->common.framebuffer_addr;
                b->text_fb_width  = fb->common.framebuffer_width;
                b->text_fb_height = fb->common.framebuffer_height;
            }
            break;
        }

        case MULTIBOOT_TAG_TYPE_MODULE: {
            struct multiboot_tag_module *mod = (void *) tag;
            pr_info("tag: module: start=%#x, end=%#x, cmdline=\"%s\"\n",
                    mod->mod_start, mod->mod_end, (char *) mod + 1);
            b->initrd_addr = (void *) (uintptr_t) mod->mod_start;
            b->initrd_size = mod->mod_end - mod->mod_start;
            break;
        }

        default:
            pr_info("tag: type %2u (size %4u)\n", tag->type, tag->size);
            break;
        };
        pos += ALIGN_UP(tag->size, MULTIBOOT_TAG_ALIGN);
    };

    return 0;
}
