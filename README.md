# STM32 AI — Pandora STM32L475 MCUboot + Shell + mcumgr 串口升级

## 项目信息

| 项目 | 详情 |
|------|------|
| **开发板** | ALIENTEK Pandora STM32L475 IoT Board |
| **MCU** | STM32L475VET6 (Cortex-M4, 80MHz) |
| **Flash/SRAM** | 512KB / 128KB |
| **RTOS** | Zephyr RTOS v4.4.99 |
| **引导程序** | MCUboot (Overwrite-Only 模式) |
| **签名算法** | RSA-2048（项目专用密钥 `root-rsa-2048.pem`） |
| **升级通道** | USART1 (PA9/PA10, ST-Link VCP) — Shell/日志/串口升级三合一 |
| **Shell** | Zephyr Shell 交互式命令行（USART1） |
| **mcumgr** | SMP over Shell 传输层（应用内，与 Shell 共存无冲突） |
| **项目目标** | LED 闪烁 + Shell 命令行 + MCUboot 安全引导 + 串口升级 |

## 硬件说明

### RGB LED

| 颜色 | 引脚 | 设备树节点 |
|------|------|-----------|
| 红 (R) | PE7 | red_led |
| **绿 (G)** | **PE8** | **green_led** |
| 蓝 (B) | PE9 | blue_led |

> **注意**：RGB LED 为共阳极结构，GPIO 输出高电平时 LED 亮，低电平时 LED 灭。

### 按键

| 按键 | 引脚 | 说明 |
|------|------|------|
| **KEY0** | **PD10** | **MCUboot 串口恢复入口（按住上电进入升级模式）** |
| KEY1 | PD9 | 用户自定义 |
| KEY2 | PD8 | 用户自定义 |
| WK_UP | PC13 | 唤醒按键 |

### 串口分配（三合一架构）

| 串口 | 引脚 | 用途 |
|------|------|------|
| **USART1** | **PA9(TX) / PA10(RX)** | **Shell 命令行 + 日志输出 + MCUboot 串口恢复（分时复用）** |
| USART2 | PA2(TX) / PA3(RX) | 空闲（可分配给其他外设） |

> **分时复用原理**：上电时 MCUboot 先运行，若检测到 KEY0 按下则接管 USART1 进入串口恢复模式；否则启动应用，应用在 USART1 上运行 Shell 和日志。

### 调试接口

- **ST-Link V2.1**（STM32F103C8T6 芯片）
- USB 连接 **ST-Link 口**（非 USB OTG 口）

---

## MCUboot 引导架构

```
┌──────────────────────────────────────────┐
│  上电 / 复位                              │
│     ↓                                    │
│  MCUboot 启动 (0x08000000)               │
│     ↓                                    │
│  检测 KEY0 (PD10) 是否按下?              │
│    ├─ 按下 → 进入串口恢复模式              │
│    │         USART1 等待 mcumgr 上传固件   │
│    │         写入 slot1 → 拷贝到 slot0      │
│    │         跳转执行新固件                  │
│    └─ 未按 → 校验 slot0 RSA-2048 签名      │
│              ├─ 通过 → 启动应用 (0x08010000)│
│              │         USART1 运行 Shell    │
│              └─ 失败 → 进入串口恢复模式     │
└──────────────────────────────────────────┘
```

### Flash 分区布局

```
┌──────────────────────┐ 0x08000000
│  boot_partition      │  64KB — MCUboot 引导程序 (~44KB used)
├──────────────────────┤ 0x08010000
│  slot0_partition     │ 224KB — 应用主镜像 (已签名)
├──────────────────────┤ 0x08048000
│  slot1_partition     │ 224KB — 升级暂存槽
└──────────────────────┘ 0x08080000
```

---

## 项目结构

```
stm32_ai/
├── CMakeLists.txt                       # CMake 构建文件
├── prj.conf                             # ★ 应用 Kconfig (Shell + mcumgr + LED)
├── Kconfig                              # 应用级 Kconfig
├── VERSION                              # 版本文件
├── README.md                            # 本文件
├── root-rsa-2048.pem                    # ★ 项目专用 RSA-2048 签名密钥
├── stm32l475的mcuboot教程.md            # ★ MCUboot 完整教程
├── sysbuild.conf                        # ★ Sysbuild 入口 (密钥路径 + MCUboot 模式)
├── sysbuild/
│   ├── mcuboot.conf                     # ★ MCUboot Kconfig (串口恢复配置)
│   └── mcuboot.overlay                  # ★ MCUboot DTS (USART1 + 分区 + GPIO)
├── boards/
│   └── pandora_stm32l475.overlay        # 应用 DTS (LED + 分区 + 代码链接)
└── src/
    ├── main.c                           # 主程序 (仅打印 + 永久休眠)
    └── led_thread.c                     # LED 闪烁线程
```

> ★ 标记为 MCUboot 关键文件。

---

## 编译

### 环境要求

- Windows 10/11 + PowerShell
- Python 3.12+（虚拟环境 `.venv`）
- Zephyr SDK 1.0.1（工具链 arm-zephyr-eabi）
- CMake + Ninja
- `pip install intelhex cbor2 pyyaml cryptography mcumgr`（imgtool.py 和 mcumgr 依赖）

### MCUboot 多镜像编译 (Sysbuild)

```powershell
# 完整清理编译（首次或修改配置后）
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -p always -b pandora_stm32l475 --sysbuild .\stm32_ai

# 增量编译（仅修改代码时）
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -b pandora_stm32l475 --sysbuild .\stm32_ai
```

| 参数 | 说明 |
|------|------|
| `-p always` | 完全清理构建 |
| `-b pandora_stm32l475` | 目标开发板 |
| `--sysbuild` | ★ 启用多镜像构建 (MCUboot + 应用) |
| `.\stm32_ai` | 应用路径 |

### 构建产物

```
build/
├── mcuboot/zephyr/
│   ├── zephyr.elf         # MCUboot ELF
│   ├── zephyr.bin         # MCUboot 二进制 (~45KB)
│   └── zephyr.hex         # MCUboot Intel HEX
├── stm32_ai/zephyr/
│   ├── zephyr.elf         # 应用 ELF
│   ├── zephyr.signed.bin  # ★ 已签名应用二进制 (~68KB)
│   └── zephyr.signed.hex  # ★ 已签名应用 HEX
└── _sysbuild/             # Sysbuild 内部文件
```

### 内存占用参考

```
MCUboot:
  FLASH:  44,948 B  / 512 KB  ( 8.57%)  — fits in 64KB boot_partition
  RAM:    35,904 B  /  96 KB  (36.52%)

Application (含 Shell + mcumgr):
  FLASH:  68,132 B  / 224 KB  (29.75%)  — fits in slot0_partition
  RAM:    18,720 B  /  96 KB  (19.04%)
```

---

## 烧录与运行

### 烧录

```powershell
# west flash 自动烧录 MCUboot + 应用两个镜像到正确地址
west flash
```

### 接线

1. Micro USB 线连接开发板 **ST-Link 接口**
2. PC 设备管理器可见 **ST-Link Debug** 和 **STMicroelectronics STLink Virtual COM Port**

### 正常启动验证（Shell 模式）

1. 连接 USART1 (ST-Link VCP, 115200):
   ```powershell
   python -m serial.tools.miniterm COM5 115200
   ```
2. 按 RESET 或重新上电
3. 预期输出:
   ```
   *** Booting Zephyr OS build v4.4.0-xxx ***
   [00:00:00.000] <inf> main: 应用启动完成, Board: pandora_stm32l475
   [00:00:00.100] <inf> LED_TASK: LED Thread started
   uart:~$
   ```
4. Shell 提示符 `uart:~$` 出现，可输入命令（按 Tab 查看可用命令）
5. LED 以 1Hz 频率闪烁

### Shell 命令示例

```
uart:~$ help              # 查看所有可用命令
uart:~$ device list       # 列出所有设备
uart:~$ kernel version    # 查看内核版本
uart:~$ mcumgr            # 进入 mcumgr SMP 模式（用于设备管理）
```

---

## 串口升级 (USART1 + mcumgr)

### 方式一：MCUboot 串口恢复模式（推荐用于固件升级）

1. 保持 USART1 (ST-Link VCP) 连接到 PC
2. **按住 KEY0 不放** → 按 RESET → 等待 1 秒 → 松开 KEY0
3. MCUboot 检测到按键 → 接管 USART1 → 进入 mcumgr 串口恢复模式

```powershell
# ① 查看当前镜像信息
mcumgr --conntype serial --connstring "COM5,baud=115200" image list

# ② 上传新固件 (必须是 .signed.bin)
mcumgr --conntype serial --connstring "COM5,baud=115200" image upload build/stm32_ai/zephyr/zephyr.signed.bin

# ③ 复位启动新固件
mcumgr --conntype serial --connstring "COM5,baud=115200" reset
```

### 方式二：应用内 mcumgr（Shell 传输层）

应用运行时通过 Shell 使用 mcumgr：

```
uart:~$ mcumgr             # 进入 mcumgr SMP 模式
```

> **注意**：Shell 传输层适合设备管理（查看镜像、复位、统计等）。固件上传推荐使用方式一（MCUboot 恢复模式），速度更快。

---

## 应用内 mcumgr 配置参考

```ini
# prj.conf — Shell + mcumgr SMP over Shell
CONFIG_GPIO=y
CONFIG_LOG=y
CONFIG_SHELL=y

# mcumgr 依赖
CONFIG_NET_BUF=y
CONFIG_ZCBOR=y
CONFIG_BASE64=y
CONFIG_CRC=y

# mcumgr SMP over Shell
CONFIG_MCUMGR=y
CONFIG_MCUMGR_TRANSPORT_SHELL=y
CONFIG_MCUMGR_GRP_IMG=y
CONFIG_MCUMGR_GRP_OS=y
CONFIG_MCUMGR_GRP_SHELL=y
```

> **关键**：`MCUMGR_GRP_XXX` 不是 `MCUMGR_CMD_XXX`；`NET_BUF` 和 `ZCBOR` 是 mcumgr 的必要依赖。

---

## 注意事项

1. **RGB LED 共阳极**：GPIO 输出高电平时 LED 亮
2. **不要接 USB OTG 口**：烧录必须用 ST-Link 口
3. **编译前必须激活虚拟环境**：`cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1`
4. **MCUboot 编译必须加 `--sysbuild`**：否则只编译应用不含引导程序
5. **升级固件必须已签名**：上传 `.signed.bin` 而非未签名的 `.bin`
6. **签名密钥需一致**：MCUboot 编译和镜像签名必须使用同一密钥
7. ★ **密钥在 `sysbuild.conf` 中配置**：`SB_CONFIG_BOOT_SIGNATURE_KEY_FILE`，不能只在 `mcuboot.conf` 中设置
8. **USART1 三合一**：Shell/日志/mcumgr 串口恢复共享 USART1，分时复用无冲突
9. **无固件自动恢复**：若 slot0 无有效固件，MCUboot 自动进入串口恢复模式
10. **Shell 和 mcumgr 共存**：通过 Shell 传输层 (`MCUMGR_TRANSPORT_SHELL`)，无 UART 冲突

## 参考资料

- [stm32l475的mcuboot教程.md](stm32l475的mcuboot教程.md) — 完整 MCUboot 配置教程
- [MCUboot 官方文档](https://docs.mcuboot.com/)
- [Zephyr Sysbuild 文档](https://docs.zephyrproject.org/latest/build/sysbuild/)
- [Zephyr Shell 文档](https://docs.zephyrproject.org/latest/services/shell/index.html)
- [mcumgr 命令行工具](https://github.com/apache/mynewt-mcumgr-cli)
- `bootloader/mcuboot/docs/readme-zephyr.md` — MCUboot Zephyr 移植文档
