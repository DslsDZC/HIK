# TODO

## Fixed (2026-06-06)

- [x] **Preemption sti #GP** — Root cause: `GDT_SIZE_64BIT` macro in `gdt.h:70` used
      `(1 << 6)` (D/B bit) instead of `(1 << 5)` (L bit — Long mode). C code's GDT[1]
      had L=0, making the descriptor a 32-bit compatibility segment. Interrupt delivery
      re-read GDT from memory → saw L=0 → #GP(0x0008). Bootloader's GDT (`entry.S`)
      was correct (0x00af...). Without sti the CS cache from bootloader was never refreshed.
      **Fix**: `GDT_SIZE_64BIT = (1 << 5)`.
      Files: `gdt.h`
- [x] **g_module_buffer 8MB → 1MB** — init_launcher's static module buffer was sized at
      8MB (`MAX_MODULE_SIZE`), the single largest contributor to kernel `.bss` (13.4MB).
      Reduced to 1MB, dropping `.bss` to 6.3MB. Recommended follow-up: dynamic allocation.
      File: `init_launcher/service.c`
- [x] **pagetable_map error check** — `.bss` mapping return value was unchecked; silent
      failure could leave GDT/IDT pages unmapped in domain page tables.
      File: `domain.c`
- [x] **thread_entry_trampoline** — New thread entry trampoline that does `sti` after
      stack switch. sti moved out of `context_switch` to avoid #GP when pending timer
      fires immediately with domain CR3 active. Trampoline runs in the thread's own
      context, so all domain page table mappings (GDT/IDT via `.bss`) are available.
      Files: `context.S`, `thread.c`, `scheduler.c`
- [x] **exec_flow.c comment out of sync** — outdated comment about sti location synced.
      File: `exec_flow.c`
- [x] **CAP_MEM_DEVICE MMIO auto-mapping** — `cap_create_memory()` now auto-maps pages
      into domain page table when `CAP_MEM_DEVICE` flag is set. VGA 0xB8000 fixed.
      Files: `capability_core.c`, `static_module.c`, `platform.yaml`

## High priority

- [ ] **Core rewrite** — Rewrite Core-0 in **Core-lang** ([github.com/dslsdzc/core](https://github.com/dslsdzc/core)).
      Repo path: `~/core`. Start with pmm → capability → ipc3, QEMU-verify each step.
      IPC3 perf baseline: bt ~2.83 cyc, full rapid path ~8 cyc (must not regress).

- [ ] **MMIO config from platform.yaml** — MMIO mapping currently driven by a built-in
      table in static_module.c (mirroring platform.yaml mmio_regions). Should parse
      directly from platform.yaml so no kernel code change is needed for new devices.
      Path: boot YAML parser → cap_create_memory(CAP_MEM_DEVICE) for each region.
      (capability mechanism is done — CAP_MEM_DEVICE + auto page mapping)
- [ ] **#PF demand paging for MMIO** — Lazy page mapping: instead of mapping all MMIO
      at boot, leave PTEs invalid. #PF handler checks domain's IO capabilities and
      maps on first access. Enables runtime capability revocation.
      Requires: #PF handler extension, capability lookup in fault path.

- [ ] **Three services crash at boot** — `memory_service`, `device_manager`, `security_monitor`
      commented out in `platform.yaml` with "crashes at boot (needs fix)"
      Impact: no memory management, device management, or security monitoring
- [x] **Chain authorization for modules** — replaced with dependency-based authorization.
      Modules declare `[dependencies]` in `hicmod.txt`, loader reads them and calls
      `ipc3_authorize` for each declared service. No more load-order chaining.

## Maintainability

- [ ] **V1/V2 HICM header conflict** — `module_types.h` and `module_format.h` both define
      `hicmod_header_t` with different field layouts. Module manager reads V2 modules through
      the V1 struct. Any layout change silently corrupts data.
      Files: `src/Privileged-1/include/module_types.h`, `src/Privileged-1/include/module_format.h`
      Current workaround: raw byte offsets (no type safety)
- [ ] **g_module_buffer should be dynamic** — static 1MB array in kernel `.bss` pollutes
      every domain's page table. Allocate via PMM at runtime instead.
- [ ] **ELF structs duplicated in three places** — Python (create_hicmod.py), C module manager,
      C kernel (module_loader.h)
- [ ] **Two thread creation paths** — `thread_create_bound` (static modules) vs `thread_create`
      (dynamic modules). Different parameters and error handling, behavior differences hidden
      in implementations.
- [ ] **Four-layer module loading** — linker(.static_modules) → static_module.c → init_launcher →
      module_manager. Each layer partially reimplements domain creation + memory mapping +
      thread startup. Bugs require tracing through all four layers.
- [ ] **Comments out of sync with code** — `fast_path.S` comment says "cooperative
      scheduling" but the code now has preemption checks and sti.
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
