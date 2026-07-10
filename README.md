# HIC Kernel

> **Core rewrite project**: Core-0 will be rewritten in **Core-lang** (short: **Core**), a systems language under active development.
> Core-lang repository: [github.com/dslsdzc/core](https://github.com/dslsdzc/core)

## Performance

IPC 3.0 entry-page self-check cross-domain call — **measured in userspace** on AMD A8-7650K @ 3.6 GHz:

| Measurement | Latency |
|---|---|
| `bt [bitmap], ecx` (single authorization check) | **2.83 ± 0.38 cycles** (~0.8 ns) |
| Full IPC3 rapid path round-trip (call → bt → jnc → jmp → ret) | **8.04 ± 0.98 cycles** (~2.2 ns) |
| Overhead vs plain indirect call (bt + jnc + jmp) | **~4.3 cycles** |

Results validated with Duff's device K-batch (K=1..128), weighted OLS fit (R²=1.0), MAD outlier filter. Run yourself: see `tests/benchmarks/`.

- IPC3 design doc prediction: **6-9 cycles** — matched ✓
- seL4 IPC: ~200-500 ns, Linux syscall: ~50-100 ns

HIC (Hierarchical Isolation Core) is a multi-architecture microkernel with a
three-tier privilege architecture and IPC 3.0 (entry-page self-check cross-domain
call).

Supported architectures:
- x86_64 (QEMU, UEFI)
- ARM64 (QEMU virt, GICv3, PL011)
- STM32F103C8T6 (Cortex-M3)

## Quick Start

```bash
# x86_64
make
make img-run

# ARM64
make ARCH=arm64
make ARCH=arm64 run
```

Build output goes to `build/`.

## Architecture

- Three-layer model: Core-0 (Ring 0), Privileged-1 (Ring 0, MMU isolated), Application-3 (Ring 3)
- IPC 3.0: entry-page self-check cross-domain call
- Per-core capability slot allocation (no global lock)
- TLV boot protocol
- Static interrupt routing table

## Documentation

- Architecture: [docs/TD/三层模型.md](docs/TD/三层模型.md)
- IPC 3.0: [docs/TD/HICIPC模型3.0.md](docs/TD/HICIPC模型3.0.md)
- ABI specification: [docs/TD/ABI规范.md](docs/TD/ABI规范.md)
- API list: [docs/TD/API列表.md](docs/TD/API列表.md)

## Contributing

See [CONTRIBUTING.md](.github/CONTRIBUTING.md) and [GOVERNANCE.md](.github/GOVERNANCE.md).

## Security

See [SECURITY.md](.github/SECURITY.md).

## License

GPL-2.0 with LicenseRef-HIC-service-exception.
See [LICENSE](LICENSE) for details.
