# HIC ABI 兼容性规范

本文档定义了HIC系统的ABI（Application Binary Interface）规范。



## 一、ABI 设计原则

### 1.1 核心原则

HIC 不使用传统的 `syscall`/`svc` 指令进行跨域调用。
所有跨域通信通过 **IPC 3.0 入口页自检机制**完成：

```
调用者 → call [入口页] → bt [bitmap], ecx → jmp [业务页]
```

- **类型稳定**: 所有公开类型的布局和大小在ABI版本内不变
- **调用约定**: 使用标准平台调用约定（同特权级调用）
- **返回值**: 所有函数返回统一的错误码类型

### 1.2 版本兼容性

| 版本 | 发布日期 | 兼容性 | 变更 |
|------|---------|--------|------|
| 1.0 | 2026-02-26 | - | 初始版本（syscall 路径） |
| 2.0 | 2026-05-26 | 不兼容 1.0 | 统一为 IPC 3.0 入口页 |

---

## 二、类型定义规范

### 2.1 基本类型

所有公开类型使用固定大小的整数类型：

```c
typedef u8  u8;   /* 8位无符号 */
typedef u16 u16;  /* 16位无符号 */
typedef u32 u32;  /* 32位无符号 */
typedef u64 u64;  /* 64位无符号 */
typedef s8  s8;   /* 8位有符号 */
typedef s16 s16;  /* 16位有符号 */
typedef s32 s32;  /* 32位有符号 */
typedef s64 s64;  /* 64位有符号 */
```

### 2.2 句柄类型

```c
typedef u64 hic_domain_t;    /* 域ID */
typedef u64 hic_cap_t;       /* 能力句柄 */
typedef u64 hic_thread_t;    /* 线程句柄 */
typedef u64 hic_shmem_t;     /* 共享内存句柄 */
typedef u64 hic_entry_t;     /* IPC 3.0 入口页地址 */
```

**ABI保证**:
- 大小永远64位
- 无效值固定为 `~0ULL`
- 值0永远保留

### 2.3 枚举类型

枚举类型使用32位有符号整数：

```c
typedef s32 hic_error_t;

enum {
    HIC_OK = 0,
    HIC_ERR_INVALID_PARAM = 1,
    /* ... */
};
```

---

## 三、调用约定

### 3.1 IPC 3.0 入口页调用

所有跨域调用通过 **入口页** 进行。调用者持有入口页地址（通过能力系统获得），
直接使用标准 `call` 指令调用：

```
调用前（调用者准备）:
  rdi = arg1
  rsi = arg2
  rdx = arg3
  rcx = arg4
  r8  = arg5
  r9  = arg6

调用:
  call [entry_page_addr]    ← 入口页地址来自能力句柄解码
```

入口页执行自检后跳转到业务页：

```
  mov rax, IPC3_DOMAIN_DATA_VA    ← domain data 页地址
  mov ecx, [rax]                   ← 读当前域 ID（硬件保护，不可伪造）
  bt  [bitmap], ecx                ← 检查授权位图
  jnc reject                       ← 未授权则 hlt
  jmp [business_addr]              ← 跳转到服务业务逻辑
```

### 3.2 Plan A（共享映射）：业务页已映射

业务页在调用者页表中已存在 → `jmp` 直接执行 → 零额外开销

### 3.3 Plan B（缺页门控）：业务页未映射

`jmp` 触发 #PF → Core-0 的 `ipc3_handle_pf` 处理：

  1. 验证错误 RIP 来自入口页范围内
  2. 重新检查授权位图
  3. 保存调用方上下文（寄存器、CR3、RSP）
  4. 修改异常帧为业务页入口点和服务栈
  5. `iretq` 进入服务执行
  6. 返回时 `ipc3_return()` 恢复调用方上下文

### 3.4 x86-64 寄存器约定

| 用途 | 寄存器 |
|------|--------|
| 参数1-6 | RDI, RSI, RDX, RCX, R8, R9 |
| 返回值 | RAX |
| 被调用者保存 | RBX, RBP, R12-R15 |
| 调用者保存 | RAX, RCX, RDX, RSI, RDI, R8-R11 |

### 3.5 ARM64 寄存器约定

| 用途 | 寄存器 |
|------|--------|
| 参数1-8 | X0-X7 |
| 返回值 | X0 |
| 被调用者保存 | X19-X29 |
| 调用者保存 | X0-X18 |

---

## 四、错误码

```c
#define HIC_SUCCESS                  0
#define HIC_ERROR_INVALID_PARAM      2
#define HIC_ERROR_NO_MEMORY          3
#define HIC_ERROR_PERMISSION         4
#define HIC_ERROR_NOT_FOUND          5
#define HIC_ERROR_TIMEOUT            6
#define HIC_ERROR_BUSY               7
#define HIC_ERROR_NOT_SUPPORTED      8
#define HIC_ERROR_CAP_INVALID        9
#define HIC_ERROR_CAP_REVOKED       10
#define HIC_ERROR_INVALID_DOMAIN    11
#define HIC_ERROR_QUOTA_EXCEEDED    12
#define HIC_ERROR_INVALID_STATE     13
#define HIC_ERROR_NO_RESOURCE       14
```

---

## 五、域内存布局

```
+------------------+ 域基地址
| 代码段          | RX
+------------------+
| 只读数据        | R
+------------------+
| 数据段          | RW
+------------------+
| BSS段           | RW
+------------------+
| 栈              | RW（向下增长）
+------------------+
| IPC3 域数据页   | R（恒等映射 0xFFFFF000）
+------------------+
| 入口页（其他服务）| RX（每授权一个映射一个）
+------------------+
| 共享内存        | RW（能力控制）
+------------------+
| MMIO           | RW（能力控制）
+------------------+
```

**ABI保证**:
- `IPC3_DOMAIN_DATA_VA` 固定为 `0xFFFFF000`，所有域可读
- 入口页地址通过能力系统传递，不可伪造
- 域 ID 存储在 `IPC3_DOMAIN_DATA_VA` 处的只读页中，用户态不可写

---

## 六、版本演进规则

### 6.1 入口页兼容性

- 入口页代码布局永不改变（`bitmap` 偏移 `0x00`、`business_addr` 偏移 `0x20`、代码偏移 `0x28`）
- 新服务端点的入口页可以添加到新物理页面
- 现有入口页地址永不失效（直到服务注销）

### 6.2 服务端点演进

- 端点 ID 只增不减
- 现有端点永不删除
- 新端点可以添加

### 6.3 破坏性变更

主版本号增加仅当：
1. 入口页代码布局改变
2. 能力句柄格式改变
3. 域 ID 存储机制改变
