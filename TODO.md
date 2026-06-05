# TODO

## Fixed

- [x] SSE `#UD` — kernel build flags add `-mno-sse -mno-sse2 -mno-mmx -mno-3dnow`
- [x] Core-0 page table = 0x7000 — `pagetable_create()` + identity map 0-32MB
- [x] Module manager thread stuck — `module_thread_yield()` cooperative scheduling
- [x] IRQ EOI write to LAPIC 0xFEE00000 unmapped — use 8259 PIC port 0x20
- [x] Remove duplicate `is_privileged_domain`
- [x] Annotate magic numbers
- [x] Remove `ap_start.S.backup` from git tracking + update `.gitignore`
- [x] **Preemptive scheduling crash** — RSP→CR3 order + fast_path.S saves all regs + `g_reschedule_needed` check
- [x] **Dynamic module loading null deref** — three-layer bug: stack canary + SSE + .bss unallocated
- [x] **EFC integration** — done
- [x] **IPC 3.0 Plan B** — done
- [x] **Dynamic module loading E2E verified** — done
- [x] **Cross-domain context_switch #GP** — IPC3 entry page jnc bug + Core-0 PT range fix
- [x] **STM32F103C8T6 (Cortex-M3) port** — arch/stm32f103/ 11 files

## High priority

- [ ] **Preemption sti #GP** — `sti` before `ret` in `context_switch` causes #GP on first
      `context_switch(NULL, first_thread)`. Pending timer fires after `ret` with new CR3 loaded
      but before first instruction of the new thread. Nested `schedule()` inside IRQ handler
      corrupts execution flow.
      Files: `fast_path.S`, `context.S`, `exec_flow_dispatch`
- [ ] **Three services crash at boot** — `memory_service`, `device_manager`, `security_monitor`
      commented out in `platform.yaml` with "crashes at boot (needs fix)"
      Impact: no memory management, device management, or security monitoring
- [ ] **Chain authorization for modules** — `ipc3_authorize` in `dynamic_module_loader.c` is
      chain-based (module A authorizes module B). Needs review if this matches the design intent.

## Maintainability

- [ ] **V1/V2 HICM header conflict** — `module_types.h` and `module_format.h` both define
      `hicmod_header_t` with different field layouts. Module manager reads V2 modules through
      the V1 struct. Any layout change silently corrupts data.
      Files: `src/Privileged-1/include/module_types.h`, `src/Privileged-1/include/module_format.h`
      Current workaround: raw byte offsets (no type safety)
- [ ] **ELF structs duplicated in three places** — Python (create_hicmod.py), C module manager,
      C kernel (module_loader.h)
- [ ] **Two thread creation paths** — `thread_create_bound` (static modules) vs `thread_create`
      (dynamic modules). Different parameters and error handling, behavior differences hidden
      in implementations.
- [ ] **Four-layer module loading** — linker(.static_modules) → static_module.c → init_launcher →
      module_manager. Each layer partially reimplements domain creation + memory mapping +
      thread startup. Bugs require tracing through all four layers.
- [ ] **Comments out of sync with code** — `exec_flow.c:155` claims "context_switch does sti
      in .L_restore" but context.S has no sti. `fast_path.S` comment says "cooperative
      scheduling" but the code now has preemption checks.
- [ ] **V2 arch section accessed via raw byte offsets** — `dynamic_module_loader.c` hardcodes
      offsets (`+12`=code_size, `+36`=entry_offset) instead of using struct field names.
      Fragile: depends on `create_hicmod.py` and `module_format.h` staying in sync.

## Medium priority

- [ ] **Mixed indentation in `fast_path.S`** — lines 101-105 use tabs, rest uses spaces
- [ ] **Unused ELF parsing functions** — `elf_find_symbols`, `elf_get_section_name`,
      `elf_find_lifecycle_functions`, `parse_int`, `find_entry` still in code (compiler warnings)
- [ ] **`create_hicmod.py` entry_offset shndx bounds** — `st_shndx < len(sections)` doesn't
      account for `SHN_ABS`(0xFFF1) and `SHN_COMMON`(0xFFF2)

### Large file splitting

- [ ] **`formal_verification.c` ~1410 lines** — string formatting mixed with verification logic
- [ ] **`bootloader/main.c` ~1557 lines** — UEFI boot + kernel load + signature verify + YAML parse mixed
- [ ] **`static_module.c` ~988 lines** — too large
- [ ] **`platform_yaml.c` ~80KB** — generated file committed to git, should be build-time generated
- [ ] **`test_output/`** — QEMU test output files should be in `.gitignore`

### Low priority

- [ ] **Build artifacts in root** — `build_output.log` etc. should be cleaned
- [ ] **Build system too complex** — 7 build methods, Makefile inline YAML parsing
- [ ] **`cap_fast_check_rights` / `cap_check_access`** — two verification paths (inline + full), could be unified
