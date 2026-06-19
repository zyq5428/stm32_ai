# STM32 AI — Pandora STM32L475 绿色 LED 闪烁

## 项目信息

| 项目 | 详情 |
|------|------|
| **开发板** | ALIENTEK Pandora STM32L475 IoT Board |
| **MCU** | STM32L475VET6 (Cortex-M4, 80MHz) |
| **Flash/SRAM** | 512KB / 128KB |
| **RTOS** | Zephyr RTOS v4.4.99 |
| **项目目标** | 绿色 LED（PE8）以 1Hz 频率（500ms 亮 + 500ms 灭）闪烁 |

## 硬件说明

### RGB LED

| 颜色 | 引脚 | 设备树节点 |
|------|------|-----------|
| 红 (R) | PE7 | red_led |
| **绿 (G)** | **PE8** | **green_led** |
| 蓝 (B) | PE9 | blue_led |

> **注意**：RGB LED 为共阳极结构，GPIO 输出高电平时 LED 亮，低电平时 LED 灭。

### 调试接口

- **ST-Link V2.1**（STM32F103C8T6 芯片）
- USB 连接 **ST-Link 口**（非 USB OTG 口）

---

## 项目结构

```
stm32_ai/
├── CMakeLists.txt                      # CMake 构建文件
├── prj.conf                            # Kconfig 应用配置
├── Kconfig                             # 应用级Kconfig
├── VERSION                             # 版本文件
├── README.md                           # 本文件
├── boards/
│   └── pandora_stm32l475.overlay       # 设备树覆盖（green-led 别名）
└── src/
    ├── main.c                          # 主程序（仅打印板信息 + 永久休眠）
    └── led_thread.c                    # 绿色 LED 闪烁独立线程
```

### 架构说明

- **main.c**：仅输出启动信息后永久休眠，不包含任何业务逻辑
- **led_thread.c**：绿色 LED 独立线程，通过 `K_THREAD_DEFINE` 自动启动

---

## 编译

### 环境要求

- Windows 10/11 + PowerShell
- Python 3.12+（虚拟环境 `.venv`）
- Zephyr SDK 1.0.1（工具链 arm-zephyr-eabi）
- CMake + Ninja

### 编译命令

```powershell
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -p always -b pandora_stm32l475 .\stm32_ai
```

| 参数 | 说明 |
|------|------|
| `-p always` | 完全清理构建 |
| `-b pandora_stm32l475` | 目标开发板 |
| `.\stm32_ai` | 应用路径 |

### 构建产物

```
build/zephyr/
├── zephyr.elf     # ELF 可执行文件
├── zephyr.bin     # 二进制固件
├── zephyr.hex     # Intel HEX 固件
└── zephyr.map     # 内存映射
```

---

## 烧录与运行

### 烧录

```powershell
west flash
```

### 接线

1. Micro USB 线连接开发板 **ST-Link 接口**
2. PC 设备管理器可见 **ST-Link Debug** 和 **STMicroelectronics STLink Virtual COM Port**

### 预期现象

- 上电后绿色 LED 以 1Hz 频率闪烁（亮 500ms + 灭 500ms）
- 串口（115200bps）输出启动日志

### 串口监视

```powershell
# 替换 COM5 为实际端口号
python -m serial.tools.miniterm COM5 115200
```

---

## 注意事项

1. **RGB LED 共阳极**：板载 RGB LED 为共阳极结构，GPIO 输出高电平时 LED 亮
2. **不要接 USB OTG 口**：烧录必须用 ST-Link 口（靠近电源指示灯一侧的 Micro USB）
3. **编译前必须激活虚拟环境**：`cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1`
4. **ST-Link V2.1**：板载调试器，无需外接 ST-Link
