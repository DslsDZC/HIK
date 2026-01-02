/*
 * HIK Core-3 (Application-3 Layer) Header
 *
 * Core-3 is the application layer that runs untrusted user code
 * in virtual address spaces with standard process abstraction.
 */

#ifndef HIK_CORE3_H
#define HIK_CORE3_H

#include "stdint.h"
#include "stddef.h"

/* Type definitions */
typedef long ssize_t;
typedef int pid_t;

/* Application entry point type */
typedef int (*app_entry_t)(int argc, char **argv);

/* Process information passed from Core-0 */
typedef struct {
    uint64_t process_id;         /* Process ID */
    uint64_t parent_pid;         /* Parent process ID */
    uint64_t entry_point;        /* Application entry point */
    uint64_t code_base;          /* Virtual code base address */
    uint64_t code_size;          /* Code size */
    uint64_t data_base;          /* Virtual data base address */
    uint64_t data_size;          /* Data size */
    uint64_t stack_base;         /* Virtual stack base address */
    uint64_t stack_size;         /* Stack size */
    uint64_t heap_base;          /* Virtual heap base address */
    uint64_t heap_size;          /* Heap size */
    uint32_t num_caps;           /* Number of capabilities */
    uint64_t cap_handles[64];    /* Capability handles */
    int argc;                    /* Argument count */
    char **argv;                 /* Argument vector */
    char **envp;                 /* Environment variables */
} __attribute__((packed)) process_info_t;

/* System call numbers */
typedef enum {
    SYS_EXIT = 0,
    SYS_READ = 1,
    SYS_WRITE = 2,
    SYS_OPEN = 3,
    SYS_CLOSE = 4,
    SYS_IOCTL = 5,
    SYS_MMAP = 6,
    SYS_MUNMAP = 7,
    SYS_IPC_CALL = 8,
    SYS_IPC_REGISTER = 9,
    SYS_IPC_WAIT = 10,
    SYS_GETPID = 11,
    SYS_GETPPID = 12,
    SYS_SLEEP = 13,
    SYS_YIELD = 14,
    SYS_GETTIME = 15
} syscall_num_t;

/* System call result */
typedef struct {
    int64_t ret;                 /* Return value */
    int64_t error;               /* Error code */
} __attribute__((packed)) syscall_result_t;

/* Global process info pointer */
extern process_info_t *g_process_info;

/* Initialize Core-3 application */
int core3_init(process_info_t *info);

/* Main application entry point */
int core3_main(int argc, char **argv);

/* Application cleanup */
void core3_cleanup(void);

/* System call wrapper */
syscall_result_t syscall(syscall_num_t num, uint64_t arg1, uint64_t arg2, 
                        uint64_t arg3, uint64_t arg4, uint64_t arg5);

/* Exit application */
void exit(int code) __attribute__((noreturn));

/* Read from file descriptor */
ssize_t read(int fd, void *buf, size_t count);

/* Write to file descriptor */
ssize_t write(int fd, const void *buf, size_t count);

/* Open file */
int open(const char *pathname, int flags);

/* Close file descriptor */
int close(int fd);

/* IPC call to service */
int ipc_call(const char *service_name, void *request, void *response, size_t size);

/* Get process ID */
pid_t getpid(void);

/* Get parent process ID */
pid_t getppid(void);

/* Sleep for milliseconds */
int sleep_ms(uint64_t milliseconds);

/* Yield CPU */
void yield(void);

#endif /* HIK_CORE3_H */