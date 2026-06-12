/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核核心入口点
 * 遵循三层模型文档：Core-0层作为系统仲裁者
 */

#include "types.h"
#include "capability.h"
#include "domain.h"
#include "pmm.h"
#include "imal.h"
#include "thread.h"
#include "irq.h"
#include "syscall.h"
#include "build_config.h"
#include "runtime_config.h"
#include "audit.h"
#include "amp.h"
#include "logical_core.h"
#include "ipc3.h"
#include <string.h>
#include "lib/console.h"
#include "boot_info.h"
#include "minimal_uart.h"
#include "module_loader.h"
#include "include/static_module.h"
#include "include/module_primitives.h"

/* 注意：buddy.h 和 memory_compact.h 功能已移到 memory_service */

/* 外部全局变量 */
extern boot_state_t g_boot_state;
extern hic_boot_info_t *g_boot_info;

/**
 * 内核主入口点
 * 由kernel_start调用，完成所有初始化后进入主循环
 */
void kernel_main(void)
{
    hal_uart_putc('A');

    hic_boot_info_t *boot_info = g_boot_info;
    if (boot_info == NULL) goto panic;

    console_puts("hello\n");

    /* ===== 审计日志系统初始化 ===== */
    audit_system_init();

    phys_addr_t audit_buffer_base = 0;
    size_t audit_buffer_size = 0;

    if (boot_info->mem_map && boot_info->mem_map_entry_count > 0) {
        for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
            hic_mem_entry_t *e = &boot_info->mem_map[i];
            if (e->type == HIC_MEM_TYPE_USABLE && e->length > audit_buffer_size) {
                audit_buffer_base = e->base_address + e->length - 0x10000;
                audit_buffer_size = 0x10000;
                break;
            }
        }
        if (audit_buffer_base) {
            audit_system_init_buffer(audit_buffer_base, audit_buffer_size);
            audit_log_event(AUDIT_EVENT_DOMAIN_CREATE, 0, 0, 0, NULL, 0, true);
        }
    }

    if (!boot_info_validate(boot_info)) {
        audit_log_event(AUDIT_EVENT_EXCEPTION, 0, 0, 0, NULL, 0, false);
        console_puts("[BOOT] >>> PANIC: boot_info validation FAILED <<<\n");
        goto panic;
    }
    console_puts("[BOOT] >>> boot_info_validate PASSED <<<\n");

    audit_log_event(AUDIT_EVENT_PMM_ALLOC, 0, 0, 0, NULL, 0, true);

    /* ===== 从 TLV 硬件数据解析到运行时状态 ===== */
    if (boot_info->hardware.hw_data && boot_info->hardware.hw_size >= sizeof(hardware_probe_result_t)) {
        memcpy(&g_boot_state.hw, boot_info->hardware.hw_data, sizeof(hardware_probe_result_t));
    }
    
    /* ===== 运行时配置 ===== */
    console_puts("\n[BOOT] STEP 0: Initializing Runtime Configuration\n");
    runtime_config_init();
    console_puts("[BOOT] Runtime configuration initialized\n");
    
    console_puts("[BOOT] Loading configuration from boot info...\n");
    runtime_config_load_from_bootinfo();
    console_puts("[BOOT] Configuration loaded from boot info\n");
    
    if (!runtime_config_validate()) {
        console_puts("[BOOT] WARNING: Runtime configuration validation failed, using defaults\n");
    } else {
        console_puts("[BOOT] Runtime configuration validated\n");
    }
    
    /* ===== 核心子系统初始化 ===== */
    
    console_puts("\n[BOOT] STEP 1: Initializing Memory Manager\n");
    boot_info_init_memory(boot_info);
    console_puts("[BOOT] Memory manager initialization completed\n");

    /* FIX: 标记审计缓冲区为已使用，防止 PMM 分配出去 */
    if (audit_buffer_base != 0) {
        pmm_mark_used(audit_buffer_base, audit_buffer_size);
    }
    
    /* 【步骤1.2：内存布局初始化 - 已移到 memory_service】 */
    /* memory_layout_init 现在由 memory_service 负责，遵循机制与策略分离原则 */
    console_puts("[BOOT] Memory layout will be initialized by memory_service\n");
    
    /* 【步骤1.5：IMAL 初始化 - 隔离机制抽象层】 */
    console_puts("\n[BOOT] STEP 1.5: Initializing IMAL\n");
    imal_init();
    console_puts("[BOOT] IMAL initialization completed\n");
    
    /* 【步骤2：能力系统初始化】 */
    console_puts("\n[BOOT] STEP 2: Initializing Capability System\n");
    capability_system_init();
    console_puts("[BOOT] Capability system initialization completed\n");
    
    /* 【步骤3：域系统初始化】 */
    console_puts("\n[BOOT] STEP 3: Initializing Domain System\n");
    domain_system_init();
    console_puts("[BOOT] Domain system initialization completed\n");
    /* 步骤3.5：IPC 3.0 子系统初始化 */
    console_puts("\n[BOOT] STEP 3.5: Initializing IPC 3.0 Subsystem\n");
    ipc3_init();
    console_puts("[BOOT] IPC 3.0 initialization completed\n");
    
    /* 【步骤4：调度器初始化】 */
    console_puts("\n[BOOT] STEP 4: Initializing Scheduler\n");
    scheduler_init();
    console_puts("[BOOT] Scheduler initialization completed\n");

    /* 【步骤4.2：定时器初始化（Core-0 调度机制）】 */
    extern void timer_init(void) __attribute__((weak));
    if (timer_init) {
        timer_init();
    }
    
    /* 【步骤4.4：逻辑核心系统初始化】 */
    console_puts("\n[BOOT] STEP 4.4: Initializing Logical Core System\n");
    logical_core_system_init();
    console_puts("[BOOT] Logical core system initialization completed\n");

    /* 【步骤4.5：AMP初始化】 */
    console_puts("\n[BOOT] STEP 4.5: Initializing AMP\n");
    amp_init();
    console_puts("[BOOT] AMP initialization completed\n");

    /* 【步骤4.6：AP启动（如果启用）】 */
    if (g_boot_state.hw.smp_enabled && g_amp_info.cpu_count > 1) {
        console_puts("\n[BOOT] STEP 4.6: Booting APs\n");
        if (amp_boot_aps() == HIC_SUCCESS) {
            amp_wait_for_aps();
        }
        console_puts("[BOOT] APs booted successfully\n");
    } else {
        console_puts("\n[BOOT] STEP 4.6: AMP not enabled (single core or disabled)\n");
    }

    /* 【步骤5：处理启动信息】 */
    console_puts("\n[BOOT] STEP 5: Processing Boot Information\n");
    boot_info_process(boot_info);
    console_puts("[BOOT] Boot information processing completed\n");
    
    /* ==================== 第五阶段：硬件和驱动初始化 ==================== */
    
    /* 【步骤6：硬件信息】 */
    console_puts("\n[BOOT] STEP 6: Hardware Information\n");
    console_puts("[BOOT] Hardware information provided by bootloader\n");
    
    /* 【步骤7：模块自动加载驱动】 */
    console_puts("\n[BOOT] STEP 7: Auto-loading Drivers\n");
    module_auto_load_drivers(&g_boot_state.hw.devices);
    console_puts("[BOOT] Driver auto-loading completed\n");
    
    /* ==================== 第六阶段：模块加载 ==================== */
    
    /* 【步骤8：解析命令行】 */
    console_puts("\n[BOOT] STEP 8: Parsing Command Line\n");
    if (boot_info->cmdline[0] != '\0') {
        console_puts("[BOOT] Command line found, parsing...\n");
        boot_info_parse_cmdline(boot_info->cmdline);
        console_puts("[BOOT] Command line parsed\n");
    } else {
        console_puts("[BOOT] No command line parameters\n");
    }
    
    /* 【步骤9：初始化模块加载器】 */
    console_puts("\n[BOOT] STEP 9: Initializing Module Loader\n");
    module_loader_init();
    console_puts("[BOOT] Module loader initialization completed\n");
    
    /* 【步骤10：加载Bootloader嵌入的模块】 */
    console_puts("\n[BOOT] STEP 10: Loading Embedded Modules\n");
    console_puts("[BOOT] Module count in boot info: ");
    console_putu64(boot_info->module_count);
    console_puts("\n");
    
    if (boot_info->module_count > 0) {
        for (u64 i = 0; i < boot_info->module_count; i++) {
            if (boot_info->modules[i].base != 0) {
                console_puts("[BOOT] Loading module ");
                console_putu64(i);
                console_puts(": ");
                console_puts(boot_info->modules[i].name);
                console_puts(" at 0x");
                console_puthex64((u64)boot_info->modules[i].base);
                console_puts(" (");
                console_putu64(boot_info->modules[i].size);
                console_puts(" bytes)\n");
                
                u64 instance_id;
                hic_status_t status = (hic_status_t)module_load_from_memory(
                    boot_info->modules[i].base,
                    (u32)boot_info->modules[i].size, &instance_id);
                    
                if (status == HIC_SUCCESS) {
                    console_puts("[BOOT]   -> Loaded successfully (instance_id=");
                    console_puthex64(instance_id);
                    console_puts(")\n");
                } else {
                    console_puts("[BOOT]   -> Failed to load (status=");
                    console_putu64(status);
                    console_puts(")\n");
                }
            }
        }
    } else {
        console_puts("[BOOT] No embedded modules found\n");
    }
    
    /* 【步骤10.5：加载 MMIO 配置（从 platform.yaml static_modules[].mmio_regions）】 */
    console_puts("\n[BOOT] STEP 10.5: Loading MMIO configuration\n");
    extern void mmio_load_config_from_yaml(void);
    mmio_load_config_from_yaml();
    console_puts("[BOOT] MMIO configuration loaded\n");

    /* 【步骤11：加载静态模块】 */
    console_puts("\n[BOOT] STEP 11: Loading Static Modules\n");
    console_puts("[BOOT] Loading static modules from configuration...\n");

    /* 在域创建前复制引导信息到静态存储 */
    boot_info_copy_to_static();

    static_module_system_init();
    int static_loaded = static_module_load_all();
    console_puts("[BOOT] Static modules loaded: ");
    console_putu32((u32)static_loaded);
    console_puts("\n");

    /* 【步骤11.5：初始化模块原语系统】 */
    console_puts("\n[BOOT] STEP 11.5: Initializing Module Primitives\n");
    console_puts("[BOOT] Initializing Core-0 module primitives for Privileged-1...\n");
    module_primitives_init();
    console_puts("[BOOT] Module primitives initialized\n");
    
    /* ==================== 第七阶段：最终化 ==================== */
    
    /* 【步骤12：标记启动完成】 */
    console_puts("\n[BOOT] STEP 12: Finalizing Boot\n");
    g_boot_state.valid = 1;
    console_puts("[BOOT] Boot state marked as VALID\n");
    
    /* 【最终报告】 */
    console_puts("[BOOT] RUN\n");

    console_puts("\n");
    
    /* ==================== 第八阶段：主循环 ==================== */
    
    console_puts("\n[HIC] Kernel boot complete - Serial Console\n");

    char cmdline[128];
    int cmdpos = 0;

    while (1) {
        /* 串口输入轮询 */
        char ch = 0;
        if (minimal_uart_try_getc(&ch)) {
            if (ch == '\r' || ch == '\n') {
                cmdline[cmdpos] = '\0';
                hal_uart_putc('\r'); hal_uart_putc('\n');
                if (cmdpos > 0) {
                    console_puts("[HIC] ");
                    console_puts(cmdline);
                    hal_uart_putc('\n');
                }
                console_puts("$ ");
                cmdpos = 0;
            } else if (ch == '\b' || ch == 0x7F) {
                if (cmdpos > 0) { cmdpos--; hal_uart_putc('\b'); hal_uart_putc(' '); hal_uart_putc('\b'); }
            } else if (ch >= ' ' && ch <= '~' && cmdpos < 127) {
                cmdline[cmdpos++] = ch; hal_uart_putc(ch);
            }
        }

        /* 首次 dispatch：依次启动所有就绪的 EFC 线程。
         * 每个线程通过 exec_flow_dispatch 首次运行（context_switch 6reg），
         * 后续 timer ISR 将透明切换已运行的线程（15reg + IRQ 帧）。 */
        if (g_current_thread == NULL) {
            extern void hic_boot_dispatch_all(void);
            hic_boot_dispatch_all();
        }

        /* 定时器与维护 */
        timer_update();
        kernel_maintenance_tasks();

        /* 短暂延迟 */
        for (volatile int i = 0; i < 5000; i++) {}
    }
    
panic:
    console_puts("\n[BOOT] >>> KERNEL PANIC! Halting system... <<<\n");
    hal_halt();
}