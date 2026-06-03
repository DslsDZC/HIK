/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC能力系统 - 零停机更新机制层实现
 *
 * 提供服务实例管理、连接迁移、主备切换等零停机更新原语。
 * 基于共享内存机制实现状态迁移通道。
 */

#include "capability.h"
#include "domain.h"
#include "pmm.h"
#include "hal.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"

/* ==================== 零停机更新机制层实现 ==================== */

/* 服务实例表 */
#define MAX_SERVICE_INSTANCES  64
static struct {
    char name[64];
    u32 version;
    domain_id_t domain;
    service_instance_state_t state;
    u32 connection_count;
    u32 flags;
    cap_id_t primary_cap;       /* 主实例能力 */
    cap_id_t standby_cap;       /* 备用实例能力 */
} g_service_instances[MAX_SERVICE_INSTANCES];

/* 连接跟踪表 */
#define MAX_CONNECTION_TRACKERS  1024
static struct {
    cap_id_t cap_id;
    domain_id_t owner;
    domain_id_t original_owner;
    u64 create_time;
    u32 flags;
    u8 state_data[256];         /* 连接状态数据 */
    size_t state_size;
} g_connection_trackers[MAX_CONNECTION_TRACKERS];

/**
 * 创建状态迁移通道（机制层）
 */
hic_status_t cap_migration_channel_create(domain_id_t from,
                                           domain_id_t to,
                                           size_t buffer_size,
                                           cap_id_t *out_cap,
                                           cap_handle_t *out_handle_from,
                                           cap_handle_t *out_handle_to) {
    if (from >= HIC_DOMAIN_MAX || to >= HIC_DOMAIN_MAX ||
        !out_cap || !out_handle_from || !out_handle_to) {
        return HIC_ERROR_INVALID_PARAM;
    }

    /* 默认缓冲区大小 */
    if (buffer_size == 0) {
        buffer_size = 64 * 1024;  /* 64KB */
    }

    /* 使用共享内存机制创建通道 */
    cap_id_t shmem_cap;
    cap_handle_t handle;
    hic_status_t status = shmem_alloc(from, buffer_size,
                                       SHMEM_FLAG_WRITABLE,
                                       &shmem_cap, &handle);
    if (status != HIC_SUCCESS) {
        return status;
    }

    /* 映射到目标域 */
    status = shmem_map(from, to, shmem_cap,
                       CAP_MEM_READ | CAP_MEM_WRITE,
                       out_handle_to);
    if (status != HIC_SUCCESS) {
        /* 回滚 */
        cap_revoke(shmem_cap);
        return status;
    }

    *out_cap = shmem_cap;
    *out_handle_from = handle;

    return HIC_SUCCESS;
}

/**
 * 迁移连接能力（机制层）
 */
hic_status_t cap_connection_migrate(cap_id_t conn_cap,
                                     domain_id_t from,
                                     domain_id_t to,
                                     const void *state_data,
                                     size_t state_size) {
    if (conn_cap >= CAP_TABLE_SIZE ||
        from >= HIC_DOMAIN_MAX || to >= HIC_DOMAIN_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }

    /* 状态数据大小限制 */
    if (state_size > 256) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cap_entry_t *entry = &g_global_cap_table[conn_cap];

    /* 检查能力有效性 */
    if (entry->cap_id != conn_cap) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    /* 检查当前所有者 */
    if (entry->owner != from) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;
    }

    /* 查找连接跟踪器 */
    u32 tracker_idx = MAX_CONNECTION_TRACKERS;
    for (u32 i = 0; i < MAX_CONNECTION_TRACKERS; i++) {
        if (g_connection_trackers[i].cap_id == conn_cap) {
            tracker_idx = i;
            break;
        }
    }

    /* 如果没有跟踪器，创建一个新的 */
    if (tracker_idx == MAX_CONNECTION_TRACKERS) {
        for (u32 i = 0; i < MAX_CONNECTION_TRACKERS; i++) {
            if (g_connection_trackers[i].cap_id == 0) {
                tracker_idx = i;
                g_connection_trackers[i].cap_id = conn_cap;
                g_connection_trackers[i].original_owner = from;
                g_connection_trackers[i].create_time = hal_get_timestamp();
                break;
            }
        }
    }

    if (tracker_idx == MAX_CONNECTION_TRACKERS) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_RESOURCE;
    }

    /* 保存状态数据 */
    if (state_data && state_size > 0) {
        memcopy(g_connection_trackers[tracker_idx].state_data,
                state_data, state_size);
        g_connection_trackers[tracker_idx].state_size = state_size;
    }

    /* 原子性转移所有权 */
    entry->owner = to;
    g_connection_trackers[tracker_idx].owner = to;

    atomic_exit_critical(irq);

    return HIC_SUCCESS;
}

/**
 * 创建服务实例能力（机制层）
 */
hic_status_t cap_create_service_instance(domain_id_t owner,
                                          const char *service_name,
                                          u32 version,
                                          u32 flags,
                                          cap_id_t *out) {
    if (owner >= HIC_DOMAIN_MAX || !service_name || !out) {
        return HIC_ERROR_INVALID_PARAM;
    }

    /* 查找空闲服务实例槽 */
    u32 idx = MAX_SERVICE_INSTANCES;
    for (u32 i = 0; i < MAX_SERVICE_INSTANCES; i++) {
        if (g_service_instances[i].name[0] == '\0') {
            idx = i;
            break;
        }
    }

    if (idx == MAX_SERVICE_INSTANCES) {
        return HIC_ERROR_NO_RESOURCE;
    }

    /* 初始化服务实例 */
    strncpy(g_service_instances[idx].name, service_name, 63);
    g_service_instances[idx].name[63] = '\0';
    g_service_instances[idx].version = version;
    g_service_instances[idx].domain = owner;
    g_service_instances[idx].state = SERVICE_INSTANCE_INIT;
    g_service_instances[idx].connection_count = 0;
    g_service_instances[idx].flags = flags;
    g_service_instances[idx].primary_cap = HIC_CAP_INVALID;
    g_service_instances[idx].standby_cap = HIC_CAP_INVALID;

    /* Per-core 分配 */
    cpu_id_t svc_core = hal_get_cpu_id();
    cap_id_t cap = cap_core_first_free();

    if (cap == HIC_CAP_INVALID) {
        g_service_instances[idx].name[0] = '\0';
        return HIC_ERROR_NO_RESOURCE;
    }

    /* 初始化能力条目 */
    g_global_cap_table[cap].cap_id = cap;
    g_global_cap_table[cap].rights = CAP_LCORE_QUERY | flags;
    g_global_cap_table[cap].owner = owner;
    g_global_cap_table[cap].owner_core = (u8)svc_core;
    g_global_cap_table[cap].flags = 0;

    *out = cap;
    return HIC_SUCCESS;
}

/**
 * 切换服务主实例（机制层）
 *
 * 安全保证：
 * 1. 验证服务能力有效性
 * 2. 验证新主实例属于同一服务（必须是当前备用实例）
 * 3. 原子性切换
 */
hic_status_t cap_service_switch_primary(cap_id_t service_cap,
                                         cap_id_t new_primary_cap,
                                         cap_id_t *old_primary_cap) {
    if (service_cap >= CAP_TABLE_SIZE || new_primary_cap >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    /* 查找服务实例 */
    u32 idx = MAX_SERVICE_INSTANCES;
    for (u32 i = 0; i < MAX_SERVICE_INSTANCES; i++) {
        if (g_service_instances[i].primary_cap == service_cap ||
            g_service_instances[i].standby_cap == service_cap) {
            idx = i;
            break;
        }
    }

    if (idx == MAX_SERVICE_INSTANCES) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NOT_FOUND;
    }

    /* 验证新主实例属于同一服务（必须是当前备用实例） */
    if (g_service_instances[idx].standby_cap != new_primary_cap) {
        atomic_exit_critical(irq);
        return HIC_ERROR_INVALID_PARAM;  /* 新主实例不属于该服务 */
    }

    /* 记录旧主实例 */
    cap_id_t old_primary = g_service_instances[idx].primary_cap;

    /* 原子性切换 */
    g_service_instances[idx].standby_cap = old_primary;
    g_service_instances[idx].primary_cap = new_primary_cap;

    atomic_exit_critical(irq);

    if (old_primary_cap) {
        *old_primary_cap = old_primary;
    }

    return HIC_SUCCESS;
}

/**
 * 获取服务实例状态（机制层）
 */
hic_status_t cap_get_service_instance_state(cap_id_t instance_cap,
                                             service_instance_state_t *state,
                                             u32 *connection_count) {
    if (instance_cap >= CAP_TABLE_SIZE || !state) {
        return HIC_ERROR_INVALID_PARAM;
    }

    /* 查找服务实例 */
    u32 idx = MAX_SERVICE_INSTANCES;
    for (u32 i = 0; i < MAX_SERVICE_INSTANCES; i++) {
        if (g_service_instances[i].primary_cap == instance_cap ||
            g_service_instances[i].standby_cap == instance_cap) {
            idx = i;
            break;
        }
    }

    if (idx == MAX_SERVICE_INSTANCES) {
        return HIC_ERROR_NOT_FOUND;
    }

    *state = g_service_instances[idx].state;
    if (connection_count) {
        *connection_count = g_service_instances[idx].connection_count;
    }

    return HIC_SUCCESS;
}
