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

- [ ] **抢占调度崩** — `context_switch` 在 IRQ handler 里调用会 #GP → Triple Fault。`scheduler_tick()` 只能空跑，已加中断使能代码但关闭。
  - 涉及：`arch/x86_64/context.S`, `arch/x86_64/fast_path.S`, `irq_dispatch`
  - 影响：不能开 `sti`，所有线程切换靠主动 yield
- [ ] **动态模块加载空指针** — module_manager 输出 `Starting dynamic module loading...` 后 #PF 在 `[module_manager_code]`，CR2=0x28 (NULL+0x28)
  - 涉及：`Privileged-1/services/module_manager_service/service.c`
  - 影响：动态模块完全不可用，静态模块工作正常
- [ ] **`memory_service`、`device_manager`、`security_monitor` 崩溃** — `platform.yaml` 中已注释，标注"启动时崩溃（需修复）"
  - 影响：无内存管理服务、设备管理服务、安全监控，系统功能不完整

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
