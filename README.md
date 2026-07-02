# STM32 AI — Pandora STM32L475 MCUboot + Shell + LVGL 传感器显示 + 串口升级

## 项目信息

| 项目 | 详情 |
|------|------|
| **开发板** | ALIENTEK Pandora STM32L475 IoT Board |
| **MCU** | STM32L475VET6 (Cortex-M4, 80MHz) |
| **Flash/SRAM** | 512KB / 128KB (SRAM0=96KB + SRAM1=32KB) |
| **RTOS** | Zephyr RTOS v4.4.99 |
| **引导程序** | MCUboot (Overwrite-Only 模式) |
| **签名算法** | RSA-2048（项目专用密钥 `root-rsa-2048.pem`） |
| **升级通道** | USART1 (PA9/PA10, ST-Link VCP) — Shell/日志/串口升级三合一 |
| **Shell** | Zephyr Shell 交互式命令行（USART1） |
| **mcumgr** | SMP over Shell 传输层（应用内，与 Shell 共存无冲突） |
| **升级入口** | ★ 无按键 — mcumgr 上传固件到 slot1 (NOR Flash) → 标记 pending → 重启 → MCUboot 覆写 |
| **slot1 位置** | ★ 外部 NOR Flash W25Q128 (QSPI indirect 模式, 不开 MEMMAP) |
| **LCD 显示** | ★ ST7789V 240×240 (MIPI-DBI-SPI, SPI3), LVGL v9 图形库 |
| **传感器** | AHT10 (温湿度) + AP3216C (环境光/接近) + ICM20608 (6轴 IMU) |
| **项目目标** | LED 闪烁 + Shell 命令行 + MCUboot 安全引导 + 串口升级 + LVGL 传感器数据显示 |

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

### 串口分配（三合一架构）

| 串口 | 引脚 | 用途 |
|------|------|------|
| **USART1** | **PA9(TX) / PA10(RX)** | **Shell 命令行 + 日志输出 + MCUboot 串口恢复（分时复用）** |

### LCD 显示

| 参数 | 值 |
|------|-----|
| 驱动芯片 | ST7789V |
| 接口 | MIPI-DBI-SPI 4-Wire (SPI3) |
| 分辨率 | 240×240 |
| 像素格式 | RGB565 (16-bit) |
| 引脚 | SCK=PB3, MOSI=PB5, CS=PD7, DC=PB4, RST=PB6, BL=PB7 |

### 传感器 (I2C)

| 传感器 | 型号 | I2C 地址 | 总线 |
|--------|------|---------|------|
| 温湿度 | AHT10 | 0x38 | GPIO-I2C1 (SCL=PD6, SDA=PC1) |
| 环境光/接近 | AP3216C | 0x1E | GPIO-I2C0 (SCL=PC0, SDA=PC1) |
| 6轴 IMU | ICM20608 | 0x68 | GPIO-I2C0 (SCL=PC0, SDA=PC1) |

### 调试接口

- **ST-Link V2.1**（STM32F103C8T6 芯片）
- USB 连接 **ST-Link 口**（非 USB OTG 口）

---

## MCUboot 引导架构

### Flash 分区布局

```
内部 Flash (512KB):                    外部 NOR Flash W25Q128 (16MB):
┌──────────────────┐ 0x08000000        ┌──────────────────────┐ 0x00000000
│ boot_partition   │ 64KB              │ slot1_partition      │ 448KB ← 升级暂存
│ (mcuboot)        │                   │ (image-1)            │
├──────────────────┤ 0x08010000        ├──────────────────────┤ 0x00070000
│ slot0_partition  │ 448KB             │ rtos_sys_partition   │ 1MB   ← 系统/备份
│ (image-0)        │                   │ (rtos-system)        │
│ ★ 应用主镜像      │                   ├──────────────────────┤ 0x00170000
└──────────────────┘ 0x08080000        │ user_raw_partition   │ 2MB   ← 用户数据
                                       │ (user-raw)           │
                                       ├──────────────────────┤ 0x00370000
                                       │ storage_partition    │ 12.6MB ← LittleFS
                                       │ (storage)            │
                                       └──────────────────────┘ 0x01000000
```

> ★ **STM32L4 QSPI 限制**：外部 NOR Flash 通过 QSPI 访问，**只能使用 indirect 模式**。
> STM32L4 QUADSPI 只有一套控制逻辑，indirect 和 memory-mapped (XIP) 互斥。
> **绝对不能开 `CONFIG_STM32_MEMMAP=y`**。

---

## 项目结构

```
stm32_ai/
├── CMakeLists.txt                       # CMake 构建文件
├── prj.conf                             # ★ 应用 Kconfig (Shell + mcumgr + LVGL + NOR Flash)
├── Kconfig                              # 应用级 Kconfig
├── VERSION                              # 版本文件
├── README.md                            # 本文件
├── root-rsa-2048.pem                    # ★ 项目专用 RSA-2048 签名密钥
├── sysbuild.conf                        # ★ Sysbuild 入口 (密钥路径 + MCUboot 模式)
├── sysbuild/
│   ├── mcuboot.conf                     # ★ MCUboot Kconfig (串口恢复 + QSPI indirect)
│   └── mcuboot.overlay                  # ★ MCUboot DTS (USART1 + 分区, slot1 在 NOR Flash)
├── boards/
│   └── pandora_stm32l475.overlay        # 应用 DTS (LED + LCD + Flash 分区 + 传感器)
├── src/
│   ├── main.c                           # 主程序 (仅打印 + 永久休眠)
│   ├── led_thread.c                     # LED 闪烁线程
│   ├── display_thread.c                 # ★ LVGL LCD 显示线程 (RGB 三色条验证)
│   ├── aht10_thread.c                   # AHT10 温湿度传感器线程
│   ├── ap3216c_thread.c                 # AP3216C 环境光/接近传感器线程
│   ├── icm20608_thread.c                # ICM20608 6 轴传感器线程
│   └── norflash_thread.c               # NOR Flash 自检线程 (已禁用)
├── drivers/sensor/                      # 自定义传感器驱动
│   ├── aht10/                           # AHT10 驱动
│   ├── ap3216c/                         # AP3216C 驱动
│   └── icm20608/                        # ICM20608 驱动
├── dts/bindings/sensor/                 # 传感器设备树绑定
└── include/                             # 公共头文件
    ├── sensor_data.h                    # 传感器共享数据结构 (mutex 保护)
    ├── aht10_app.h
    ├── ap3216c_app.h
    └── icm20608_app.h
```

---

## 编译

### MCUboot 多镜像编译 (Sysbuild)

```powershell
# 完整清理编译（首次或修改 Kconfig/设备树后）
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -p always -b pandora_stm32l475 --sysbuild .\stm32_ai

# 增量编译（仅修改代码时）
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -p auto -b pandora_stm32l475 --sysbuild .\stm32_ai

# Kconfig 图形化配置
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -b pandora_stm32l475 -t menuconfig .\stm32_ai
```

| 参数 | 说明 |
|------|------|
| `-p always` | 完全清理构建 |
| `-p auto` | 增量构建（仅编译变更文件） |
| `--sysbuild` | ★ 启用多镜像构建 (MCUboot + 应用) |
| `-t menuconfig` | 启动 Kconfig 图形化配置界面 |

### 内存占用 (2026-07)

```
MCUboot:
  FLASH:  ~48 KB / 64 KB

Application (含 Shell + mcumgr + LVGL + 传感器驱动):
  FLASH:  ~398 KB / 448 KB (86.8%)  ← 含 Montserrat 24/32/48 字体
  SRAM0:  ~53 KB /  96 KB (54.1%)  ← 应用代码 + 线程栈
  SRAM1:  ~28 KB /  32 KB (85.2%)  ← LVGL 内存池(16KB) + VDB 缓冲区(~12KB)
```

---

## 烧录与运行

### 烧录

```powershell
west flash
```

### 正常启动验证

1. 连接 USART1 (ST-Link VCP, 115200)
2. 按 RESET 或重新上电
3. 预期输出:

```
*** Booting Zephyr OS build v4.4.0-xxx ***
[00:00:00.000] <inf> main: 应用启动完成, Board: pandora_stm32l475
[00:00:00.100] <inf> LED_TASK: LED Thread started
[00:00:00.500] <inf> display_thread: Display Thread started
[00:00:00.501] <inf> display_thread: Found display device: st7789v@0
[00:00:00.501] <inf> display_thread: Display panel ON
[00:00:00.502] <inf> display_thread: Backlight ON (PB7)
[00:00:00.503] <inf> display_thread: RGB bars drawn — entering LVGL loop
uart:~$
```

4. LCD 屏幕显示：**左红 | 中绿 | 右蓝** 三色竖条
5. Shell 提示符 `uart:~$` 出现，可输入命令（按 Tab 查看可用命令）

---

## ★ 串口升级 (USART1 + mcumgr, 无按键)

### Overwrite-Only 升级流程（4 步）

```powershell
# ① 查看当前镜像状态
mcumgr --conntype serial --connstring "COM8,baud=115200" image list

# ② 上传新固件到 slot1（写入外部 NOR Flash，不影响正在运行的固件）
mcumgr --conntype serial --connstring "COM8,baud=115200" image upload build/stm32_ai/zephyr/zephyr.signed.bin

# ③ 获取 slot1 hash
mcumgr --conntype serial --connstring "COM8,baud=115200" image list

# ④ 标记 pending + 重启 + 确认
mcumgr --conntype serial --connstring "COM8,baud=115200" image test <hash>
mcumgr --conntype serial --connstring "COM8,baud=115200" reset
mcumgr --conntype serial --connstring "COM8,baud=115200" image confirm
```

---

## LVGL 显示架构

```
┌────────────────────────────────────────────┐
│ main.c (永久休眠)                           │
│     ├── led_thread.c    — LED 1Hz 闪烁     │
│     ├── aht10_thread.c  — 温湿度采集 (2s)   │
│     ├── ap3216c_thread.c — 环境光采集 (1s)  │
│     ├── icm20608_thread.c — IMU 采集 (50ms) │
│     └── display_thread.c — LVGL 主循环      │
│              │                              │
│      LVGL auto-init (CONFIG_LV_Z_AUTO_INIT) │
│              │                              │
│      display_blanking_off() → 背光 PB7 ON   │
│              │                              │
│      lv_timer_handler() 主循环 (30ms)       │
└────────────────────────────────────────────┘

SRAM 分配:
  SRAM0 (96KB): 应用代码 bss/data (53KB) + 线程栈 + 运行时
  SRAM1 (32KB): LVGL 内存池 (16KB) + VDB 双缓冲 (~12KB)
```

### LVGL 配置要点

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `CONFIG_LV_Z_AUTO_INIT` | y (默认) | LVGL 在 SYS_INIT 阶段自动初始化 |
| `CONFIG_LV_COLOR_16_SWAP` | y | ★ ST7789V RGB565 字节交换 |
| `CONFIG_LV_Z_MEM_POOL_SIZE` | 16384 | 16KB LVGL 内存池 |
| `CONFIG_LV_Z_VDB_SIZE` | 10 | 10% 屏幕缓冲 (24 行) |
| SRAM1 放置 | y | 内存池 + VDB 均放 SRAM1 |
| `CONFIG_LV_FONT_MONTSERRAT_24` | y | 24px 字体 |
| `CONFIG_LV_FONT_MONTSERRAT_32` | y | 32px 字体 |
| `CONFIG_LV_FONT_MONTSERRAT_48` | y | 48px 字体 |

---

## 注意事项

1. **RGB LED 共阳极**：GPIO 输出高电平时 LED 亮
2. **不要接 USB OTG 口**：烧录必须用 ST-Link 口
3. **编译前必须激活虚拟环境**：`cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1`
4. **MCUboot 编译必须加 `--sysbuild`**：否则只编译应用不含引导程序
5. **升级固件必须已签名**：上传 `.signed.bin` 而非未签名的 `.bin`
6. ★ **绝对不开 `CONFIG_STM32_MEMMAP`**：STM32L4 QUADSPI 单控制逻辑，indirect/MEMMAP 互斥
7. ★ **LVGL 不手动调用 `lvgl_init()`**：AUTO_INIT 自动完成
8. ★ **`lv_timer_handler()` 而非 `lv_task_handler()`**：LVGL v9 API
9. ★ **ST7789V 必须开 `CONFIG_LV_COLOR_16_SWAP=y`**：否则颜色完全错误
10. ★ **LVGL 内存必须放 SRAM1**：SRAM0 空间不足，SRAM1 专门承载 LVGL 内存池和缓冲区

## 参考资料

- [stm32l475的mcuboot教程.md](doc/stm32l475的mcuboot教程.md) — 完整 MCUboot 配置教程
- [MCUboot 官方文档](https://docs.mcuboot.com/)
- [Zephyr Sysbuild 文档](https://docs.zephyrproject.org/latest/build/sysbuild/)
- [Zephyr LVGL 文档](https://docs.zephyrproject.org/latest/modules/lvgl/index.html)
- [Zephyr Display 文档](https://docs.zephyrproject.org/latest/hardware/peripherals/display/index.html)
- [Zephyr Shell 文档](https://docs.zephyrproject.org/latest/services/shell/index.html)
- [mcumgr 命令行工具](https://github.com/apache/mynewt-mcumgr-cli)
