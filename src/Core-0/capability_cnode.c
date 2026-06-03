/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC能力系统 - 树状能力空间（CNode/CSpace/CPtr）
 *
 * 平衡 seL4 灵活性与嵌入式确定性：
 * - 全局能力表：固定大小数组，编译时常量
 * - CNode 树：任意深度，灵活权限组织
 * - CPtr：编码路径的整数，快速解析
 * - CDT（能力派生树）：支持撤销传播
 *
 * 包含：CNode 内存管理、CNode 操作、CPtr 解析、
 *       CSpace 操作、能力撤销扩展、能力缓存
 */

#include "capability.h"
#include "domain.h"
#include "pmm.h"
#include "hal.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"

/* ==================== 树状能力空间（CNode/CSpace/CPtr）实现 ==================== */

/* CNode 内存池（预分配） */
#define CNODE_POOL_SIZE     256
#define CNODE_BLOCK_SIZE    8192

/* CNode 内存池结构 */
static struct {
    cnode_t *cnode;
    u8 in_use;
} g_cnode_pool[CNODE_POOL_SIZE];

/* CNode 内存块池（移到函数外部，确保正确初始化） */
static char g_cnode_memory_pool[CNODE_POOL_SIZE][CNODE_BLOCK_SIZE];

/* 域 CSpace 表 */
static cspace_t g_domain_cspaces[HIC_DOMAIN_MAX];

/* 能力缓存 */
cap_cache_entry_t g_cap_cache[CAP_CACHE_SIZE];

/* ==================== CNode 内存管理 ==================== */

/**
 * @brief 计算槽位索引位数
 */
static u8 cnode_calc_slot_bits(u32 slot_count) {
    u8 bits = 0;
    while ((1U << bits) < slot_count && bits < 16) {
        bits++;
    }
    return bits;
}

/**
 * @brief 分配 CNode 内存（从预分配池）
 */
static cnode_t* cnode_alloc(u16 slot_count) {
    /* 计算需要的内存大小：cnode_t + 槽位数组 */
    size_t size = sizeof(cnode_t) + slot_count * sizeof(cnode_slot_t);

    /* 从池中查找空闲块 */
    for (u32 i = 0; i < CNODE_POOL_SIZE; i++) {
        if (!g_cnode_pool[i].in_use) {
            /* 简化实现：每个池项是一个固定大小的内存块 */
            /* 实际实现应该从 PMM 分配 */

            if (size <= CNODE_BLOCK_SIZE) {
                g_cnode_pool[i].cnode = (cnode_t*)g_cnode_memory_pool[i];
                g_cnode_pool[i].in_use = 1;
                memzero(g_cnode_pool[i].cnode, size);
                return g_cnode_pool[i].cnode;
            }
        }
    }

    return NULL;
}

/**
 * @brief 获取 CNode 块的尾部空闲区（供 domain_create 零栈临时存储）
 *
 * CNode 内存块为 8KB，顶部被 cnode_t 头部 + slot 数组占用。
 * 尾部剩余空间可复用为域创建时的临时数据区，零栈开销。
 *
 * 内存布局:
 * ┌─ CNODE_BLOCK_SIZE (8192) ──────────────────┐
 * │ cnode_t + slot[]  (实际使用 ~4128B)        │
 * ├────────────────────────────────────────────┤
 * │ 空闲尾部 (4064B) ← 用于 domain_create 暂存  │
 * └────────────────────────────────────────────┘
 *
 * @param cnode  CNode 指针（cnode_alloc 的返回值）
 * @param nslots 此 CNode 的槽位数
 * @return 尾部空闲区的起始地址
 */
void* cnode_tail_scratch(cnode_t *cnode, u16 nslots)
{
    if (!cnode) return NULL;
    size_t used = sizeof(cnode_t) + (size_t)nslots * sizeof(cnode_slot_t);
    if (used >= CNODE_BLOCK_SIZE) return NULL;
    return (u8*)cnode + used;
}

/**
 * @brief 释放 CNode 内存
 */
static void cnode_free(cnode_t *cnode) {
    for (u32 i = 0; i < CNODE_POOL_SIZE; i++) {
        if (g_cnode_pool[i].cnode == cnode) {
            g_cnode_pool[i].in_use = 0;
            g_cnode_pool[i].cnode = NULL;
            return;
        }
    }
}

/* ==================== CNode 操作实现 ==================== */

/**
 * @brief 创建 CNode
 */
hic_status_t cnode_create(domain_id_t owner, u8 slot_bits, cap_id_t *out_cnode_cap) {
    if (owner >= HIC_DOMAIN_MAX || slot_bits > 12 || !out_cnode_cap) {
        return HIC_ERROR_INVALID_PARAM;
    }

    u16 slot_count = 1U << slot_bits;

    cap_id_t cap = cap_core_first_free();
    if (cap == HIC_CAP_INVALID) {
        return HIC_ERROR_NO_RESOURCE;
    }

    bool irq = atomic_enter_critical();

    cnode_t *cnode = cnode_alloc(slot_count);
    if (!cnode) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_MEMORY;
    }

    /* 初始化 CNode */
    cnode->self_cap = cap;
    cnode->owner = owner;
    cnode->slot_count = slot_count;
    cnode->slot_bits = slot_bits;
    cnode->depth = 0;  /* 创建时为 0，插入到父 CNode 时更新 */
    cnode->guard = 0;
    cnode->guard_bits = 0;
    cnode->flags = 0;

    /* 初始化能力表条目 */
    g_global_cap_table[cap].cap_id = cap;
    g_global_cap_table[cap].rights = CAP_CNODE;
    g_global_cap_table[cap].owner = owner;
    g_global_cap_table[cap].owner_core = (u8)hal_get_cpu_id();
    g_global_cap_table[cap].flags = 0;

    /* 存储指向 CNode 的指针 */
    g_global_cap_table[cap].memory.base = (phys_addr_t)cnode;
    g_global_cap_table[cap].memory.size = sizeof(cnode_t) + slot_count * sizeof(cnode_slot_t);

    /* 初始化槽位数组 */
    cnode_slot_t *slots = cnode_get_slots(cnode);
    for (u32 i = 0; i < slot_count; i++) {
        slots[i].cap_id = HIC_CAP_INVALID;
        slots[i].rights_mask = 0;
        slots[i].guard = 0;
        slots[i].flags = CNODE_SLOT_EMPTY;
    }

    atomic_exit_critical(irq);

    *out_cnode_cap = cap;
    return HIC_SUCCESS;
}

/**
 * @brief 销毁 CNode
 */
hic_status_t cnode_destroy(cap_id_t cnode_cap) {
    if (cnode_cap >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cap_entry_t *entry = &g_global_cap_table[cnode_cap];

    /* 检查是否为 CNode */
    if (!(entry->rights & CAP_CNODE)) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    cnode_t *cnode = (cnode_t*)entry->memory.base;
    if (!cnode) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    /* 递归撤销子树 */
    cnode_revoke_subtree(cnode_cap);

    /* 释放 CNode 内存 */
    cnode_free(cnode);

    /* 清零能力表条目 */
    entry->cap_id = 0;
    entry->rights = 0;
    entry->flags = 0;

    atomic_exit_critical(irq);

    return HIC_SUCCESS;
}

/**
 * @brief 在 CNode 中插入能力
 */
hic_status_t cnode_insert(cap_id_t cnode_cap, u32 index,
                          cap_id_t cap_id, cap_rights_t rights_mask) {
    if (cnode_cap >= CAP_TABLE_SIZE || cap_id >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cap_entry_t *entry = &g_global_cap_table[cnode_cap];

    /* 检查是否为 CNode */
    if (!(entry->rights & CAP_CNODE)) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    cnode_t *cnode = (cnode_t*)entry->memory.base;
    if (!cnode) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    /* 检查索引范围 */
    if (index >= cnode->slot_count) {
        atomic_exit_critical(irq);
        return HIC_ERROR_INVALID_PARAM;
    }

    cnode_slot_t *slots = cnode_get_slots(cnode);

    /* 检查目标能力有效性 */
    if (g_global_cap_table[cap_id].cap_id != cap_id) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    /* 检查权限单调性 */
    cap_rights_t base_rights = g_global_cap_table[cap_id].rights;
    if ((rights_mask & base_rights) != rights_mask) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;
    }

    /* 插入能力 */
    slots[index].cap_id = cap_id;
    slots[index].rights_mask = rights_mask;
    slots[index].flags = CNODE_SLOT_VALID;

    /* 如果是 CNode，更新其深度 */
    if (g_global_cap_table[cap_id].rights & CAP_CNODE) {
        cnode_t *child_cnode = (cnode_t*)g_global_cap_table[cap_id].memory.base;
        if (child_cnode) {
            child_cnode->depth = cnode->depth + 1;
            slots[index].flags |= CNODE_SLOT_CNODE;
        }
    }

    /* 增加 CDT 引用计数 */
    g_derivatives[cap_id].child_count++;  /* 简化：实际应该单独跟踪 */

    atomic_exit_critical(irq);

    return HIC_SUCCESS;
}

/**
 * @brief 从 CNode 中移除能力
 */
hic_status_t cnode_remove(cap_id_t cnode_cap, u32 index) {
    if (cnode_cap >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cap_entry_t *entry = &g_global_cap_table[cnode_cap];

    if (!(entry->rights & CAP_CNODE)) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    cnode_t *cnode = (cnode_t*)entry->memory.base;
    if (!cnode || index >= cnode->slot_count) {
        atomic_exit_critical(irq);
        return HIC_ERROR_INVALID_PARAM;
    }

    cnode_slot_t *slots = cnode_get_slots(cnode);

    /* 清空槽位 */
    slots[index].cap_id = HIC_CAP_INVALID;
    slots[index].rights_mask = 0;
    slots[index].flags = CNODE_SLOT_EMPTY;

    atomic_exit_critical(irq);

    return HIC_SUCCESS;
}

/**
 * @brief 移动能力
 */
hic_status_t cnode_move(cap_id_t src_cnode, u32 src_index,
                        cap_id_t dst_cnode, u32 dst_index) {
    if (src_cnode >= CAP_TABLE_SIZE || dst_cnode >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cap_entry_t *src_entry = &g_global_cap_table[src_cnode];
    cap_entry_t *dst_entry = &g_global_cap_table[dst_cnode];

    if (!(src_entry->rights & CAP_CNODE) || !(dst_entry->rights & CAP_CNODE)) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    cnode_t *src = (cnode_t*)src_entry->memory.base;
    cnode_t *dst = (cnode_t*)dst_entry->memory.base;

    if (!src || !dst || src_index >= src->slot_count || dst_index >= dst->slot_count) {
        atomic_exit_critical(irq);
        return HIC_ERROR_INVALID_PARAM;
    }

    cnode_slot_t *src_slots = cnode_get_slots(src);
    cnode_slot_t *dst_slots = cnode_get_slots(dst);

    /* 移动（不增加引用计数） */
    dst_slots[dst_index] = src_slots[src_index];

    src_slots[src_index].cap_id = HIC_CAP_INVALID;
    src_slots[src_index].flags = CNODE_SLOT_EMPTY;

    atomic_exit_critical(irq);

    return HIC_SUCCESS;
}

/**
 * @brief 复制能力
 */
hic_status_t cnode_copy(cap_id_t src_cnode, u32 src_index,
                        cap_id_t dst_cnode, u32 dst_index,
                        cap_rights_t rights_mask) {
    if (src_cnode >= CAP_TABLE_SIZE || dst_cnode >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cap_entry_t *src_entry = &g_global_cap_table[src_cnode];
    cap_entry_t *dst_entry = &g_global_cap_table[dst_cnode];

    if (!(src_entry->rights & CAP_CNODE) || !(dst_entry->rights & CAP_CNODE)) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    cnode_t *src = (cnode_t*)src_entry->memory.base;
    cnode_t *dst = (cnode_t*)dst_entry->memory.base;

    if (!src || !dst || src_index >= src->slot_count || dst_index >= dst->slot_count) {
        atomic_exit_critical(irq);
        return HIC_ERROR_INVALID_PARAM;
    }

    cnode_slot_t *src_slots = cnode_get_slots(src);
    cnode_slot_t *dst_slots = cnode_get_slots(dst);

    /* 检查源槽位有效性 */
    if (!(src_slots[src_index].flags & CNODE_SLOT_VALID)) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    cap_id_t cap_id = src_slots[src_index].cap_id;
    cap_rights_t src_rights = src_slots[src_index].rights_mask;

    /* 权限单调性检查：新掩码必须是原掩码的子集 */
    if ((rights_mask & src_rights) != rights_mask) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;
    }

    /* 复制槽位 */
    dst_slots[dst_index].cap_id = cap_id;
    dst_slots[dst_index].rights_mask = rights_mask;
    dst_slots[dst_index].flags = src_slots[src_index].flags;

    atomic_exit_critical(irq);

    return HIC_SUCCESS;
}

/* ==================== CPtr 解析实现 ==================== */

/**
 * @brief 解析 CPtr 获取能力信息
 */
hic_status_t cptr_lookup(cspace_t *cspace, cptr_t cptr,
                         cap_id_t *out_cap_id, cap_rights_t *out_rights) {
    if (!cspace || !out_cap_id) {
        return HIC_ERROR_INVALID_PARAM;
    }

    /* 首先检查缓存 */
    if (cap_cache_lookup(cspace->owner, cptr, out_cap_id, out_rights)) {
        return HIC_SUCCESS;
    }

    bool irq = atomic_enter_critical();

    cap_id_t current_cap = cspace->root_cnode;
    cap_rights_t accumulated_rights = 0xFFFFFFFF;  /* 初始为全权限 */
    u8 level = 0;

    while (level < CNODE_MAX_DEPTH) {
        if (current_cap >= CAP_TABLE_SIZE) {
            atomic_exit_critical(irq);
            return HIC_ERROR_CAP_INVALID;
        }

        cap_entry_t *entry = &g_global_cap_table[current_cap];

        /* 检查是否为 CNode */
        if (!(entry->rights & CAP_CNODE)) {
            atomic_exit_critical(irq);
            return HIC_ERROR_CAP_INVALID;
        }

        cnode_t *cnode = (cnode_t*)entry->memory.base;
        if (!cnode) {
            atomic_exit_critical(irq);
            return HIC_ERROR_CAP_INVALID;
        }

        /* 检查 Guard */
        if (cnode->guard_bits > 0) {
            u32 guard_value = (u32)(cptr & ((1ULL << cnode->guard_bits) - 1));
            if (guard_value != cnode->guard) {
                atomic_exit_critical(irq);
                return HIC_ERROR_CAP_INVALID;  /* Guard 不匹配 */
            }
            cptr >>= cnode->guard_bits;
        }

        /* 提取当前级别索引 */
        u32 index = cptr_extract_index(cptr, cnode->slot_bits, 0);
        cptr >>= cnode->slot_bits;

        cnode_slot_t *slots = cnode_get_slots(cnode);

        if (index >= cnode->slot_count) {
            atomic_exit_critical(irq);
            return HIC_ERROR_INVALID_PARAM;
        }

        cnode_slot_t *slot = &slots[index];

        /* 检查槽位有效性 */
        if (!(slot->flags & CNODE_SLOT_VALID)) {
            atomic_exit_critical(irq);
            return HIC_ERROR_CAP_INVALID;
        }

        /* 权限衰减：累积掩码 */
        accumulated_rights &= slot->rights_mask;

        current_cap = slot->cap_id;

        /* 检查是否到达叶子节点 */
        if (!(slot->flags & CNODE_SLOT_CNODE)) {
            /* 找到目标能力 */
            *out_cap_id = current_cap;
            if (out_rights) {
                /* 计算实际权限：基础权限 & 累积掩码 */
                cap_rights_t base_rights = g_global_cap_table[current_cap].rights;
                *out_rights = base_rights & accumulated_rights;
            }

            /* 更新缓存 */
            if (out_rights) {
                cap_cache_update(cspace->owner, cptr, current_cap, *out_rights);
            }

            atomic_exit_critical(irq);
            return HIC_SUCCESS;
        }

        level++;
    }

    /* 超过最大深度 */
    atomic_exit_critical(irq);
    return HIC_ERROR_INVALID_PARAM;
}

/**
 * @brief 解析 CPtr 获取槽位指针
 */
hic_status_t cptr_resolve(cspace_t *cspace, cptr_t cptr,
                          cnode_slot_t **out_slot, cnode_t **out_cnode) {
    if (!cspace || !out_slot) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cap_id_t current_cap = cspace->root_cnode;
    u8 level = 0;

    while (level < CNODE_MAX_DEPTH) {
        if (current_cap >= CAP_TABLE_SIZE) {
            atomic_exit_critical(irq);
            return HIC_ERROR_CAP_INVALID;
        }

        cap_entry_t *entry = &g_global_cap_table[current_cap];

        if (!(entry->rights & CAP_CNODE)) {
            atomic_exit_critical(irq);
            return HIC_ERROR_CAP_INVALID;
        }

        cnode_t *cnode = (cnode_t*)entry->memory.base;
        if (!cnode) {
            atomic_exit_critical(irq);
            return HIC_ERROR_CAP_INVALID;
        }

        /* 处理 Guard */
        if (cnode->guard_bits > 0) {
            cptr >>= cnode->guard_bits;
        }

        u32 index = cptr_extract_index(cptr, cnode->slot_bits, 0);
        cptr >>= cnode->slot_bits;

        cnode_slot_t *slots = cnode_get_slots(cnode);

        if (index >= cnode->slot_count) {
            atomic_exit_critical(irq);
            return HIC_ERROR_INVALID_PARAM;
        }

        cnode_slot_t *slot = &slots[index];

        if (!(slot->flags & CNODE_SLOT_VALID)) {
            atomic_exit_critical(irq);
            return HIC_ERROR_CAP_INVALID;
        }

        current_cap = slot->cap_id;

        if (!(slot->flags & CNODE_SLOT_CNODE)) {
            /* 到达叶子节点 */
            *out_slot = slot;
            if (out_cnode) {
                *out_cnode = cnode;
            }
            atomic_exit_critical(irq);
            return HIC_SUCCESS;
        }

        level++;
    }

    atomic_exit_critical(irq);
    return HIC_ERROR_INVALID_PARAM;
}

/* ==================== CSpace 操作实现 ==================== */

/**
 * @brief 初始化域的 CSpace
 */
hic_status_t cspace_init(domain_id_t domain, u8 root_slot_bits) {
    if (domain >= HIC_DOMAIN_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cspace_t *cspace = &g_domain_cspaces[domain];

    /* 创建根 CNode */
    cap_id_t root_cap;
    hic_status_t status = cnode_create(domain, root_slot_bits, &root_cap);
    if (status != HIC_SUCCESS) {
        atomic_exit_critical(irq);
        return status;
    }

    /* 标记为根 CNode */
    cap_entry_t *entry = &g_global_cap_table[root_cap];
    cnode_t *root_cnode = (cnode_t*)entry->memory.base;
    if (root_cnode) {
        root_cnode->flags |= CNODE_FLAG_ROOT;
        root_cnode->depth = 0;
    }

    /* 初始化 CSpace */
    cspace->root_cnode = root_cap;
    cspace->owner = domain;
    cspace->total_caps = 0;
    cspace->max_depth = 0;
    cspace->flags = CSPACE_FLAG_ACTIVE;

    atomic_exit_critical(irq);

    return HIC_SUCCESS;
}

/**
 * @brief 销毁域的 CSpace
 */
hic_status_t cspace_destroy(domain_id_t domain) {
    if (domain >= HIC_DOMAIN_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cspace_t *cspace = &g_domain_cspaces[domain];

    if (cspace->root_cnode == HIC_CAP_INVALID) {
        atomic_exit_critical(irq);
        return HIC_SUCCESS;  /* 已销毁 */
    }

    /* 递归撤销根 CNode 子树 */
    cnode_revoke_subtree(cspace->root_cnode);

    /* 销毁根 CNode */
    cnode_destroy(cspace->root_cnode);

    /* 清零 CSpace */
    cspace->root_cnode = HIC_CAP_INVALID;
    cspace->flags = 0;

    /* 使缓存失效 */
    cap_cache_invalidate(domain, CPTR_INVALID);

    atomic_exit_critical(irq);

    return HIC_SUCCESS;
}

/**
 * @brief 获取域的 CSpace
 */
cspace_t* cspace_get(domain_id_t domain) {
    if (domain >= HIC_DOMAIN_MAX) {
        return NULL;
    }

    cspace_t *cspace = &g_domain_cspaces[domain];

    if (!(cspace->flags & CSPACE_FLAG_ACTIVE)) {
        return NULL;
    }

    return cspace;
}

/**
 * @brief 从 CPtr 生成句柄
 */
cap_handle_t cptr_to_handle(domain_id_t domain, cptr_t cptr) {
    /* 使用现有的句柄生成机制 */
    /* 将 CPtr 作为能力ID传递 */
    return cap_make_handle(domain, (cap_id_t)(cptr & 0xFFFFFFFF));
}

/**
 * @brief 从句柄解析 CPtr
 */
hic_status_t handle_to_cptr(domain_id_t domain, cap_handle_t handle, cptr_t *out_cptr) {
    if (!out_cptr) {
        return HIC_ERROR_INVALID_PARAM;
    }

    cap_id_t cap_id = cap_get_cap_id(handle);
    u32 token = cap_get_token(handle);

    /* 验证令牌 */
    if (!cap_validate_token(domain, cap_id, token)) {
        return HIC_ERROR_PERMISSION;
    }

    *out_cptr = (cptr_t)cap_id;
    return HIC_SUCCESS;
}

/* ==================== 能力撤销扩展实现 ==================== */

/**
 * @brief 撤销 CNode 槽位中的能力
 */
hic_status_t cnode_revoke_slot(cap_id_t cnode_cap, u32 index) {
    if (cnode_cap >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cap_entry_t *entry = &g_global_cap_table[cnode_cap];

    if (!(entry->rights & CAP_CNODE)) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    cnode_t *cnode = (cnode_t*)entry->memory.base;
    if (!cnode || index >= cnode->slot_count) {
        atomic_exit_critical(irq);
        return HIC_ERROR_INVALID_PARAM;
    }

    cnode_slot_t *slots = cnode_get_slots(cnode);
    cnode_slot_t *slot = &slots[index];

    if (!(slot->flags & CNODE_SLOT_VALID)) {
        atomic_exit_critical(irq);
        return HIC_SUCCESS;  /* 已为空 */
    }

    cap_id_t cap_id = slot->cap_id;

    /* 如果是 CNode，递归撤销子树 */
    if (slot->flags & CNODE_SLOT_CNODE) {
        cnode_revoke_subtree(cap_id);
    } else {
        /* 普通能力，调用标准撤销 */
        cap_revoke(cap_id);
    }

    /* 清空槽位 */
    slot->cap_id = HIC_CAP_INVALID;
    slot->rights_mask = 0;
    slot->flags = CNODE_SLOT_EMPTY;

    atomic_exit_critical(irq);

    return HIC_SUCCESS;
}

/**
 * @brief 撤销 CNode 子树（内部实现）
 */
static void cnode_revoke_subtree_impl(cap_id_t cnode_cap, u32 depth) {
    if (depth > CNODE_MAX_DEPTH || cnode_cap >= CAP_TABLE_SIZE) {
        return;
    }

    cap_entry_t *entry = &g_global_cap_table[cnode_cap];

    if (!(entry->rights & CAP_CNODE)) {
        return;
    }

    cnode_t *cnode = (cnode_t*)entry->memory.base;
    if (!cnode) {
        return;
    }

    cnode_slot_t *slots = cnode_get_slots(cnode);

    for (u32 i = 0; i < cnode->slot_count; i++) {
        cnode_slot_t *slot = &slots[i];

        if (!(slot->flags & CNODE_SLOT_VALID)) {
            continue;
        }

        cap_id_t cap_id = slot->cap_id;

        if (slot->flags & CNODE_SLOT_CNODE) {
            /* 递归撤销子 CNode */
            cnode_revoke_subtree_impl(cap_id, depth + 1);
        } else {
            /* 撤销普通能力 */
            cap_revoke(cap_id);
        }

        /* 清空槽位 */
        slot->cap_id = HIC_CAP_INVALID;
        slot->flags = CNODE_SLOT_EMPTY;
    }
}

/**
 * @brief 撤销 CNode 子树
 */
hic_status_t cnode_revoke_subtree(cap_id_t cnode_cap) {
    if (cnode_cap >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cnode_revoke_subtree_impl(cnode_cap, 0);

    atomic_exit_critical(irq);

    return HIC_SUCCESS;
}

/* ==================== 能力缓存实现 ==================== */

/**
 * @brief 查询能力缓存
 */
bool cap_cache_lookup(domain_id_t domain, cptr_t cptr,
                      cap_id_t *out_cap_id, cap_rights_t *out_rights) {
    for (u32 i = 0; i < CAP_CACHE_SIZE; i++) {
        cap_cache_entry_t *entry = &g_cap_cache[i];

        if (entry->valid && entry->domain == domain && entry->cptr == cptr) {
            *out_cap_id = entry->cap_id;
            if (out_rights) {
                *out_rights = entry->rights;
            }

            /* 更新时间戳（LRU） */
            entry->timestamp = hal_get_timestamp();

            return true;
        }
    }

    return false;
}

/**
 * @brief 更新能力缓存
 */
void cap_cache_update(domain_id_t domain, cptr_t cptr,
                      cap_id_t cap_id, cap_rights_t rights) {
    /* 查找空闲项或最老的项 */
    u32 oldest_idx = 0;
    u64 oldest_ts = g_cap_cache[0].timestamp;

    for (u32 i = 0; i < CAP_CACHE_SIZE; i++) {
        if (!g_cap_cache[i].valid) {
            oldest_idx = i;
            break;
        }

        if (g_cap_cache[i].timestamp < oldest_ts) {
            oldest_ts = g_cap_cache[i].timestamp;
            oldest_idx = i;
        }
    }

    /* 更新缓存项 */
    g_cap_cache[oldest_idx].domain = domain;
    g_cap_cache[oldest_idx].cptr = cptr;
    g_cap_cache[oldest_idx].cap_id = cap_id;
    g_cap_cache[oldest_idx].rights = rights;
    g_cap_cache[oldest_idx].timestamp = hal_get_timestamp();
    g_cap_cache[oldest_idx].valid = 1;
}

/**
 * @brief 使能力缓存失效
 */
void cap_cache_invalidate(domain_id_t domain, cptr_t cptr) {
    for (u32 i = 0; i < CAP_CACHE_SIZE; i++) {
        cap_cache_entry_t *entry = &g_cap_cache[i];

        if (!entry->valid) {
            continue;
        }

        bool match = true;

        if (domain != HIC_DOMAIN_MAX && entry->domain != domain) {
            match = false;
        }

        if (cptr != CPTR_INVALID && entry->cptr != cptr) {
            match = false;
        }

        if (match) {
            entry->valid = 0;
        }
    }
}
