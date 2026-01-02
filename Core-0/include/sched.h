/*
 * HIK Core-0 Scheduler
 * 
 * This file defines the thread scheduler for Core-0.
 * It manages thread execution and provides scheduling services.
 */

#ifndef HIK_CORE0_SCHED_H
#define HIK_CORE0_SCHED_H

#include <stdint.h>

/* Thread states */
typedef enum {
    THREAD_STATE_READY = 0,
    THREAD_STATE_RUNNING = 1,
    THREAD_STATE_BLOCKED = 2,
    THREAD_STATE_TERMINATED = 3
} thread_state_t;

/* Thread priority */
typedef enum {
    THREAD_PRIORITY_IDLE = 0,
    THREAD_PRIORITY_LOW = 1,
    THREAD_PRIORITY_NORMAL = 2,
    THREAD_PRIORITY_HIGH = 3,
    THREAD_PRIORITY_REALTIME = 4
} thread_priority_t;

/* Maximum number of threads */
#define MAX_THREADS 128
#define STACK_SIZE (64 * 1024)  /* 64KB stack */

/* Thread control block */
typedef struct {
    uint64_t thread_id;        /* Thread ID */
    uint64_t domain_id;        /* Owning domain ID */
    thread_state_t state;      /* Thread state */
    thread_priority_t priority; /* Thread priority */
    uint64_t stack_base;       /* Stack base address */
    uint64_t stack_size;       /* Stack size */
    uint64_t stack_ptr;        /* Current stack pointer */
    void (*entry_point)(void*); /* Thread entry point */
    void *arg;                 /* Thread argument */
    uint64_t time_slice;       /* Time slice remaining */
    uint64_t total_time;       /* Total CPU time */
    uint32_t flags;            /* Thread flags */
} tcb_t;

/* Scheduler state */
typedef struct {
    tcb_t threads[MAX_THREADS];     /* Thread table */
    uint32_t num_threads;            /* Number of threads */
    uint32_t current_thread;         /* Current running thread */
    uint64_t next_thread_id;         /* Next thread ID */
    uint64_t lock;                   /* Spinlock */
    uint64_t timer_ticks;            /* Timer ticks */
} sched_state_t;

/* Initialize scheduler */
int sched_init(void);

/* Create a new thread */
uint64_t sched_create_thread(uint64_t domain_id, void (*entry_point)(void*),
                             void *arg, thread_priority_t priority);

/* Terminate a thread */
int sched_terminate_thread(uint64_t thread_id);

/* Yield CPU to next thread */
void sched_yield(void);

/* Sleep for specified milliseconds */
void sched_sleep(uint64_t milliseconds);

/* Block current thread */
int sched_block(void);

/* Unblock a thread */
int sched_unblock(uint64_t thread_id);

/* Get current thread */
tcb_t* sched_get_current(void);

/* Schedule next thread (called by timer interrupt) */
void sched_schedule(void);

/* Timer interrupt handler */
void sched_timer_interrupt(void);

/* Idle thread */
void sched_idle_thread(void *arg);

/* Dump scheduler state (for debugging) */
void sched_dump(void);

#endif /* HIK_CORE0_SCHED_H */