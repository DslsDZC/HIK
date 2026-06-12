/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/*
 * HIC 引导信息 TLV 格式
 *
 * Bootloader 将引导信息写入固定物理地址 BOOT_INFO_ADDR，
 * 内核从此地址读取并解析 TLV 记录。内核不做任何硬件探测，
 * 所有硬件信息由 Bootloader 提供。
 */

#ifndef HIC_KERNEL_BOOT_INFO_H
#define HIC_KERNEL_BOOT_INFO_H

#include <stdint.h>
#include "types.h"
#include "kernel.h"
#include "hardware_probe.h"
#include "hardware_probe.h"

/* ===== TLV 传输格式 ===== */

#define BOOT_INFO_ADDR      0x50000000
#define BOOT_INFO_MAGIC     0x48494342  /* "HICB" */

typedef struct {
    u32 magic;
    u32 total_size;
    u32 version;
    u32 flags;
    u32 entry_count;
    u8  reserved[8];
} boot_info_header_t;

typedef struct {
    u32 tag;
    u32 len;
    u8  data[];
} __attribute__((packed)) boot_info_tlv_t;

/* TLV Tags */
#define TAG_END             0
#define TAG_MEM_MAP         1
#define TAG_CPU_COUNT       2
#define TAG_CPU_FEAT        3
#define TAG_CMDLINE         4
#define TAG_RSDP            5
#define TAG_FRAMEBUFFER     6
#define TAG_SERIAL_PORT     7
#define TAG_KERNEL_BASE     8
#define TAG_KERNEL_SIZE     9
#define TAG_ENTRY_POINT     10
#define TAG_STACK_TOP       11
#define TAG_MODULE          12
#define TAG_DISK_INFO       13
#define TAG_PLATFORM_DATA   14
#define TAG_HARDWARE_DATA   15
#define TAG_GDT             16
#define TAG_ARCH            17

/* ===== 运行时表示（由 TLV 解析填充） ===== */

#define HIC_BOOT_INFO_MAGIC  0x48494B21
#define HIC_BOOT_INFO_VERSION 1

#define HIC_MEM_TYPE_USABLE      1
#define HIC_MEM_TYPE_RESERVED    2
#define HIC_MEM_TYPE_ACPI        3
#define HIC_MEM_TYPE_NVS         4
#define HIC_MEM_TYPE_UNUSABLE    5
#define HIC_MEM_TYPE_BOOTLOADER  6
#define HIC_MEM_TYPE_KERNEL      7
#define HIC_MEM_TYPE_MODULE      8

typedef struct {
    u64 base_address;
    u64 length;
    u32 type;
    u32 attributes;
} hic_mem_entry_t;

struct hardware_probe_result;
typedef struct hardware_probe_result hardware_probe_result_t;

typedef struct {
    u32 magic;
    u32 version;
    u64 flags;

    hic_mem_entry_t* mem_map;
    u64 mem_map_entry_count;

    void* rsdp;

    void* kernel_base;
    u64 kernel_size;
    u64 entry_point;

    char cmdline[256];

    struct {
        void* base;
        u64 size;
        char name[64];
    } modules[16];
    u64 module_count;

    struct {
        void* disk_base;
        u64 disk_size;
        u32 sector_size;
    } disk;

    struct {
        u32 cpu_count;
        u32 memory_size_mb;
        u32 architecture;
        u32 platform_type;
    } system;

    u8 firmware_type;
    u8 reserved[7];

    u64 stack_top;
    u64 stack_size;

    struct {
        u32 framebuffer_base;
        u32 framebuffer_size;
        u32 width;
        u32 height;
        u32 pitch;
        u32 bpp;
    } video;

    struct {
        u16 serial_port;
        u16 debug_flags;
        void* log_buffer;
        u64 log_size;
    } debug;

    struct {
        void* config_data;
        u64 config_size;
        u64 config_hash;
    } config;

    struct {
        void* platform_data;
        u64 platform_size;
        u64 platform_hash;
    } platform;

    struct {
        void* hw_data;
        u64 hw_size;
        u64 hw_hash;
    } hardware;

    struct {
        u64 gdt[6];
        u16 gdt_limit;
        u64 gdt_base;
    } gdt;

    struct {
        void* magic_region_base;
        u64 magic_region_size;
        u32 embedded_module_count;
    } embedded_modules;

} hic_boot_info_t;

#define HIC_BOOT_FLAG_ACPI_ENABLED  (1ULL << 1)

typedef struct boot_state {
    hic_boot_info_t* boot_info;
    hardware_probe_result_t hw;
    u8 valid;
    bool recovery_mode;
    u16 serial_port;
    u32 serial_baud;
    bool debug_enabled;
    bool quiet_mode;
} boot_state_t;

extern boot_state_t g_boot_state;

/* ===== TLV 解析接口 ===== */

extern hic_boot_info_t *g_boot_info;

/* 从固定地址读取并解析 TLV，填充 g_boot_info */
void boot_info_parse_tlv(void);





/* 按 tag 查找 TLV 记录 */
const boot_info_tlv_t* boot_info_find_tag(u32 tag);

/* 内部运行时接口（由 TLV 解析结果驱动） */
bool boot_info_validate(hic_boot_info_t* boot_info);
void boot_info_process(hic_boot_info_t* boot_info);
void boot_info_init_memory(hic_boot_info_t* boot_info);
void boot_info_parse_cmdline(const char* cmdline);
void boot_info_copy_to_static(void);
boot_state_t* get_boot_state(void);

void timer_update(void);
void kernel_maintenance_tasks(void);

#endif /* BOOT_INFO_H */
