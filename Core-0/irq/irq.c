/*
 * HIK Core-0 Interrupt Routing Implementation
 * 
 * This file implements the interrupt routing table and handlers.
 * The table is configured at build time to ensure deterministic
 * execution control.
 */

#include "../include/irq.h"
#include "../include/capability.h"
#include "../include/string.h"

/* Global interrupt routing table */
static irq_route_table_t g_irq_table;

/* Spinlock operations */
static inline void spin_lock(uint32_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        /* Spin */
    }
}

static inline void spin_unlock(uint32_t *lock) {
    __sync_lock_release(lock);
}

/*
 * Initialize interrupt routing table
 */
int irq_init(void) {
    memset(&g_irq_table, 0, sizeof(irq_route_table_t));
    g_irq_table.num_entries = MAX_IRQ_VECTORS;
    g_irq_table.lock = 0;
    
    /* Configure exception handlers (Core-0 internal) */
    for (int i = 0; i < 32; i++) {
        g_irq_table.entries[i].handler_address = (uint64_t)irq_handler;
        g_irq_table.entries[i].type = IRQ_HANDLER_CORE0;
        g_irq_table.entries[i].capability_id = 0;
        g_irq_table.entries[i].flags = IRQ_FLAG_ENABLED;
    }
    
    /* Configure standard IRQs (32-47) */
    for (int i = 32; i < 48; i++) {
        g_irq_table.entries[i].handler_address = (uint64_t)irq_handler;
        g_irq_table.entries[i].type = IRQ_HANDLER_CORE0;
        g_irq_table.entries[i].capability_id = 0;
        g_irq_table.entries[i].flags = IRQ_FLAG_ENABLED;
    }
    
    /* Mark remaining vectors as disabled */
    for (int i = 48; i < MAX_IRQ_VECTORS; i++) {
        g_irq_table.entries[i].flags = IRQ_FLAG_MASKED;
    }
    
    return 0;
}

/*
 * Route interrupt to handler
 */
int irq_route(uint8_t vector, uint64_t handler, irq_handler_type_t type, uint64_t cap_id) {
    spin_lock(&g_irq_table.lock);
    
    g_irq_table.entries[vector].handler_address = handler;
    g_irq_table.entries[vector].type = type;
    g_irq_table.entries[vector].capability_id = cap_id;
    
    spin_unlock(&g_irq_table.lock);
    
    return 0;
}

/*
 * Enable interrupt
 */
int irq_enable(uint8_t vector) {
    spin_lock(&g_irq_table.lock);
    g_irq_table.entries[vector].flags |= IRQ_FLAG_ENABLED;
    g_irq_table.entries[vector].flags &= ~IRQ_FLAG_MASKED;
    spin_unlock(&g_irq_table.lock);
    
    return 0;
}

/*
 * Disable interrupt
 */
int irq_disable(uint8_t vector) {
    spin_lock(&g_irq_table.lock);
    g_irq_table.entries[vector].flags |= IRQ_FLAG_MASKED;
    g_irq_table.entries[vector].flags &= ~IRQ_FLAG_ENABLED;
    spin_unlock(&g_irq_table.lock);
    
    return 0;
}

/*
 * Handle interrupt (called from assembly)
 * This is the main interrupt dispatcher that uses the routing table
 */
void irq_handler(uint64_t vector, uint64_t error_code) {
    if (vector >= MAX_IRQ_VECTORS) {
        return;
    }
    
    irq_route_entry_t *entry = &g_irq_table.entries[vector];
    
    /* Check if interrupt is enabled */
    if (!(entry->flags & IRQ_FLAG_ENABLED)) {
        return;
    }
    
    /* Check if interrupt is masked */
    if (entry->flags & IRQ_FLAG_MASKED) {
        return;
    }
    
    /* Based on handler type, dispatch appropriately */
    switch (entry->type) {
        case IRQ_HANDLER_CORE0:
            /* Core-0 internal handler - direct call */
            /* In real implementation, this would call specific handler */
            break;
            
        case IRQ_HANDLER_SERVICE:
            /* Service handler - verify capability first */
            if (cap_check(entry->capability_id, CAP_TYPE_IRQ, CAP_PERM_READ) == 0) {
                /* Valid capability, dispatch to service */
                /* In real implementation, this would switch to service context */
            }
            break;
            
        case IRQ_HANDLER_APPLICATION:
            /* Application handler - verify capability first */
            if (cap_check(entry->capability_id, CAP_TYPE_IRQ, CAP_PERM_READ) == 0) {
                /* Valid capability, dispatch to application */
                /* In real implementation, this would switch to application context */
            }
            break;
    }
}

/*
 * Get interrupt routing table
 */
irq_route_table_t* irq_get_table(void) {
    return &g_irq_table;
}