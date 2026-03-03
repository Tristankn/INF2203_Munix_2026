#include "pagemap.h"

#include <abi.h>
#include <cpu.h>
#include <cpu_pagemap.h>

#include <drivers/log.h>

#include <core/errno.h>
#include <core/macros.h>
#include <core/sprintf.h>
#include <core/string.h>

#define PHYSPAGES_MAX 5

struct addrspc kernel_addrspc;

static struct physpage physpages[PHYSPAGES_MAX];

struct physpage *physpage_alloc(void)
{
    for (unsigned i = 0; i < PHYSPAGES_MAX; i++) {
        if (!physpages[i].paddr) {
            physpages[i].paddr = PM_AREA + i * PAGESZ;
            return &physpages[i];
        }
    }
    return NULL;
}

void physpage_free(struct physpage *page) { page->paddr = 0; }

struct physpage *physpage_find(paddr_t paddr)
{
    int i = (paddr - PM_AREA) / PAGESZ;
    if (0 <= i && i < PHYSPAGES_MAX) return &physpages[i];
    else return NULL;
}

void *physpage_access(struct physpage *page)
{
    return page->vaddr = (void *) page->paddr;
}

void physpage_close(struct physpage *page) { page->vaddr = 0; }

static void pm_offsets(uintptr_t vaddr_val, size_t offsets[PM_LVL_MAX])
{
    for (int i = pm_mode->lvlct - 1; i >= 0; i--) {
        unsigned  bits = pm_mode->lvls[i].idx_bits;
        uintptr_t mask = (1 << bits) - 1;
        offsets[i]     = vaddr_val & mask;
        vaddr_val >>= bits;
    }
}

static size_t pme_coversz(int lvl)
{
    if (pm_mode->lvls[lvl].is_page) return 1 << pm_mode->lvls[lvl].idx_bits;
    else return pme_coversz(lvl + 1) << pm_mode->lvls[lvl].idx_bits;
}

static void pme_map_trace(pme_t *entry, int lvl)
{
    const size_t DBGSZ = 80;
    char         dbgbuf[DBGSZ];

    const char *lvlname = pm_mode->lvls[lvl].name;
    pme_t *tbl_start = (void *) ALIGN_DOWN((uintptr_t) entry, pm_tblsz(lvl));
    size_t offset    = entry - tbl_start;
    pr_trace(
            "\t%-5s:%8p[%4zu] = " FMT_PME " (%s)\n", lvlname, tbl_start,
            offset, *entry, (pme_tostr(dbgbuf, DBGSZ, *entry, lvl), dbgbuf)
    );
}

static int addrspc_map_recursive(
        int      lvl,
        pme_t   *entry,
        size_t   offsets[PM_LVL_MAX],
        paddr_t *paddr,
        size_t  *size,
        pme_t    flags
)
{
    /* If this entry points to a page, simply map the page. */
    if (pm_mode->lvls[lvl + 1].is_page) {
        *entry = pme_pack(*paddr, flags | PME_PRESENT, lvl);
        pme_map_trace(entry, lvl);
        *paddr += PAGESZ, *size -= PAGESZ;
        return 0;
    }

    /* This entry points to another table, so we need to recurse... */
    int              res    = 0;
    struct physpage *tbl_pg = NULL;
    pme_t           *tbl    = NULL;

    /* Do we need to allocate a page for the next table? */
    if (!pme_ispresent(*entry, lvl)) {
        /* Table is not present: allocate a new page for the table. */
        tbl_pg = physpage_alloc();
        if (!tbl_pg) return -ENOMEM;
        tbl = physpage_access(tbl_pg);
        memset(tbl, 0, PAGESZ);
        pr_debug(
                "\tallocated page " FMT_PADDR " for %s\n", tbl_pg->paddr,
                pm_mode->lvls[lvl + 1].name
        );
        *entry = pme_pack(tbl_pg->paddr, flags | PME_PRESENT, lvl);

    } else {
        /* Table is present: find and open the existing table. */
        tbl_pg = physpage_find(pme_paddr(*entry, lvl));
        if (!tbl_pg) return -ENOMEM;
        tbl    = physpage_access(tbl_pg);
        *entry = pme_set_flags(*entry, flags, lvl);
    }
    pme_map_trace(entry, lvl);

    /* Loop through mappings on table. */
    int    child_lvl  = lvl + 1;
    size_t offset_max = 1 << pm_mode->lvls[child_lvl].idx_bits;

    while (*size) {
        pme_t *child_entry = tbl + offsets[child_lvl];

        res = addrspc_map_recursive(
                child_lvl, child_entry, offsets, paddr, size, flags
        );
        if (res < 0) goto exit;

        offsets[child_lvl]++;
        if (offsets[child_lvl] == offset_max) {
            offsets[child_lvl] = 0;
            break;
        }
    }

    res = 0;
exit:
    if (tbl) physpage_close(tbl_pg);
    return res;
}

int addrspc_unmap_recursive(
        int lvl, pme_t *entry, size_t offsets[PM_LVL_MAX], size_t *size
)
{
    /* If this entry is not present, there is nothing to do. */
    if (!pme_ispresent(*entry, lvl)) {
        *size -= MIN(pme_coversz(lvl), *size);
        return 0;
    }

    /* If this entry points to a page, mark it as not present. */
    if (pm_mode->lvls[lvl + 1].is_page) {
        *entry &= ~PME_PRESENT;
        pme_map_trace(entry, lvl);
        *size -= PAGESZ;
        return 0;
    }

    /* This entry points to another table, so we need to recurse... */
    int              res       = 0;
    int              tbl_empty = 0;
    struct physpage *tbl_pg    = NULL;
    pme_t           *tbl       = NULL;

    /* Find table for this level. */
    tbl_pg = physpage_find(pme_paddr(*entry, lvl));
    if (!tbl_pg) return -ENOMEM;
    tbl = physpage_access(tbl_pg);
    if (!tbl) return -ENOMEM;

    /* Loop through mappings on table. */
    int    child_lvl  = lvl + 1;
    size_t offset_max = 1 << pm_mode->lvls[child_lvl].idx_bits;
    while (*size) {
        pme_t *child_entry = tbl + offsets[child_lvl];
        res = addrspc_unmap_recursive(child_lvl, child_entry, offsets, size);
        if (res < 0) goto exit;

        offsets[child_lvl]++;
        if (offsets[child_lvl] == offset_max) {
            offsets[child_lvl] = 0;
            break;
        }
    }

    /* Check if the table is empty so we can free it. */
    tbl_empty = 1;
    for (size_t i = 0; i < offset_max; i++)
        if (pme_ispresent(tbl[i], child_lvl)) tbl_empty = 0;

    res = 0;
exit:
    if (tbl) physpage_close(tbl_pg);
    if (tbl_pg && tbl_empty) {
        *entry &= ~PME_PRESENT;
        pr_debug(
                "\tfreeing %s " FMT_PADDR "\n", pm_mode->lvls[child_lvl].name,
                tbl_pg->paddr
        );
        physpage_free(tbl_pg);
    }
    return res;
}

int addrspc_map(
        struct addrspc *space,
        void           *vaddr,
        paddr_t         paddr,
        size_t          size,
        pme_t           flags
)
{
    const size_t DBGSZ = 80;
    char         dbgbuf[DBGSZ];

    uintptr_t vaddr_val = ALIGN_DOWN((uintptr_t) vaddr, PAGESZ);
    paddr               = ALIGN_DOWN(paddr, PAGESZ);
    size                = ALIGN_UP(size, PAGESZ);

    size_t offsets[PM_LVL_MAX];
    pm_offsets(vaddr_val, offsets);
    pr_debug(
            "space %8p mapping v%8p -> p" FMT_PADDR " size %#8zx, flags %s\n",
            space, vaddr, paddr, size,
            (pme_tostr(dbgbuf, DBGSZ, flags, 3), dbgbuf)
    );
    return addrspc_map_recursive(
            0, &space->root_entry, offsets, &paddr, &size, flags
    );
}

int addrspc_unmap(struct addrspc *space, void *vaddr, size_t size)
{
    uintptr_t vaddr_val = ALIGN_DOWN((uintptr_t) vaddr, PAGESZ);
    if (size < SIZE_MAX - PAGESZ) size = ALIGN_UP(size, PAGESZ);

    pr_debug("space %8p unmapping v%8p size %#8zx\n", space, vaddr, size);
    size_t offsets[PM_LVL_MAX];
    pm_offsets(vaddr_val, offsets);
    return addrspc_unmap_recursive(0, &space->root_entry, offsets, &size);
}

int addrspc_init(struct addrspc *space)
{
    /* Set up kernel mapping. */
    return addrspc_map(space, KMAP_MIN, KMAP_MIN, KMAP_MAX - KMAP_MIN, 0);
}

int addrspc_cleanup(struct addrspc *space)
{
    return addrspc_unmap(space, 0, SIZE_MAX);
}

int init_pm(void)
{
    int res;

    res = addrspc_init(&kernel_addrspc);
    log_result(res, "create a page map for kernel\n");
    if (res < 0) return res;

    res = init_cpu_pm(kernel_addrspc.root_entry);
    log_result(res, "initialize page mapping in CPU\n");
    if (res < 0) return res;

    return res;
}

