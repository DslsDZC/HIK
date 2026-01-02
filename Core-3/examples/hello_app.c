/*
 * HIK Core-3 Hello World Application Example
 * 
 * This is a simple "Hello World" application that demonstrates
 * how to write applications for the HIK operating system.
 */

#include "../include/core3.h"
#include "../include/string.h"

/* Simple printf implementation */
static void simple_printf(const char *str) {
    write(1, str, strlen(str));
}

static void simple_print_hex(uint64_t value) {
    char hex_chars[] = "0123456789abcdef";
    char buffer[17];
    buffer[16] = '\0';
    
    for (int i = 15; i >= 0; i--) {
        buffer[i] = hex_chars[value & 0xf];
        value >>= 4;
    }
    
    write(1, "0x", 2);
    write(1, buffer, 16);
}

/* Main application entry point */
int main(int argc, char **argv) {
    /* Print greeting */
    simple_printf("Hello from HIK Application-3!\n");
    
    /* Print process ID */
    simple_printf("Process ID: ");
    pid_t pid = getpid();
    simple_print_hex(pid);
    simple_printf("\n");
    
    /* Print parent process ID */
    simple_printf("Parent PID: ");
    pid_t ppid = getppid();
    simple_print_hex(ppid);
    simple_printf("\n");
    
    /* Print arguments */
    simple_printf("Arguments: ");
    for (int i = 0; i < argc; i++) {
        if (i > 0) {
            simple_printf(" ");
        }
        simple_printf(argv[i]);
    }
    simple_printf("\n");
    
    /* Test memory allocation */
    simple_printf("Testing memory allocation...\n");
    void *ptr1 = malloc(1024);
    void *ptr2 = malloc(2048);
    void *ptr3 = malloc(4096);
    
    simple_printf("Allocated: ptr1=");
    simple_print_hex((uint64_t)ptr1);
    simple_printf(", ptr2=");
    simple_print_hex((uint64_t)ptr2);
    simple_printf(", ptr3=");
    simple_print_hex((uint64_t)ptr3);
    simple_printf("\n");
    
    /* Free memory */
    free(ptr1);
    free(ptr2);
    free(ptr3);
    simple_printf("Memory freed\n");
    
    /* Test IPC call to network service */
    simple_printf("Testing IPC call to network service...\n");
    char request[] = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    char response[256];
    int ret = ipc_call("network", request, response, sizeof(request));
    
    if (ret >= 0) {
        simple_printf("IPC call successful\n");
    } else {
        simple_printf("IPC call failed\n");
    }
    
    /* Sleep for a while */
    simple_printf("Sleeping for 1000ms...\n");
    sleep_ms(1000);
    
    /* Print goodbye */
    simple_printf("Goodbye from HIK Application-3!\n");
    
    return 0;
}