# STM32 AI — Pandora STM32L475 MCUboot + USART2 串口升级

## 项目信息

| 项目 | 详情 |
|------|------|
| **开发板** | ALIENTEK Pandora STM32L475 IoT Board |
| **MCU** | STM32L475VET6 (Cortex-M4, 80MHz) |
| **Flash/SRAM** | 512KB / 128KB |
| **RTOS** | Zephyr RTOS v4.4.99 |
| **引导程序** | MCUboot (Overwrite-Only 模式) |
| **签名算法** | RSA-2048 |
| **升级通道** | USART2 (PA2/PA3) + mcumgr SMP 协议 |
| **项目目标** | 绿色 LED 1Hz 闪烁 + MCUboot 安全引导 + 串口升级 |

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

### 串口分配

| 串口 | 引脚 | 用途 |
|------|------|------|
| USART1 | PA9(TX) / PA10(RX) | 应用控制台 (ST-Link VCP, 115200) |
| **USART2** | **PA2(TX) / PA3(RX)** | **MCUboot 串口升级通道 (115200)** |

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
│    │         USART2 等待 mcumgr 上传固件   │
│    │         写入 slot1 → 拷贝到 slot0      │
│    │         跳转执行新固件                  │
│    └─ 未按 → 校验 slot0 RSA-2048 签名      │
│              ├─ 通过 → 启动应用 (0x08010000)│
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
├── prj.conf                             # 应用 Kconfig 配置
├── Kconfig                              # 应用级 Kconfig
├── VERSION                              # 版本文件
├── README.md                            # 本文件
├── root-rsa-2048.pem                    # 项目签名密钥
├── stm32l475的mcuboot教程.md            # ★ MCUboot 完整教程
├── sysbuild.conf                        # ★ Sysbuild 入口 (启用 MCUboot)
├── sysbuild/
│   ├── mcuboot.conf                     # ★ MCUboot Kconfig (串口恢复配置)
│   └── mcuboot.overlay                  # ★ MCUboot DTS (USART2 + 分区 + GPIO)
├── boards/
│   └── pandora_stm32l475.overlay        # 应用 DTS (LED + 分区定义)
└── src/
    ├── main.c                           # 主程序 (仅打印 + 永久休眠)
    └── led_thread.c                     # 绿色 LED 闪烁线程
```

> ★ 标记为 MCUboot 关键文件。

---

## 编译

### 环境要求

- Windows 10/11 + PowerShell
- Python 3.12+（虚拟环境 `.venv`）
- Zephyr SDK 1.0.1（工具链 arm-zephyr-eabi）
- CMake + Ninja
- `pip install intelhex cbor2 pyyaml cryptography`（imgtool.py 依赖）

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
│   ├── zephyr.elf         # MCUboot ELF (~1.8MB)
│   ├── zephyr.bin         # MCUboot 二进制 (~44KB)
│   └── zephyr.hex         # MCUboot Intel HEX
├── stm32_ai/zephyr/
│   ├── zephyr.elf         # 应用 ELF (~1MB)
│   ├── zephyr.signed.bin  # ★ 已签名应用二进制 (~28KB)
│   └── zephyr.signed.hex  # ★ 已签名应用 HEX
└── _sysbuild/             # Sysbuild 内部文件
```

### 内存占用参考

```
MCUboot:
  FLASH:  45,120 B  / 512 KB  ( 8.61%)  — fits in 64KB boot_partition
  RAM:    35,968 B  /  96 KB  (36.59%)

Application:
  FLASH: ~28,000 B  / 224 KB  (12.50%)  — signed, fits in slot0_partition
  RAM:    ~5,000 B  /  96 KB  ( 5.00%)
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

### 正常启动验证

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
   ```
4. 绿色 LED 以 1Hz 频率闪烁

---

## 串口升级 (USART2 + mcumgr)

### 进入串口恢复模式

1. 连接 USART2 (PA2/PA3) 到 PC USB-TTL 模块
2. **按住 KEY0 不放** → 按 RESET → 等待 1 秒 → 松开 KEY0
3. MCUboot 检测到按键 → 进入 mcumgr 串口恢复模式

### 升级流程

```powershell
# ① 安装 mcumgr
pip install mcumgr

# ② 查看当前镜像信息
mcumgr --conntype serial --connstring "COM6,baud=115200" image list

# ③ 上传新固件 (必须是 .signed.bin)
mcumgr --conntype serial --connstring "COM6,baud=115200" image upload build/stm32_ai/zephyr/zephyr.signed.bin

# ④ 复位启动新固件
mcumgr --conntype serial --connstring "COM6,baud=115200" reset
```

---

## 注意事项

1. **RGB LED 共阳极**：GPIO 输出高电平时 LED 亮
2. **不要接 USB OTG 口**：烧录必须用 ST-Link 口
3. **编译前必须激活虚拟环境**：`cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1`
4. **MCUboot 编译必须加 `--sysbuild`**：否则只编译应用不含引导程序
5. **升级固件必须已签名**：上传 `.signed.bin` 而非未签名的 `.bin`
6. **签名密钥需一致**：MCUboot 编译和镜像签名必须使用同一密钥
7. **USART1/USART2 不冲突**：控制台用 USART1，升级用 USART2，互不影响
8. **无固件自动恢复**：若 slot0 无有效固件，MCUboot 自动进入串口恢复模式

## 参考资料

- [stm32l475的mcuboot教程.md](stm32l475的mcuboot教程.md) — 完整 MCUboot 配置教程
- [MCUboot 官方文档](https://docs.mcuboot.com/)
- [Zephyr Sysbuild 文档](https://docs.zephyrproject.org/latest/build/sysbuild/)
- `bootloader/mcuboot/docs/readme-zephyr.md` — MCUboot Zephyr 移植文档
