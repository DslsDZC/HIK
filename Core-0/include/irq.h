/*
 * HIK Core-0 Interrupt Routing Table
 * 
 * This file defines the interrupt routing table that maps interrupts
 * to Core-0 handlers. This table is configured at build time to ensure
 * execution control and deterministic interrupt handling.
 */

#ifndef HIK_CORE0_IRQ_H
#define HIK_CORE0_IRQ_H

#include "stdint.h"

/* Maximum number of interrupt vectors */
#define MAX_IRQ_VECTORS 256

/* Interrupt handler types */
typedef enum {
    IRQ_HANDLER_CORE0 = 0,      /* Core-0 internal handler */
    IRQ_HANDLER_SERVICE = 1,    /* Service handler (via capability) */
    IRQ_HANDLER_APPLICATION = 2 /* Application handler (via capability) */
} irq_handler_type_t;

/* Interrupt routing entry (configured at build time) */
typedef struct {
    uint64_t handler_address;    /* Handler address */
    irq_handler_type_t type;     /* Handler type */
    uint64_t capability_id;      /* Required capability for service/app handlers */
    uint32_t flags;              /* Interrupt flags */
    uint32_t reserved;           /* Reserved for future use */
} __attribute__((packed)) irq_route_entry_t;

/* Interrupt routing table (global, build-time configured) */
typedef struct {
    irq_route_entry_t entries[MAX_IRQ_VECTORS];
    uint32_t num_entries;
    uint32_t lock;
} irq_route_table_t;

/* Interrupt flags */
#define IRQ_FLAG_ENABLED     0x01
#define IRQ_FLAG_MASKED      0x02
#define IRQ_FLAG_EDGE        0x04
#define IRQ_FLAG_LEVEL       0x08

/* Standard interrupt vectors */
#define IRQ_VECTOR_DIVIDE_ERROR    0
#define IRQ_VECTOR_DEBUG           1
#define IRQ_VECTOR_NMI             2
#define IRQ_VECTOR_BREAKPOINT      3
#define IRQ_VECTOR_OVERFLOW        4
#define IRQ_VECTOR_BOUND_RANGE     5
#define IRQ_VECTOR_INVALID_OPCODE  6
#define IRQ_VECTOR_DEVICE_NOT_AVAIL 7
#define IRQ_VECTOR_DOUBLE_FAULT    8
#define IRQ_VECTOR_INVALID_TSS     10
#define IRQ_VECTOR_SEGMENT_NOT_PRESENT 11
#define IRQ_VECTOR_STACK_FAULT     12
#define IRQ_VECTOR_GENERAL_PROTECTION 13
#define IRQ_VECTOR_PAGE_FAULT      14
#define IRQ_VECTOR_X87_FPU_ERROR   16
#define IRQ_VECTOR_ALIGNMENT_CHECK 17
#define IRQ_VECTOR_MACHINE_CHECK   18
#define IRQ_VECTOR_SIMD_FP         19

/* IRQ vectors (32-47 are standard IRQs) */
#define IRQ_VECTOR_TIMER           32
#define IRQ_VECTOR_KEYBOARD        33
#define IRQ_VECTOR_CASCADE         34
#define IRQ_VECTOR_COM2            35
#define IRQ_VECTOR_COM1            36
#define IRQ_VECTOR_LPT2            37
#define IRQ_VECTOR_FLOPPY          38
#define IRQ_VECTOR_LPT1            39
#define IRQ_VECTOR_RTC             40
#define IRQ_VECTOR_MOUSE           46
#define IRQ_VECTOR_FPU             47

/* Initialize interrupt routing table */
int irq_init(void);

/* Route interrupt to handler */
int irq_route(uint8_t vector, uint64_t handler, irq_handler_type_t type, uint64_t cap_id);

/* Enable interrupt */
int irq_enable(uint8_t vector);

/* Disable interrupt */
int irq_disable(uint8_t vector);

/* Handle interrupt (called from assembly) */
void irq_handler(uint64_t vector, uint64_t error_code);

/* Get interrupt routing table */
irq_route_table_t* irq_get_table(void);

#endif /* HIK_CORE0_IRQ_H */