/*
 * HIK Core-0 Process Management Implementation
 */

#include "../include/process.h"
#include "../include/mm.h"
#include "../include/capability.h"
#include "../include/sched.h"
#include "../include/isolation.h"
#include "../include/string.h"

/* Global process manager state */
static process_manager_t g_process_manager;

/* Spinlock operations */
static inline void spin_lock(uint64_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        /* Spin */
    }
}

static inline void spin_unlock(uint64_t *lock) {
    __sync_lock_release(lock);
}

/* Current process ID (per-CPU) */
static __thread uint64_t g_current_pid = 0;

/*
 * Initialize process manager
 */
int process_init(void) {
    memset(&g_process_manager, 0, sizeof(process_manager_t));
    
    g_process_manager.num_processes = 0;
    g_process_manager.next_pid = 1;
    g_process_manager.lock = 0;
    
    return 0;
}

/*
 * Create a new process
 */
uint64_t process_create(const char *path, int argc, char **argv) {
    spin_lock(&g_process_manager.lock);
    
    /* Find free process slot */
    uint32_t slot = 0;
    for (slot = 0; slot < MAX_PROCESSES; slot++) {
        if (g_process_manager.processes[slot].state == 0) {
            break;
        }
    }
    
    if (slot >= MAX_PROCESSES) {
        spin_unlock(&g_process_manager.lock);
        return 0;  /* No free slots */
    }
    
    /* Allocate physical memory for process */
    uint64_t total_size = 0x100000;  /* 1MB for code, data, stack, heap */
    uint64_t phys_addr = mm_alloc(total_size, PAGE_SIZE, MEM_TYPE_APPLICATION, 0);
    if (phys_addr == 0) {
        spin_unlock(&g_process_manager.lock);
        return 0;
    }
    
    /* Create domain for process */
    uint64_t domain_id = cap_create_domain(phys_addr, total_size);
    if (domain_id == 0) {
        mm_free(phys_addr);
        spin_unlock(&g_process_manager.lock);
        return 0;
    }
    
    /* Create page tables for process */
    if (isolation_create_page_tables(domain_id, DOMAIN_FLAG_APP) != 0) {
        cap_delete_domain(domain_id);
        mm_free(phys_addr);
        spin_unlock(&g_process_manager.lock);
        return 0;
    }
    
    /* Initialize process */
    process_t *process = &g_process_manager.processes[slot];
    process->process_id = g_process_manager.next_pid++;
    process->parent_pid = g_current_pid;
    process->state = PROCESS_STATE_NEW;
    process->domain_id = domain_id;
    process->entry_point = 0x400000;  /* User base address */
    process->code_base = 0x400000;
    process->code_size = 0x10000;
    process->data_base = 0x410000;
    process->data_size = 0x10000;
    process->stack_base = 0x420000;
    process->stack_size = 0x10000;
    process->heap_base = 0x430000;
    process->heap_size = 0x10000;
    process->page_table = 0;  /* Would be set by isolation_create_page_tables */
    process->argc = argc;
    process->argv = argv;
    process->envp = NULL;
    process->exit_code = 0;
    process->uptime = 0;
    
    /* Setup virtual memory mappings */
    isolation_map_memory(domain_id, process->code_base, phys_addr,
                        process->code_size, MAP_TYPE_CODE, 0);
    isolation_map_memory(domain_id, process->data_base, phys_addr + process->code_size,
                        process->data_size, MAP_TYPE_DATA, 0);
    isolation_map_memory(domain_id, process->stack_base, phys_addr + process->code_size + process->data_size,
                        process->stack_size, MAP_TYPE_DATA, 0);
    isolation_map_memory(domain_id, process->heap_base, phys_addr + process->code_size + process->data_size + process->stack_size,
                        process->heap_size, MAP_TYPE_DATA, 0);
    
    g_process_manager.num_processes++;
    
    spin_unlock(&g_process_manager.lock);
    
    return process->process_id;
}

/*
 * Fork current process
 */
uint64_t process_fork(void) {
    /* In a full implementation, this would copy the current process */
    return 0;
}

/*
 * Execute a new program
 */
int process_exec(const char *path, int argc, char **argv) {
    /* In a full implementation, this would load and execute a new program */
    return 0;
}

/*
 * Wait for child process to exit
 */
int process_wait(uint64_t pid, int *status) {
    /* In a full implementation, this would wait for a child process */
    return 0;
}

/*
 * Exit current process
 */
void process_exit(int code) {
    uint64_t pid = g_current_pid;
    
    /* Terminate process */
    process_t *process = process_get(pid);
    if (process) {
        process->state = PROCESS_STATE_TERMINATED;
        process->exit_code = code;
    }
    
    /* Clean up resources */
    /* In a full implementation, this would free memory and capabilities */
    
    /* Switch to another process */
    sched_yield();
    
    /* Should not reach here */
    __builtin_unreachable();
}

/*
 * Get process by ID
 */
process_t* process_get(uint64_t pid) {
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_process_manager.processes[i].process_id == pid) {
            return &g_process_manager.processes[i];
        }
    }
    return NULL;
}

/*
 * Get current process ID
 */
uint64_t process_getpid(void) {
    return g_current_pid;
}

/*
 * Get parent process ID
 */
uint64_t process_getppid(void) {
    process_t *process = process_get(g_current_pid);
    if (process) {
        return process->parent_pid;
    }
    return 0;
}

/*
 * Kill process
 */
int process_kill(uint64_t pid, int signal) {
    /* In a full implementation, this would send a signal to a process */
    return 0;
}

/*
 * Process system call handler
 */
int process_handle_syscall(uint64_t syscall_num, uint64_t arg1, uint64_t arg2,
                          uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    switch (syscall_num) {
        case 0:  /* SYS_EXIT */
            process_exit(arg1);
            break;
            
        case 1:  /* SYS_READ */
            /* Handle read */
            break;
            
        case 2:  /* SYS_WRITE */
            /* Handle write */
            break;
            
        case 3:  /* SYS_OPEN */
            /* Handle open */
            break;
            
        case 4:  /* SYS_CLOSE */
            /* Handle close */
            break;
            
        case 5:  /* SYS_IOCTL */
            /* Handle ioctl */
            break;
            
        case 6:  /* SYS_MMAP */
            /* Handle mmap */
            break;
            
        case 7:  /* SYS_MUNMAP */
            /* Handle munmap */
            break;
            
        case 8:  /* SYS_IPC_CALL */
            /* Handle IPC call to service */
            break;
            
        case 9:  /* SYS_IPC_REGISTER */
            /* Handle IPC register */
            break;
            
        case 10:  /* SYS_IPC_WAIT */
            /* Handle IPC wait */
            break;
            
        case 11:  /* SYS_GETPID */
            return process_getpid();
            
        case 12:  /* SYS_GETPPID */
            return process_getppid();
            
        case 13:  /* SYS_SLEEP */
            sched_sleep(arg1);
            break;
            
        case 14:  /* SYS_YIELD */
            sched_yield();
            break;
            
        case 15:  /* SYS_GETTIME */
            /* Handle gettime */
            break;
            
        default:
            return -1;  /* Unknown syscall */
    }
    
    return 0;
}