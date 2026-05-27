/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核主头文件
 * 包含所有核心子系统
 */

#ifndef HIC_KERNEL_H
#define HIC_KERNEL_H

#include "types.h"
#include "capability.h"
#include "domain.h"
#include "thread.h"
#include "exec_flow.h"
#include "pmm.h"

/* 内核入口点 */
extern void kernel_main(void);

#endif /* HIC_KERNEL_H */