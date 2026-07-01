# STM32/Pandora 构建与烧录指南

> 适用于 ALIENTEK Pandora STM32L475 IoT Board + Zephyr RTOS

---

## 1. 环境激活（每次新终端必须执行）

**必须用 `&&` 串联成一行执行**，否则 PowerShell 进程间环境变量不保留：

```powershell
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -p always -b pandora_stm32l475 .\stm32_ai
```

> `west` 安装在项目的 `.venv` 中，不激活会报 `command not found`。

---

## 2. 构建命令

```powershell
# 完整清理构建（首次构建或修改 prj.conf/overlay 后）
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -p always -b pandora_stm32l475 .\stm32_ai

# 增量构建（仅修改代码时，更快）
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -b pandora_stm32l475 .\stm32_ai

# 构建官方示例验证环境
cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build -p always -b pandora_stm32l475 samples\basic\blinky
```

| 参数 | 说明 |
|------|------|
| `-p always` | 完全清理后构建（pristine），删除缓存 |
| `-p auto` | 自动判断（默认） |
| `-b <board>` | 目标板 `pandora_stm32l475` |

### 构建配置工具

```powershell
# 文本菜单配置
west build -b pandora_stm32l475 -t menuconfig .\stm_ai

# 图形化配置
west build -b pandora_stm32l475 -t guiconfig .\stm_ai
```

---

## 3. 烧录命令

Pandora 使用 **ST-Link V2.1**（STM32F103C8T6）烧录：

```powershell
west flash
```

### 烧录前检查
1. Micro USB 连接 **ST-Link 口**（非 USB OTG 口）
2. 设备管理器可见 **ST-Link Debug** 和 **STMicroelectronics STLink Virtual COM Port**
3. 板载电源指示灯亮

---

## 4. 串口监视

ST-Link V2.1 提供虚拟串口（VCP），连接 USART1 PA9/PA10，默认波特率 115200。

```powershell
# Python 串口工具（COM号在设备管理器查看）
python -m serial.tools.miniterm COM5 115200

# 保存日志到文件
python -m serial.tools.miniterm COM5 115200 | Tee-Object -FilePath serial.log
```

### 串口输出示例

```
*** Booting Zephyr OS build v4.4.0-xxx ***
[00:00:00.000] <inf> stm_ai: STM_AI 多线程应用启动
[00:00:00.001] <inf> stm_ai: 硬件初始化完成
[00:00:00.001] <inf> stm_ai: [LED] 线程启动
[00:00:00.001] <inf> stm_ai: [状态] 线程启动
[00:00:02.000] <inf> stm_ai: [状态:1] uptime=2000 ms
```

---

## 5. 构建产物

`build/zephyr/` 目录关键文件：

| 文件 | 说明 |
|------|------|
| `zephyr.elf` | ELF 可执行文件（含调试信息） |
| `zephyr.bin` | 二进制固件 |
| `zephyr.hex` | Intel HEX 格式固件 |
| `zephyr.map` | 内存映射（分析 RAM/Flash 使用） |
| `.config` | 最终 Kconfig 合并配置 |
| `zephyr.dts` | 预处理后的设备树（调试用） |
| `include/generated/zephyr/autoconf.h` | Kconfig 生成的 C 宏 |
| `include/generated/zephyr/devicetree_generated.h` | 设备树生成的 C 宏 |
| `include/generated/zephyr/app_version.h` | VERSION 文件生成的版本宏 |

---

## 6. 清理

```powershell
# 清理构建产物（保留配置）
west build -t clean

# 完全删除构建目录
Remove-Item -Recurse -Force build
```

---

## 7. 调试

### SWD 调试（通过 ST-Link）

```powershell
# 启动 OpenOCD
openocd -f board/stm32l4discovery.cfg

# GDB 连接
arm-zephyr-eabi-gdb build/zephyr/zephyr.elf
# (gdb) target remote :3333
# (gdb) monitor reset halt
# (gdb) continue
```

### 打印调试

```c
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(my_app, LOG_LEVEL_DBG);
LOG_DBG("调试: x=%d", x);
LOG_INF("运行正常");
LOG_ERR("错误: err=%d", err);
```

---

## 8. 常见错误及解决

| 错误 | 原因 | 解决 |
|------|------|------|
| `west: command not found` | 未激活 venv | `cd E:\zephyrproject\ && .\.venv\scripts\Activate.ps1 && west build ...` |
| Kconfig "undefined symbol" | 应用 Kconfig 缺少 `source "Kconfig.zephyr"` | 在 Kconfig 文件的 mainmenu 后添加 |
| `Board pandora_stm32l475 not found` | Zephyr 环境未加载 | 执行激活步骤，运行 `west boards \| Select-String "pandora"` 确认 |
| `section '.text' will not fit in 'FLASH'` | Flash 空间不足 | 关闭非必要功能，使用 `-Os` 优化 |
| `undefined reference to __device_dts_ord_` | 设备树引用错误 | 检查 overlay 中 status="okay"和节点名拼写 |
| `k_msleep(K_FOREVER)` 编译错误 | 类型不匹配 | 改为 `k_sleep(K_FOREVER)` |

---

## 9. 快速排查清单

1. USB 接 ST-Link 口（非 USB OTG），数据线非仅充电线
2. 设备管理器中有 ST-Link Debug 和 COM 端口
3. 已执行 `cd E:\zephyrproject\` + `.\.venv\scripts\Activate.ps1`
4. prj.conf 启用了所需功能
5. overlay 中对应外设 `status = "okay"`
6. 应用 Kconfig 文件中包含 `source "Kconfig.zephyr"`
7. 板载红色电源指示灯亮
