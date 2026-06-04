# TODO

## 已修复

- [x] SSE `#UD` — 内核编译加 `-mno-sse -mno-sse2 -mno-mmx -mno-3dnow`
- [x] Core-0 页表 = 0x7000 — `pagetable_create()` + 恒等映射 0-32MB
- [x] 模块管理器线程不执行 — `module_thread_yield()` 协作调度
- [x] IRQ EOI 写 LAPIC 0xFEE00000 未映射 — 改回 8259 PIC 端口 0x20
- [x] 删除 `is_privileged_domain` 重复函数
- [x] 魔数加注释
- [x] `ap_start.S.backup` 移除 git 追踪 + `.gitignore` 补充

## 未修复

### 高优先级

- [x] **抢占调度崩** — 已修复。
  - 根因：
    1. `isr_fast_stub` 未保存全部 GP 寄存器且未在 IRQ 返回前检查抢占
    2. `context_switch` 先切 CR3 再切 RSP，导致旧栈在新域页表中不可见 → #PF
  - 修复：
    1. `fast_path.S`: 保存全部 15 个 GP 寄存器，`irq_dispatch` 后检查 `g_reschedule_needed` → `call schedule()`
    2. `irq.c`: EOI 移到 handler 之前
    3. `context.S`: 先切 RSP 再切 CR3，确保 CR3 切换时已在目标线程栈上
    4. `scheduler_tick()` 只设标志位，上下文切换在汇编中完成
- [x] **动态模块加载空指针** — 已修复。
  - 根因：三层 bug：
    1. **Stack canary** (`%fs:0x28`)：模块编译缺 `-fno-stack-protector` → 代码读 `%fs:0x28` 作为栈金丝雀 → `%fs` 基址 NULL → #PF at CR2=0x28
    2. **SSE 非法指令**：模块编译缺 `-mno-sse` → 编译器生成 SSE 指令 → CR4.OSFXSR=0 → #UD
    3. **`.bss` 未分配**：`create_hicmod.py` 把 `bss_size` 设 0 → init_launcher 只分配代码段不分配 .bss → #PF
  - 修复：
    1. `Makefile.x86_64` (P1_CFLAGS): 加 `-fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -mno-3dnow`
    2. `create_hicmod.py`: 新增 `get_elf_bss_size()`，正确设置 `bss_size`
    3. `init_launcher/service.c`: 读取 `bss_size` 计入 `total_size`，分配后清零 .bss；修正 .bss 符号重定位基址（`bss_base_offset`）
    4. `module_primitives.c`: 域配额从 128KB 增至 512KB
    5. `dynamic_module_loader.c`: 4MB `g_module_buffer` 从静态数组改为 `module_memory_alloc` 按需分配
- [x] **EFC 集成** — 执行流能力化：`exec_flow_dispatch()` 成为 `schedule()` 的唯一切换路径，线程创建走 `exec_flow_create()` → 能力验证 → `context_switch`
- [x] **IPC 3.0 Plan B** — #PF 门控全路径可用：`ipc3_register_service` + `build_entry_page` + `ipc3_authorize` + `ipc3_handle_pf` + `ipc3_return`
- [x] **动态模块加载端到端验证通过** — cli_service 成功作为动态模块加载
  - `create_efi_disk_no_root.py`: 自动生成 `MODULES.LIS`，模块用 8.3 短名存储
  - `init_launcher`: 增加 `MODULE_M.HIC` 短名 fallback
  - `Makefile.x86_64`: `cli_service` 加入 `P1_NEEDED_MODULES`
  - 验证：系统稳定，两个动态模块（CLI_SERV, MODULE_M）成功加载
- [ ] **`memory_service`、`device_manager`、`security_monitor` 崩溃** — `platform.yaml` 中已注释，标注"启动时崩溃（需修复）"
  - 影响：无内存管理服务、设备管理服务、安全监控，系统功能不完整
- [ ] **跨域 context_switch #GP** — `module_thread_yield` → `schedule` → `context_switch` 切换到另一域线程时 #GP/Triple Fault
  - 现状：RSP→CR3 顺序已修，入口点 `phys_addr + 0x40` 已修，模块正确启动
  - 崩溃点：`module_thread_yield` 内 `schedule()` 调用 `context_switch` 跨域切换时
  - 推测：新域页表未映射目标线程栈（栈在 PMM 分配，`_kernel_end` 之前的地址可能未映射）
  - 涉及：`domain.c`, `context.S`, `pagetable.c`
  - 影响：`module_thread_yield` 不能切到其他域线程，所有跨域切换需走 IPC 3.0

### 已完成架构移植

- [x] **STM32F103C8T6 (Cortex-M3) 架构移植**
  - 架构目录: `src/Core-0/arch/stm32f103/` 共 11 个文件
  - 关键文件：
    - `entry.S` — 向量表、Reset/PendSys/SVCall/SysTick/IRQ 处理
    - `context.S` — PendSV 上下文切换 + 栈初始化
    - `hal_impl.c` — 系统时钟(72MHz PLL)、USART1、SysTick、中断控制
    - `pagetable.c` — MPU 配置（8 区域） + 全部页表桩函数
    - `timer.c` — SysTick 1000Hz 调度 tick
    - `linker.ld` — 64KB Flash / 20KB SRAM 内存布局
    - `hardware_probe_arch.c` — MCU ID 探测
    - `thread_arch.c` — Cortex-M3 PendSV 帧格式栈初始化
    - `Makefile.stm32f103` — ARM 交叉编译 + QEMU 仿真支持
  - 共享文件改动：
    - `types.h` — 修复 `size_t/uintptr_t` 编译器冲突
    - `thread.c` — 弱符号 `arch_thread_setup_stack()`
    - `kernel_start.c` — boot_info 通用 fallback
    - `hal.h` / `hal.c` — 新增 `HAL_ARCH_STM32F103`
  - 构建: `make ARCH=stm32f103`
  - 仿真: `make ARCH=stm32f103 qemu` (qemu-system-arm -M stm32-h103)

### 中优先级

- [ ] **`formal_verification.c` ~1410 行** — 字符串格式化工具代码与验证逻辑混在一起
- [ ] **`bootloader/main.c` ~1557 行** — UEFI 启动 + 内核加载 + 验签 + YAML 解析混在一起
- [ ] **`static_module.c` ~988 行** — 偏大
- [ ] **`platform_yaml.c` ~80KB** — 生成文件被提交到 Git，应改为构建时生成
- [ ] **`test_output/` 目录** — QEMU 测试脚本生成的文件要加 `.gitignore`

### 低优先级

- [ ] **构建产物散落在根目录** — `build_output.log` 等应清理
- [ ] **构建系统过复杂** — 7 种构建方式、Makefile Python 内联解析 YAML
- [ ] **`cap_fast_check_rights` / `cap_check_access` 简化** — 验证路径有两条路径（内联 + 完整），可统一
