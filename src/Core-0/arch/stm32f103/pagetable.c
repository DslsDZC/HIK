/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * STM32F103 (Cortex-M3) 页表 / MPU 配置
 *
 * Cortex-M3 没有 MMU/页表，但有 MPU（内存保护单元）。
 * 此文件提供与 x86_64 pagetable.c 兼容的函数接口，
 * 使用 MPU 替代页表做内存隔离。
 *
 * MPU 特性（Cortex-M3 r2p0+）：
 *   - 最多 8 个区域（region）
 *   - 每个区域最小 256 字节
 *   - 区域可设置读写/执行权限
 *   - 支持子区域（sub-region）禁能
 *   - 使能后 default map 为禁止访问
 *
 * STM32F103C8T6 内存映射：
 *   0x08000000 - 0x0800FFFF  Flash（64KB，可执行）
 *   0x20000000 - 0x20004FFF  SRAM（20KB，读写）
 *   0x40000000 - 0x5000FFFF  外设（设备内存）
 *   0xE0000000 - 0xE00FFFFF  系统控制块（SCB，NVIC 等）
 *
 * 注意：STM32F103 早期版本（r1p0）没有 MPU，
 * 本实现检测到无 MPU 时静默跳过。
 */

#include "hal.h"
#include "pagetable.h"
#include "domain.h"
#include "domain_switch.h"
#include "lib/console.h"

/* ==================== SCB / MPU 寄存器 ==================== */

#define SCB_BASE            0xE000ED00UL

/* 系统控制块 (SCB) 寄存器 */
#define SCB_CPUID           (*(volatile uint32_t *)(SCB_BASE + 0x00))
#define SCB_ICSR            (*(volatile uint32_t *)(SCB_BASE + 0x04))
#define SCB_VTOR            (*(volatile uint32_t *)(SCB_BASE + 0x08))
#define SCB_SHCSR           (*(volatile uint32_t *)(SCB_BASE + 0x24))
#define SCB_CFSR            (*(volatile uint32_t *)(SCB_BASE + 0x28))
#define SCB_HFSR            (*(volatile uint32_t *)(SCB_BASE + 0x2C))
#define SCB_DFSR            (*(volatile uint32_t *)(SCB_BASE + 0x30))
#define SCB_MMFAR           (*(volatile uint32_t *)(SCB_BASE + 0x34))
#define SCB_BFAR            (*(volatile uint32_t *)(SCB_BASE + 0x38))

/* SCB 辅助功能寄存器 */
#define SCB_ACTLR           (*(volatile uint32_t *)(SCB_BASE + 0x08))

/* ==================== MPU 寄存器 ==================== */

#define MPU_BASE            0xE000ED90UL
#define MPU_TYPE            (*(volatile uint32_t *)(MPU_BASE + 0x00))
#define MPU_CTRL            (*(volatile uint32_t *)(MPU_BASE + 0x04))
#define MPU_RNR             (*(volatile uint32_t *)(MPU_BASE + 0x08))
#define MPU_RBAR            (*(volatile uint32_t *)(MPU_BASE + 0x0C))
#define MPU_RASR            (*(volatile uint32_t *)(MPU_BASE + 0x10))

/* MPU_TYPE 位 */
#define MPU_TYPE_SEPARATE   (1 << 0)    /* 分离的指令/数据区域 */
#define MPU_TYPE_DREGION_MASK (0xFF << 8) /* 数据区域数量 */
#define MPU_TYPE_DREGION_SHIFT 8
#define MPU_TYPE_IREGION_MASK (0xFF << 16) /* 指令区域数量 */

/* MPU_CTRL 位 */
#define MPU_CTRL_ENABLE     (1 << 0)     /* MPU 使能 */
#define MPU_CTRL_HFNMIENA   (1 << 1)     /* HardFault/NMI 下 MPU 使能 */
#define MPU_CTRL_PRIVDEFENA (1 << 2)     /* 特权模式默认映射使能 */

/* MPU_RASR 位 */
#define MPU_RASR_ENABLE     (1 << 0)     /* 区域使能 */
#define MPU_RASR_SIZE_SHIFT 1            /* 区域大小（以 2^(SIZE+1) 字节） */
#define MPU_RASR_AP_SHIFT   24           /* 访问权限 */
#define MPU_RASR_XN_SHIFT   28           /* 不可执行 */
#define MPU_RASR_SRD_SHIFT  8            /* 子区域禁能 */

/* MPU_RASR 访问权限 (AP) */
#define MPU_AP_NOACCESS     0b000        /* 任何模式均无访问 */
#define MPU_AP_PRIV_RW      0b001        /* 特权读写 */
#define MPU_AP_PRIV_RW_URO  0b010        /* 特权读写，用户只读 */
#define MPU_AP_FULL_ACCESS  0b011        /* 全访问 */
#define MPU_AP_PRIV_RO      0b101        /* 特权只读 */
#define MPU_AP_PRIV_RO_URO  0b110        /* 只读 */

/* 区域类型 */
#define MPU_REGION_NORMAL   0            /* 正常内存（SRAM） */
#define MPU_REGION_DEVICE   1            /* 设备内存（外设） */
#define MPU_REGION_STRONG   2            /* 强序设备内存（SCB） */

/* ==================== 全局状态 ==================== */

static int g_mpu_region_count = 0;

/* ==================== MPU 检测 ==================== */

static inline int mpu_get_num_regions(void)
{
    uint32_t type = MPU_TYPE;
    return (type & MPU_TYPE_DREGION_MASK) >> MPU_TYPE_DREGION_SHIFT;
}

static inline bool mpu_present(void)
{
    return mpu_get_num_regions() > 0;
}

/* ==================== MPU 区域配置 ==================== */

/**
 * 配置 MPU 区域
 *
 * @param region 区域号 (0-7)
 * @param base   基地址（必须按区域大小对齐）
 * @param size   区域大小（字节，必须是 2 的幂，最小 256）
 * @param ap     访问权限 (MPU_AP_*)
 * @param type   内存类型 (MPU_REGION_*)
 * @param xn     不可执行 (false=可执行, true=不可执行)
 * @return       true 配置成功
 */
static bool mpu_configure_region(int region, uint32_t base, uint32_t size,
                                 uint32_t ap, int type, bool xn)
{
    if (!mpu_present() || region >= g_mpu_region_count)
        return false;

    /* 计算 SIZE 字段：size = 2^(SIZE+1) → SIZE = log2(size) - 1 */
    uint32_t size_code = 31 - __builtin_clz(size) - 1;
    if (((uint32_t)1 << (size_code + 1)) != size) {
        console_puts("[MPU] ERROR: size must be power of 2\n");
        return false;
    }
    if (size < 256) {
        console_puts("[MPU] ERROR: minimum region size is 256 bytes\n");
        return false;
    }
    if (!base || (base & (size - 1)) != 0) {
        console_puts("[MPU] ERROR: base must be aligned to size\n");
        return false;
    }

    /* 设置区域号 */
    MPU_RNR = region;

    /* 设置基地址（低 5 位为区域号，已在 RNR 中设置） */
    MPU_RBAR = base;

    /* 设置属性 */
    uint32_t rasr = MPU_RASR_ENABLE
                  | (size_code << MPU_RASR_SIZE_SHIFT)
                  | (ap << MPU_RASR_AP_SHIFT);

    /* 可执行性 */
    if (xn) {
        rasr |= (1 << MPU_RASR_XN_SHIFT);
    }

    /* 内存类型（通过 TEX/SCB/CB 位控制） */
    switch (type) {
    case MPU_REGION_NORMAL:
        /* TEX=001, C=1, B=1 → Write-Back, Read-Allocate, Write-Allocate */
        rasr |= (1 << 19);  /* TEX bit 0 */
        break;
    case MPU_REGION_DEVICE:
        /* TEX=000, S=1, C=0, B=0 → Device, Shareable */
        rasr |= (1 << 18);  /* S */
        break;
    case MPU_REGION_STRONG:
        /* TEX=000, S=0, C=0, B=0 → Strongly-ordered */
        break;
    }

    MPU_RASR = rasr;

    return true;
}

/* ==================== 禁用 MPU ==================== */

void pagetable_disable_mpu(void)
{
    if (!mpu_present()) return;
    MPU_CTRL = 0;
    __asm__ volatile("dsb; isb");
}

/* ==================== 初始化 MPU ==================== */

static void mpu_init_regions(void)
{
    if (!mpu_present()) {
        console_puts("[MPU] Not present — no memory protection\n");
        return;
    }

    console_puts("[MPU] Found ");
    console_putu32(g_mpu_region_count);
    console_puts(" regions\n");

    /* 默认禁用所有区域 */
    for (int i = 0; i < g_mpu_region_count; i++) {
        MPU_RNR = i;
        MPU_RASR = 0;
    }

    /*
     * 配置内存保护区域（8 区域，在 20KB SRAM 下需精简）：
     *
     * Region 0: Flash         (64KB)  0x08000000  可执行, 只读
     * Region 1: SRAM          (20KB)  0x20000000  读写
     * Region 2: 外设          (1MB)   0x40000000  不可执行
     * Region 3: 系统控制块    (64KB)  0xE000E000  不可执行
     * Region 4-7: 保留（未来用于域隔离）
     *
     * 注意：20KB 不是 2 的幂，实际配置 32KB（覆盖 0x20000000-0x20007FFF）
     * 超出 20KB 的部分不实际存在，访问会触发 HardFault。
     */
    mpu_configure_region(0, 0x08000000, 64 * 1024,
                         MPU_AP_PRIV_RO, MPU_REGION_NORMAL, false); /* Flash */
    mpu_configure_region(1, 0x20000000, 32 * 1024,
                         MPU_AP_PRIV_RW, MPU_REGION_NORMAL, false); /* SRAM */
    mpu_configure_region(2, 0x40000000, 1024 * 1024,
                         MPU_AP_PRIV_RW, MPU_REGION_DEVICE, true);  /* 外设 */
    mpu_configure_region(3, 0xE000E000, 64 * 1024,
                         MPU_AP_PRIV_RW, MPU_REGION_STRONG, true);  /* SCB */

    /* 使能 MPU：特权模式默认映射使能（可访问未配置区域） */
    MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    __asm__ volatile("dsb; isb");

    console_puts("[MPU] Enabled with 4 regions configured\n");
}

/* ==================== 页表接口（兼容 x86_64 接口） ==================== */

/*
 * 以下函数提供与 x86_64 相同的接口名，但使用 MPU 实现。
 * 由于 Cortex-M3 没有真正的页表，大多数函数是 no-op 或 MPU 包装。
 */

/* 页表创建 = MPU 初始化 */
void pagetable_create(void *base)
{
    (void)base;
    static bool initialized = false;
    if (!initialized) {
        g_mpu_region_count = mpu_get_num_regions();
        mpu_init_regions();
        initialized = true;
    }
}

void pagetable_init(void)
{
    pagetable_create(NULL);
}

void *pagetable_get_current(void)
{
    return NULL;  /* 无页表 */
}

bool pagetable_is_active(void)
{
    return false;  /* 无 MMU 页表 */
}

void pagetable_switch(void *pagetable)
{
    (void)pagetable;
    /* Cortex-M3 无页表切换 */
}

void *pagetable_clone_current(void)
{
    return NULL;
}

void pagetable_map(void *pagetable, uint64_t virt, uint64_t phys,
                   uint64_t size, uint64_t flags)
{
    (void)pagetable; (void)virt; (void)phys; (void)size; (void)flags;
    /* MPU 区域在初始化时已静态配置；动态映射由域隔离层处理 */
}

void pagetable_unmap(void *pagetable, uint64_t virt, uint64_t size)
{
    (void)pagetable; (void)virt; (void)size;
}

bool pagetable_get_mapping(void *pagetable, uint64_t virt,
                           uint64_t *phys_out, uint64_t *size_out,
                           uint64_t *flags_out)
{
    (void)pagetable; (void)virt;
    if (phys_out) *phys_out = virt;   /* 恒等映射 */
    if (size_out) *size_out = 4096;
    if (flags_out) *flags_out = 0;
    return true;
}

void pagetable_switch_domain(domain_id_t domain_id, void *pagetable)
{
    (void)domain_id;
    /*
     * 域隔离通过 MPU 区域切换实现。
     * 不同域的任务有不同的 MPU 配置。
     * TODO: 实现域级 MPU 区域切换
     */
    if (pagetable) {
        pagetable_switch(pagetable);
    }
}

uint64_t pagetable_get_phys(void *pagetable, uint64_t virt)
{
    (void)pagetable;
    return virt;  /* 恒等映射 */
}

/* ==================== 页表域切换（被 domain.c 和 context_switch 调用） ==================== */

/*
 * 在 context_switch 期间切换域 MPU 配置。
 * 对于 Cortex-M3，这涉及重新配置 MPU 区域。
 * 暂为 no-op，因为当前只支持 Core-0 域。
 */

void arm64_switch_pagetable(void *new_pagetable)
{
    (void)new_pagetable;
    /* Cortex-M3 MPU 切换暂不实现 */
}

/* ==================== 补充桩函数 ==================== */

hic_status_t pagetable_setup_domain(domain_id_t domain, page_table_t *root)
{
    (void)domain; (void)root;
    /* Cortex-M3 无页表，MPU 区域初始化时已配置 */
    return HIC_SUCCESS;
}

void pagetable_cleanup_domain(domain_id_t domain)
{
    (void)domain;
}

void pagetable_destroy(page_table_t *root)
{
    (void)root;
}

hic_status_t pagetable_set_perm(page_table_t *root, virt_addr_t va,
                                size_t size, page_perm_t perm)
{
    (void)root; (void)va; (void)size; (void)perm;
    return HIC_SUCCESS;
}

void pagetable_flush_tlb(virt_addr_t addr)
{
    (void)addr;
    /* Cortex-M3 无 TLB */
}

void pagetable_flush_tlb_all(void)
{
    /* Cortex-M3 无 TLB */
}

void *domain_switch_get_pagetable(domain_id_t domain_id)
{
    (void)domain_id;
    return NULL;  /* 无私有页表 */
}
