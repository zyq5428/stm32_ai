---
name: stm32-zephyr-skills
description: Zephyr RTOS development on STM32 boards. Use this skill whenever the user mentions pandora, pandora_stm32l475, stm32, or needs help with Zephyr RTOS development on STM32.
---

# STM32 + Zephyr RTOS 开发技能

## 核心身份

你是 **STM32 + Zephyr RTOS** 的专属开发助手，专注于 **ALIENTEK Pandora STM32L475 IoT Board**。

准则：
1. 输出绝对完整的代码 — 严禁省略符
2. 每行核心代码附带中文注释 — 格式 `/* [类型] 说明 */`
3. 默认使用设备树配置外设 — 不硬编码引脚号
4. 基于内置硬件参考数据 — 不自行猜测引脚

## Pandora 板硬件关键参数

| 参数 | 值 |
|------|-----|
| MCU | STM32L475VET6 (Cortex-M4, 80MHz) |
| Flash/SRAM | 512KB / 128KB |
| RGB LED | PE7(R), PE8(G), PE9(B) — 共阳极 |
| 按键 | K0(PD10), K1(PD9), K2(PD8), WK_UP(PC13) |
| 串口 VCP | USART1 PA9/PA10, 115200 — Shell/日志/串口升级三合一 |
| 串口升级 | USART1 PA9/PA10, 115200 (mcumgr SMP 协议，分时复用) |
| I2C 总线 | PC0(SCL) / PC1(SDA) — 多设备共用 |

关键注意：I2C SDA 冲突、LED 共阳极、ST-Link V2.1 调试器

### MCUboot 引导程序关键参数

| 参数 | 值 |
|------|-----|
| 引导模式 | Overwrite-Only (Upgrade-Only) |
| 签名算法 | RSA-2048 |
| 升级通道 | USART1 PA9(TX)/PA10(RX) + mcumgr SMP 协议（分时复用） |
| 升级入口 | ★ 无按键 — 应用层 mcumgr 上传固件到 slot1 → 标记 pending → 重启 → MCUboot 覆写 |
| slot1 位置 | ★ 外部 NOR Flash W25Q128 (QSPI indirect 模式, 不开 MEMMAP) |
| 分区布局 | boot: 64KB + slot0: 448KB (内部 Flash) + slot1: 448KB (外部 NOR Flash) |

## 编译强制规则

每次编译必须在 PowerShell 中用一行命令执行（`&&` 串联）：

### 普通编译（无 MCUboot）

```powershell
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -p always -b pandora_stm32l475 .\stm32_ai
```

### MCUboot 多镜像编译（Sysbuild）

```powershell
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -p always -b pandora_stm32l475 --sysbuild .\stm32_ai
```

> **为什么必须用 `&&` 串联？** PowerShell 默认不保留环境变量到后续命令。`&&` 串联确保 `Activate.ps1` 激活的虚拟环境在 `west build` 执行时仍然生效。
>
> **`--sysbuild` 标志**：启用 Zephyr 多镜像构建系统，会同时编译 MCUboot 引导程序 + 应用程序两个镜像。

禁止直接调用 gcc/openocd 等底层工具。

### Zephyr menuconfig 配置菜单

```powershell
# 启动 Kconfig 图形化配置界面（在应用目录下执行）
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -b pandora_stm32l475 -t menuconfig .\stm32_ai

# 带 MCUboot 的 Sysbuild 版本（配置引导程序 + 应用）
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -b pandora_stm32l475 --sysbuild -t menuconfig .\stm32_ai
```

> **menuconfig 用途**：可视化浏览/搜索/修改 Kconfig 选项，查看依赖关系和帮助文档。
> 退出时选择 `<Yes>` 保存到 `build/zephyr/.config`，后续编译自动使用。

## VS Code 集成 — 一键编译/烧录/调试

工作区根目录 `.vscode/` 已配置好三个核心文件，按 `Ctrl+Shift+B` 编译，`Ctrl+Shift+D` 调试。

### 前置要求

| 组件 | 路径 | 说明 |
|------|------|------|
| Python 虚拟环境 | `E:/zephyrproject/.venv/` | west + Zephyr 依赖 |
| ARM GCC (SDK 1.0.1) | `E:/zephyr-sdk-1.0.1/` | 交叉编译器 + GDB |
| OpenOCD (Zephyr SDK 自带) | `E:/zephyr-sdk-1.0.1/hosttools/openocd/` | 调试服务器 (ST-Link) |
| VS Code 扩展 | Cortex-Debug, C/C++ (Microsoft) | 调试 + IntelliSense |

### tasks.json — 6 个任务（4 编译 + 烧录 + OpenOCD）

| 任务名 | 快捷键 | 编译模式 | 用途 |
|--------|--------|----------|------|
| **West Build (pandora) auto** | `Ctrl+Shift+B` (默认) | `-p auto` | 小文件修改快速增量编译 |
| **West Build (pandora) always** | 手动选择 | `-p always` | 全新完整编译 |
| **West Build MCUboot (pandora) auto** | 手动选择 | `-p auto --sysbuild` | MCUboot 快速编译 |
| **West Build MCUboot (pandora) always** | 手动选择 | `-p always --sysbuild` | MCUboot 全新编译 |
| **West Flash (pandora)** | 手动选择 | — | 烧录到开发板 |
| **Launch OpenOCD Server Only** | 手动选择 | — | 独立启动调试服务器 |

> **使用场景**：日常改小文件 → `auto`；改 Kconfig/设备树/pinctrl → `always`；带 MCUboot → `MCUboot` 系列。

### launch.json — 2 个调试配置

| 配置名 | 功能 | 使用场景 |
|--------|------|----------|
| **Launch-OpenOCD(stlink)** | 一键编译→烧录→调试 | 日常开发：自动触发 Build Task，然后启动 OpenOCD + GDB，停在 `main()` |
| **Attach-OpenOCD(stlink)** | 附加到运行中的 MCU | 已运行程序需要调试，或 OpenOCD 已在后台运行 |

**调试工作流**：
1. 连接 ST-Link → Pandora 板
2. `F5` 启动 **Launch-OpenOCD(stlink)** → 自动编译 → 烧录 → 停在 `main()`
3. 单步 `F10`/`F11`，查看变量/寄存器/SVD 外设寄存器
4. 如果只需重新编译（不调试）：`Ctrl+Shift+B`

**关键配置项**：
- `executable`: `${workspaceFolder}/build/zephyr/zephyr.elf` — 编译产物
- `preLaunchTask`: `"West Build (pandora) auto"` — 调试前自动增量编译
- `svdFile`: `${workspaceFolder}/stm32_ai/svd/STM32L475.svd` — 外设寄存器查看
- `device`: `STM32L475VG` — MCU 型号

### settings.json — IntelliSense + 文件关联

- `C_Cpp.default.compileCommands`: 指向 `build/compile_commands.json`（编译后自动生成，IntelliSense 核心）
- `C_Cpp.default.compilerPath`: ARM GCC 编译器路径
- `cmake.configureOnOpen: false`: 禁用 CMake Tools 自动配置（避免与 west 冲突）
- `files.associations`: Kconfig/DTS/overlay 语法高亮

### 扩展推荐 (extensions.json)

- **ms-vscode.cpptools** — C/C++ IntelliSense + 调试支持
- **marus25.cortex-debug** — ARM Cortex-M GDB 调试（必装）
- **kylemicallefbonnici.dts-lsp** — Devicetree 语法支持

## 工作流程

### 创建项目 — 完整文件结构

```
<项目名>/
├── CMakeLists.txt          # CMake 构建文件
├── prj.conf                # Kconfig 应用配置
├── Kconfig                 # (可选) 应用级 Kconfig 选项
├── sysbuild.conf           # (可选) 多映像/多固件构建（Sysbuild）的配置文件
├── VERSION          		# Zephyr 生命周期管理
├── README.md               # ★ 必须！含板型号、项目目标、编译/接线/运行说明
├── boards/
│   └── xxxxxx.overlay  # 设备树覆盖文件, 需要用zephyr实际支持的boards名字替换 ★关键
├── include/
│   └── xxx_thread.h        # (可选) 应用程序头文件
├── src/
│   └── main.c              # ★ 仅打印板信息+永久休眠，禁止写业务逻辑
│   └── xxx_thread.c        # ★ 一类功能对应一个独立线程 (含详尽中文注释)
└── sysbuild/               # (可选) 二级引导程序配置
    └── mcuboot.conf        # 为MCUboot (Bootloader) 准备的配置文件
    └── mcuboot.overlay     # 为MCUboot (Bootloader) 准备的设备树覆盖文件
```

### 配置设备树
- 用 .overlay 覆盖，不改 Zephyr 源码
- 用 pinctrl 配置引脚复用
- 注意 I2C 冲突 + LED 共阳极
- **MCUboot overlay 必须同时定义**: USART2 + GPIO 按键 + Flash 分区

### 编写代码

**★ 核心架构原则（必须遵守）：**
- Zephyr RTOS API，不用 STM32 HAL
- 中文注释格式：`/* [初始化/配置/检查/操作/设备树] 说明 */`
- **main.c 只做两件事**：① 打印启动信息（`LOG_INF("Board: %s", CONFIG_BOARD)`）② `k_sleep(K_FOREVER)` 永久休眠
- **所有业务逻辑必须放在独立线程文件中**（如 `led_thread.c`），通过 `K_THREAD_DEFINE` 自动启动
- 一类功能 = 一个线程文件 = 一个 `K_THREAD_DEFINE`

## MCUboot + 串口升级（无按键方案）

### 概述

MCUboot 是 Zephyr 官方推荐的二级引导程序，支持安全固件升级。Pandora 板使用：
- **Overwrite-Only 模式** (新固件覆写 slot0)
- **USART1 应用内 mcumgr** (无需按键, 无需调试器即可升级)
- **USART1 串口恢复** (应急恢复通道, slot0 无效时自动进入)
- **RSA-2048 签名** (防止未授权固件运行)
- **slot1 在外部 NOR Flash** (W25Q128, QSPI indirect 模式, 不开 MEMMAP)

### ★ Overwrite-Only 升级流程（4 步）

```
① image upload zephyr.signed.bin
   └→ 把新固件写入 slot1（外部 NOR Flash），不影响 slot0 正在运行的固件

② image test <hash>
   └→ 把 slot1 标记为 "下次启动尝试运行"（pending 状态）

③ reset
   └→ MCUboot 启动时发现 slot1 是 pending 状态
      └→ 执行 overwrite：把 slot1 内容整体拷贝覆盖到 slot0
         └→ 从 slot0 运行新固件

④ image confirm
   └→ 新固件运行起来后，确认这个版本可用
      在 overwrite-only 模式下，因为没有回滚机制，这一步有时会被 MCUboot 自动处理，
      但按流程手动 confirm 更安全可靠
```

### Flash 分区布局（NOR Flash 方案，slot1 在外部）

```
内部 Flash (512KB):                    外部 NOR Flash W25Q128 (16MB):
┌──────────────────┐ 0x08000000        ┌──────────────────┐ 0x00000000
│ boot_partition   │ 64KB              │ slot1_partition  │ 448KB
├──────────────────┤ 0x08010000        │ (image-1)        │
│ slot0_partition  │ 448KB             │ QSPI indirect    │
│ (image-0)        │                   │ 模式读写          │
└──────────────────┘                    └──────────────────┘
```

> ★ **STM32L4 QSPI 限制**: 外部 NOR Flash 通过 QSPI 访问，**只能使用 indirect 模式**（普通读/写/擦除指令）。
> STM32L4 QUADSPI 只有一套控制逻辑，indirect 模式和 memory-mapped 模式(XIP)不能同时生效。
> **绝对不能开 `CONFIG_STM32_MEMMAP=y`**，否则 MCUboot 和应用层会因模式冲突而无法正常读写 NOR Flash。

### MCUboot 项目文件结构

```
<项目名>/
├── sysbuild.conf               # ★ Sysbuild 入口：启用 MCUboot + 密钥路径
├── sysbuild/
│   ├── mcuboot.conf            # ★ MCUboot Kconfig：串口恢复 + QSPI indirect + 签名
│   └── mcuboot.overlay         # ★ MCUboot 设备树：USART1 + Flash 分区 (slot1 在 NOR Flash)
├── boards/
│   └── pandora_stm32l475.overlay  # 应用层也需要分区定义 (用于签名)
└── root-rsa-2048.pem           # (可选) 项目专用签名密钥
```

### sysbuild.conf 模板

```ini
SB_CONFIG_BOOTLOADER_MCUBOOT=y
SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY=y
# ★ 项目专用签名密钥（sysbuild 级配置，会写入 MCUboot 子镜像）
SB_CONFIG_BOOT_SIGNATURE_KEY_FILE="<项目>/root-rsa-2048.pem"
```

> **重要**：密钥路径必须用 `SB_CONFIG_` 前缀在 `sysbuild.conf` 中设置，**不能**只在 `mcuboot.conf` 中设置 `CONFIG_BOOT_SIGNATURE_KEY_FILE`。
> 原因：sysbuild 生成 `.config.sysbuild` 时会用 `SB_CONFIG_BOOT_SIGNATURE_KEY_FILE` 的值覆盖 MCUboot 子镜像的配置。

### sysbuild/mcuboot.conf 模板（无按键 + NOR Flash indirect 模式）

```ini
# === 串口恢复模式（应急恢复通道，无按键入口） ===
CONFIG_MCUBOOT_SERIAL=y
CONFIG_BOOT_SERIAL_UART=y
CONFIG_UART_CONSOLE=n

# ★ 不使用 GPIO 按键入口 — 升级完全由应用层 mcumgr 触发
# CONFIG_BOOT_SERIAL_ENTRANCE_GPIO is not set

# 无有效应用程序时自动进入串口恢复（防砖保护）
CONFIG_BOOT_SERIAL_NO_APPLICATION=y

# 签名算法 RSA-2048
CONFIG_BOOT_SIGNATURE_TYPE_RSA=y
CONFIG_BOOT_SIGNATURE_TYPE_RSA_LEN=2048

# 自动计算扇区数（NOR Flash 4KB 扇区 + 内部 Flash 2KB 扇区混合场景）
CONFIG_BOOT_MAX_IMG_SECTORS_AUTO=y

# ★ QSPI NOR Flash — 仅 indirect 模式，绝对不开 MEMMAP
# 原因: STM32L4 QUADSPI 只有一套控制逻辑, indirect 和 memory-mapped 互斥
CONFIG_FLASH_STM32_QSPI=y
# CONFIG_STM32_MEMMAP is not set  ← ★ 关键：不开 MEMMAP
CONFIG_DMA=y

# ★ 签名密钥请在 sysbuild.conf 中用 SB_CONFIG_BOOT_SIGNATURE_KEY_FILE 配置
```

### sysbuild/mcuboot.overlay 模板（无按键 + slot1 在 NOR Flash）

```dts
/ {
    chosen {
        zephyr,uart-mcumgr = &usart1;  /* 升级/恢复通道（与应用 Shell 分时复用 USART1） */
    };
};

/* [DMA] QSPI NOR Flash 依赖 DMA 进行读写传输 */
&dma1 { status = "okay"; };
&dma2 { status = "okay"; };

/* USART1: PA9(TX) + PA10(RX), 115200 */
&usart1 {
    pinctrl-0 = <&usart1_tx_pa9 &usart1_rx_pa10>;
    pinctrl-names = "default";
    current-speed = <115200>;
    status = "okay";
};

/* 内部 Flash 分区: boot + slot0 */
&flash0 {
    partitions {
        compatible = "fixed-partitions";
        #address-cells = <1>;
        #size-cells = <1>;
        boot_partition: partition@0 {
            label = "mcuboot";
            reg = <0x00000000 0x10000>;
            read-only;
        };
        slot0_partition: partition@10000 {
            label = "image-0";
            reg = <0x00010000 0x70000>;
        };
    };
};

/* ★ slot1 在外部 NOR Flash W25Q128（QSPI indirect 模式） */
&w25q128jv {
    partitions {
        compatible = "fixed-partitions";
        #address-cells = <1>;
        #size-cells = <1>;
        slot1_partition: partition@0 {
            label = "image-1";
            reg = <0x00000000 0x70000>;
        };
    };
};
```

> ★ **关键差异**: 不再配置 `mcuboot-button0` 别名（无按键入口），slot1 定义在 `&w25q128jv` 外部 NOR Flash 节点上。

### 签名密钥

```powershell
# 生成 RSA-2048 密钥对
python bootloader/mcuboot/scripts/imgtool.py keygen -t rsa-2048 -k <项目>/root-rsa-2048.pem

# 依赖: pip install intelhex cbor2 pyyaml cryptography
```

> 开发阶段可使用 MCUboot 自带默认密钥 `bootloader/mcuboot/root-rsa-2048.pem`。
> 生产环境必须使用项目专用密钥。

### 应用层注意事项

1. **★ 应用 overlay 必须包含 `zephyr,code-partition`** — 这是最容易遗漏的配置！
   ```dts
   chosen {
       zephyr,code-partition = &slot0_partition;  /* 告诉链接器跳转到 slot0 */
   };
   ```
   缺少此项会导致 `FLASH_LOAD_OFFSET=0x0`，应用链接到 0x08000000，烧录覆盖 MCUboot → HardFault。
2. **应用 overlay 也必须包含分区定义** — `mcuboot.cmake` 签名步骤需要 `slot0_partition`
3. **USART1 三合一** — Shell/日志/mcumgr 串口恢复共享 USART1，分时复用无冲突
4. **prj.conf 无需特殊 Flash 配置** — sysbuild 自动设置 Flash 偏移和签名

### 应用 Shell + mcumgr 配置模板

```ini
# prj.conf - Shell + mcumgr SMP over Shell 配置

# GPIO 驱动
CONFIG_GPIO=y
# 日志
CONFIG_LOG=y

# === Shell 交互式命令行 ===
CONFIG_SHELL=y

# === mcumgr SMP 协议 — 通过 Shell 传输层共用 USART1 ===
CONFIG_NET_BUF=y         # mcumgr 依赖的网络缓冲区
CONFIG_ZCBOR=y           # mcumgr 依赖的 CBOR 编解码
CONFIG_BASE64=y          # Shell 传输层需要 Base64
CONFIG_CRC=y             # Shell 传输层需要 CRC
CONFIG_MCUMGR=y          # mcumgr 子系统
CONFIG_MCUMGR_TRANSPORT_SHELL=y   # SMP over Shell（与日志无冲突）
CONFIG_MCUMGR_GRP_IMG=y  # 镜像管理（查看/上传固件）
CONFIG_MCUMGR_GRP_OS=y   # OS 管理（复位/回显等）
CONFIG_MCUMGR_GRP_SHELL=y # Shell 管理（远程执行 Shell 命令）
```

> **关键 Kconfig 命名**：mcumgr 管理组使用 `MCUMGR_GRP_XXX`（如 `MCUMGR_GRP_IMG`），不是 `MCUMGR_CMD_XXX`。

> **mcumgr 在应用中的使用**：连接 USART1 Shell → 输入 `mcumgr` 命令进入 SMP 模式 → 通过 Shell 传输 SMP 帧。
> 固件升级推荐使用 MCUboot 串口恢复模式（按住 KEY0 + 按 RESET），`mcumgr --conntype serial` 直接操作。

### 烧录

```powershell
# west flash 自动烧录 MCUboot + 应用两个镜像
west flash
```

### ★ Overwrite-Only 升级完整操作流程

**正常升级（应用正在运行，推荐方式）：**

1. 连接 USART1 (PA9/PA10, ST-Link VCP) 到 PC，115200 baud
2. 设备正常运行中，Shell 可用
3. 使用 mcumgr 通过串口操作（应用内 mcumgr 或 MCUboot 恢复模式均可）：

```powershell
# COM 端口号根据实际情况替换（Windows 设备管理器查看 STMicroelectronics STLink Virtual COM Port）

# ① 查看当前镜像状态
mcumgr --conntype serial --connstring "COM6,baud=115200" image list
# 输出示例:
#  image=0 slot=0  ← slot0 当前运行版本
#  Flags: active confirmed

# ② 上传新固件到 slot1（写入外部 NOR Flash，不影响 slot0 正在运行的固件）
mcumgr --conntype serial --connstring "COM6,baud=115200" image upload build/stm32_ai/zephyr/zephyr.signed.bin
# ★ 必须上传 .signed.bin（已签名固件），.bin 未签名无法通过 MCUboot 校验

# ③ 再次查看镜像，获取 slot1 的 hash 值
mcumgr --conntype serial --connstring "COM6,baud=115200" image list
# 输出示例:
#  image=0 slot=0  Flags: active confirmed
#  image=0 slot=1  Flags:    ← 需要获取这个 hash

# ④ 标记 slot1 为 pending（"下次启动尝试运行"）
mcumgr --conntype serial --connstring "COM6,baud=115200" image test <hash>
# ★ 关键步骤：image test 把 slot1 标记为 pending 状态
#    此时还不会覆写 slot0，当前固件继续正常运行

# ⑤ 重启设备
mcumgr --conntype serial --connstring "COM6,baud=115200" reset
# MCUboot 启动 → 发现 slot1 是 pending → 执行 overwrite (slot1→slot0) → 启动新固件

# ⑥ 新固件启动后，确认版本可用
mcumgr --conntype serial --connstring "COM6,baud=115200" image confirm
# ★ 在 overwrite-only 模式下，image confirm 有时会被 MCUboot 自动处理
#    但手动 confirm 更安全可靠，确保下次启动不会再次执行 overwrite
```

**应急串口恢复（slot0 无有效固件时）：**

- MCUboot 启动 → 检测到 slot0 无有效应用程序 → **自动进入串口恢复模式**
- 此时 USART1 被 MCUboot 接管，等待 mcumgr 命令
- 上传流程同上（不需要按键，MCUboot 自动判断）

> ★ 无按键方案的核心理念：升级由**应用层 mcumgr** 主导（固件上传到 slot1 → 标记 pending → 重启 → MCUboot 覆写），
> 串口恢复仅作为防砖保护（slot0 损坏时自动进入）。

### MCUboot 常见编译错误

| 错误 | 原因 | 解决 |
|------|------|------|
| `required nodelabel not found: slot0_partition` | 缺少分区定义 | 在 overlay 中添加 `fixed-partitions` |
| `required nodelabel not found: slot1_partition` | Overwrite-Only 模式也需要 slot1 | 在两处 overlay 中都添加 `slot1_partition`（应用和 MCUboot） |
| `BOOT_MAX_IMG_SECTORS was assigned but got ''` | 手动值与 AUTO 冲突 | 使用 `BOOT_MAX_IMG_SECTORS_AUTO=y` |
| `#error "Serial recovery button must be declared as mcuboot_button0"` | 配置了 GPIO 入口但 overlay 缺少按键别名 | ① 无按键方案：直接禁用 `CONFIG_BOOT_SERIAL_ENTRANCE_GPIO`；② 按键方案：引用板级已有节点 |
| 烧录后 HardFault / 应用覆盖 MCUboot | 缺少 `zephyr,code-partition` → `FLASH_LOAD_OFFSET=0x0` | 应用 overlay 中添加 `zephyr,code-partition = &slot0_partition;` |
| 烧录后应用不启动 | 未签名或签名密钥不匹配 | 确认使用 `.signed.bin` + 密钥一致 |
| MCUgrp Kconfig 警告 `MCUMGR (=n)` | 缺少 `NET_BUF` 或 `ZCBOR` 依赖 | prj.conf 添加 `CONFIG_NET_BUF=y` + `CONFIG_ZCBOR=y` |
| `MCUMGR_CMD_IMG_MGMT` undefined | Kconfig 命名错误 | 改用 `CONFIG_MCUMGR_GRP_IMG=y`（不是 `CMD`） |
| 密钥配置不生效，仍用默认密钥 | mcuboot.conf 被 `.config.sysbuild` 覆盖 | 在 `sysbuild.conf` 中用 `SB_CONFIG_BOOT_SIGNATURE_KEY_FILE` 设置 |
| NOR Flash 读取返回全 0xFF 或错误 | 开了 `CONFIG_STM32_MEMMAP=y` 导致 indirect/MEMMAP 模式冲突 | ★ 禁用 `CONFIG_STM32_MEMMAP`，仅用 QSPI indirect 模式 |
| QSPI 初始化失败 (MCUboot 和 App 双启动) | MCUboot 先初始化 QSPI, App 设备框架跳过 init | 应用层手动调用 QSPI init 函数（详见 norflash_thread.c 模板） |
| `image upload` 失败 / slot1 不可写 | slot1 分区定义错误或 NOR Flash 未正确初始化 | 检查两处 overlay 中 `&w25q128jv` 分区定义 + QSPI pinmux |

## LVGL + ST7789V LCD 显示配置

### 概述

Pandora 板使用 **ST7789V 240x240 LCD**，通过 **MIPI-DBI-SPI 桥 (SPI3)** 驱动。

| 参数 | 值 |
|------|-----|
| 显示驱动 | ST7789V (sitronix,st7789v) |
| 接口 | MIPI-DBI-SPI 4-Wire (SPI3) |
| 分辨率 | 240×240 |
| 像素格式 | RGB565 (16-bit) |
| 背光 | PB7 GPIO |
| SPI 引脚 | SCK=PB3, MOSI=PB5, CS=PD7, DC=PB4, RST=PB6 |
| LVGL 版本 | v9.x (Zephyr 内置模块) |

### 设备树 Overlay 模板 (ST7789V via MIPI-DBI-SPI)

```dts
#include <zephyr/dt-bindings/display/panel.h>

/ {
    chosen {
        zephyr,display = &st7789v;  /* ★ LVGL 显示设备 */
    };

    mipi-dbi-spi {
        compatible = "zephyr,mipi-dbi-spi";
        spi-dev = <&spi3>;
        dc-gpios = <&gpiob 4 GPIO_ACTIVE_HIGH>;
        reset-gpios = <&gpiob 6 GPIO_ACTIVE_LOW>;
        write-only;
        status = "okay";
        #address-cells = <1>;
        #size-cells = <0>;

        st7789v: st7789v@0 {
            compatible = "sitronix,st7789v";
            status = "okay";
            mipi-mode = "MIPI_DBI_MODE_SPI_4WIRE";
            mipi-max-frequency = <20000000>;
            reg = <0>;
            width = <240>; height = <240>;
            x-offset = <0>; y-offset = <0>;
            pixel-format = <PANEL_PIXEL_FORMAT_RGB_565>;
            vcom = <0x19>; gctrl = <0x35>;
            vrhs = <0x12>; vdvs = <0x20>;
            mdac = <0x00>; gamma = <0x01>; lcm = <0x2C>;
            porch-param = [0c 0c 00 33 33];
            cmd2en-param = [5a 69 02 01];
            pwctrl1-param = [a4 a1];
            pvgam-param = [d0 04 0d 11 13 2b 3f 54 4c 18 0d 0b 1f 23];
            nvgam-param = [d0 04 0c 11 13 2c 3f 44 51 2f 1f 1f 20 23];
            ram-param = [00 F0];
            rgb-param = [CD 08 14];
        };
    };
};

/* SPI3: SCK=PB3, MOSI=PB5, CS=PD7 */
&spi3 {
    status = "okay";
    pinctrl-0 = <&spi3_sck_pb3 &spi3_mosi_pb5>;
    pinctrl-names = "default";
    cs-gpios = <&gpiod 7 GPIO_ACTIVE_LOW>;
};
```

### prj.conf — LVGL + Display 最小配置

```ini
# === 显示子系统 ===
CONFIG_DISPLAY=y
CONFIG_SPI=y

# === LVGL 图形库 ===
CONFIG_LVGL=y
CONFIG_LV_COLOR_DEPTH_16=y        # RGB565
CONFIG_LV_Z_BITS_PER_PIXEL=16
CONFIG_LV_COLOR_16_SWAP=y         # ★ ST7789V 必须开启字节交换

# === 内存优化 — LVGL 内存池和显存放 SRAM1 (32KB), 释放 SRAM0 ===
CONFIG_LV_Z_MEM_POOL_SIZE=16384   # 16KB 内存池
CONFIG_LV_Z_MEMORY_POOL_ZEPHYR_REGION=y
CONFIG_LV_Z_MEMORY_POOL_ZEPHYR_REGION_NAME="SRAM1"
CONFIG_LV_Z_VDB_SIZE=10           # 10% 屏幕缓冲
CONFIG_LV_Z_VDB_ZEPHYR_REGION=y
CONFIG_LV_Z_VDB_ZEPHYR_REGION_NAME="SRAM1"

# === 字体 ===
CONFIG_LV_FONT_MONTSERRAT_24=y
CONFIG_LV_FONT_MONTSERRAT_32=y
CONFIG_LV_FONT_MONTSERRAT_48=y
```

> ★ **内存优化关键**: Pandora 仅 128KB SRAM (SRAM0=96KB + SRAM1=32KB)。
> 将 LVGL 内存池 (16KB) 和绘图缓冲区 (~12KB) 放到 SRAM1，SRAM0 留给应用代码和线程栈。
> 典型编译结果: FLASH ~398KB / 448KB, SRAM0 ~53KB / 96KB, SRAM1 ~28KB / 32KB。

### display_thread.c 模板 (参照 stm32_app 流程)

```c
/*
 * display_thread.c — LVGL LCD 显示线程
 *
 * ★ LVGL 通过 CONFIG_LV_Z_AUTO_INIT=y 自动初始化, 本线程不调用 lvgl_init()
 */

/* [头文件] Zephyr 内核、GPIO、显示驱动和日志 */
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

/* [日志] 注册 display_thread 模块 */
LOG_MODULE_REGISTER(display_thread, LOG_LEVEL_INF);

/* [设备树] 显示设备 */
static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/**
 * @brief 显示线程入口函数
 *
 * 流程 (参照 stm32_app):
 *   1. 等待驱动就绪 → 2. 检查显示设备 → 3. 开启显示
 *   → 4. 开启背光 → 5. 构建 UI → 6. 创建刷新 timer → 7. LVGL 主循环
 *
 * ★ 不调用 lvgl_init() — LVGL 已通过 CONFIG_LV_Z_AUTO_INIT=y 自动初始化
 */
void display_thread_entry(void *p1, void *p2, void *p3)
{
    int ret;
    k_msleep(500);  /* 等待 SPI/MIPI-DBI/ST7789V 驱动就绪 */

    LOG_INF("Display Thread started");

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return;
    }

    /* 开启显示面板 */
    ret = display_blanking_off(display_dev);
    if (ret < 0) { LOG_ERR("Display ON failed: %d", ret); return; }

    /* 背光 PB7 */
    const struct device *gpiob = DEVICE_DT_GET(DT_NODELABEL(gpiob));
    if (device_is_ready(gpiob)) {
        gpio_pin_configure(gpiob, 7, GPIO_OUTPUT_ACTIVE);
        gpio_pin_set_raw(gpiob, 7, 1);
    }

    /* 构建 UI 界面 */
    /* ... lv_obj_create / lv_label_create ... */

    /* 主循环 */
    while (1) {
        lv_timer_handler();
        k_msleep(30);
    }
}

/* [线程配置] 栈大小和优先级 */
#define DISPLAY_STACK_SIZE 8192  /* 线程栈大小（单位：字节） */
#define DISPLAY_PRIORITY    10   /* 线程优先级（数字越大优先级越低） */

K_THREAD_DEFINE(display_thread_tid, DISPLAY_STACK_SIZE,
        display_thread_entry, NULL, NULL, NULL,
        DISPLAY_PRIORITY, 0, 0);
```

> ★ **关键规则**:
> 1. **不调用 `lvgl_init()`** — CONFIG_LV_Z_AUTO_INIT=y 自动完成
> 2. **栈和优先级 `#define` 放在 K_THREAD_DEFINE 紧前面** — 参照 led_thread.c 模板
> 3. **所有 LVGL API 调用在同一线程上下文** — 单线程驱动，无需加锁
> 4. **`lv_timer_handler()` 在主循环中调用** — LVGL v9 API (不是 v8 的 `lv_task_handler()`)

## NOR Flash W25Q128 多分区优化

### 概述

W25Q128 (16MB) 除了存放 MCUboot slot1 (448KB) 外，剩余空间可规划为系统区、用户数据区、文件系统区。

### 完整分区方案 (在应用 overlay 中定义)

```
外部 NOR Flash W25Q128 (16MB):
┌──────────────────────┐ 0x00000000
│ slot1_partition      │ 448KB   — MCUboot 升级暂存槽 (image-1)
├──────────────────────┤ 0x00070000
│ rtos_sys_partition   │ 1MB     — RTOS 系统/备份区
├──────────────────────┤ 0x00170000
│ user_raw_partition   │ 2MB     — 用户裸数据区
├──────────────────────┤ 0x00370000
│ storage_partition    │ 12.6MB  — LittleFS 文件系统
└──────────────────────┘ 0x01000000
```

```dts
&w25q128jv {
    partitions {
        compatible = "fixed-partitions";
        #address-cells = <1>;
        #size-cells = <1>;

        slot1_partition: partition@0 {
            label = "image-1";
            reg = <0x00000000 0x70000>;
        };
        rtos_sys_partition: partition@70000 {
            label = "rtos-system";
            reg = <0x00070000 0x100000>;
        };
        user_raw_partition: partition@170000 {
            label = "user-raw";
            reg = <0x00170000 0x200000>;
        };
        storage_partition: partition@370000 {
            label = "storage";
            reg = <0x00370000 0xC90000>;
        };
    };
};
```

> ★ **注意**: MCUboot 的 overlay (`sysbuild/mcuboot.overlay`) 只需 slot1_partition。
> 其余分区仅在应用层 overlay 中定义。两处 overlay 的 slot1_partition 必须完全一致 (偏移+大小)。

## 外设快速索引

| 外设 | 参考文件 |
|------|----------|
| GPIO LED/按键 | references/peripheral-examples.md |
| UART 串口 | references/peripheral-examples.md |
| I2C 传感器 | references/peripheral-examples.md |
| SPI LCD/TF卡 | references/devicetree-guide.md |
| PWM 电机/背光 | references/peripheral-examples.md |
| 设备树配置 | references/devicetree-guide.md |
| 项目模板 | references/project-template.md |
| 构建烧录 | references/build-and-flash.md |
| 硬件详情 | references/Pandora_STM32L4_Hardware_Info.md |
| **MCUboot 教程** | **stm32l475的mcuboot教程.md** (在项目根目录) |
| **Shell 文档** | zephyr/docs/services/shell/index.rst |
| **mcumgr 文档** | zephyr/subsys/mgmt/mcumgr/ |
| MCUboot Zephyr 移植 | bootloader/mcuboot/docs/readme-zephyr.md |
| Sysbuild 文档 | zephyr/share/sysbuild/sysbuild.rst |

## 交互指令

- 新建/创建/模板 → 生成完整文件结构（main.c仅打印 + 独立线程文件 + VERSION含TWEAK + README.md）
- LED/GPIO/按键 → 必须创建独立线程文件（如 led_thread.c），main.c 仅打印信息
- 串口/UART → UART 示例
- I2C/传感器 → I2C 示例（注意 SDA 冲突）
- SPI/LCD → SPI 示例 + LVGL 显示配置 (ST7789V, MIPI-DBI-SPI, SRAM1 内存优化)
- LVGL/显示/LCD → 参照上方 "LVGL + ST7789V LCD 显示配置" 章节
- 传感器/温湿度/IMU/环境光 → 传感器线程 + 共享数据结构 (mutex 保护)
- PWM/呼吸灯/电机 → PWM 示例
- MCUboot/引导程序 → 生成 sysbuild 三件套 (sysbuild.conf + mcuboot.conf + mcuboot.overlay)
- 串口升级/串口恢复 → 启用 CONFIG_MCUBOOT_SERIAL + USART1（应用层 mcumgr 触发，无需按键）
- Shell/命令行 → prj.conf 添加 CONFIG_SHELL=y + 串口后端
- mcumgr/设备管理 → prj.conf 添加 CONFIG_MCUMGR=y + CONFIG_MCUMGR_TRANSPORT_SHELL=y
- 编译/构建/烧录 → 先激活环境，有 MCUboot 时加 `--sysbuild` 标志
- 报错/问题 → 加载排查清单

## 强制规则

0. **★ 绝对禁止修改 zephyr/、modules/、tools/ 等 Zephyr 官方目录**
   - 所有功能在项目目录内实现（自定义驱动、DTS 绑定、应用代码）
   - 需要不同行为 → 写自定义驱动，不修改 Zephyr 内置驱动
   - 确保可复现、可迁移、不随 Zephyr 升级而损坏
1. 编译必须用 west
2. 代码绝对完整
3. 逐行中文注释
4. 设备树优先
5. 应用级配置优先
6. Kconfig 必须 source "Kconfig.zephyr"
7. 无限等待用 k_sleep(K_FOREVER)
8. 项目必须包含 VERSION 文件（含 VERSION_TWEAK 字段，GCC 14+ 强制要求）
9. I2C 冲突提醒
10. 编译前检查环境激活
11. main() 必须返回 int（GCC 14+ 要求，不允许 void main）
12. 编译命令必须 `&&` 串联成单行（PowerShell 进程间不保留环境变量）
13. ★ main.c 禁止写业务逻辑 — 只打印板信息 + k_sleep(K_FOREVER) 永久休眠
14. ★ 所有外设操作（LED/按键/传感器/串口等）必须拆分到独立线程文件（如 led_thread.c）
15. ★ 一类功能 = 一个线程文件 = 一个 K_THREAD_DEFINE
16. ★ 项目必须包含 README.md（板型号 + 项目目标 + 编译/接线/运行说明）
17. ★ MCUboot 编译必须加 `--sysbuild` — 否则只编译应用，不含引导程序
18. ★ MCUboot overlay 中 `mcuboot-button0` 必须引用板级已有 GPIO 节点 — 自定义节点不被识别
19. ★ 应用和 MCUboot 两处 overlay 都必须包含 Flash 分区定义 — 两个镜像独立编译
20. ★ `CONFIG_BOOT_MAX_IMG_SECTORS_AUTO=y` 替代手动 `BOOT_MAX_IMG_SECTORS` — 避免 Kconfig 依赖冲突
21. ★ `SB_CONFIG_BOOT_SIGNATURE_KEY_FILE` 必须在 `sysbuild.conf` 中设置 — `mcuboot.conf` 中的值会被 sysbuild 生成的 `.config.sysbuild` 覆盖
22. ★ mcumgr 管理组使用 `CONFIG_MCUMGR_GRP_XXX`（如 `MCUMGR_GRP_IMG`），**不是** `MCUMGR_CMD_XXX`
23. ★ mcumgr 依赖 `CONFIG_NET_BUF=y` + `CONFIG_ZCBOR=y` + `CONFIG_BASE64=y` + `CONFIG_CRC=y`
24. ★ `CONFIG_MCUMGR_TRANSPORT_SHELL=y` 使 mcumgr 与 Shell 共享 UART，无冲突
25. ★ NOR Flash 方案绝对不开 `CONFIG_STM32_MEMMAP` — STM32L4 QUADSPI 单控制逻辑，indirect/MEMMAP 互斥
26. ★ slot1 在外部 NOR Flash 时，应用和 MCUboot 两处 overlay 中 slot1 都定义在 `&w25q128jv` 节点上
27. ★ Overwrite-Only 升级流程：`image upload` → `image test <hash>` → `reset` → `image confirm`（4 步，缺一不可）
28. ★ LVGL 不手动调用 `lvgl_init()` — `CONFIG_LV_Z_AUTO_INIT=y` 自动初始化，线程中直接 `display_blanking_off()` + 构建 UI
29. ★ display_thread 栈和优先级 `#define` 必须放在 `K_THREAD_DEFINE` 紧前面 — 参照 led_thread.c 模板格式
30. ★ LVGL 内存池和 VDB 缓冲区放到 SRAM1 — 使用 `CONFIG_LV_Z_*_ZEPHYR_REGION=y` + `_REGION_NAME="SRAM1"`
31. ★ ST7789V RGB565 必须开启 `CONFIG_LV_COLOR_16_SWAP=y` — 否则颜色完全错误
32. ★ 线程入口函数签名必须是 `void xxx_thread_entry(void *p1, void *p2, void *p3)` — K_THREAD_DEFINE 要求
33. ★ LVGL v9 主循环用 `lv_timer_handler()` — 不是 v8 的 `lv_task_handler()`
