# HIC Kernel

> **Core rewrite project**: Core-0 will be rewritten in **Core-lang** (short: **Core**), a systems language under active development.
> Core-lang repository: [github.com/dslsdzc/core](https://github.com/dslsdzc/core)

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
