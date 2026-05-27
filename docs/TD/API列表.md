# HIC API 接口列表

本文档列出了HIC系统的所有API接口。
所有调用通过 **IPC 3.0 入口页** 完成，不使用传统的 `syscall` 指令。



## 接口分类

### 官方接口（Official API）

范围:
- Core-0 提供的服务端点（0x0000-0x0FFF）
- Privileged-1 层的官方服务端点（0x1000-0x8FFF）

维护原则:
- 只向后兼容
- 永不删除现有端点
- 只添加新端点

### 第三方接口（Third-party API）

范围:
- 第三方服务端点（0x9000-0xAFFF）
- 版本管理由第三方自行决定

---

## 一、Core-0 服务端点

Core-0 提供的基础服务，所有应用和服务都可以使用。
调用方式：`call [入口页地址]`（地址通过能力系统获得）→ `bt` 自检 → `jmp` 到业务逻辑。

### 1.1 能力管理

| 端点 | 操作 | 参数 | 说明 |
|------|------|------|------|
| 0x0000 | `cap_create` | owner, rights, type | 创建能力 |
| 0x0001 | `cap_revoke` | cap_id | 撤销能力（仅 owner_core） |
| 0x0002 | `cap_derive` | parent, sub_rights | 派生权限子集 |
| 0x0003 | `cap_transfer` | target_domain, cap_id | 传递能力 |
| 0x0004 | `cap_fast_check` | handle, required | 内联验证（非入口页，芯片指令级） |

### 1.2 域管理

| 端点 | 操作 | 参数 | 说明 |
|------|------|------|------|
| 0x0010 | `domain_create` | type, quota | 创建域 |
| 0x0011 | `domain_destroy` | domain_id | 销毁域 |

### 1.3 线程管理

| 端点 | 操作 | 参数 | 说明 |
|------|------|------|------|
| 0x0020 | `thread_create` | domain, entry, stack | 创建线程 |
| 0x0021 | `thread_yield` | 无 | 让出 CPU |
| 0x0022 | `thread_sleep` | us | 睡眠 |

### 1.4 物理内存管理

| 端点 | 操作 | 参数 | 说明 |
|------|------|------|------|
| 0x0030 | `pmm_alloc` | count, type | 分配物理页 |
| 0x0031 | `pmm_free` | addr, count | 释放物理页 |

### 1.5 IPC 3.0 管理

| 端点 | 操作 | 参数 | 说明 |
|------|------|------|------|
| 0x0040 | `ipc3_register` | owner, bus_entry, stack | 注册服务，创建入口页 |
| 0x0041 | `ipc3_authorize` | service_id, domain | 授权域调用 |
| 0x0042 | `ipc3_deauthorize` | service_id, domain | 撤销授权 |
| 0x0043 | `ipc3_unregister` | service_id | 注销服务 |

---

## 二、Privileged-1 服务端点

### 2.1 能力管理器服务（Capability Manager）

**服务**: `capability_manager`

| 端点 | 名称 | 功能 |
|------|------|------|
| 0x1000 | `CAP_ENDPOINT_VERIFY` | 验证能力有效性 |
| 0x1001 | `CAP_ENDPOINT_REVOKE` | 撤销能力 |
| 0x1002 | `CAP_ENDPOINT_DELEGATE` | 委托能力 |
| 0x1003 | `CAP_ENDPOINT_TRANSFER` | 转移能力 |
| 0x1004 | `CAP_ENDPOINT_DERIVE` | 派生能力 |

### 2.2 调度器服务（Scheduler）

**服务**: `scheduler_service`

| 端点 | 名称 | 功能 |
|------|------|------|
| 0x2000 | `SCHED_ENDPOINT_CREATE` | 创建线程 |
| 0x2001 | `SCHED_ENDPOINT_TERMINATE` | 终止线程 |
| 0x2002 | `SCHED_ENDPOINT_YIELD` | 让出CPU |

### 2.3 内存管理器服务（Memory Manager）

**服务**: `memory_service`

| 端点 | 名称 | 功能 |
|------|------|------|
| 0x3000 | `MEM_ENDPOINT_ALLOC` | 分配内存 |
| 0x3001 | `MEM_ENDPOINT_FREE` | 释放内存 |
| 0x3002 | `MEM_ENDPOINT_SHARED` | 分配共享内存 |

### 2.4 中断控制器服务（IRQ Controller）

**服务**: `irq_controller_service`

| 端点 | 名称 | 功能 |
|------|------|------|
| 0x4000 | `IRQ_ENDPOINT_REGISTER` | 注册中断处理 |
| 0x4001 | `IRQ_ENDPOINT_UNREGISTER` | 注销中断处理 |
| 0x4002 | `IRQ_ENDPOINT_ENABLE` | 启用中断 |
| 0x4003 | `IRQ_ENDPOINT_DISABLE` | 禁用中断 |

### 2.5 模块管理器服务（Module Manager）

**服务**: `module_manager_service`

| 端点 | 名称 | 功能 |
|------|------|------|
| 0x6000 | `MODULE_ENDPOINT_LOAD` | 加载模块 |
| 0x6001 | `MODULE_ENDPOINT_UNLOAD` | 卸载模块 |
| 0x6002 | `MODULE_ENDPOINT_QUERY` | 查询模块信息 |

### 2.6 共享库管理器服务（Library Manager）

**服务**: `lib_manager_service`

| 端点 | 名称 | 功能 |
|------|------|------|
| 0x6800 | `LIB_ENDPOINT_REGISTER` | 注册共享库 |
| 0x6801 | `LIB_ENDPOINT_LOOKUP` | 查询共享库 |
| 0x6802 | `LIB_ENDPOINT_REFERENCE` | 引用共享库 |
| 0x6803 | `LIB_ENDPOINT_RELEASE` | 释放共享库 |
| 0x6804 | `LIB_ENDPOINT_UPDATE` | 热更新共享库 |
| 0x6805 | `LIB_ENDPOINT_SYMBOL` | 查询符号 |
| 0x6806 | `LIB_ENDPOINT_LIST` | 列出共享库 |
| 0x6807 | `LIB_ENDPOINT_STATS` | 统计信息 |
| 0x6808 | `LIB_ENDPOINT_UNLOAD` | 卸载共享库 |

---

## 三、调用流程

### App → P1 服务

```
1. App 持有 P1 服务的入口页能力句柄
2. App: call [入口页地址]
3. 入口页: bt [bitmap], ecx → 验证通过
4. 入口页: jmp [business_addr]
5. 若业务页未映射 → #PF → Core-0 仲裁 → iretq 进入
6. 若已映射         → 直接执行
7. P1 服务执行业务逻辑
8. 返回调用者
```

### P1 → P1（同特权级）

```
1. 同上，但无需特权级切换
2. 热路径 ~3ns（业务页映射时）
3. 首次调用 ~90-140ns（#PF 缺页路径）
```

### App → Core-0（资源请求）

```
1. App 持有 Core-0 服务端点的入口页能力
2. call [Core-0 入口页]
3. 入口页 bt 自检 → jmp Core-0 业务代码
4. Core-0 处理（域创建、内存分配等）
5. 返回
```

---

## 四、性能目标

| 操作 | IPC 3.0 热路径 | IPC 3.0 首次调用 |
|------|---------------|-----------------|
| App → P1 服务 | ~3ns | ~140ns（#PF） |
| P1 → P1 | ~3ns | ~140ns（#PF） |
| App → Core-0 | ~3ns | ~140ns（#PF） |
| 能力验证（`bt`） | ~1ns | - |
| 能力验证（`cap_fast_check`） | ~5ns | - |
