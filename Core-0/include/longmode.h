/*
 * HIK Core-0 Long Mode Support
 * 
 * This file defines the long mode (64-bit) support for x86-64.
 */

#ifndef HIK_CORE0_LONGMODE_H
#define HIK_CORE0_LONGMODE_H

#include "stdint.h"

/* Page table entries */
typedef struct {
    uint64_t present:1;
    uint64_t writable:1;
    uint64_t user:1;
    uint64_t pwt:1;
    uint64_t pcd:1;
    uint64_t accessed:1;
    uint64_t dirty:1;
    uint64_t pat:1;
    uint64_t global:1;
    uint64_t available:3;
    uint64_t frame:40;
    uint64_t reserved:11;
    uint64_t nx:1;
} __attribute__((packed)) pml4e_t;

typedef struct {
    uint64_t present:1;
    uint64_t writable:1;
    uint64_t user:1;
    uint64_t pwt:1;
    uint64_t pcd:1;
    uint64_t accessed:1;
    uint64_t dirty:1;
    uint64_t pat:1;
    uint64_t global:1;
    uint64_t available:3;
    uint64_t frame:40;
    uint64_t reserved:11;
    uint64_t nx:1;
} __attribute__((packed)) pdpe_t;

typedef struct {
    uint64_t present:1;
    uint64_t writable:1;
    uint64_t user:1;
    uint64_t pwt:1;
    uint64_t pcd:1;
    uint64_t accessed:1;
    uint64_t dirty:1;
    uint64_t pat:1;
    uint64_t global:1;
    uint64_t available:3;
    uint64_t frame:40;
    uint64_t reserved:11;
    uint64_t nx:1;
} __attribute__((packed)) pde_t;

typedef struct {
    uint64_t present:1;
    uint64_t writable:1;
    uint64_t user:1;
    uint64_t pwt:1;
    uint64_t pcd:1;
    uint64_t accessed:1;
    uint64_t dirty:1;
    uint64_t pat:1;
    uint64_t global:1;
    uint64_t available:3;
    uint64_t frame:40;
    uint64_t reserved:11;
    uint64_t nx:1;
} __attribute__((packed)) pte_t;

/* GDT entry for 64-bit mode */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

/* GDT pointer */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

/* Page table addresses */
#define PML4_BASE  0x10000
#define PDP_BASE   0x11000
#define PD_BASE    0x12000
#define PT_BASE    0x13000

/* Initialize long mode */
int longmode_init(void);

/* Enable PAE */
void longmode_enable_pae(void);

/* Setup page tables */
void longmode_setup_page_tables(void);

/* Enable long mode */
void longmode_enable(void);

/* Jump to 64-bit code */
void longmode_jump(uint64_t entry_point, uint64_t boot_info);

/* Check if CPU supports long mode */
int longmode_check_support(void);

#endif /* HIK_CORE0_LONGMODE_H */