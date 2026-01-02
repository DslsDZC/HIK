/*
 * HIK Core-0 Scheduler Implementation
 */

#include "../include/sched.h"
#include "../include/mm.h"
#include <string.h>

/* Global scheduler state */
static sched_state_t g_sched_state;

/* Spinlock operations */
static inline void spin_lock(uint64_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        /* Spin */
    }
}

static inline void spin_unlock(uint64_t *lock) {
    __sync_lock_release(lock);
}

/*
 * Initialize scheduler
 */
int sched_init(void) {
    memset(&g_sched_state, 0, sizeof(sched_state_t));
    
    g_sched_state.num_threads = 0;
    g_sched_state.current_thread = 0;
    g_sched_state.next_thread_id = 1;
    g_sched_state.timer_ticks = 0;
    g_sched_state.lock = 0;
    
    /* Create idle thread */
    sched_create_thread(0, sched_idle_thread, NULL, THREAD_PRIORITY_IDLE);
    
    return 0;
}

/*
 * Create a new thread
 */
uint64_t sched_create_thread(uint64_t domain_id, void (*entry_point)(void*),
                             void *arg, thread_priority_t priority) {
    spin_lock(&g_sched_state.lock);
    
    /* Find free thread slot */
    uint32_t slot = 0;
    for (slot = 0; slot < MAX_THREADS; slot++) {
        if (g_sched_state.threads[slot].state == THREAD_STATE_TERMINATED ||
            g_sched_state.threads[slot].state == 0) {
            break;
        }
    }
    
    if (slot >= MAX_THREADS) {
        spin_unlock(&g_sched_state.lock);
        return 0;  /* No free slots */
    }
    
    /* Allocate stack */
    uint64_t stack_base = mm_alloc(STACK_SIZE, 16, MEM_TYPE_KERNEL, domain_id);
    if (stack_base == 0) {
        spin_unlock(&g_sched_state.lock);
        return 0;
    }
    
    /* Initialize TCB */
    tcb_t *tcb = &g_sched_state.threads[slot];
    tcb->thread_id = g_sched_state.next_thread_id++;
    tcb->domain_id = domain_id;
    tcb->state = THREAD_STATE_READY;
    tcb->priority = priority;
    tcb->stack_base = stack_base;
    tcb->stack_size = STACK_SIZE;
    tcb->stack_ptr = stack_base + STACK_SIZE;  /* Stack grows down */
    tcb->entry_point = entry_point;
    tcb->arg = arg;
    tcb->time_slice = 10;  /* Default time slice */
    tcb->total_time = 0;
    tcb->flags = 0;
    
    g_sched_state.num_threads++;
    
    spin_unlock(&g_sched_state.lock);
    
    return tcb->thread_id;
}

/*
 * Terminate a thread
 */
int sched_terminate_thread(uint64_t thread_id) {
    spin_lock(&g_sched_state.lock);
    
    for (uint32_t i = 0; i < MAX_THREADS; i++) {
        tcb_t *tcb = &g_sched_state.threads[i];
        
        if (tcb->thread_id == thread_id) {
            tcb->state = THREAD_STATE_TERMINATED;
            
            /* Free stack */
            mm_free(tcb->stack_base);
            
            g_sched_state.num_threads--;
            
            spin_unlock(&g_sched_state.lock);
            
            return 0;
        }
    }
    
    spin_unlock(&g_sched_state.lock);
    
    return -1;  /* Thread not found */
}

/*
 * Yield CPU to next thread
 */
void sched_yield(void) {
    /* In real implementation, would trigger context switch */
    /* For now, just return */
}

/*
 * Sleep for specified milliseconds
 */
void sched_sleep(uint64_t milliseconds) {
    /* In real implementation, would block thread and set wake time */
    /* For now, just busy wait */
    volatile uint64_t count = 0;
    for (uint64_t i = 0; i < milliseconds * 1000; i++) {
        for (uint64_t j = 0; j < 1000; j++) {
            count++;
        }
    }
}

/*
 * Block current thread
 */
int sched_block(void) {
    spin_lock(&g_sched_state.lock);
    
    tcb_t *current = sched_get_current();
    if (current) {
        current->state = THREAD_STATE_BLOCKED;
    }
    
    spin_unlock(&g_sched_state.lock);
    
    return 0;
}

/*
 * Unblock a thread
 */
int sched_unblock(uint64_t thread_id) {
    spin_lock(&g_sched_state.lock);
    
    for (uint32_t i = 0; i < MAX_THREADS; i++) {
        tcb_t *tcb = &g_sched_state.threads[i];
        
        if (tcb->thread_id == thread_id && tcb->state == THREAD_STATE_BLOCKED) {
            tcb->state = THREAD_STATE_READY;
            
            spin_unlock(&g_sched_state.lock);
            
            return 0;
        }
    }
    
    spin_unlock(&g_sched_state.lock);
    
    return -1;  /* Thread not found */
}

/*
 * Get current thread
 */
tcb_t* sched_get_current(void) {
    if (g_sched_state.current_thread < MAX_THREADS) {
        return &g_sched_state.threads[g_sched_state.current_thread];
    }
    return NULL;
}

/*
 * Schedule next thread (called by timer interrupt)
 */
void sched_schedule(void) {
    spin_lock(&g_sched_state.lock);
    
    g_sched_state.timer_ticks++;
    
    /* Decrement current thread's time slice */
    tcb_t *current = sched_get_current();
    if (current && current->time_slice > 0) {
        current->time_slice--;
    }
    
    /* Find next ready thread */
    uint32_t next_thread = 0;
    int found = 0;
    
    /* Simple round-robin scheduling */
    for (uint32_t i = 1; i <= g_sched_state.num_threads; i++) {
        uint32_t idx = (g_sched_state.current_thread + i) % MAX_THREADS;
        tcb_t *tcb = &g_sched_state.threads[idx];
        
        if (tcb->state == THREAD_STATE_READY) {
            next_thread = idx;
            found = 1;
            break;
        }
    }
    
    if (found && next_thread != g_sched_state.current_thread) {
        /* Context switch would happen here */
        g_sched_state.current_thread = next_thread;
        
        if (current) {
            current->state = THREAD_STATE_READY;
        }
        
        tcb_t *next = &g_sched_state.threads[next_thread];
        next->state = THREAD_STATE_RUNNING;
        next->time_slice = 10;  /* Reset time slice */
    }
    
    spin_unlock(&g_sched_state.lock);
}

/*
 * Timer interrupt handler
 */
void sched_timer_interrupt(void) {
    sched_schedule();
}

/*
 * Idle thread
 */
void sched_idle_thread(void *arg) {
    while (1) {
        /* Halt until next interrupt */
        __asm__ volatile("hlt");
    }
}

/*
 * Dump scheduler state (for debugging)
 */
void sched_dump(void) {
    /* Would print to console in real implementation */
    /* Simplified for now */
}