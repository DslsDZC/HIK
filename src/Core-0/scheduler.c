/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC Minimal Scheduler — Mechanism only, no policy.
 *
 * Core-0 provides context-switch mechanism and idle thread.
 * Scheduling POLICY is delegated entirely to Privileged-1 layer
 * via exec_flow_dispatch() in exec_flow.c.
 */

#include "thread.h"
#include "types.h"
#include "atomic.h"
#include "hal.h"
#include "lib/mem.h"
#include "lib/console.h"

/* context_switch in arch/x86_64/context.S */
extern void context_switch(void *prev, void *next);

/* Current running thread (NULL during early boot) */
thread_t *g_current_thread = NULL;

/* ==================== Idle Thread ==================== */

thread_t idle_thread;
static u64 g_idle_stack[1024] __attribute__((aligned(16)));

void scheduler_init(void)
{
    console_puts("[SCHED] Initializing minimal scheduler (mechanism only)\n");

    g_current_thread = NULL;

    memzero(&idle_thread, sizeof(thread_t));
    idle_thread.thread_id = 0xFFFFFFFF;
    idle_thread.state = THREAD_STATE_READY;
    idle_thread.priority = HIC_PRIORITY_IDLE;
    idle_thread.logical_core_id = INVALID_LOGICAL_CORE;

    /* Set up idle thread stack */
    idle_thread.stack_base = (virt_addr_t)g_idle_stack;
    idle_thread.stack_size = sizeof(g_idle_stack);
    u64 *stack_top = &g_idle_stack[1024];
    stack_top--;
    *stack_top = (u64)hal_halt;    /* idle entry = hal_halt */
    stack_top -= 6;                 /* callee-saved regs */
    idle_thread.stack_ptr = (virt_addr_t)stack_top;

    console_puts("[SCHED] Minimal scheduler initialized\n");
}

#include "exec_flow.h"

/* Boot: dispatch all ready EFCs sequentially */
void hic_boot_dispatch_all(void)
{
    for (u32 i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &g_threads[i];
        if (t->thread_id != i || t->state != THREAD_STATE_READY || t->stack_ptr == 0)
            continue;
        exec_flow_id_t efc = exec_flow_id_for_thread(i);
        if (efc == EXEC_FLOW_INVALID)
            continue;
        exec_flow_dispatch(efc, 0);
    }
}
