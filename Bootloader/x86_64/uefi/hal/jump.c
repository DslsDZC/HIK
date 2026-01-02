#include "hal.h"

__attribute__((naked)) VOID HalJumpToKernel(JUMP_CONTEXT* Context) {
    __asm__ volatile(
        "mov %0, %%rsp\n"
        "mov %1, %%rax\n"
        "jmp *%%rax\n"
        :
        : "r"(Context->StackTop), "r"(Context->EntryPoint)
        : "rax", "rsp"
    );
}