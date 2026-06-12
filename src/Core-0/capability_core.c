/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC能力系统 - 简洁安全实现
 * 平衡：简洁性 + 安全性 + 性能
 * 目标：验证速度 < 5ns @ 3GHz
 * 
 * 安全保证（TD文档）：
 * - 域间句柄不可推导（域特定密钥）
 * - 句柄不可伪造（混淆令牌）
 * - 撤销立即生效
 * 
 * 性能优化：
 * - 轻量级混淆（~5条指令）
 * - 内联验证函数
 * - 缓存行对齐
 */

#include "capability.h"
#include "domain.h"
#include "domain_switch.h"
#include "pagetable.h"
#include "pmm.h"
#include "hal.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"
#include "formal_verification.h"

/* ==================== 全局能力表 ==================== */
__capability cap_entry_t g_global_cap_table[CAP_TABLE_SIZE];

/* ==================== Per-core 槽位分配器 ==================== */
/* 每个核独占 CAP_SLOTS_PER_CORE 个槽位，互不竞争 */
u32 g_cap_next_free[CAP_MAX_CORES];

/* ==================== 共享内存区域配置 ==================== */

/* 共享内存区域表 */
shmem_region_t g_shmem_regions[MAX_SHMEM_REGIONS];

/* ==================== 域密钥表（每个域一个） ==================== */
domain_key_t g_domain_keys[HIC_DOMAIN_MAX];

/* ==================== 能力派生跟踪 ==================== */

cap_derivative_t g_derivatives[CAP_TABLE_SIZE];

/* ==================== 初始化 ==================== */

void capability_system_init(void) {
    console_puts("[CAP] Initializing capability system...\n");
    
    memzero(g_global_cap_table, sizeof(g_global_cap_table));
    console_puts("[CAP] Global capability table cleared (");
    console_putu32(CAP_TABLE_SIZE);
    console_puts(" entries)\n");
    
    /* 初始化 per-core 分配指针 */
    for (u32 i = 0; i < CAP_MAX_CORES; i++) {
        g_cap_next_free[i] = cap_core_base(i) + 1;  /* skip slot 0 (HIC_CAP_INVALID) */
    }
    console_puts("[CAP] Per-core slot allocators initialized (");
    console_putu32(CAP_SLOTS_PER_CORE);
    console_puts(" slots/core, ");
    console_putu32(CAP_MAX_CORES);
    console_puts(" cores max)\n");
    
    memzero(g_derivatives, sizeof(g_derivatives));
    console_puts("[CAP] Derivative table cleared\n");
    
    memzero(g_domain_keys, sizeof(g_domain_keys));
    console_puts("[CAP] Domain key table cleared (");
    console_putu32(HIC_DOMAIN_MAX);
    console_puts(" domains)\n");
    
    /* 为 Core-0 初始化密钥 */
    g_domain_keys[HIC_DOMAIN_CORE].seed = 0x12345678;          /* 固定种子，Core-0 域专用 */
    g_domain_keys[HIC_DOMAIN_CORE].multiplier = 0x9E3779B9;    /* 黄金分割比例的小数部分（用于混淆哈希） */
    console_puts("[CAP] Core-0 domain key initialized\n");
    
    console_puts("[CAP] Capability system initialized (no-lock per-core slots)\n");
    console_puts("[CAP] Security: Handle obfuscation enabled\n");
    console_puts("[CAP] Performance: Inlined verification ready\n");
}

/* 初始化域密钥（使用伪随机数） */
void cap_init_domain_key(domain_id_t domain) {
    console_puts("[CAP] Initializing domain key for domain ");
    console_putu64(domain);
    console_puts("...\n");
    
    if (domain >= HIC_DOMAIN_MAX) {
        console_puts("[CAP] ERROR: Invalid domain ID (>= ");
        console_putu32(HIC_DOMAIN_MAX);
        console_puts(")\n");
        return;
    }
    
    /* 使用域ID和时间戳生成伪随机密钥
     * 0x9E3779B9 为黄金分割比小数部分，用作哈希散列乘数 */
    extern u64 hal_get_timestamp(void);
    u64 ts = hal_get_timestamp();

    g_domain_keys[domain].seed = (u32)(ts ^ (domain * 0x9E3779B9));
    g_domain_keys[domain].multiplier = 0x9E3779B9 + domain;
    
    console_puts("[CAP] Domain key initialized: seed=0x");
    console_puthex64((u64)g_domain_keys[domain].seed);
    console_puts(", mult=0x");
    console_puthex64(g_domain_keys[domain].multiplier);
    console_puts("\n");
    console_puts("[CAP] >>> Domain ");
    console_putu64(domain);
    console_puts(" key is now ACTIVE <<<\n");
}

/* ==================== 能力创建 ==================== */

hic_status_t cap_create_memory(domain_id_t owner, phys_addr_t base,
                               size_t size, cap_rights_t rights, cap_id_t *out) {
    if (owner >= HIC_DOMAIN_MAX || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }

    cpu_id_t core = hal_get_cpu_id();
    cap_id_t cap = cap_core_first_free();

    if (cap == HIC_CAP_INVALID) {
        return HIC_ERROR_NO_MEMORY;
    }

    g_global_cap_table[cap].cap_id = cap;
    g_global_cap_table[cap].rights = rights;
    g_global_cap_table[cap].owner = owner;
    g_global_cap_table[cap].owner_core = (u8)core;
    g_global_cap_table[cap].flags = 0;
    g_global_cap_table[cap].memory.base = base;
    g_global_cap_table[cap].memory.size = size;

    /* 如果是设备 MMIO 能力（CAP_MEM_DEVICE），同时映射页表 */
    if (rights & CAP_MEM_DEVICE) {
        page_table_t *pt = domain_switch_get_pagetable(owner);
        if (pt) {
            hic_status_t mst = pagetable_map(pt,
                (virt_addr_t)base, base, size,
                PERM_RW, MAP_TYPE_KERNEL);
            if (mst == HIC_SUCCESS) {
                console_puts("[CAP] Device memory mapped: domain=");
                console_putu32(owner);
                console_puts(" phys=0x");
                console_puthex64(base);
                console_puts(" size=0x");
                console_puthex64(size);
                console_puts("\n");
            }
        }
    }

    *out = cap;
    return HIC_SUCCESS;
}

/* 创建执行流能力 (EFC: Execution Flow Capability) */
hic_status_t cap_create_thread(domain_id_t owner, thread_id_t thread_id, cap_id_t *out) {
    if (owner >= HIC_DOMAIN_MAX || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }

    cpu_id_t core = hal_get_cpu_id();
    cap_id_t cap = cap_core_first_free();

    if (cap == HIC_CAP_INVALID) {
        return HIC_ERROR_NO_RESOURCE;
    }

    g_global_cap_table[cap].cap_id = cap;
    g_global_cap_table[cap].rights = CAP_TYPE_THREAD;
    g_global_cap_table[cap].owner = owner;
    g_global_cap_table[cap].owner_core = (u8)core;
    g_global_cap_table[cap].flags = 0;
    g_global_cap_table[cap].thread_efc.thread_id = thread_id;
    g_global_cap_table[cap].thread_efc.reserved = 0;

    *out = cap;
    return HIC_SUCCESS;
}

/* ==================== 能力授予（返回混淆句柄） ==================== */

hic_status_t cap_grant(domain_id_t domain, cap_id_t cap, cap_handle_t *out) {
    if (domain >= HIC_DOMAIN_MAX || cap >= CAP_TABLE_SIZE || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查能力是否存在 */
    if (g_global_cap_table[cap].cap_id != cap) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 生成混淆句柄 */
    cap_handle_t handle = cap_make_handle(domain, cap);
    
    *out = handle;
    return HIC_SUCCESS;
}

/* ==================== 能力撤销（迭代实现 — 无栈溢出风险） ==================== */

/* 工作队列最大容量 = 全表大小，确保深度任意都不会溢出 */
#define CAP_REVOKE_WORKLIST_SIZE  CAP_TABLE_SIZE

/**
 * @brief 检查并释放共享内存资源
 *
 * 如果能力对应共享内存区域，减少引用计数，
 * 引用计数为0时释放物理内存。
 *
 * 注意：调用者必须持有原子锁
 */
static void cap_release_shmem_if_needed(cap_id_t cap) {
    /* 外部引用：共享内存区域表 */
    extern shmem_region_t g_shmem_regions[];

    if (cap >= CAP_TABLE_SIZE) return;

    cap_entry_t *entry = &g_global_cap_table[cap];

    /* 检查是否为内存能力 */
    if (entry->memory.base == 0 || entry->memory.size == 0) return;

    /* 检查是否属于共享内存区域 */
    for (u32 i = 0; i < MAX_SHMEM_REGIONS; i++) {
        if (g_shmem_regions[i].phys_base == entry->memory.base) {
            /* 找到匹配的共享内存区域 */
            if (g_shmem_regions[i].ref_count > 0) {
                g_shmem_regions[i].ref_count--;

                /* 引用计数为0，释放物理内存 */
                if (g_shmem_regions[i].ref_count == 0) {
                    u32 pages = (u32)((g_shmem_regions[i].size + PAGE_SIZE - 1) / PAGE_SIZE);
                    pmm_free_frames(g_shmem_regions[i].phys_base, pages);
                    g_shmem_regions[i].phys_base = 0;
                    g_shmem_regions[i].size = 0;
                }
            }
            break;
        }
    }
}

/**
 * @brief 迭代式撤销能力树 — 代替递归实现
 *
 * 使用静态工作队列，分两阶段处理：
 *   阶段1：自上而下广度遍历，标记所有要撤销的节点
 *   阶段2：自下而上清理（逆序），释放资源、从父列表移除、清零槽位
 *
 * 安全保证：
 * 1. 先设置 CAP_FLAG_REVOKED 标志，避免重复撤销
 * 2. 派生能力（子树）全部撤销后才清理父能力
 * 3. 自动处理共享内存引用计数
 * 4. 从父能力的子列表中移除，保持一致性
 * 5. 撤销完成后清零 cap_id，使槽位可重用
 * 6. 零栈开销 — 不受内核栈深度限制
 *
 * 注意：调用者必须持有原子锁
 */
static void cap_revoke_iterative(cap_id_t root) {
    static cap_id_t s_worklist[CAP_REVOKE_WORKLIST_SIZE];
    u32 count = 0;

    /* 边界检查 */
    if (root >= CAP_TABLE_SIZE) return;
    if (g_global_cap_table[root].cap_id != root) return;

    /* ---- 阶段1：广度遍历，标记全部 ---- */
    s_worklist[count++] = root;

    u32 head = 0;
    while (head < count) {
        cap_id_t cap = s_worklist[head++];

        if (g_global_cap_table[cap].flags & CAP_FLAG_REVOKED) continue;
        g_global_cap_table[cap].flags |= CAP_FLAG_REVOKED;

        cap_derivative_t *deriv = &g_derivatives[cap];
        for (u32 i = 0; i < deriv->child_count && i < MAX_DERIVATIVES_PER_CAP; i++) {
            cap_id_t child = deriv->children[i];
            if (child != HIC_CAP_INVALID && child < CAP_TABLE_SIZE) {
                if (count < CAP_REVOKE_WORKLIST_SIZE) {
                    s_worklist[count++] = child;
                }
            }
        }
    }

    /* ---- 阶段2：逆序清理（等阶后序遍历） ---- */
    while (count > 0) {
        cap_id_t cap = s_worklist[--count];

        /* 检查并释放共享内存资源 */
        cap_release_shmem_if_needed(cap);

        cap_derivative_t *deriv = &g_derivatives[cap];

        /* 从父能力的子列表中移除当前能力 */
        cap_id_t parent = deriv->parent;
        if (parent != HIC_CAP_INVALID && parent < CAP_TABLE_SIZE) {
            cap_derivative_t *parent_deriv = &g_derivatives[parent];
            for (u32 i = 0; i < parent_deriv->child_count && i < MAX_DERIVATIVES_PER_CAP; i++) {
                if (parent_deriv->children[i] == cap) {
                    /* 找到，用最后一个元素覆盖（避免移动数组） */
                    parent_deriv->children[i] = parent_deriv->children[parent_deriv->child_count - 1];
                    parent_deriv->child_count--;
                    break;
                }
            }
        }

        /* 清空子能力列表 */
        deriv->child_count = 0;
        deriv->parent = HIC_CAP_INVALID;

        /* 清零 cap_id，使槽位可重用 */
        g_global_cap_table[cap].cap_id = 0;
        g_global_cap_table[cap].flags = 0;
        g_global_cap_table[cap].rights = 0;
        g_global_cap_table[cap].owner = 0;
    }
}

/**
 * @brief 撤销能力及其所有派生能力
 * 
 * 满足"撤销立即生效"的安全规范：
 * - 撤销父能力时，所有派生能力同步失效
 * - 整个递归过程在原子操作保护内
 * 
 * @param cap 要撤销的能力ID
 * @return 状态码
 */
hic_status_t cap_revoke(cap_id_t cap) {
    if (cap == HIC_CAP_INVALID) return HIC_ERROR_CAP_INVALID;
    if (cap >= CAP_TABLE_SIZE) return HIC_ERROR_INVALID_PARAM;

    /* Per-core slot 分区：只有 owner_core 能撤销 */
    u8 core = g_global_cap_table[cap].owner_core;
    if (hal_get_cpu_id() != (cpu_id_t)core) {
        return HIC_ERROR_PERMISSION;
    }

    if (g_global_cap_table[cap].cap_id != cap) {
        return HIC_ERROR_CAP_INVALID;
    }
    if (g_global_cap_table[cap].flags & CAP_FLAG_REVOKED) {
        return HIC_SUCCESS;
    }

    cap_revoke_iterative(cap);
    return HIC_SUCCESS;
}

/* BSP 强制撤销（域回收、系统清理，可跨核操作） */
hic_status_t cap_force_revoke(cap_id_t cap) {
    if (cap >= CAP_TABLE_SIZE) return HIC_ERROR_INVALID_PARAM;
    if (g_global_cap_table[cap].cap_id != cap) return HIC_ERROR_CAP_INVALID;
    if (g_global_cap_table[cap].flags & CAP_FLAG_REVOKED) return HIC_SUCCESS;
    cap_revoke_iterative(cap);
    return HIC_SUCCESS;
}

/* ==================== 能力传递（创建新句柄） ==================== */

hic_status_t cap_transfer(domain_id_t from, domain_id_t to, cap_id_t cap, cap_handle_t *out) {
    if (from >= HIC_DOMAIN_MAX || to >= HIC_DOMAIN_MAX || 
        cap >= CAP_TABLE_SIZE || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (g_global_cap_table[cap].cap_id != cap) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    if (g_global_cap_table[cap].owner != from) {
        return HIC_ERROR_PERMISSION;
    }
    
    /* 改变所有者 */
    g_global_cap_table[cap].owner = to;
    
    /* 为目标域生成新句柄 */
    return cap_grant(to, cap, out);
}

/* ==================== 能力传递（带权限衰减） - 机制层 ==================== */

hic_status_t cap_transfer_with_attenuation(domain_id_t from, domain_id_t to, 
                                            cap_id_t cap, cap_rights_t attenuated_rights,
                                            cap_handle_t *out) {
    if (from >= HIC_DOMAIN_MAX || to >= HIC_DOMAIN_MAX || 
        cap >= CAP_TABLE_SIZE || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }

    cap_entry_t *entry = &g_global_cap_table[cap];
    if (entry->cap_id != cap || entry->owner != from) {
        return HIC_ERROR_CAP_INVALID;
    }
    if ((attenuated_rights & entry->rights) != attenuated_rights) {
        return HIC_ERROR_PERMISSION;
    }

    cap_id_t new_cap = cap_core_first_free();
    if (new_cap == HIC_CAP_INVALID) {
        return HIC_ERROR_NO_MEMORY;
    }

    cap_entry_t *new_entry = &g_global_cap_table[new_cap];
    new_entry->cap_id = new_cap;
    new_entry->rights = attenuated_rights;
    new_entry->owner = to;
    new_entry->owner_core = (u8)hal_get_cpu_id();
    new_entry->flags = 0;
    new_entry->memory = entry->memory;
    new_entry->logical_core = entry->logical_core;

    if (g_derivatives[cap].child_count < MAX_DERIVATIVES_PER_CAP) {
        g_derivatives[cap].children[g_derivatives[cap].child_count++] = new_cap;
    }

    return cap_grant(to, new_cap, out);
}

/* ==================== 能力派生 ==================== */

hic_status_t cap_derive(domain_id_t owner, cap_id_t parent, cap_rights_t sub_rights, cap_id_t *out) {
    if (owner >= HIC_DOMAIN_MAX || parent >= CAP_TABLE_SIZE || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查父能力是否存在 */
    if (g_global_cap_table[parent].cap_id != parent || 
        (g_global_cap_table[parent].flags & CAP_FLAG_REVOKED)) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查所有权 */
    if (g_global_cap_table[parent].owner != owner) {
        return HIC_ERROR_PERMISSION;
    }
    
    /* 检查权限单调性：派生权限必须是父权限的子集 */
    if ((sub_rights & ~g_global_cap_table[parent].rights) != 0) {
        return HIC_ERROR_PERMISSION;
    }

    cap_id_t cap = cap_core_first_free();
    if (cap == HIC_CAP_INVALID) {
        return HIC_ERROR_NO_MEMORY;
    }

    g_global_cap_table[cap].cap_id = cap;
    g_global_cap_table[cap].rights = sub_rights;
    g_global_cap_table[cap].owner = owner;
    g_global_cap_table[cap].owner_core = (u8)hal_get_cpu_id();
    g_global_cap_table[cap].flags = 0;
    g_global_cap_table[cap].memory = g_global_cap_table[parent].memory;
    g_global_cap_table[cap].logical_core = g_global_cap_table[parent].logical_core;

    u8 pmp = g_global_cap_table[parent].logical_core.max_derived_policy;
    g_global_cap_table[cap].logical_core.max_derived_policy = pmp;

    if (g_derivatives[parent].child_count < MAX_DERIVATIVES_PER_CAP) {
        g_derivatives[parent].children[g_derivatives[parent].child_count++] = cap;
        g_derivatives[cap].parent = parent;
    }

    *out = cap;
    return HIC_SUCCESS;
}

/**
 * 能力派生（带策略衰减）
 * 
 * 派生能力时同时衰减调度策略。
 * 子能力的策略不能高于父能力的 max_derived_policy。
 */
hic_status_t cap_derive_with_policy(domain_id_t owner, cap_id_t parent, 
                                     cap_rights_t sub_rights,
                                     u8 derived_policy,
                                     cap_id_t *out) {
    /* 先进行基础派生 */
    hic_status_t status = cap_derive(owner, parent, sub_rights, out);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    cap_id_t cap = *out;
    
    /* 检查策略单调性 */
    u8 parent_max_policy = g_global_cap_table[parent].logical_core.max_derived_policy;
    
    if (!domain_check_policy_derivation((domain_sched_policy_t)parent_max_policy,
                                         (domain_sched_policy_t)derived_policy)) {
        /* 策略提升被禁止，回滚 */
        g_global_cap_table[cap].cap_id = 0;  /* 标记为无效 */
        return HIC_ERROR_PERMISSION;
    }
    
    /* 设置派生能力的策略 */
    g_global_cap_table[cap].logical_core.sched_policy = derived_policy;
    g_global_cap_table[cap].logical_core.max_derived_policy = derived_policy;
    
    return HIC_SUCCESS;
}

/* ==================== 能力验证（完整版本） ==================== */

hic_status_t cap_check_access(domain_id_t domain, cap_handle_t handle, cap_rights_t required) {
    if (handle == CAP_HANDLE_INVALID) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    cap_id_t cap_id = cap_get_cap_id(handle);
    
    if (cap_id >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 内存屏障确保读取顺序 */
    atomic_acquire_barrier();
    
    cap_entry_t *entry = &g_global_cap_table[cap_id];
    
    /* 检查能力ID */
    if (entry->cap_id != cap_id) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查是否撤销 */
    if (entry->flags & CAP_FLAG_REVOKED) {
        return HIC_ERROR_CAP_REVOKED;
    }
    
    /* 检查权限 */
    if ((entry->rights & required) != required) {
        return HIC_ERROR_PERMISSION;
    }
    
    /* 验证令牌（确保句柄属于该域） */
    u32 token = cap_get_token(handle);
    if (!cap_validate_token(domain, cap_id, token)) {
        return HIC_ERROR_PERMISSION;
    }
    
    return HIC_SUCCESS;
}


/* ==================== 能力辅助函数 ==================== */

/* 检查能力是否存在 */
bool capability_exists(cap_id_t cap) {
    if (cap >= CAP_TABLE_SIZE) {
        return false;
    }
    return g_global_cap_table[cap].cap_id == cap && 
           !(g_global_cap_table[cap].flags & CAP_FLAG_REVOKED);
}

/* 获取能力权限 */
u64 get_capability_permissions(cap_id_t cap) {
    if (!capability_exists(cap)) {
        return 0;
    }
    return (u64)g_global_cap_table[cap].rights;
}

/* 获取能力对象类型 */
obj_type_t get_capability_object_type(cap_id_t cap) {
    if (!capability_exists(cap)) {
        return OBJ_MEMORY;  /* 默认返回内存类型 */
    }

    cap_rights_t rights = g_global_cap_table[cap].rights;
    cap_entry_t *entry = &g_global_cap_table[cap];

    /* 根据权限特征和大小判断对象类型 */
    if (rights & CAP_MEM_DEVICE) {
        return OBJ_DEVICE;  /* 设备 */
    }

    /* 检查是否为共享内存 */
    if ((rights & (CAP_MEM_READ | CAP_MEM_WRITE)) == (CAP_MEM_READ | CAP_MEM_WRITE) &&
        entry->memory.size > 0 && !(rights & CAP_MEM_EXEC)) {
        return OBJ_SHARED;
    }

    /* 检查是否为线程相关 */
    if (rights & (CAP_LCORE_USE | CAP_LCORE_QUERY)) {
        return OBJ_THREAD;
    }

    return OBJ_MEMORY;  /* 默认内存类型 */
}

/* 获取能力类型 */
cap_type_t get_capability_type(cap_id_t cap) {
    if (!capability_exists(cap)) {
        return CAP_MEMORY;
    }
    
    cap_rights_t rights = g_global_cap_table[cap].rights;
    cap_entry_t *entry = &g_global_cap_table[cap];
    
    /* 根据权限特征判断能力类型 */
    
    /* MMIO区域能力 */
    if (rights & CAP_MEM_DEVICE) {
        return CAP_MMIO;
    }
    
    /* 中断能力 */
    if (rights & (1U << 20)) {  /* 假设中断权限位 */
        return CAP_IRQ;
    }
    
    /* 服务能力 */
    if (rights & CAP_LCORE_QUERY) {
        return CAP_SERVICE;
    }
    
    /* 线程能力 */
    if (rights & (CAP_LCORE_USE | CAP_LCORE_MIGRATE)) {
        return CAP_THREAD;
    }
    
    /* 共享内存能力 */
    if ((rights & (CAP_MEM_READ | CAP_MEM_WRITE)) == (CAP_MEM_READ | CAP_MEM_WRITE) &&
        !(rights & CAP_MEM_EXEC)) {
        return CAP_SHARED;
    }
    
    /* 设备能力 */
    if (rights & (1U << 16)) {  /* 假设设备权限位 */
        return CAP_DEVICE;
    }
    
    /* 派生能力 */
    if (rights & CAP_LCORE_BORROW) {
        return CAP_CAP_DERIVE;
    }
    
    /* 默认内存能力 */
    if (rights & (CAP_MEM_READ | CAP_MEM_WRITE | CAP_MEM_EXEC)) {
        return CAP_MEMORY;
    }
    
    return CAP_MEMORY;
}

/* 查找能力的派生信息 */
cap_id_t* get_capability_derivatives(cap_id_t cap) {
    if (!capability_exists(cap)) {
        return NULL;
    }
    
    /* 
     * 注意：这是一个静态缓冲区，调用者应该立即使用返回值
     * 在下一次调用此函数前使用完毕
     */
    static cap_id_t derivative_list[MAX_DERIVATIVES_PER_CAP + 1];
    
    cap_derivative_t *deriv = &g_derivatives[cap];
    
    if (deriv->child_count == 0) {
        derivative_list[0] = HIC_CAP_INVALID;
        return derivative_list;
    }
    
    /* 复制子能力列表 */
    for (u32 i = 0; i < deriv->child_count && i < MAX_DERIVATIVES_PER_CAP; i++) {
        derivative_list[i] = deriv->children[i];
    }
    derivative_list[deriv->child_count] = HIC_CAP_INVALID;  /* 终止标记 */
    
    return derivative_list;
}




