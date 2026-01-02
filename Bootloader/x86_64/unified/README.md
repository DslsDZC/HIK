# HIK 统一引导系统

HIK (Hierarchical Isolation Kernel) 统一引导系统 - 支持 BIOS 和 UEFI 两种引导方式，使用同一个内核映像。

## 概述

HIK 统一引导系统允许用户在同一个磁盘上同时使用 BIOS 和 UEFI 两种引导方式启动 HIK 操作系统。系统会自动检测当前的引导模式，并加载相应的引导加载程序，最终加载相同的内核映像。

### 主要特性

- **双模式支持**：同时支持 BIOS（传统引导）和 UEFI（现代引导）
- **统一内核**：两种引导方式使用相同的内核映像格式
- **自动检测**：系统自动检测引导模式并加载相应的引导加载程序
- **灵活配置**：支持多种启动配置和启动参数
- **安全验证**：支持 RSA-3072 + SHA-384 签名验证
- **易于安装**：提供自动化安装脚本

## 架构设计

### 分区布局

统一引导系统使用 GPT 分区表，包含以下分区：

```
分区 1: BIOS 引导分区 (1MB)
  - 类型: BIOS boot (EF02)
  - 用途: BIOS 引导加载程序代码
  - 文件系统: 无

分区 2: EFI 系统分区 (100MB)
  - 类型: EFI System Partition (EF00)
  - 用途: UEFI 引导加载程序和内核
  - 文件系统: FAT32
  - 挂载点: /boot/efi

分区 3: HIK 系统分区 (剩余空间)
  - 类型: Linux filesystem (8300)
  - 用途: HIK 内核和系统文件
  - 文件系统: ext4
  - 挂载点: /
```

### 引导流程

#### BIOS 引导流程

```
BIOS → MBR → BIOS Boot Partition → Stage 2 → 内核
```

1. BIOS 加载 MBR 到 0x7C00
2. MBR 切换到保护模式
3. 从 BIOS Boot Partition 加载 Stage 2
4. Stage 2 加载内核映像
5. 验证内核签名
6. 切换到长模式
7. 跳转到内核入口点

#### UEFI 引导流程

```
UEFI → EFI Application → 内核
```

1. UEFI 固件加载 EFI 应用程序
2. EFI 应用程序初始化 UEFI 服务
3. 从 ESP 加载内核映像
4. 验证内核签名
5. 退出 Boot Services
6. 跳转到内核入口点

### 内核映像格式

两种引导方式使用相同的内核映像格式：

```c
typedef struct {
    uint64_t signature;          // HIK_KERNEL_MAGIC (0x48494B00)
    uint32_t version;            // 内核版本
    uint32_t flags;              // 标志位 (如 HIK_FLAG_SIGNED)
    uint64_t entry_point;        // 入口点偏移
    uint64_t code_offset;        // 代码段偏移
    uint64_t code_size;          // 代码段大小
    uint64_t data_offset;        // 数据段偏移
    uint64_t data_size;          // 数据段大小
    uint64_t config_offset;      // 配置段偏移
    uint64_t config_size;        // 配置段大小
    uint64_t signature_offset;   // 签名段偏移
    uint64_t signature_size;     // 签名段大小
    uint8_t  reserved[32];       // 保留字段
} kernel_header_t;
```

## 构建系统

### 依赖项

- **GCC**: GNU C 编译器（支持 32 位和 64 位）
- **NASM**: Netwide 汇编器
- **Binutils**: GNU 二进制工具（ld, objcopy）
- **Make**: 构建自动化工具
- **Parted**: 分区工具
- **GRUB**: GNU GRUB 引导加载程序

### 安装依赖

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install build-essential nasm binutils make parted grub-pc grub-efi-amd64
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc nasm binutils make parted grub2-pc grub2-efi-x64
```

**Arch Linux:**
```bash
sudo pacman -S base-devel nasm binutils make parted grub
```

### 构建步骤

1. **构建所有组件**:
   ```bash
   cd Bootloader/x86_64/unified
   make
   ```

2. **构建特定组件**:
   ```bash
   make kernel      # 仅构建内核
   make bios        # 仅构建 BIOS 引导加载程序
   make uefi        # 仅构建 UEFI 引导加载程序
   make unified     # 仅创建统一映像
   ```

3. **清理构建产物**:
   ```bash
   make clean
   ```

### 输出文件

构建完成后，会在 `build/` 目录下生成以下文件：

- `kernel.hik` - HIK 内核映像
- `bios-boot.img` - BIOS 引导加载程序映像
- `uefi-boot.efi` - UEFI 引导加载程序
- `hik-unified.img` - 统一可引导映像（64MB）

## 安装

### 使用安装脚本（推荐）

安装脚本提供了自动化安装功能，支持三种安装模式：

1. **仅 BIOS 模式**
2. **仅 UEFI 模式**
3. **双模式（推荐）**

```bash
cd Bootloader/x86_64/unified
sudo ./install.sh
```

按照提示选择安装模式和目标磁盘。

### 手动安装

如果需要手动安装，请按照以下步骤：

1. **创建分区**:
   ```bash
   sudo parted /dev/sdX mklabel gpt
   sudo parted /dev/sdX mkpart bios_grub 1MiB 2MiB
   sudo parted /dev/sdX set 1 bios_grub on
   sudo parted /dev/sdX mkpart EFI fat32 2MiB 102MiB
   sudo parted /dev/sdX set 2 esp on
   sudo parted /dev/sdX mkpart HIK ext4 102MiB 100%
   ```

2. **格式化分区**:
   ```bash
   sudo mkfs.vfat -F 32 /dev/sdX2
   sudo mkfs.ext4 /dev/sdX3
   ```

3. **挂载分区**:
   ```bash
   sudo mkdir -p /mnt/hik/efi /mnt/hik/root
   sudo mount /dev/sdX2 /mnt/hik/efi
   sudo mount /dev/sdX3 /mnt/hik/root
   ```

4. **安装 BIOS 引导加载程序**:
   ```bash
   sudo grub-install --target=i386-pc --boot-directory=/mnt/hik/root/boot /dev/sdX
   sudo dd if=build/bios-boot.img of=/dev/sdX1 bs=512 conv=notrunc
   ```

5. **安装 UEFI 引导加载程序**:
   ```bash
   sudo grub-install --target=x86_64-efi --efi-directory=/mnt/hik/efi --bootloader-id=HIK /dev/sdX
   sudo mkdir -p /mnt/hik/efi/EFI/HIK
   sudo cp build/uefi-boot.efi /mnt/hik/efi/EFI/HIK/
   ```

6. **复制内核和配置**:
   ```bash
   sudo mkdir -p /mnt/hik/root/HIK /mnt/hik/efi/EFI/HIK
   sudo cp build/kernel.hik /mnt/hik/root/HIK/
   sudo cp build/kernel.hik /mnt/hik/efi/EFI/HIK/
   sudo cp ../common/boot.cfg /mnt/hik/root/HIK/
   sudo cp ../common/boot.conf /mnt/hik/efi/EFI/HIK/
   ```

7. **添加 UEFI 启动项**:
   ```bash
   sudo efibootmgr -c -d /dev/sdX -p 2 -L "HIK Boot" -l /EFI/HIK/hikboot.efi
   ```

8. **卸载分区**:
   ```bash
   sudo umount /mnt/hik/efi /mnt/hik/root
   ```

## 配置

### BIOS 配置文件

BIOS 配置文件位于 `/HIK/boot.cfg`：

```
title = "HIK Boot Manager"
timeout = 5
default = 0

[entry]
name = "HIK Kernel"
kernel = "/HIK/kernel.hik"
args = "console=ttyS0,115200"
default = true
```

### UEFI 配置文件

UEFI 配置文件位于 `/EFI/HIK/boot.conf`：

```
title = "HIK Boot Manager"
timeout = 5
default = 0

[entry]
name = "HIK Kernel"
kernel = "\\EFI\\HIK\\kernel.hik"
args = "console=ttyS0,115200"
default = true
```

### 配置选项

- `title`: 引导管理器标题
- `timeout`: 默认引导超时（秒）
- `default`: 默认引导项索引
- `[entry]`: 定义引导项
  - `name`: 显示名称
  - `kernel`: 内核路径
  - `initrd`: initrd 路径（可选）
  - `args`: 内核命令行参数
  - `default`: 是否为默认项

## 测试

### 使用测试脚本

测试脚本可以自动测试两种引导模式：

```bash
cd Bootloader/x86_64/unified
./test.sh all          # 运行所有测试
./test.sh bios         # 仅测试 BIOS 引导
./test.sh uefi         # 仅测试 UEFI 引导
./test.sh kernel       # 仅测试内核加载
./test.sh config       # 仅测试配置文件
./test.sh clean        # 清理测试产物
```

### 使用 QEMU 手动测试

**测试 BIOS 引导**:
```bash
qemu-system-x86_64 \
  -drive file=build/hik-unified.img,format=raw \
  -serial stdio \
  -m 512M
```

**测试 UEFI 引导**:
```bash
qemu-system-x86_64 \
  -drive file=build/hik-unified.img,format=raw \
  -bios /usr/share/ovmf/OVMF.fd \
  -serial stdio \
  -m 512M
```

## 故障排除

### BIOS 引导失败

**问题**: 系统无法从 BIOS 引导

**解决方案**:
1. 检查 BIOS 引导顺序，确保磁盘在第一位
2. 验证 MBR 签名是否为 0xAA55
3. 检查 BIOS Boot Partition 是否存在
4. 查看串口输出获取错误信息

### UEFI 引导失败

**问题**: 系统无法从 UEFI 引导

**解决方案**:
1. 检查 UEFI 启动顺序
2. 验证 ESP 是否正确格式化为 FAT32
3. 检查 UEFI 启动项是否存在：`sudo efibootmgr`
4. 查看 UEFI 日志获取错误信息

### 内核加载失败

**问题**: 引导加载程序无法加载内核

**解决方案**:
1. 检查内核文件是否存在
2. 验证内核魔数是否为 0x48494B00
3. 检查内核路径是否正确
4. 验证内核签名（如果启用）

### 签名验证失败

**问题**: 内核签名验证失败

**解决方案**:
1. 检查内核是否已正确签名
2. 验证公钥是否正确
3. 临时禁用签名验证进行测试
4. 检查签名算法是否匹配

## 与传统引导系统的对比

| 特性 | 传统 BIOS | 传统 UEFI | HIK 统一系统 |
|------|----------|-----------|-------------|
| 引导模式 | 仅 BIOS | 仅 UEFI | 同时支持 |
| 内核映像 | 单独格式 | 单独格式 | 统一格式 |
| 分区表 | MBR | GPT | GPT |
| 安装复杂度 | 低 | 中 | 中 |
| 兼容性 | 仅旧系统 | 仅新系统 | 全兼容 |
| 维护成本 | 高 | 高 | 低 |

## 最佳实践

1. **使用双模式安装**: 推荐同时安装 BIOS 和 UEFI 引导加载程序，以获得最大兼容性
2. **定期备份**: 备份引导分区和配置文件
3. **测试更新**: 在更新前先在测试环境中验证
4. **监控日志**: 定期检查引导日志以发现潜在问题
5. **保持更新**: 及时更新引导加载程序和内核

## 安全建议

1. **启用签名验证**: 始终启用内核签名验证
2. **使用安全启动**: 在 UEFI 系统上启用 Secure Boot
3. **保护密钥**: 妥善保管签名密钥
4. **定期审计**: 定期审计引导配置和文件
5. **限制访问**: 限制对引导分区的访问权限

## 未来改进

计划中的改进：

1. **网络引导**: 支持 PXE/iSCSI 网络引导
2. **多内核支持**: 支持多个内核版本并存
3. **回滚机制**: 自动回滚到上一个稳定版本
4. **TPM 集成**: 支持 TPM 2.0 度量启动
5. **加密支持**: 支持加密内核映像
6. **容器化**: 支持容器化内核映像

## 参考资源

- [HIK 架构文档](../../../../三层模型.md)
- [BIOS 引导加载程序](../boot/README.md)
- [UEFI 引导加载程序](../uefi/README.md)
- [引导加载程序设计](../../../../引导加载程序.md)
- [UEFI 规范](https://uefi.org/specifications)
- [GRUB 文档](https://www.gnu.org/software/grub/manual/)

## 贡献

欢迎贡献！请确保：

- 代码遵循现有的风格和约定
- 更改有良好的文档
- 进行充分测试
- 如需要，更新构建系统

## 许可证

本引导加载程序是 HIK (Hierarchical Isolation Kernel) 项目的一部分。

## 联系方式

如有问题和建议，请参考主 HIK 项目文档。

---

**最后更新**: 2026-01-02  
**版本**: 1.0  
**状态**: 完成 ✅