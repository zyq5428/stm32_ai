# STM32 AI — Pandora STM32L475 MCUboot + Shell + mcumgr 串口升级（无按键方案）

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
| **升级入口** | ★ 无按键 — mcumgr 上传固件到 slot1 (NOR Flash) → 标记 pending → 重启 → MCUboot 覆写 |
| **slot1 位置** | ★ 外部 NOR Flash W25Q128 (QSPI indirect 模式, 不开 MEMMAP) |
| **项目目标** | LED 闪烁 + Shell 命令行 + MCUboot 安全引导 + 串口升级 + 传感器数据采集 |

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
| KEY0 | PD10 | 用户自定义 |
| KEY1 | PD9 | 用户自定义 |
| KEY2 | PD8 | 用户自定义 |
| WK_UP | PC13 | 唤醒按键 |

> ★ 串口升级不需要按键 — 完全由应用层 mcumgr 通过串口触发。

### 串口分配（三合一架构）

| 串口 | 引脚 | 用途 |
|------|------|------|
| **USART1** | **PA9(TX) / PA10(RX)** | **Shell 命令行 + 日志输出 + MCUboot 串口恢复（分时复用）** |
| USART2 | PA2(TX) / PA3(RX) | 空闲（可分配给其他外设） |

> **分时复用原理**：上电时 MCUboot 先运行，若 slot0 无有效应用则自动进入串口恢复模式接管 USART1；否则启动应用，应用在 USART1 上运行 Shell 和日志。

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
│  检测 slot1 是否有 pending 镜像?          │
│    ├─ 有 → 执行 overwrite                    │
│    │        把 slot1 内容整体拷贝覆盖到 slot0   │
│    │        跳转到 slot0 执行新固件              │
│    └─ 无 → 校验 slot0 RSA-2048 签名           │
│              ├─ 通过 → 启动应用 (0x08010000)   │
│              │         USART1 运行 Shell       │
│              │         应用层 mcumgr 接收升级指令 │
│              └─ 失败 → 进入串口恢复模式          │
│                        USART1 等待 mcumgr 上传  │
└──────────────────────────────────────────┘
```

### ★ Overwrite-Only 升级流程（4 步）

```
① image upload zephyr.signed.bin
   └→ 把新固件写入 slot1（外部 NOR Flash, QSPI indirect 模式）
      不影响 slot0 里正在运行的固件

② image test <hash>
   └→ 把 slot1 标记为 "下次启动尝试运行"（pending 状态）

③ reset
   └→ MCUboot 启动时发现 slot1 是 pending 状态
      └→ 执行 overwrite：把 slot1 内容整体拷贝覆盖到 slot0
         └→ 从 slot0 启动新固件

④ image confirm
   └→ 新固件运行起来后，确认这个版本可用
      在 overwrite-only 模式下，因为没有回滚机制，这一步有时会被 MCUboot 自动处理，
      但手动 confirm 更安全可靠
```

### Flash 分区布局（NOR Flash 方案）

```
内部 Flash (512KB):                    外部 NOR Flash W25Q128 (16MB):
┌──────────────────┐ 0x08000000        ┌──────────────────┐ 0x00000000
│ boot_partition   │ 64KB              │ slot1_partition  │ 448KB
│ (mcuboot)        │                   │ (image-1)        │
├──────────────────┤ 0x08010000        │ ★ QSPI indirect  │
│ slot0_partition  │ 448KB             │   模式读写        │
│ (image-0)        │                   │   不开 MEMMAP     │
└──────────────────┘ 0x08080000        └──────────────────┘
```

> ★ **STM32L4 QSPI 限制**：外部 NOR Flash 通过 QSPI 访问，**只能使用 indirect 模式**。
> STM32L4 QUADSPI 只有一套控制逻辑，indirect 模式和 memory-mapped 模式 (XIP) 不能同时生效。
> **绝对不能开 `CONFIG_STM32_MEMMAP=y`**。

---

## 项目结构

```
stm32_ai/
├── CMakeLists.txt                       # CMake 构建文件
├── prj.conf                             # ★ 应用 Kconfig (Shell + mcumgr + NOR Flash)
├── Kconfig                              # 应用级 Kconfig
├── VERSION                              # 版本文件
├── README.md                            # 本文件
├── root-rsa-2048.pem                    # ★ 项目专用 RSA-2048 签名密钥
├── sysbuild.conf                        # ★ Sysbuild 入口 (密钥路径 + MCUboot 模式)
├── sysbuild/
│   ├── mcuboot.conf                     # ★ MCUboot Kconfig (串口恢复 + QSPI indirect)
│   └── mcuboot.overlay                  # ★ MCUboot DTS (USART1 + 分区, slot1 在 NOR Flash)
├── boards/
│   └── pandora_stm32l475.overlay        # 应用 DTS (LED + 分区 + 代码链接 + 传感器)
├── src/
│   ├── main.c                           # 主程序 (仅打印 + 永久休眠)
│   ├── led_thread.c                     # LED 闪烁线程
│   ├── aht10_thread.c                   # AHT10 温湿度传感器线程
│   ├── ap3216c_thread.c                 # AP3216C 环境光/接近传感器线程
│   ├── icm20608_thread.c                # ICM20608 6 轴传感器线程
│   └── norflash_thread.c               # NOR Flash 自检线程 (QSPI indirect 模式验证)
├── drivers/sensor/                      # 自定义传感器驱动
│   ├── aht10/                           # AHT10 驱动
│   ├── ap3216c/                         # AP3216C 驱动
│   └── icm20608/                        # ICM20608 驱动
└── include/                             # 公共头文件
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
│   ├── zephyr.bin         # MCUboot 二进制 (~48KB)
│   └── zephyr.hex         # MCUboot Intel HEX
├── stm32_ai/zephyr/
│   ├── zephyr.elf         # 应用 ELF
│   ├── zephyr.signed.bin  # ★ 已签名应用二进制 (~100KB)
│   └── zephyr.signed.hex  # ★ 已签名应用 HEX
└── _sysbuild/             # Sysbuild 内部文件
```

### 内存占用参考

```
MCUboot:
  FLASH:  ~48 KB  / 512 KB  — fits in 64KB boot_partition ✓

Application (含 Shell + mcumgr + 传感器 + NOR Flash 测试):
  FLASH:  ~100 KB / 448 KB  — fits in slot0_partition ✓
```

---

## VS Code 一键编译/烧录/调试

项目已配置 `.vscode/` 集成，打开根目录 `zephyrproject/` 即可使用。

### 前置环境

| 组件 | 路径 |
|------|------|
| Python 虚拟环境 | `E:/zephyrproject/.venv/` |
| Zephyr SDK 1.0.1 | `E:/zephyr-sdk-1.0.1/` |
| OpenOCD (Zephyr SDK 自带) | `E:/zephyr-sdk-1.0.1/hosttools/openocd/` |
| VS Code 扩展 | Cortex-Debug (`marus25.cortex-debug`) + C/C++ (`ms-vscode.cpptools`) |

### 6 个快捷任务 (`Ctrl+Shift+P` → "Tasks: Run Task")

| 任务名 | 默认快捷键 | 编译标志 | 用途 |
|--------|-----------|----------|------|
| **West Build (pandora) auto** | `Ctrl+Shift+B` | `-p auto` | 小文件修改 → 增量编译 |
| **West Build (pandora) always** | 手动 | `-p always` | 完整清理编译 |
| **West Build MCUboot (pandora) auto** | 手动 | `-p auto --sysbuild` | MCUboot 增量编译 |
| **West Build MCUboot (pandora) always** | 手动 | `-p always --sysbuild` | MCUboot 完整编译 |
| **West Flash (pandora)** | 手动 | — | 一键烧录 |
| **Launch OpenOCD Server Only** | 手动 | — | 独立调试服务器 |

### 2 个调试配置 (`F5`)

| 配置名 | 功能 |
|--------|------|
| **Launch-OpenOCD(stlink)** | 自动编译 → 烧录 → 启动 GDB → 停在 `main()` |
| **Attach-OpenOCD(stlink)** | 附加到运行中的 MCU（不重新烧录） |

### 日常工作流

```
修改代码 → Ctrl+Shift+B (增量编译) → F5 (烧录 + 调试)
                            ↓
              改 Kconfig/设备树 → 用 "always" 或 "MCUboot always"
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
   [00:00:00.200] <inf> NOR_TASK: Nor Flash 测试线程已启动 (QSPI indirect 模式)
   uart:~$
   ```
4. Shell 提示符 `uart:~$` 出现，可输入命令（按 Tab 查看可用命令）
5. LED 以 1Hz 频率闪烁，传感器线程间隔输出数据

### Shell 命令示例

```
uart:~$ help              # 查看所有可用命令
uart:~$ device list       # 列出所有设备
uart:~$ kernel version    # 查看内核版本
uart:~$ mcumgr            # 进入 mcumgr SMP 模式（用于设备管理/固件升级）
```

---

## ★ 串口升级 (USART1 + mcumgr, 无按键)

### Overwrite-Only 升级完整流程（4 步）

1. 保持 USART1 (ST-Link VCP) 连接到 PC，115200 baud
2. 设备正常运行中（Shell 可用，应用层 mcumgr 在线）

```powershell
# COM 端口号根据实际替换（Windows 设备管理器 → STMicroelectronics STLink Virtual COM Port）

# ① 查看当前镜像状态
mcumgr --conntype serial --connstring "COM8,baud=115200" image list
# 输出示例:
#  image=0 slot=0  ← slot0 当前运行版本
#  Flags: active confirmed

# ② 上传新固件到 slot1（写入外部 NOR Flash，不影响 slot0 正在运行的固件）
mcumgr --conntype serial --connstring "COM8,baud=115200" image upload build/stm32_ai/zephyr/zephyr.signed.bin
# ★ 必须上传 .signed.bin（已签名固件），.bin 未签名无法通过 MCUboot 校验

# ③ 再次查看镜像，获取 slot1 的 hash
mcumgr --conntype serial --connstring "COM8,baud=115200" image list
# 输出示例:
#  image=0 slot=0  Flags: active confirmed
#  image=0 slot=1  ← 新上传的固件, 记下它的 hash

# ④ 标记 slot1 为 pending（"下次启动尝试运行"）
mcumgr --conntype serial --connstring "COM8,baud=115200" image test <hash>
# ★ 关键步骤：image test 把 slot1 标记为 pending 状态
#    此时还不会覆写 slot0，当前固件继续正常运行

# ⑤ 重启设备
mcumgr --conntype serial --connstring "COM8,baud=115200" reset
# MCUboot 启动 → 发现 slot1 是 pending → 执行 overwrite (slot1→slot0) → 启动新固件

# ⑥ 新固件启动后，确认版本可用
mcumgr --conntype serial --connstring "COM8,baud=115200" image confirm
# ★ 在 overwrite-only 模式下，image confirm 有时会被 MCUboot 自动处理
#    但手动 confirm 更安全可靠
```

### 应急串口恢复（slot0 损坏时自动进入）

- 若 slot0 无有效固件（签名校验失败或未烧录），MCUboot **自动进入串口恢复模式**
- USART1 被 MCUboot 接管，等待 mcumgr 命令
- 上传流程同上
- **不需要任何按键操作**

### 方式二：应用内 mcumgr（Shell 传输层）

应用运行时通过 Shell 使用 mcumgr（查看状态、复位等轻量操作）：

```
uart:~$ mcumgr             # 进入 mcumgr SMP 模式
```

> **注意**：Shell 传输层适合设备管理（查看镜像、复位、统计等）。固件上传推荐直接在串口上使用 `mcumgr --conntype serial`，速度更快。

---

## 应用内 mcumgr 配置参考

```ini
# prj.conf — Shell + mcumgr SMP over Shell

CONFIG_GPIO=y
CONFIG_LOG=y
CONFIG_SHELL=y

# mcumgr 必要依赖
CONFIG_NET_BUF=y
CONFIG_ZCBOR=y
CONFIG_BASE64=y
CONFIG_CRC=y

# mcumgr SMP over Shell（与日志/Shell 无冲突）
CONFIG_MCUMGR=y
CONFIG_MCUMGR_TRANSPORT_SHELL=y
CONFIG_MCUMGR_GRP_IMG=y
CONFIG_MCUMGR_GRP_OS=y
CONFIG_MCUMGR_GRP_SHELL=y

# STM32 Flash 驱动（内部 + NOR Flash, QSPI indirect 模式）
CONFIG_FLASH=y
CONFIG_SOC_FLASH_STM32=y
CONFIG_FLASH_MAP=y
CONFIG_STREAM_FLASH=y
CONFIG_IMG_MANAGER=y
CONFIG_FLASH_STM32_QSPI=y
# CONFIG_STM32_MEMMAP is not set  ← ★ 关键：不开 MEMMAP
CONFIG_DMA=y
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
8. ★ **绝对不开 `CONFIG_STM32_MEMMAP`**：STM32L4 QUADSPI 单控制逻辑，indirect/MEMMAP 互斥
9. **USART1 三合一**：Shell/日志/mcumgr 串口恢复共享 USART1，分时复用无冲突
10. **无固件自动恢复**：若 slot0 无有效固件，MCUboot 自动进入串口恢复模式
11. **Shell 和 mcumgr 共存**：通过 Shell 传输层 (`MCUMGR_TRANSPORT_SHELL`)，无 UART 冲突
12. ★ **升级必须走完整流程**：`upload` → `test` → `reset` → `confirm`（4 步，缺一不可）
13. ★ **slot1 在外部 NOR Flash**：通过 QSPI indirect 模式读写，速度比内部 Flash 慢但容量更大

## 参考资料

- [stm32l475的mcuboot教程.md](doc/stm32l475的mcuboot教程.md) — 完整 MCUboot 配置教程
- [MCUboot 官方文档](https://docs.mcuboot.com/)
- [Zephyr Sysbuild 文档](https://docs.zephyrproject.org/latest/build/sysbuild/)
- [Zephyr Shell 文档](https://docs.zephyrproject.org/latest/services/shell/index.html)
- [mcumgr 命令行工具](https://github.com/apache/mynewt-mcumgr-cli)
- `bootloader/mcuboot/docs/readme-zephyr.md` — MCUboot Zephyr 移植文档
