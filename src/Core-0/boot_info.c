/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核启动信息处理实现
 * 
 * 本文件只负责处理 Bootloader 传递的启动信息：
 * - 验证 boot_info 结构
 * - 解析内存映射
 * - 解析 ACPI 表
 * - 解析命令行参数
 * 
 * 注意：启动流程代码应该在 main.c 中，而不是这里！
 */

#include "boot_info.h"
#include "console.h"
#include "pmm.h"
#include "string.h"
#include "hardware_probe.h"
#include "audit.h"
#include "hal.h"
#include "lib/mem.h"

#include <stdarg.h>
#include <stddef.h>

/* 全局启动状态 */
boot_state_t g_boot_state = {0};

/* 全局引导信息指针 */
hic_boot_info_t *g_boot_info = NULL;

/* 引导信息静态备份（确保在所有域页表中可访问） */
static hic_boot_info_t g_boot_info_storage;

/**
 * 复制引导信息到静态存储
 *
 * 引导信息由 Bootloader 动态分配，位于不可预测的物理地址。
 * 当服务在自己的域中运行时，其页表可能不包含引导信息的物理页面，
 * 导致通过 g_boot_info 访问字段时触发页错误。
 *
 * 此函数将完整的引导信息复制到内核 BSS 段的静态缓冲区中，
 * 该区域已映射到所有特权域的页表中。
 *
 * 调用时机：在域创建之前调用（main.c 中 Step 10 之后、Step 11 之前）
 */
void boot_info_copy_to_static(void)
{
    if (g_boot_info == NULL) {
        return;
    }

    /* 复制整个引导信息结构 */
    memcpy(&g_boot_info_storage, g_boot_info, sizeof(hic_boot_info_t));

    /* 修正 magic_region_base：Bootloader 在 load_kernel_segments
     * 更新 kernel_base 前已赋值，可能指向 UEFI 缓冲区地址，
     * 该地址在服务域页表中不可访问。修正为内核链接基址。 */
    {
        extern u8 _text_start[];
        phys_addr_t kernel_phys_base = (phys_addr_t)_text_start;
        g_boot_info_storage.embedded_modules.magic_region_base = (void*)kernel_phys_base;
    }

    /* 同时也修复原始 boot_info（如果后续有直接引用） */
    if (g_boot_info != &g_boot_info_storage) {
        extern u8 _text_start[];
        g_boot_info->embedded_modules.magic_region_base = (void*)(phys_addr_t)_text_start;
    }

    /* 更新全局指针指向静态复制 */
    g_boot_info = &g_boot_info_storage;

    console_puts("[BOOT] Boot info copied to static storage (domain-safe)\n");

    /* 调试：打印 magic_region_base 最终值 */
    {
        char dbg[80];
        dbg[0] = '['; dbg[1] = 'B'; dbg[2] = 'O'; dbg[3] = 'O'; dbg[4] = 'T'; dbg[5] = ']';
        dbg[6] = ' '; dbg[7] = 'm'; dbg[8] = 'a'; dbg[9] = 'g'; dbg[10] = 'i'; dbg[11] = 'c';
        dbg[12] = '_'; dbg[13] = 'r'; dbg[14] = 'e'; dbg[15] = 'g'; dbg[16] = 'i'; dbg[17] = 'o';
        dbg[18] = 'n'; dbg[19] = '_'; dbg[20] = 'b'; dbg[21] = 'a'; dbg[22] = 's'; dbg[23] = 'e';
        dbg[24] = '='; dbg[25] = '0'; dbg[26] = 'x';
        u64 val = (u64)g_boot_info_storage.embedded_modules.magic_region_base;
        const char hex[] = "0123456789ABCDEF";
        dbg[27] = hex[(val >> 60) & 0xF]; dbg[28] = hex[(val >> 56) & 0xF];
        dbg[29] = hex[(val >> 52) & 0xF]; dbg[30] = hex[(val >> 48) & 0xF];
        dbg[31] = hex[(val >> 44) & 0xF]; dbg[32] = hex[(val >> 40) & 0xF];
        dbg[33] = hex[(val >> 36) & 0xF]; dbg[34] = hex[(val >> 32) & 0xF];
        dbg[35] = hex[(val >> 28) & 0xF]; dbg[36] = hex[(val >> 24) & 0xF];
        dbg[37] = hex[(val >> 20) & 0xF]; dbg[38] = hex[(val >> 16) & 0xF];
        dbg[39] = hex[(val >> 12) & 0xF]; dbg[40] = hex[(val >> 8) & 0xF];
        dbg[41] = hex[(val >> 4) & 0xF]; dbg[42] = hex[val & 0xF];
        dbg[43] = '\n'; dbg[44] = 0;
        console_puts(dbg);
    }
}

/* ==================== BSS 错误检查 ==================== */

/* ==================== TLV 解析 ==================== */

static hic_mem_entry_t g_mem_map_storage[64];

/* ==================== 最小引导信息 ==================== */

/**
 * boot_info_init_minimal — 在无 bootloader 传 TLV 时使用。
 *
 * 为 STM32 等直启平台提供最小引导信息，使用默认内存布局。
 */
void boot_info_init_minimal(void)
{
    memzero(&g_boot_info_storage, sizeof(g_boot_info_storage));
    g_boot_info_storage.magic = HIC_BOOT_INFO_MAGIC;
    g_boot_info_storage.version = HIC_BOOT_INFO_VERSION;
    g_boot_info_storage.system.cpu_count = 1;

    /* 设置单个内存区域（覆盖全部可用内存） */
    g_boot_info_storage.mem_map = g_mem_map_storage;
    g_boot_info_storage.mem_map_entry_count = 1;
    g_mem_map_storage[0].base_address = 0;
    g_mem_map_storage[0].length = 0xFFFFFFFF;
    g_mem_map_storage[0].type = HIC_MEM_TYPE_USABLE;

    g_boot_info = &g_boot_info_storage;

    console_puts("[BOOT] Minimal boot info initialized (no TLV)\n");
}

void boot_info_parse_tlv(void)
{
    volatile boot_info_header_t *hdr = (boot_info_header_t *)BOOT_INFO_ADDR;
    if (hdr->magic != BOOT_INFO_MAGIC) return;

    memzero(&g_boot_info_storage, sizeof(g_boot_info_storage));
    g_boot_info_storage.magic = HIC_BOOT_INFO_MAGIC;
    g_boot_info_storage.version = HIC_BOOT_INFO_VERSION;

    u8 *pos = (u8 *)(hdr + 1);
    u8 *end = (u8 *)hdr + hdr->total_size;

    for (u32 i = 0; i < hdr->entry_count && pos < end; i++) {
        boot_info_tlv_t *tlv = (boot_info_tlv_t *)pos;
        if (tlv->tag == TAG_END) break;
        if (pos + sizeof(boot_info_tlv_t) + tlv->len > end) break;

        switch (tlv->tag) {
        case TAG_MEM_MAP:;
            u32 mc = tlv->len / sizeof(hic_mem_entry_t);
            if (mc > 64) mc = 64;
            memcpy(g_mem_map_storage, tlv->data, mc * sizeof(hic_mem_entry_t));
            g_boot_info_storage.mem_map = g_mem_map_storage;
            g_boot_info_storage.mem_map_entry_count = mc;
            break;
        case TAG_CPU_COUNT:
            if (tlv->len >= 4)
                g_boot_info_storage.system.cpu_count = *(u32 *)tlv->data;
            break;
        case TAG_CMDLINE:
            if (tlv->len > 0) {
                u32 cl = tlv->len < 255 ? tlv->len : 255;
                memcpy(g_boot_info_storage.cmdline, tlv->data, cl);
                g_boot_info_storage.cmdline[cl] = 0;
            }
            break;
        case TAG_RSDP:
            if (tlv->len >= 8) {
                g_boot_info_storage.rsdp = *(void **)tlv->data;
                g_boot_info_storage.flags |= HIC_BOOT_FLAG_ACPI_ENABLED;
            }
            break;
        case TAG_FRAMEBUFFER:
            if (tlv->len >= sizeof(g_boot_info_storage.video))
                memcpy(&g_boot_info_storage.video, tlv->data, sizeof(g_boot_info_storage.video));
            break;
        case TAG_SERIAL_PORT:
            if (tlv->len >= 2)
                g_boot_info_storage.debug.serial_port = *(u16 *)tlv->data;
            break;
        case TAG_KERNEL_BASE:
            if (tlv->len >= 8)
                g_boot_info_storage.kernel_base = *(void **)tlv->data;
            break;
        case TAG_KERNEL_SIZE:
            if (tlv->len >= 8)
                g_boot_info_storage.kernel_size = *(u64 *)tlv->data;
            break;
        case TAG_ENTRY_POINT:
            if (tlv->len >= 8)
                g_boot_info_storage.entry_point = *(u64 *)tlv->data;
            break;
        case TAG_STACK_TOP:
            if (tlv->len >= 8)
                g_boot_info_storage.stack_top = *(u64 *)tlv->data;
            break;
        case TAG_GDT:
            if (tlv->len >= sizeof(g_boot_info_storage.gdt))
                memcpy(&g_boot_info_storage.gdt, tlv->data, sizeof(g_boot_info_storage.gdt));
            break;
        case TAG_ARCH:
            if (tlv->len >= 4)
                g_boot_info_storage.system.architecture = *(u32 *)tlv->data;
            break;
        case TAG_DISK_INFO:
            if (tlv->len >= sizeof(g_boot_info_storage.disk))
                memcpy(&g_boot_info_storage.disk, tlv->data, sizeof(g_boot_info_storage.disk));
            break;
        case TAG_HARDWARE_DATA:
            g_boot_info_storage.hardware.hw_data = tlv->data;
            g_boot_info_storage.hardware.hw_size = tlv->len;
            break;
        case TAG_MODULE: {
            u32 modc = tlv->len / sizeof(g_boot_info_storage.modules[0]);
            if (modc > 16) modc = 16;
            memcpy(g_boot_info_storage.modules, tlv->data, modc * sizeof(g_boot_info_storage.modules[0]));
            g_boot_info_storage.module_count = modc;
            break;
        }
        }
        pos += sizeof(boot_info_tlv_t) + tlv->len;
    }

    g_boot_info_storage.firmware_type = 0;
    g_boot_info = &g_boot_info_storage;
    g_boot_state.boot_info = &g_boot_info_storage;
}

/* ==================== 验证接口 ==================== */

/**
 * 验证启动信息
 */
bool boot_info_validate(hic_boot_info_t* boot_info) {
    console_puts("[BOOT] Validating boot_info...\n");
    
    if (!boot_info) {
        console_puts("[BOOT] ERROR: boot_info pointer is NULL\n");
        return false;
    }

    if (boot_info->magic != HIC_BOOT_INFO_MAGIC) {
        console_puts("[BOOT] ERROR: Invalid magic: 0x");
        console_puthex32(boot_info->magic);
        console_puts(" (expected: 0x");
        console_puthex32(HIC_BOOT_INFO_MAGIC);
        console_puts(")\n");
        return false;
    }
    
    if (boot_info->version != HIC_BOOT_INFO_VERSION) {
        console_puts("[BOOT] ERROR: Version mismatch: ");
        console_putu32(boot_info->version);
        console_puts(" (expected: ");
        console_putu32(HIC_BOOT_INFO_VERSION);
        console_puts(")\n");
        return false;
    }
    
    if (!boot_info->mem_map) {
        console_puts("[BOOT] ERROR: mem_map pointer is NULL\n");
        return false;
    }
    
    if (boot_info->mem_map_entry_count == 0) {
        console_puts("[BOOT] ERROR: mem_map_entry_count is 0\n");
        return false;
    }
    
    if (!boot_info->kernel_base) {
        console_puts("[BOOT] ERROR: kernel_base is NULL\n");
        return false;
    }
    
    if (boot_info->kernel_size == 0) {
        console_puts("[BOOT] ERROR: kernel_size is 0\n");
        return false;
    }
    
    if (boot_info->entry_point == 0) {
        console_puts("[BOOT] ERROR: entry_point is 0\n");
        return false;
    }
    
    console_puts("[BOOT] boot_info validation PASSED\n");
    return true;
}

/* ==================== 处理接口 ==================== */

/**
 * 处理启动信息（打印摘要）
 */
void boot_info_process(hic_boot_info_t* boot_info) {
    console_puts("[BOOT] Processing boot information...\n");
    
    /* 打印固件信息 */
    console_puts("[BOOT] Firmware: ");
    console_puts(boot_info->firmware_type == 0 ? "UEFI" : "BIOS");
    console_puts("\n");
    
    /* 打印架构信息 */
    console_puts("[BOOT] Architecture: ");
    switch (boot_info->system.architecture) {
        case 1: console_puts("x86_64"); break;
        case 2: console_puts("ARM64"); break;
        case 3: console_puts("RISC-V64"); break;
        default: console_puts("Unknown"); break;
    }
    console_puts("\n");
    
    /* 打印内存信息 */
    console_puts("[BOOT] Memory reported by bootloader: ");
    console_putu32(boot_info->system.memory_size_mb);
    console_puts(" MB\n");
    
    /* 计算可用内存 */
    u64 total_usable = 0;
    for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
        hic_mem_entry_t* entry = &boot_info->mem_map[i];
        if (entry->type == HIC_MEM_TYPE_USABLE) {
            total_usable += entry->length;
        }
    }
    console_puts("[BOOT] Usable memory: ");
    console_putu64(total_usable / (1024 * 1024));
    console_puts(" MB\n");
    
    /* 打印 ACPI 信息 */
    if (boot_info->flags & HIC_BOOT_FLAG_ACPI_ENABLED) {
        console_puts("[BOOT] ACPI RSDP: 0x");
        console_puthex64((u64)boot_info->rsdp);
        console_puts("\n");
    }
    
    /* 打印模块信息 */
    if (boot_info->module_count > 0) {
        console_puts("[BOOT] Preloaded modules: ");
        console_putu64(boot_info->module_count);
        console_puts("\n");
        for (u64 i = 0; i < boot_info->module_count; i++) {
            console_puts("[BOOT]   Module ");
            console_putu64(i);
            console_puts(": ");
            console_puts(boot_info->modules[i].name);
            console_puts(" (");
            console_putu64(boot_info->modules[i].size);
            console_puts(" bytes)\n");
        }
    }
}

/* ==================== 内存初始化 ==================== */

/**
 * 初始化内存管理器
 */
void boot_info_init_memory(hic_boot_info_t* boot_info) {
    console_puts("[BOOT] Initializing memory manager...\n");
    
    /* 计算最大物理地址 */
    phys_addr_t max_phys_addr = 0;
    
    for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
        hic_mem_entry_t* entry = &boot_info->mem_map[i];
        if (entry->type == HIC_MEM_TYPE_USABLE) {
            phys_addr_t region_end = entry->base_address + entry->length;
            if (region_end > max_phys_addr) {
                max_phys_addr = region_end;
            }
        }
    }
    
    if (max_phys_addr == 0) {
        max_phys_addr = 256 * 1024 * 1024;  /* 默认 256MB */
    }
    
    console_puts("[BOOT] Max physical address: 0x");
    console_puthex64(max_phys_addr);
    console_puts("\n");
    
    /* 初始化 PMM */
    pmm_init_with_range(max_phys_addr);
    
    /* 处理内存映射 */
    u64 usable_regions = 0;
    u64 kernel_regions = 0;
    
    for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
        hic_mem_entry_t* entry = &boot_info->mem_map[i];
        
        switch (entry->type) {
            case HIC_MEM_TYPE_USABLE:
                pmm_add_region(entry->base_address, entry->length);
                usable_regions++;
                break;
                
            case HIC_MEM_TYPE_KERNEL:
                pmm_mark_used(entry->base_address, entry->length);
                kernel_regions++;
                break;
                
            default:
                /* 其他类型跳过 */
                break;
        }
    }
    
    console_puts("[BOOT] Memory initialized: ");
    console_putu64(usable_regions);
    console_puts(" usable regions, ");
    console_putu64(kernel_regions);
    console_puts(" kernel regions\n");
    
    /* 显式保护内核内存区域（防止 bootloader 未正确标记） */
    extern u8 _text_start[];
    extern u8 _kernel_end[];
    extern u8 _static_modules_region_end[];
    
    phys_addr_t kernel_start = (phys_addr_t)_text_start;
    phys_addr_t kernel_end = (phys_addr_t)_static_modules_region_end;
    
    console_puts("[BOOT] Protecting kernel memory: 0x");
    console_puthex64(kernel_start);
    console_puts(" - 0x");
    console_puthex64(kernel_end);
    console_puts("\n");
    
    pmm_mark_used(kernel_start, kernel_end - kernel_start);
}

/* ==================== 命令行解析 ==================== */

/**
 * 解析命令行参数
 */
void boot_info_parse_cmdline(const char* cmdline) {
    if (!cmdline || cmdline[0] == '\0') {
        return;
    }
    
    console_puts("[BOOT] Parsing command line: ");
    console_puts(cmdline);
    console_puts("\n");
    
    const char* p = cmdline;
    
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        
        const char* start = p;
        while (*p && *p != ' ') p++;

        char param[128];
        u64 len = (u64)(p - start);
        if (len >= sizeof(param)) len = sizeof(param) - 1;
        memmove(param, start, len);
        param[len] = '\0';
        
        /* 解析参数 */
        if (strcmp(param, "debug") == 0) {
            g_boot_state.debug_enabled = true;
        } else if (strcmp(param, "quiet") == 0) {
            g_boot_state.quiet_mode = true;
        } else if (strcmp(param, "recovery") == 0) {
            g_boot_state.recovery_mode = true;
        } else if (strcmp(param, "noapic") == 0) {
            g_boot_state.hw.local_irq.enabled = false;
        } else if (strcmp(param, "nosmp") == 0) {
            g_boot_state.hw.smp_enabled = false;
        }
        /* 更多参数解析... */
    }
}

/* ==================== 硬件信息处理 ==================== */

/**
 * 从 Bootloader 复制硬件探测结果
 */
void boot_info_copy_hardware_info(hic_boot_info_t* boot_info) {
    if (boot_info->hardware.hw_data != NULL && 
        boot_info->hardware.hw_size >= sizeof(hardware_probe_result_t)) {
        
        hardware_probe_result_t *bootloader_hw = (hardware_probe_result_t *)boot_info->hardware.hw_data;
        memcpy(&g_boot_state.hw, bootloader_hw, sizeof(hardware_probe_result_t));
        
        console_puts("[BOOT] Hardware info from bootloader:\n");
        console_puts("[BOOT]   CPU: ");
        console_puts(g_boot_state.hw.cpu.brand_string);
        console_puts("\n");
        console_puts("[BOOT]   Cores: ");
        console_putu64(g_boot_state.hw.cpu.logical_cores);
        console_puts(" logical, ");
        console_putu64(g_boot_state.hw.cpu.physical_cores);
        console_puts(" physical\n");
        console_puts("[BOOT]   Memory: ");
        console_putu64(g_boot_state.hw.memory.total_usable / (1024 * 1024));
        console_puts(" MB usable\n");
    } else {
        console_puts("[BOOT] No hardware info from bootloader, performing minimal detection...\n");
        
        /* 执行最小化硬件探测（机制层） */
        detect_cpu_info_minimal(&g_boot_state.hw.cpu);
        detect_memory_topology(&g_boot_state.hw.memory);
        
        /* 设置默认值 */
        g_boot_state.hw.devices.pci_count = 0;
        g_boot_state.hw.local_irq.base_address = 0xFEE00000;
        g_boot_state.hw.io_irq.base_address = 0xFEC00000;
        g_boot_state.hw.smp_enabled = (g_boot_state.hw.cpu.logical_cores > 1);
    }
}

/* ==================== 查询接口 ==================== */

/**
 * 获取启动状态
 */
boot_state_t* get_boot_state(void) {
    return &g_boot_state;
}

/**
 * 打印启动信息摘要
 */
void boot_info_print_summary(void) {
    console_puts("\n[BOOT] ========== Boot Summary ==========\n");
    
    console_puts("[BOOT] Bootloader provided:\n");
    console_puts("[BOOT]   Memory map entries: ");
    console_putu64(g_boot_state.boot_info->mem_map_entry_count);
    console_puts("\n");
    
    console_puts("[BOOT] Detected hardware:\n");
    console_puts("[BOOT]   CPU: ");
    console_puts(g_boot_state.hw.cpu.brand_string);
    console_puts("\n");
    console_puts("[BOOT]   Memory: ");
    console_putu64(g_boot_state.hw.memory.total_usable / (1024 * 1024));
    console_puts(" MB\n");
    
    console_puts("[BOOT] ===================================\n\n");
}

/* ==================== 内核维护任务 ==================== */

/**
 * 内核维护任务
 * 
 * 执行周期性维护任务：
 * - 审计日志刷新
 * - 监控统计更新
 * - 能力清理
 */
void kernel_maintenance_tasks(void) {
    /* 周期性维护任务 */
    static u64 last_maintenance = 0;
    u64 now = hal_get_timestamp();
    
    /* 每 1 秒执行一次维护 */
    if (now - last_maintenance < 1000000000ULL) {
        return;
    }
    last_maintenance = now;
    
    /* TODO: 实现以下维护任务：
     * 1. 审计日志刷新到持久存储
     * 2. 监控统计信息更新
     * 3. 过期能力清理
     * 4. 内存碎片整理检查
     */
}