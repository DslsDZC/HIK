/*
 * ARM64 硬件探测 — 从系统寄存器/引导信息读取
 */

#include "hardware_probe.h"
#include "boot_info.h"
#include "lib/mem.h"

cpu_info_t g_cpu_info;
bool g_use_fsgsbase = false;

void cpu_init(void) {
    memzero(&g_cpu_info, sizeof(g_cpu_info));
    /* 从 MPIDR_EL1 读取 CPU 数量（遍历唯一 Aff0 值） */
    u64 mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    if (g_boot_info && g_boot_info->system.cpu_count > 0)
        g_cpu_info.logical_cores = g_boot_info->system.cpu_count;
    else
        g_cpu_info.logical_cores = 1;
    g_cpu_info.clock_frequency = 2000000000ULL;
}

void detect_memory_topology(memory_topology_t *topo) {
    memzero(topo, sizeof(*topo));
    if (g_boot_info && g_boot_info->mem_map) {
        for (u64 i = 0; i < g_boot_info->mem_map_entry_count; i++) {
            if (g_boot_info->mem_map[i].type == 1)
                topo->total_usable += g_boot_info->mem_map[i].length;
        }
    }
    if (topo->total_usable == 0)
        topo->total_usable = 1024UL * 1024 * 1024;
}

void detect_cpu_info_minimal(cpu_info_t *cpu) {
    cpu_init();
    if (cpu) *cpu = g_cpu_info;
}

void hardware_probe_mechanism_init(void) {}
cpu_info_t *hardware_probe_get_cpu_info(void) { return &g_cpu_info; }
