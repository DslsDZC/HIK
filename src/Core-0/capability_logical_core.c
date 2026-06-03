/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC能力系统 - 逻辑核心能力
 *
 * 提供逻辑核心能力的创建和查询接口。
 * 支持调度策略的分层管理，派生能力只能衰减不能提升策略。
 */

#include "capability.h"
#include "domain.h"
#include "hal.h"
#include "atomic.h"

/* ==================== 逻辑核心能力函数 ==================== */

/**
 * 创建逻辑核心能力
 */
hic_status_t cap_create_logical_core(domain_id_t owner,
                                    logical_core_id_t logical_core_id,
                                    logical_core_flags_t flags,
                                    logical_core_quota_t quota,
                                    cap_rights_t rights,
                                    cap_id_t *out) {
    if (owner >= HIC_DOMAIN_MAX || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }

    cap_id_t cap = cap_core_first_free();
    if (cap == HIC_CAP_INVALID) {
        return HIC_ERROR_NO_RESOURCE;
    }

    g_global_cap_table[cap].cap_id = cap;
    g_global_cap_table[cap].rights = rights;
    g_global_cap_table[cap].owner = owner;
    g_global_cap_table[cap].owner_core = (u8)hal_get_cpu_id();
    g_global_cap_table[cap].flags = 0;

    /* 设置逻辑核心信息 */
    g_global_cap_table[cap].logical_core.logical_core_id = logical_core_id;
    g_global_cap_table[cap].logical_core.flags = flags;
    g_global_cap_table[cap].logical_core.quota = quota;

    /* 默认调度策略：共享模式 */
    g_global_cap_table[cap].logical_core.sched_policy = DOMAIN_SCHED_POLICY_SHARED;
    g_global_cap_table[cap].logical_core.max_derived_policy = DOMAIN_SCHED_POLICY_SHARED;

    *out = cap;
    return HIC_SUCCESS;
}

/**
 * 创建逻辑核心能力（带调度策略）
 *
 * 策略分层检查：
 * - 调用者域必须有权限创建指定策略的能力
 * - 策略必须符合域的安全等级
 */
hic_status_t cap_create_logical_core_with_policy(domain_id_t owner,
                                                  logical_core_id_t logical_core_id,
                                                  logical_core_flags_t flags,
                                                  logical_core_quota_t quota,
                                                  cap_rights_t rights,
                                                  u8 sched_policy,
                                                  cap_id_t *out) {
    /* 检查域是否有权限创建此策略的能力 */
    if (!domain_can_create_sched_policy(owner, (domain_sched_policy_t)sched_policy)) {
        return HIC_ERROR_PERMISSION_DENIED;
    }

    /* 调用基础创建函数 */
    hic_status_t status = cap_create_logical_core(owner, logical_core_id,
                                                   flags, quota, rights, out);
    if (status != HIC_SUCCESS) {
        return status;
    }

    /* 设置调度策略 */
    cap_id_t cap = *out;
    g_global_cap_table[cap].logical_core.sched_policy = sched_policy;
    /* 允许派生的最高策略 = 当前策略（派生时只能衰减） */
    g_global_cap_table[cap].logical_core.max_derived_policy = sched_policy;

    return HIC_SUCCESS;
}

/**
 * 获取逻辑核心能力信息
 */
hic_status_t cap_get_logical_core_info(cap_id_t cap_id,
                                      logical_core_id_t *logical_core_id,
                                      logical_core_flags_t *flags,
                                      logical_core_quota_t *quota) {
    if (cap_id >= CAP_TABLE_SIZE || !capability_exists(cap_id)) {
        return HIC_ERROR_INVALID_PARAM;
    }

    cap_entry_t *entry = &g_global_cap_table[cap_id];

    if (logical_core_id) {
        *logical_core_id = entry->logical_core.logical_core_id;
    }
    if (flags) {
        *flags = entry->logical_core.flags;
    }
    if (quota) {
        *quota = entry->logical_core.quota;
    }

    return HIC_SUCCESS;
}

/**
 * 获取逻辑核心能力的调度策略
 */
hic_status_t cap_get_logical_core_policy(cap_id_t cap_id,
                                          u8 *sched_policy,
                                          u8 *max_derived_policy) {
    if (cap_id >= CAP_TABLE_SIZE || !capability_exists(cap_id)) {
        return HIC_ERROR_INVALID_PARAM;
    }

    cap_entry_t *entry = &g_global_cap_table[cap_id];

    if (sched_policy) {
        *sched_policy = entry->logical_core.sched_policy;
    }
    if (max_derived_policy) {
        *max_derived_policy = entry->logical_core.max_derived_policy;
    }

    return HIC_SUCCESS;
}
