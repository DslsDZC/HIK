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

- [x] **抢占调度崩** — 已修复（RSP→CR3 顺序 + fast_path.S 保存所有寄存器 + g_reschedule_needed 检查）
- [x] **动态模块加载空指针** — 已修复（三层 bug：stack canary + SSE + .bss 未分配）
- [x] **EFC 集成** — 完成
- [x] **IPC 3.0 Plan B** — 完成
- [x] **动态模块加载端到端验证** — 完成
- [x] **跨域 context_switch #GP** — 已修复（IPC3 entry page jnc bug + Core-0 PT 范围扩展）
- [ ] **抢占调度 sti 导致 #GP** — `context.S` 的 `.L_restore` 加 `sti` 后，第一个 context_switch(NULL, first_thread) 
      从主循环切换时，挂起时钟中断在 `ret` 后立即触发，此时新域页表已加载但新线程首指令未执行。
      中断处理内嵌调用 `schedule()` 导致执行流混乱。
      涉及: `fast_path.S`, `context.S`, `exec_flow_dispatch`
- [ ] **`memory_service`、`device_manager`、`security_monitor` 崩溃** — `platform.yaml` 中已注释，标注"启动时崩溃（需修复）"
  - 影响：无内存管理服务、设备管理服务、安全监控，系统功能不完整
- [ ] **模块加载链式授权** — `dynamic_module_loader.c:1501-1529` 的 `ipc3_authorize` 是链式的
      （模块A授权模块B），需要验证这种模式是否符合设计意图

### 中优先级（本轮引入）

- [ ] **`fast_path.S` 混用缩进** — 101-105 行用 tab，其余用 space（sed 替换导致）
- [ ] **清理未使用的 ELF 解析函数** — `elf_find_symbols`, `elf_get_section_name`, 
      `elf_find_lifecycle_functions`, `parse_int`, `find_entry` 仍在代码中（编译警告）
- [ ] **`create_hicmod.py` `.o` entry_offset 的 shndx 边界检查** — `st_shndx < len(sections)` 
      未考虑 `SHN_ABS`(0xFFF1) 和 `SHN_COMMON`(0xFFF2) 的情况

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

### 可维护性

- [ ] **V1/V2 HICM 头文件冲突** — `module_types.h` 和 `module_format.h` 都定义了 
      `hicmod_header_t`，字段偏移完全不同。模块管理器默认用 V1 struct 读 V2 模块。
      任何一边改 layout 都会静默数据错乱。
      - 涉及: `src/Privileged-1/include/module_types.h`, `src/Privileged-1/include/module_format.h`
      - `dynamic_module_loader.c` 现在用原始字节偏移绕过的，没有类型安全
- [ ] **ELF 结构体定义散落三处** — Python (create_hicmod.py)、C 模块管理器、
      C 内核 (module_loader.h)，同一套 ELF 解析有三个拷贝
- [ ] **线程创建两条路径未统一** — `thread_create_bound`(静态模块) vs `thread_create`(动态模块)，
      参数和错误处理不同，行为区别靠读函数体才能发现
- [ ] **模块加载四层嵌套** — linker(.static_modules) → static_module.c → init_launcher → 
      module_manager。每层都实现了创建域+映射内存+启动线程的子集，修 bug 要追四层
- [ ] **注释声明和代码不同步** — `exec_flow.c:155` 说"context_switch 在 .L_restore 中已 sti"，
      但实际上 context.S 没有 sti。`fast_path.S` 注释说"协作调度"但代码已加了抢占检查
- [ ] **V2 架构段用原始字节偏移访问** — `dynamic_module_loader.c` 无法用 struct 字段名，
      全部硬编码 offset（`+12`=code_size, `+36`=entry_offset），布局依赖 `create_hicmod.py` 
      和 `module_format.h` 同时更新，易碎

### 大文件拆分

- [ ] **`formal_verification.c` ~1410 行** — 字符串格式化工具代码与验证逻辑混在一起
- [ ] **`bootloader/main.c` ~1557 行** — UEFI 启动 + 内核加载 + 验签 + YAML 解析混在一起
- [ ] **`static_module.c` ~988 行** — 偏大
- [ ] **`platform_yaml.c` ~80KB** — 生成文件被提交到 Git，应改为构建时生成
- [ ] **`test_output/` 目录** — QEMU 测试脚本生成的文件要加 `.gitignore`

### 低优先级

- [ ] **构建产物散落在根目录** — `build_output.log` 等应清理
- [ ] **构建系统过复杂** — 7 种构建方式、Makefile Python 内联解析 YAML
- [ ] **`cap_fast_check_rights` / `cap_check_access` 简化** — 验证路径有两条路径（内联 + 完整），可统一
