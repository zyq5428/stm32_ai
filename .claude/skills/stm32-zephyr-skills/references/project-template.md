# STM32/Pandora 项目模板指南

> 创建新的 Zephyr 项目时，生成以下完整文件。所有文件必须完整，不使用省略号。

---

## ★ 核心架构原则（违反则视为错误）

| 规则 | 说明 |
|------|------|
| **main.c 禁止写业务逻辑** | main() 只做两件事：① `LOG_INF("Board: %s", CONFIG_BOARD)` ② `k_sleep(K_FOREVER)` |
| **业务逻辑独立线程化** | 每个功能（LED/按键/传感器/通信）一个独立线程文件（如 `led_thread.c`） |
| **K_THREAD_DEFINE 自启动** | 线程通过 `K_THREAD_DEFINE` 定义并自动启动，main.c 不手动创建线程 |
| **一类功能 = 一个文件** | LED → led_thread.c、按键 → key_thread.c、传感器 → sensor_thread.c |
| **项目必须含 README.md** | 板型号 + 项目目标 + 编译/接线/运行说明 |

> **反面案例（禁止）**：把 LED 的 GPIO 初始化、while(1) 循环直接写在 main() 中。
> **正面案例（必须）**：main.c 仅打印信息后 `k_sleep(K_FOREVER)`，LED 操作全部放在 `led_thread.c` 中。

---

## 文件结构总览

```
<项目名>/
├── CMakeLists.txt          # CMake 构建文件
├── prj.conf                # Kconfig 应用配置
├── Kconfig                 # (可选) 应用级 Kconfig 选项
├── sysbuild.conf           # (可选) 多映像/多固件构建（Sysbuild）的配置文件
├── VERSION          		# Zephyr 生命周期管理
├── README.md               # 需要包含项目信息、如何使用、注意事项 项目的说明文档，通常包含项目的开发板型号、项目目标，以及如何编译、接线和运行。
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

> **重要**：`VERSION` 是 Zephyr 应用的标准文件。构建后在 `build/zephyr/include/generated/zephyr/app_version.h` 生成 `APP_VERSION_MAJOR/MINOR/PATCHLEVEL/TWEAK` 宏供代码引用。

---

## VERSION — 应用版本号

```makefile
VERSION_MAJOR = 0
VERSION_MINOR = 1
PATCHLEVEL = 0
VERSION_TWEAK = 0
```

| 字段 | 说明 |
|------|------|
| `VERSION_MAJOR` | 主版本号，重大架构变更时递增 |
| `VERSION_MINOR` | 次版本号，新增功能时递增 |
| `PATCHLEVEL` | 补丁级别，Bug修复时递增 |
| `VERSION_TWEAK` | 微调版本，细微调整时递增 |

---

## CMakeLists.txt — 构建入口

```cmake
# SPDX-License-Identifier: Apache-2.0
# [构建配置] STM32/Pandora Zephyr 项目的 CMake 构建入口

cmake_minimum_required(VERSION 3.20.0)

# [查找 Zephyr] 引入 Zephyr 构建系统
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# [定义项目] 项目名称，建议英文小写+下划线
project(application)

# [添加源文件] app 是 Zephyr 预定义的应用目标
target_sources(app PRIVATE src/main.c)
```

---

## prj.conf — Kconfig 基础配置

```
# ==================== Zephyr 基础配置 ====================

# [GPIO] 启用 GPIO 驱动
CONFIG_GPIO=y

# [日志] 启用 Zephyr 日志子系统
CONFIG_LOG=y

# [线程命名] 为线程启用名称，便于调试
CONFIG_THREAD_NAME=y

# [断言] 开发阶段建议开启
CONFIG_ASSERT=y
```

### 按需启用

```
# [I2C] 传感器/音频编解码器 → CONFIG_I2C=y
# [SPI] LCD/TF卡/NRF24L01 → CONFIG_SPI=y
# [PWM] 电机/背光/蜂鸣器 → CONFIG_PWM=y
# [ADC] 模拟量采集 → CONFIG_ADC=y
# [传感器] → CONFIG_SENSOR=y
# [LED API] → CONFIG_LED=y + CONFIG_LED_GPIO=y
# [Shell] 交互调试 → CONFIG_SHELL=y
```

---

## Kconfig — 应用级自定义配置

> **★ 关键规则**：应用级 `Kconfig` 文件**必须**包含 `source "Kconfig.zephyr"`，否则所有 Zephyr 系统符号（GPIO、LOG、I2C 等）都无法识别。

```kconfig
mainmenu "我的 STM32 应用"

# ★★★ 必须！加载 Zephyr 系统 Kconfig 符号树 ★★★
source "Kconfig.zephyr"

# [自定义] LED 闪烁间隔
config MY_LED_BLINK_MS
	int "LED blink interval (ms)"
	default 500
	help
	  LED 闪烁间隔时间，单位毫秒。

# [自定义] 状态上报间隔
config MY_STATUS_INTERVAL_MS
	int "Status report interval (ms)"
	default 2000
	help
	  状态上报线程的运行间隔，单位毫秒。
```

---

## boards/pandora_stm32l475.overlay — 设备树覆盖

```dts
/*
 * [设备树覆盖] Pandora STM32L475 IoT Board
 *
 * 板级 DTS 已定义：
 *   - red_led (led_0) on PE7
 *   - green_led (led_1) on PE8
 *   - blue_led (led_2) on PE9
 *   - usart1 PA9(TX)/PA10(RX)
 *   - 按键: joy_up(PC13), joy_down(PD9), joy_left(PD8), joy_right(PD10)
 */

/ {
	aliases {
		/* 为绿色 LED 添加别名，代码中 DT_ALIAS(ledg) 引用 */
		ledg = &green_led;
	};
};
```

---

## src/main.c — 模板

```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("应用启动完成, Board: %s", CONFIG_BOARD);

    // 关键：进入永久休眠，把所有的 CPU 资源和栈空间释放给那些重要线程
    while (1) {
        k_sleep(K_FOREVER);
    }
    return 0;
}
```

---

## src/xxx_thread.c — 线程模板

```c
/*
 * xxx任务独立线程
 */

#include <errno.h>
#include <string.h>
#include <zephyr/logging/log.h>

// 注册日志模块，名称为 LED_TASK，日志级别设置为 INF (信息)
// 这样在控制台可以看到系统的运行状态和错误信息
LOG_MODULE_REGISTER(LED_TASK, LOG_LEVEL_INF);

#include <zephyr/drivers/gpio.h>

// 从设备树 (Device Tree) 中获取别名为 "led0" 的节点标识符
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/**
 * @brief LED 线程入口函数
 * 
 * @param p1, p2, p3 线程参数（由 K_THREAD_DEFINE 传入，此处未使用）
 */
void led_thread_entry(void *p1, void *p2, void *p3)
{
    /* --- 线程启动后先短暂休眠 100 毫秒，确保系统其他组件初始化完成 --- */
    k_msleep(100);
    
    LOG_INF("LED Thread started");
    
    int ret;           // 用于接收函数返回状态码

    /* --- 检查硬件设备是否已准备就绪(驱动是否加载成功) --- */
    if (gpio_is_ready_dt(&led)) {
        LOG_DBG("Found LED device %s", led->name);
    } else {
        LOG_ERR("LED device %s is not ready", strip->name);
        return; // 如果设备不可用，终止线程
    }

	/* --- 设置GPIO为输出状态 --- */
    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Error: Failed to configure LED pin (ret=%d)", ret);
        while(1) { return; } 
	}

    // 线程主循环
    while (1) {
        /* 每隔1s翻转一次led */
		k_msleep(1000);
		gpio_pin_toggle_dt(&led);
    }
}

/* 线程配置参数 */
#define LED_STACK_SIZE 1024  // 线程栈大小（单位：字节）
#define LED_PRIORITY 15      // 线程优先级（数字越大优先级越低）

/**
 * @brief 定义并自动启动线程
 * 
 * 参数依次为：线程 ID, 栈大小, 入口函数, 参数1, 参数2, 参数3, 优先级, 选项, 启动延迟
 */
K_THREAD_DEFINE(led_tid, LED_STACK_SIZE, 
                led_thread_entry, NULL, NULL, NULL,
                LED_PRIORITY, 0, 0);
```
