/*
 * HIK Core-0 Isolation Enforcement
 * 
 * This file defines the isolation enforcement mechanisms including
 * page table configuration and call gates for service isolation.
 */

#ifndef HIK_CORE0_ISOLATION_H
#define HIK_CORE0_ISOLATION_H

#include <stdint.h>

/* Page table entry flags */
#define PT_FLAG_PRESENT     0x01
#define PT_FLAG_WRITABLE    0x02
#define PT_FLAG_USER        0x04
#define PT_FLAG_PWT         0x08
#define PT_FLAG_PCD         0x10
#define PT_FLAG_ACCESSED    0x20
#define PT_FLAG_DIRTY       0x40
#define PT_FLAG_PS          0x80
#define PT_FLAG_GLOBAL      0x100
#define PT_FLAG_NX          0x8000000000000000ULL

/* Page table structure */
typedef struct {
    uint64_t entries[512];
} __attribute__((aligned(4096))) page_table_t;

/* Domain page table configuration */
typedef struct {
    page_table_t *pml4;           /* PML4 table */
    uint64_t domain_id;           /* Domain ID */
    uint64_t capabilities;        /* Capabilities for this domain */
    uint32_t flags;               /* Domain flags */
} domain_page_table_t;

/* Domain flags */
#define DOMAIN_FLAG_KERNEL    0x01  /* Kernel domain */
#define DOMAIN_FLAG_SERVICE   0x02  /* Service domain */
#define DOMAIN_FLAG_APP       0x04  /* Application domain */

/* Call gate descriptor (x86-64 call gate mechanism) */
typedef struct {
    uint64_t offset_low : 16;     /* Offset bits 0-15 */
    uint64_t selector : 16;       /* Code segment selector */
    uint64_t ist : 3;             /* Interrupt stack table */
    uint64_t type : 5;            /* Descriptor type */
    uint64_t dpl : 2;             /* Descriptor privilege level */
    uint64_t present : 1;         /* Present flag */
    uint64_t offset_high : 48;    /* Offset bits 16-63 */
} __attribute__((packed)) call_gate_t;

/* Call gate types */
#define CALL_GATE_TYPE_AVAILABLE 0xC  /* Available call gate */

/* Maximum number of call gates */
#define MAX_CALL_GATES 16

/* Call gate table */
typedef struct {
    call_gate_t gates[MAX_CALL_GATES];
    uint32_t num_gates;
    uint32_t lock;
} call_gate_table_t;

/* Memory mapping types */
typedef enum {
    MAP_TYPE_CODE = 0,        /* Executable code */
    MAP_TYPE_DATA = 1,        /* Read/write data */
    MAP_TYPE_READONLY = 2,    /* Read-only data */
    MAP_TYPE_DEVICE = 3,      /* Device MMIO */
    MAP_TYPE_SHARED = 4       /* Shared memory */
} map_type_t;

/* Initialize isolation system */
int isolation_init(void);

/* Create page tables for a domain */
int isolation_create_page_tables(uint64_t domain_id, uint32_t flags);

/* Destroy page tables for a domain */
int isolation_destroy_page_tables(uint64_t domain_id);

/* Map memory into domain's address space */
int isolation_map_memory(uint64_t domain_id, uint64_t virt_addr, uint64_t phys_addr,
                         uint64_t size, map_type_t map_type, uint64_t cap_id);

/* Unmap memory from domain's address space */
int isolation_unmap_memory(uint64_t domain_id, uint64_t virt_addr, uint64_t size);

/* Verify domain has access to memory */
int isolation_verify_access(uint64_t domain_id, uint64_t addr, uint64_t size, uint32_t access);

/* Create call gate for service-to-service communication */
int isolation_create_call_gate(uint64_t target_domain, uint64_t entry_point, uint64_t cap_id);

/* Call through call gate */
int isolation_call_gate(uint64_t gate_id, uint64_t *args);

/* Get domain page tables */
domain_page_table_t* isolation_get_page_tables(uint64_t domain_id);

/* Get call gate table */
call_gate_table_t* isolation_get_call_gates(void);

#endif /* HIK_CORE0_ISOLATION_H */