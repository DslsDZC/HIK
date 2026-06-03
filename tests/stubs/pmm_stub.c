#include "pmm.h"
#include <string.h>

#define PMM_STUB_POOL_PAGES 1024
#define PMM_STUB_BASE_ADDR  0x10000000

/* Simple bitmap: 1 = allocated */
static u8 s_pmm_bitmap[PMM_STUB_POOL_PAGES / 8];
static u32 s_alloc_count = 0;

hic_status_t pmm_alloc_frames(domain_id_t owner, u32 count,
                              page_frame_type_t type, phys_addr_t *out) {
    (void)owner;
    (void)type;
    u32 found = 0;
    u32 start = 0;

    for (u32 i = 0; i < PMM_STUB_POOL_PAGES; i++) {
        u32 byte = i / 8;
        u8  bit  = 1U << (i % 8);
        if (!(s_pmm_bitmap[byte] & bit)) {
            if (found == 0) start = i;
            found++;
            if (found == count) {
                /* Mark allocated */
                for (u32 j = start; j < start + count; j++) {
                    s_pmm_bitmap[j / 8] |= (1U << (j % 8));
                }
                s_alloc_count += count;
                *out = PMM_STUB_BASE_ADDR + start * PAGE_SIZE;
                return HIC_SUCCESS;
            }
        } else {
            found = 0;
        }
    }
    return HIC_ERROR_NO_MEMORY;
}

hic_status_t pmm_free_frames(phys_addr_t addr, u32 count) {
    u32 start = (u32)((addr - PMM_STUB_BASE_ADDR) / PAGE_SIZE);
    if (start + count > PMM_STUB_POOL_PAGES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    for (u32 i = start; i < start + count; i++) {
        s_pmm_bitmap[i / 8] &= ~(1U << (i % 8));
    }
    s_alloc_count -= count;
    return HIC_SUCCESS;
}

void pmm_init_with_range(phys_addr_t max_phys_addr) { (void)max_phys_addr; }

hic_status_t pmm_add_region(phys_addr_t base, size_t size) {
    (void)base; (void)size;
    return HIC_SUCCESS;
}

/* Test helpers */
void test_pmm_reset(void) {
    memset(s_pmm_bitmap, 0, sizeof(s_pmm_bitmap));
    s_alloc_count = 0;
}

u32 test_pmm_alloc_count(void) { return s_alloc_count; }
