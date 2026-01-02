/*
 * HIK Core-3 Main Implementation
 * 
 * Core-3 is the application layer that provides a standard
 * process abstraction for untrusted user code.
 */

#include "core3.h"
#include "virtual_mem.h"
#include "ipc.h"
#include "string.h"

/* Global process info pointer */
process_info_t *g_process_info = NULL;

/* Initialize Core-3 application */
int core3_init(process_info_t *info) {
    if (!info) {
        return -1;
    }

    g_process_info = info;

    /* Initialize virtual memory manager */
    if (vmm_init(info->heap_base, info->heap_size) != 0) {
        return -1;
    }

    /* Initialize IPC library */
    if (ipc_init() != 0) {
        return -1;
    }

    return 0;
}

/* Main application entry point */
int core3_main(int argc, char **argv) {
    /* Call application main function */
    extern int main(int argc, char **argv);
    return main(argc, argv);
}

/* Simple main function for testing */
int main(int argc, char **argv) {
    /* Simple test output */
    const char *msg = "Core-3 Application Running\n";
    write(1, msg, strlen(msg));

    /* Test memory allocation */
    void *ptr = malloc(1024);
    if (ptr) {
        const char *alloc_msg = "Memory allocation successful\n";
        write(1, alloc_msg, strlen(alloc_msg));
        free(ptr);
    }

    /* Test getpid */
    pid_t pid = getpid();
    (void)pid;  /* Suppress unused variable warning */
    const char *pid_msg = "Process ID: ";
    write(1, pid_msg, strlen(pid_msg));
    /* Would print pid here */

    return 0;
}

/* Application cleanup */
void core3_cleanup(void) {
    /* Cleanup IPC */
    /* Note: IPC cleanup would be implemented here */

    /* Cleanup memory */
    /* Note: Memory cleanup would be implemented here */
}

/* System call wrapper */
syscall_result_t syscall(syscall_num_t num, uint64_t arg1, uint64_t arg2, 
                        uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    syscall_result_t result = {0, 0};

    /* Make system call via syscall instruction */
    __asm__ volatile (
        "mov %1, %%rax\n"
        "mov %2, %%rdi\n"
        "mov %3, %%rsi\n"
        "mov %4, %%rdx\n"
        "mov %5, %%r10\n"
        "mov %6, %%r8\n"
        "syscall\n"
        "mov %%rax, %0\n"
        : "=m" (result.ret)
        : "r" ((uint64_t)num), "r" (arg1), "r" (arg2), "r" (arg3), "r" (arg4), "r" (arg5)
        : "%rax", "%rdi", "%rsi", "%rdx", "%r10", "%r8", "%rcx", "%r11", "memory"
    );

    /* Check for error (negative return value) */
    if ((int64_t)result.ret < 0) {
        result.error = -result.ret;
        result.ret = -1;
    }

    return result;
}

/* Exit application */
void exit(int code) {
    syscall(SYS_EXIT, code, 0, 0, 0, 0);
    __builtin_unreachable();
}

/* Read from file descriptor */
ssize_t read(int fd, void *buf, size_t count) {
    syscall_result_t result = syscall(SYS_READ, fd, (uint64_t)buf, count, 0, 0);
    return result.ret;
}

/* Write to file descriptor */
ssize_t write(int fd, const void *buf, size_t count) {
    syscall_result_t result = syscall(SYS_WRITE, fd, (uint64_t)buf, count, 0, 0);
    return result.ret;
}

/* Open file */
int open(const char *pathname, int flags) {
    syscall_result_t result = syscall(SYS_OPEN, (uint64_t)pathname, flags, 0, 0, 0);
    return result.ret;
}

/* Close file descriptor */
int close(int fd) {
    syscall_result_t result = syscall(SYS_CLOSE, fd, 0, 0, 0, 0);
    return result.ret;
}

/* IPC call to service */
int ipc_call(const char *service_name, void *request, void *response, size_t size) {
    /* Build IPC message */
    ipc_msg_t msg;
    msg.header.msg_type = IPC_TYPE_REQUEST;
    msg.header.data_size = size;
    if (size > IPC_MAX_MSG_SIZE) {
        size = IPC_MAX_MSG_SIZE;
    }
    memcpy(msg.data, request, size);

    /* Make system call */
    syscall_result_t result = syscall(SYS_IPC_CALL, (uint64_t)service_name, 
                                     (uint64_t)&msg, size, 0, 0);

    /* Copy response */
    if (result.ret >= 0 && response) {
        memcpy(response, msg.data, msg.header.data_size);
    }

    return result.ret;
}

/* Get process ID */
pid_t getpid(void) {
    syscall_result_t result = syscall(SYS_GETPID, 0, 0, 0, 0, 0);
    return result.ret;
}

/* Get parent process ID */
pid_t getppid(void) {
    syscall_result_t result = syscall(SYS_GETPPID, 0, 0, 0, 0, 0);
    return result.ret;
}

/* Sleep for milliseconds */
int sleep_ms(uint64_t milliseconds) {
    syscall_result_t result = syscall(SYS_SLEEP, milliseconds, 0, 0, 0, 0);
    return result.ret;
}

/* Yield CPU */
void yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}
