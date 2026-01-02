/*
 * HIK Core-0 Process Management
 * 
 * This file defines the process management system for Core-3 applications.
 * It manages user-space processes with virtual address spaces.
 */

#ifndef HIK_CORE0_PROCESS_H
#define HIK_CORE0_PROCESS_H

#include "stdint.h"
#include "capability.h"

/* Maximum number of processes */
#define MAX_PROCESSES 256

/* Process state */
typedef enum {
    PROCESS_STATE_NEW = 0,
    PROCESS_STATE_READY = 1,
    PROCESS_STATE_RUNNING = 2,
    PROCESS_STATE_BLOCKED = 3,
    PROCESS_STATE_TERMINATED = 4
} process_state_t;

/* Process structure */
typedef struct {
    uint64_t process_id;         /* Process ID */
    uint64_t parent_pid;         /* Parent process ID */
    process_state_t state;       /* Process state */
    uint64_t domain_id;          /* Associated domain ID */
    uint64_t entry_point;        /* Process entry point */
    uint64_t code_base;          /* Virtual code base address */
    uint64_t code_size;          /* Code size */
    uint64_t data_base;          /* Virtual data base address */
    uint64_t data_size;          /* Data size */
    uint64_t stack_base;         /* Virtual stack base address */
    uint64_t stack_size;         /* Stack size */
    uint64_t heap_base;          /* Virtual heap base address */
    uint64_t heap_size;          /* Heap size */
    uint64_t page_table;         /* Page table physical address */
    int argc;                    /* Argument count */
    char **argv;                 /* Argument vector */
    char **envp;                 /* Environment variables */
    int exit_code;               /* Exit code */
    uint64_t uptime;             /* Process uptime */
} __attribute__((packed)) process_t;

/* Process manager state */
typedef struct {
    process_t processes[MAX_PROCESSES];  /* Process table */
    uint32_t num_processes;              /* Number of processes */
    uint64_t next_pid;                   /* Next process ID */
    uint64_t lock;                       /* Spinlock */
} process_manager_t;

/* Initialize process manager */
int process_init(void);

/* Create a new process */
uint64_t process_create(const char *path, int argc, char **argv);

/* Fork current process */
uint64_t process_fork(void);

/* Execute a new program */
int process_exec(const char *path, int argc, char **argv);

/* Wait for child process to exit */
int process_wait(uint64_t pid, int *status);

/* Exit current process */
void process_exit(int code) __attribute__((noreturn));

/* Get process by ID */
process_t* process_get(uint64_t pid);

/* Get current process ID */
uint64_t process_getpid(void);

/* Get parent process ID */
uint64_t process_getppid(void);

/* Kill process */
int process_kill(uint64_t pid, int signal);

/* Process system call handler */
int process_handle_syscall(uint64_t syscall_num, uint64_t arg1, uint64_t arg2,
                          uint64_t arg3, uint64_t arg4, uint64_t arg5);

#endif /* HIK_CORE0_PROCESS_H */