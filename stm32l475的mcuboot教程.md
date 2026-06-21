# STM32L475 Pandora 板 MCUboot + USART2 串口升级教程

> 适用硬件: ALIENTEK Pandora STM32L475 IoT Board (STM32L475VET6)
> 适用 SDK: Zephyr RTOS v4.4.99 + Zephyr SDK 1.0.1
> 编译日期: 2026-06-21

---

## 目录

1. [背景与目标](#1-背景与目标)
2. [核心概念](#2-核心概念)
3. [硬件环境](#3-硬件环境)
4. [Flash 分区设计](#4-flash-分区设计)
5. [项目文件结构](#5-项目文件结构)
6. [Step-by-Step 实现](#6-step-by-step-实现)
7. [编译](#7-编译)
8. [烧录与验证](#8-烧录与验证)
9. [使用 mcumgr 串口升级固件](#9-使用-mcumgr-串口升级固件)
10. [故障排查](#10-故障排查)
11. [进阶扩展](#11-进阶扩展)

---

## 1. 背景与目标

### 问题

裸机/RTOS 应用通过 ST-Link 调试器烧录固件，但在产品部署后无法方便地进行现场固件升级。

### 解决方案

引入 **MCUboot** 作为二级引导程序 (Second-stage Bootloader)，配合 **USART2 串口恢复模式**，
使设备可以通过串口接收新固件并自动完成升级，无需调试器。

### 最终效果

```
┌──────────────────────────────────────────┐
│  上电 / 复位                              │
│     ↓                                    │
│  MCUboot 启动                            │
│     ↓                                    │
│  检测 KEY0 (PD10) 是否按下?              │
│    ├─ 按下 → 进入串口恢复模式              │
│    │        等待 mcumgr 上传新固件         │
│    │        写入 slot1 → 拷贝到 slot0      │
│    │        跳转执行新固件                  │
│    └─ 未按 → 校验 slot0 签名              │
│              ├─ 通过 → 启动应用            │
│              └─ 失败 → 进入串口恢复模式     │
└──────────────────────────────────────────┘
```

---

## 2. 核心概念

### 2.1 MCUboot

MCUboot 是一个开源的跨平台安全引导程序，支持:
- 镜像签名验证 (RSA / ECDSA / Ed25519)
- 安全固件升级 (Overwrite-Only / Swap / Direct-XIP 等多种模式)
- 串口恢复 (mcumgr SMP 协议)
- Flash 分区管理

本项目采用 **Overwrite-Only (Upgrade-Only)** 模式:
- 新固件先写入 slot1 (暂存槽)
- MCUboot 将 slot1 内容复制到 slot0 (主镜像槽)
- 跳转执行 slot0 中的新固件

### 2.2 Sysbuild (多镜像构建系统)

Zephyr 的 sysbuild 系统允许在一个构建过程中编译多个镜像:
- **MCUboot** (bootloader 镜像)
- **Application** (应用镜像)

最终通过 `west flash` 一次性烧录全部镜像。

### 2.3 串口恢复 (Serial Recovery)

基于 **mcumgr** SMP (Simple Management Protocol) 协议:
- 物理层: UART (USART2)
- 帧协议: 二进制 SMP 帧
- 功能: 镜像上传、设备管理、状态查询

### 2.4 签名密钥

MCUboot 要求所有可启动镜像必须经过数字签名:
- 算法: RSA-2048 (本项目选择)
- 密钥文件: `root-rsa-2048.pem`
- 签名工具: `imgtool.py` (MCUboot 自带)

---

## 3. 硬件环境

### 3.1 Pandora STM32L475 IoT Board 关键参数

| 参数 | 值 |
|------|-----|
| MCU | STM32L475VET6 (Cortex-M4, 80MHz) |
| Flash | 512 KB (双 Bank, RWW) |
| SRAM | 128 KB (96KB SRAM0 + 32KB SRAM1) |
| 调试器 | ST-Link V2.1 (STM32F103C8T6) |

### 3.2 使用的引脚

| 功能 | GPIO | AF | 说明 |
|------|------|-----|------|
| USART2 TX | PA2 | AF7 | 串口升级数据发送 |
| USART2 RX | PA3 | AF7 | 串口升级数据接收 |
| KEY0 (入口按键) | PD10 | GPIO IN | 按住上电进入串口恢复 |
| USART1 TX (控制台) | PA9 | AF7 | 应用日志输出 (ST-Link VCP) |
| USART1 RX (控制台) | PA10 | AF7 | 应用日志输出 (ST-Link VCP) |
| LED_G (状态指示) | PE8 | GPIO OUT | 应用运行指示 |

---

## 4. Flash 分区设计

### 4.1 设计约束

- 总 Flash: 512 KB (0x08000000 ~ 0x08080000)
- MCUboot 固件大小估算: ~48 KB (含串口恢复 + RSA 签名验证 + mbedTLS)
- 应用固件大小估算: ~30 KB (当前) / 预留 224 KB (未来扩展)
- 需要三个分区: boot + slot0 + slot1

### 4.2 最终分区布局

```
┌──────────────────────────────┐ 0x08000000
│  boot_partition (64 KB)      │  MCUboot 引导程序
│  label: "mcuboot"            │  实际占用 ~44 KB
│  read-only                   │
├──────────────────────────────┤ 0x08010000
│  slot0_partition (224 KB)    │  应用程序主镜像
│  label: "image-0"            │  MCUboot 从此处启动应用
├──────────────────────────────┤ 0x08048000
│  slot1_partition (224 KB)    │  升级镜像暂存槽
│  label: "image-1"            │  串口升级时固件先写入此处
└──────────────────────────────┘ 0x08080000
```

### 4.3 分区 DTS 定义

```dts
&flash0 {
    partitions {
        compatible = "fixed-partitions";
        #address-cells = <1>;
        #size-cells = <1>;

        boot_partition: partition@0 {
            label = "mcuboot";
            reg = <0x00000000 0x10000>;    /* 64 KB */
            read-only;
        };

        slot0_partition: partition@10000 {
            label = "image-0";
            reg = <0x00010000 0x38000>;    /* 224 KB */
        };

        slot1_partition: partition@48000 {
            label = "image-1";
            reg = <0x00048000 0x38000>;    /* 224 KB */
        };
    };
};
```

> **注意**: `boot_partition`、`slot0_partition`、`slot1_partition` 三个标签名称
> 是 MCUboot 的硬性要求，不能修改。

---

## 5. 项目文件结构

```
stm32_ai/
├── CMakeLists.txt              # 应用 CMake 构建文件
├── prj.conf                    # 应用 Kconfig 配置
├── Kconfig                     # 应用级 Kconfig
├── VERSION                     # Zephyr 生命周期管理
├── README.md                   # 项目说明
├── root-rsa-2048.pem           # ★ 项目签名密钥 (生产环境需保密)
├── sysbuild.conf               # ★ Sysbuild 配置 (启用 MCUboot)
├── sysbuild/
│   ├── mcuboot.conf            # ★ MCUboot Kconfig (串口恢复 + 签名配置)
│   └── mcuboot.overlay         # ★ MCUboot 设备树覆盖 (USART2 + 分区 + GPIO)
├── boards/
│   └── pandora_stm32l475.overlay  # 应用设备树覆盖 (LED 别名 + 分区定义)
├── include/
│   └── led_thread.h            # LED 线程头文件
└── src/
    ├── main.c                  # 应用入口 (仅打印信息 + 永久休眠)
    └── led_thread.c            # LED 闪烁线程
```

> ★ 标记为本次 MCUboot 新增/关键修改文件。

---

## 6. Step-by-Step 实现

### Step 1: 生成签名密钥

```powershell
cd E:\zephyrproject
python bootloader/mcuboot/scripts/imgtool.py keygen -t rsa-2048 -k stm32_ai/root-rsa-2048.pem
```

**依赖**: 需要安装 `intelhex`、`cbor2`、`pyyaml`、`cryptography`:
```powershell
pip install intelhex cbor2 pyyaml cryptography
```

**密钥说明**:
- 默认 MCUboot 密钥路径解析逻辑:
  1. 如果路径是绝对路径 → 直接使用
  2. 如果文件存在于 `APPLICATION_CONFIG_DIR/` → 使用
  3. 否则 → 使用 `${MCUBOOT_DIR}/` 前缀 (即 `bootloader/mcuboot/`)
- 本项目使用默认开发密钥 `bootloader/mcuboot/root-rsa-2048.pem`
- 生产环境应使用专用密钥，并在 `mcuboot.conf` 中指定:
  ```
  CONFIG_BOOT_SIGNATURE_KEY_FILE="path/to/production-key.pem"
  ```

### Step 2: 创建 `sysbuild.conf`

**文件**: `stm32_ai/sysbuild.conf`

```ini
# 启用 MCUboot 作为二级引导程序
SB_CONFIG_BOOTLOADER_MCUBOOT=y
# 采用 Overwrite-Only 升级模式
SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY=y
```

**Sysbuild 模式说明**:

| 模式 | sysbuild 选项 | MCUboot Kconfig | 需要分区 |
|------|-------------|-----------------|---------|
| Single App | `SB_CONFIG_MCUBOOT_MODE_SINGLE_APP` | `CONFIG_SINGLE_APPLICATION_SLOT` | boot + slot0 |
| Overwrite-Only | `SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY` | `CONFIG_BOOT_UPGRADE_ONLY` | boot + slot0 + slot1 |
| Swap Scratch | `SB_CONFIG_MCUBOOT_MODE_SWAP_SCRATCH` | `CONFIG_BOOT_SWAP_USING_SCRATCH` | boot + slot0 + slot1 + scratch |
| Swap Move | `SB_CONFIG_MCUBOOT_MODE_SWAP_USING_MOVE` | `CONFIG_BOOT_SWAP_USING_MOVE` | boot + slot0 + slot1 |

**为什么选择 Overwrite-Only?**
- 512 KB Flash 不足以支撑双槽 Swap + Scratch (需要至少 4 个分区)
- Overwrite-Only 仅需 3 个分区，升级时直接覆写主槽
- 没有回滚能力，但 Flash 利用率最高

### Step 3: 创建 `sysbuild/mcuboot.conf`

**文件**: `stm32_ai/sysbuild/mcuboot.conf`

```ini
# === 串口恢复模式 ===
CONFIG_MCUBOOT_SERIAL=y            # 启用串口恢复功能
CONFIG_BOOT_SERIAL_UART=y          # 使用 UART (而非 CDC ACM)
CONFIG_UART_CONSOLE=n              # 禁用控制台 (USART2 专用于 mcumgr)

# === GPIO 按键检测进入串口恢复 ===
CONFIG_BOOT_SERIAL_ENTRANCE_GPIO=y # GPIO 按键触发
CONFIG_BOOT_SERIAL_DETECT_DELAY=50 # 按键保持检测延时 (ms)

# === 无有效应用程序时自动进入串口恢复 ===
CONFIG_BOOT_SERIAL_NO_APPLICATION=y

# === 日志级别 (警告级别，节省 Flash 空间) ===
CONFIG_MCUBOOT_LOG_LEVEL_WRN=y

# === 签名算法 RSA-2048 ===
CONFIG_BOOT_SIGNATURE_TYPE_RSA=y
CONFIG_BOOT_SIGNATURE_TYPE_RSA_LEN=2048

# === 自动计算最大镜像扇区数 ===
CONFIG_BOOT_MAX_IMG_SECTORS_AUTO=y
```

> **关键参数说明**:
> - `CONFIG_UART_CONSOLE=n` 必须设置，否则 MCUboot 的 USART2 会与控制台冲突
> - `CONFIG_BOOT_MAX_IMG_SECTORS_AUTO=y` 优于手动设置 `BOOT_MAX_IMG_SECTORS`，
>   它会根据 Flash 扇区大小和分区尺寸自动计算最优值 (本例自动算出 112)

### Step 4: 创建 `sysbuild/mcuboot.overlay`

**文件**: `stm32_ai/sysbuild/mcuboot.overlay`

这是最关键的配置文件，同时定义以下内容:

#### 4.1 USART2 串口恢复通道

```dts
/ {
    chosen {
        /* 指定 USART2 为 mcumgr 协议通道 */
        zephyr,uart-mcumgr = &usart2;
    };
};

/* 启用 USART2: PA2(TX) + PA3(RX), 115200 */
&usart2 {
    pinctrl-0 = <&usart2_tx_pa2 &usart2_rx_pa3>;
    pinctrl-names = "default";
    current-speed = <115200>;
    status = "okay";
};
```

> **关键设计决策**: `zephyr,uart-mcumgr` 和 `zephyr,console` 必须指向不同的设备。
> 控制台使用 USART1 (ST-Link VCP)，mcumgr 使用 USART2 (扩展串口)，互不冲突。

#### 4.2 GPIO 入口按键

```dts
/ {
    aliases {
        /* 引用板级 DTS 已有节点 joy_right (PD10 = KEY0) */
        mcuboot-button0 = &joy_right;
    };
};
```

> **踩坑记录**: 最初尝试定义自定义 GPIO 节点，但 `DT_NODE_HAS_PROP` 检测不到
> 自定义节点的 `gpios` 属性。改为引用板级 DTS 中已有的 `joy_right` 节点 (同属
> `gpio_keys` 组，已正确定义 `gpios` 属性)，问题解决。

#### 4.3 Flash 分区定义

已在 [4.3 节](#43-分区-dts-定义) 列出。

### Step 5: 更新应用层文件

#### 5.1 `prj.conf` — 添加 MCUboot 兼容性说明

```ini
# prj.conf - STM32 AI 应用 Kconfig 配置
#
# MCUboot 兼容性说明:
#   - MCUboot 占用 USART2 (PA2/PA3) 用于串口恢复
#   - 应用控制台仍使用 USART1 (PA9/PA10) — 由板级 DTS 定义
#   - 应用从 Flash 偏移 0x08010000 处链式加载（sysbuild 自动配置）

CONFIG_GPIO=y
CONFIG_LOG=y
```

> 应用配置无需特殊修改 — sysbuild 自动处理 Flash 偏移、签名等事项。

#### 5.2 `boards/pandora_stm32l475.overlay` — 添加分区定义和代码分区

应用也需要看到 Flash 分区信息 (用于 `mcuboot.cmake` 签名步骤)。
将 [4.3 节](#43-分区-dts-定义) 的分区定义复制到应用 overlay 中。

**★ 关键!** 应用 overlay 还必须设置 `zephyr,code-partition`，告诉链接器
将应用代码放在 slot0 (0x08010000) 而非 Flash 起始地址 (0x08000000):

```dts
/ {
    chosen {
        /* ★ 缺失此项会导致应用链接到 0x08000000, 烧录时覆盖 MCUboot → HardFault */
        zephyr,code-partition = &slot0_partition;
    };
};
```

> **为什么两份 overlay 都需要分区定义?**
> - `sysbuild/mcuboot.overlay` → MCUboot 镜像编译时使用
> - `boards/pandora_stm32l475.overlay` → 应用镜像编译 + 签名时使用
> - 两个镜像独立编译，无法共享 DTS 节点
>
> **`zephyr,code-partition` 的作用:**
> - 告诉 Zephyr 链接器把应用 ROM 段放在哪个分区
> - 设置 `CONFIG_FLASH_LOAD_OFFSET` → 本例应等于 0x10000
> - 缺失时 `FLASH_LOAD_OFFSET=0x0` → 应用写入 0x08000000 → 覆盖 MCUboot → HardFault

---

## 7. 编译

### 7.1 完整清理编译 (首次)

```powershell
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -p always -b pandora_stm32l475 --sysbuild .\stm32_ai
```

**编译参数说明**:

| 参数 | 说明 |
|------|------|
| `-p always` | 完全清理后构建 (pristine) |
| `-b pandora_stm32l475` | 目标开发板 |
| `--sysbuild` | 启用多镜像构建 (关键!) |
| `.\stm32_ai` | 应用源代码路径 |

### 7.2 增量编译 (仅修改代码时)

```powershell
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -b pandora_stm32l475 --sysbuild .\stm32_ai
```

### 7.3 编译产物

```
build/
├── mcuboot/zephyr/
│   ├── zephyr.elf          # MCUboot ELF (含调试信息, ~1.8MB)
│   ├── zephyr.bin          # MCUboot 原始二进制 (~44KB)
│   └── zephyr.hex          # MCUboot Intel HEX (~127KB)
├── stm32_ai/zephyr/
│   ├── zephyr.elf          # 应用 ELF (~1MB)
│   ├── zephyr.signed.bin   # ★ 已签名应用二进制 (~28KB)
│   └── zephyr.signed.hex   # ★ 已签名应用 HEX (~81KB)
└── _sysbuild/              # Sysbuild 内部文件
```

### 7.4 预期内存占用

```
MCUboot:
  FLASH:  45,120 B  / 512 KB  ( 8.61%)
  RAM:    35,968 B  /  96 KB  (36.59%)

Application:
  FLASH: ~28,000 B  / 224 KB  (12.50%)  (signed)
  RAM:    ~5,000 B  /  96 KB  ( 5.00%)
```

---

## 8. 烧录与验证

### 8.1 烧录

```powershell
# 连接 ST-Link USB 口，确保设备管理器可见 ST-Link Debug
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west flash
```

`west flash` 自动烧录 MCUboot + 应用两个镜像到正确地址。

### 8.2 正常启动验证

1. 使用串口工具连接 USART1 (ST-Link VCP, 115200):
   ```powershell
   python -m serial.tools.miniterm COM5 115200
   ```
2. 按 RESET 键或重新上电
3. 预期输出:
   ```
   *** Booting Zephyr OS build v4.4.0-xxx ***
   [00:00:00.000] <inf> main: 应用启动完成, Board: pandora_stm32l475
   [00:00:00.100] <inf> LED_TASK: LED Thread started
   ```
4. 绿色 LED 以 1Hz 频率闪烁

### 8.3 串口恢复模式验证

1. 断开 USART1 连接
2. 使用另一个串口工具连接 USART2 (PA2/PA3, 115200)
3. **按住 KEY0 不放** → 按 RESET → 等待 1 秒 → 松开 KEY0
4. MCUboot 应输出 mcumgr 就绪提示
5. 发送 mcumgr 命令验证:
   ```powershell
   mcumgr --conntype serial --connstring "COM6,baud=115200" image list
   ```

---

## 9. 使用 mcumgr 串口升级固件

### 9.1 安装 mcumgr

```powershell
pip install mcumgr
```

### 9.2 升级流程

#### ① 进入串口恢复模式

按住 KEY0 → 复位 → 等待 LED 指示进入恢复模式 → 松开 KEY0。

#### ② 查看当前镜像信息

```powershell
mcumgr --conntype serial --connstring "COM6,baud=115200" image list
```

#### ③ 上传新固件

```powershell
mcumgr --conntype serial --connstring "COM6,baud=115200" image upload build/stm32_ai/zephyr/zephyr.signed.bin
```

> 注意: 必须上传 `.signed.bin` (已签名版本)，而非未签名的 `.bin`。

#### ④ 标记为待测试 (可选)

```powershell
mcumgr --conntype serial --connstring "COM6,baud=115200" image test <hash>
```

#### ⑤ 确认新固件 (确认后无法回滚)

```powershell
mcumgr --conntype serial --connstring "COM6,baud=115200" image confirm <hash>
```

#### ⑥ 复位启动新固件

```powershell
mcumgr --conntype serial --connstring "COM6,baud=115200" reset
```

---

## 10. 故障排查

### 10.1 编译错误: `required nodelabel not found: slot0_partition`

**原因**: MCUboot 或应用缺少 Flash 分区定义。

**解决**: 确保两份 overlay 文件都包含 `boot_partition`、`slot0_partition`、`slot1_partition` 的完整分区定义。

### 10.2 编译错误: `required nodelabel not found: slot1_partition`

**原因**: `CONFIG_BOOT_UPGRADE_ONLY` 需要 slot1 分区 (即使你不需要存两个镜像，
升级时固件需要先存到 slot1 再拷贝到 slot0)。

**解决**: 添加 `slot1_partition` 到 overlay。

### 10.3 编译警告: `BOOT_MAX_IMG_SECTORS` 值被覆盖

**原因**: `BOOT_MAX_IMG_SECTORS` 依赖 `!BOOT_MAX_IMG_SECTORS_AUTO`，
不能同时设置。

**解决**: 使用 `CONFIG_BOOT_MAX_IMG_SECTORS_AUTO=y` 替代手动指定。

### 10.4 编译错误: `#error "Serial recovery/USB DFU button must be declared in device tree as 'mcuboot_button0'"`

**原因**: `mcuboot-button0` 别名指向的节点不被 `DT_NODE_HAS_PROP(node, gpios)` 识别。

**解决**: 引用板级 DTS 中已有的 GPIO 节点 (如 `joy_right`)，而非自定义节点。

### 10.5 烧录后应用不启动

**检查清单**:
1. MCUboot 是否烧录到 `0x08000000`
2. 应用是否烧录到 `0x08010000` (slot0 起始地址)
3. 应用是否经过签名 (`.signed.bin`)
4. 签名密钥与 MCUboot 编译时使用的密钥是否一致
5. 分区布局是否一致 (MCUboot overlay 和 App overlay)

### 10.6 烧录后 HardFault (应用覆盖了 MCUboot)

**现象**:
```
Error: [stm32l4x.cpu] clearing lockup after double fault
[stm32l4x.cpu] halted due to debug-request, current mode: Handler HardFault
```

**验证方法**:
```bash
grep FLASH_LOAD_OFFSET build/stm32_ai/zephyr/.config
# 错误: CONFIG_FLASH_LOAD_OFFSET=0x0   → 应用链接到 0x08000000
# 正确: CONFIG_FLASH_LOAD_OFFSET=0x10000 → 应用链接到 0x08010000
```

**原因**: 应用 layer overlay 缺少 `zephyr,code-partition` chosen 节点。

**解决**: 在 `boards/pandora_stm32l475.overlay` 中添加:
```dts
/ {
    chosen {
        zephyr,code-partition = &slot0_partition;
    };
};
```

然后重新编译烧录。

### 10.7 串口恢复无响应

**检查清单**:
1. USART2 引脚是否正确连接 (PA2=TX, PA3=RX)
2. 波特率是否匹配 (115200)
3. `zephyr,uart-mcumgr` chosen 节点是否指向 `&usart2`
4. MCUboot 是否正确启用了 `CONFIG_MCUBOOT_SERIAL`
5. GPIO 按键电平是否匹配 (KEY0 按下为低电平)

---

## 11. 进阶扩展

### 11.1 使用 QSPI Flash 存储升级镜像

Pandora 板有 16MB 外部 QSPI Flash (W25Q128)。可以将 slot1 放置到外部 Flash，
释放内部 Flash 空间:

```dts
&w25q128jv {
    partitions {
        /* 外部 Flash 上的 slot1 分区 */
        ext_slot1: partition@0 {
            reg = <0x0 0x38000>;
        };
    };
};
```

### 11.2 启用镜像加密

在 `mcuboot.conf` 中添加:
```ini
CONFIG_BOOT_ENCRYPT_IMAGE=y
CONFIG_BOOT_ENCRYPT_ALG_AES_256=y
```

生成加密密钥:
```powershell
python imgtool.py keygen -t ecdsa-p256 -k root-ec-p256.pem
```

### 11.3 使用 LED 指示 MCUboot 状态

在 overlay 中添加:
```dts
aliases {
    mcuboot-led0 = &blue_led;  /* 串口恢复模式指示灯 */
};
```

并在 `mcuboot.conf` 中:
```ini
CONFIG_MCUBOOT_INDICATION_LED=y
```

### 11.4 启用看门狗

参考 `disco_l475_iot1` 板配置:
```ini
CONFIG_WATCHDOG=y
```

---

## 参考资料

- [MCUboot 官方文档](https://docs.mcuboot.com/)
- [Zephyr Sysbuild 文档](https://docs.zephyrproject.org/latest/build/sysbuild/)
- [MCUboot Serial Recovery 设计文档](https://docs.mcuboot.com/design.html#serial-recovery)
- [mcumgr 命令行工具](https://github.com/apache/mynewt-mcumgr-cli)
- `bootloader/mcuboot/docs/readme-zephyr.md` — MCUboot Zephyr 移植文档
- `zephyr/share/sysbuild/sysbuild.rst` — Sysbuild 官方参考
