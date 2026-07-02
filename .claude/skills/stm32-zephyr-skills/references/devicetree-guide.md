# STM32/Pandora 设备树配置指南

> 适用于 ALIENTEK Pandora STM32L475 IoT Board + Zephyr RTOS

---

## 核心原则

1. **始终使用 `.overlay` 覆盖**，不改 Zephyr 源码中的板级文件
2. **使用设备树宏** 获取硬件配置，不硬编码引脚号
3. **使用 pinctrl 子系统** 配置引脚复用功能
4. **注意 Pandora 特殊硬件**：RGB LED 共阳极，I2C 多设备 SDA 重叠

---

## STM32 pinctrl 命名规则

```
<外设>_<信号>_<端口><引脚>
```

| 宏名称 | 含义 |
|--------|------|
| `usart1_tx_pa9` | USART1 TX → PA9 |
| `usart1_rx_pa10` | USART1 RX → PA10 |
| `spi1_sck_pa5` | SPI1 SCK → PA5 |
| `i2c3_scl_pc0` | I2C3 SCL → PC0 |
| `pwm3_ch1_pb4` | PWM3 CH1 → PB4 |

---

## GPIO — LED 与按键

### LED（输出）

Pandora 板级 DTS 已定义 RGB LED，overlay 只需添加别名：

```dts
/ {
	aliases {
		ledr = &red_led;    /* 红色 PE7 */
		ledg = &green_led;  /* 绿色 PE8 */
		ledb = &blue_led;   /* 蓝色 PE9 */
	};
};
```

> RGB LED 为共阳极（低电平物理点亮），但 DTS 使用 GPIO_ACTIVE_HIGH，通过 Zephyr 的 GPIO API 统一处理。

### 按键（输入）

```dts
/ {
	aliases {
		key0 = &joy_down;   /* K0: PD9, 低电平 */
		key1 = &joy_left;   /* K1: PD8, 低电平 */
		key2 = &joy_right;  /* K2: PD10, 低电平 */
		wkup = &joy_up;     /* WK_UP: PC13, 高电平 */
	};
};
```

---

## UART — 串口

```dts
/* USART1 — ST-Link VCP（板级已启用） */
&usart1 {
	status = "okay";
	current-speed = <115200>;
	pinctrl-0 = <&usart1_tx_pa9 &usart1_rx_pa10>;
	pinctrl-names = "default";
};

/* USART2 — 扩展串口 */
&usart2 {
	status = "okay";
	current-speed = <115200>;
	pinctrl-0 = <&usart2_tx_pa2 &usart2_rx_pa3>;
	pinctrl-names = "default";
};
```

---

## I2C — 总线通信

### ⚠️ Pandora I2C 冲突

| 逻辑总线 | 设备 | SCL | SDA | 地址 |
|---------|------|-----|-----|------|
| I2C-A | ES8388+ICM20608+AP3216C | PC0 | PC1 | 0x10/0x68/0x1E |
| I2C-B | AHT10 | PD6 | PC1 | 0x38 |

AHT10 的 SDA 与其它设备重叠（都使用 PC1），**强烈建议使用 GPIO 软件模拟 I2C**：

```dts
/ {
	/* [软件 I2C-A] ES8388 + ICM20608 + AP3216C */
	i2c_shared: i2c-gpio-a {
		compatible = "i2c-gpio";
		scl-gpios = <&gpioc 0 (GPIO_PULL_UP | GPIO_ACTIVE_HIGH)>;
		sda-gpios = <&gpioc 1 (GPIO_PULL_UP | GPIO_ACTIVE_HIGH)>;
		#address-cells = <1>;
		#size-cells = <0>;
	};

	/* [软件 I2C-B] AHT10（独立 SCL: PD6） */
	i2c_aht10: i2c-gpio-b {
		compatible = "i2c-gpio";
		scl-gpios = <&gpiod 6 (GPIO_PULL_UP | GPIO_ACTIVE_HIGH)>;
		sda-gpios = <&gpioc 1 (GPIO_PULL_UP | GPIO_ACTIVE_HIGH)>;
		#address-cells = <1>;
		#size-cells = <0>;
	};
};
```

---

## ST7789V LCD (240x240) — Zephyr 内置驱动方案

> **推荐方案**: 使用 Zephyr 内置 `sitronix,st7789v` 驱动 + MIPI-DBI-SPI 桥。
> 此方案经过验证, 配合 LVGL auto-init 可正常工作, 无需自定义驱动。

### 硬件连接 (SPI3)

| LCD 引脚 | STM32 引脚 | 说明 |
|----------|-----------|------|
| SCK | PB3 | SPI3 时钟 |
| MOSI | PB5 | SPI3 数据 |
| CS | PD7 | 片选 |
| DC | PB4 | 数据/命令选择 |
| RST | PB6 | 复位 |
| BL | PB7 | 背光 (GPIO) |

### 1. DTS overlay (MIPI-DBI-SPI 桥)

```dts
#include <zephyr/dt-bindings/display/panel.h>

/ {
	chosen {
		zephyr,display = &display;  /* ★ LVGL auto-init 需要 */
	};

	/* MIPI-DBI-SPI 桥 + ST7789V */
	mipi_dbi: mipi-dbi {
		compatible = "zephyr,mipi-dbi-spi";
		dc-gpios = <&gpiob 4 GPIO_ACTIVE_HIGH>;
		reset-gpios = <&gpiob 6 GPIO_ACTIVE_LOW>;
		spi-dev = <&spi3>;
		write-only;
		#address-cells = <1>;
		#size-cells = <0>;

		display: st7789v@0 {
			compatible = "sitronix,st7789v";
			reg = <0>;
			mipi-mode = "MIPI_DBI_MODE_SPI_4WIRE";
			mipi-max-frequency = <20000000>;
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

&spi3 {
	status = "okay";
	pinctrl-0 = <&spi3_sck_pb3 &spi3_mosi_pb5>;
	pinctrl-names = "default";
	cs-gpios = <&gpiod 7 GPIO_ACTIVE_LOW>;
};
```

### 2. prj.conf (LCD+LVGL 最小)

```ini
# 显示驱动
CONFIG_SPI=y
CONFIG_DISPLAY=y
CONFIG_ST7789V=y
CONFIG_MIPI_DBI=y

# LVGL
CONFIG_LVGL=y
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_Z_BITS_PER_PIXEL=16
CONFIG_LV_Z_VDB_SIZE=10
CONFIG_LV_Z_MEM_POOL_SIZE=16384

# ★ 关键: RGB565 字节交换 — STM32(小端) → ST7789V(大端)
# Zephyr 4.x 的 lv_conf.h 已正确桥接此选项
CONFIG_LV_COLOR_16_SWAP=y

CONFIG_LV_USE_THEME_DEFAULT=y
CONFIG_LV_THEME_DEFAULT_DARK=y
```

### 3. display_thread.c (auto-init 模式)

LVGL auto-init (`CONFIG_LV_Z_AUTO_INIT=y`, 默认) 在系统启动时自动完成 `lv_init()` + 显示创建 + 渲染回调设置。应用线程只需开启显示 + 构建 UI + 驱动 LVGL:

```c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>

void display_thread_entry(void *p1, void *p2, void *p3)
{
	/* 背光 */
	const struct device *gb = DEVICE_DT_GET(DT_NODELABEL(gpiob));
	k_msleep(300);
	gpio_pin_configure(gb, 7, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_raw(gb, 7, 1);

	/* 显示设备 (auto-init 已完成 LVGL 初始化) */
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(dev)) { return; }
	display_blanking_off(dev);

	/* 构建 UI */
	make_ui();

	/* 主循环: 驱动 LVGL (所有操作在同一线程, 无需加锁) */
	while (1) {
		lv_timer_handler();  /* LVGL v9 API */
		k_msleep(30);
	}
}
K_THREAD_DEFINE(display_tid, 6144, display_thread_entry, NULL, NULL, NULL, 10, 0, 0);
```

> **架构要点**: 所有 LVGL API 调用在同一线程 (display 线程) 中完成, `lv_timer_handler()` 驱动所有 timer 回调和渲染。传感器数据通过共享内存 + mutex 传入, UI 更新通过 `lv_timer_create()` 注册的回调在 LVGL 上下文中执行。

### LCD 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| 颜色异常/花屏 | 缺少 `CONFIG_LV_COLOR_16_SWAP=y` | 添加此选项 (Zephyr 4.x lv_conf.h 已桥接) |
| 显示不亮 | 背光未开启 | PB7 GPIO 手动控制背光 |
| "Display device not found" | auto-init 失败 | 检查 `zephyr,display` chosen 节点是否正确 |
| 编译错误 `MIPI_DBI` | 缺少 Kconfig | prj.conf 添加 `CONFIG_ST7789V=y` + `CONFIG_MIPI_DBI=y` |
| LVGL 堆内存不足 | 默认 2KB 不够 | `CONFIG_LV_Z_MEM_POOL_SIZE=16384` |

---

### SPI1 — TF 卡槽

```dts
&spi1 {
	status = "okay";
	pinctrl-0 = <&spi1_sck_pa5 &spi1_miso_pa6 &spi1_mosi_pa7>;
	pinctrl-names = "default";
	cs-gpios = <&gpioc 3 GPIO_ACTIVE_LOW>;
};
```

---

## PWM — 脉宽调制

```dts
#include <dt-bindings/pwm/pwm.h>

/* PWM3 CH1 — PB4 */
&pwm3 {
	status = "okay";
	pinctrl-0 = <&pwm3_ch1_pb4>;
	pinctrl-names = "default";
};

/* TIM1 CH1 — PE9（蓝色 LED PWM） */
&timers1 {
	status = "okay";
	pwm1: pwm {
		status = "okay";
		pinctrl-0 = <&tim1_ch1_pe9>;
		pinctrl-names = "default";
	};
};
```

---

## ADC — 模数转换

```dts
&adc1 {
	status = "okay";
	pinctrl-0 = <&adc1_in5_pa0>;
	pinctrl-names = "default";
};
```

---

## 代码中使用设备树

### 方式一：gpio_dt_spec（推荐）

```c
#define LED_NODE DT_NODELABEL(green_led)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

gpio_is_ready_dt(&led);
gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
gpio_pin_toggle_dt(&led);
```

### 方式二：DT_ALIAS（通过别名）

```c
#define LED_NODE DT_ALIAS(ledg)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
```

### 方式三：DEVICE_DT_GET（获取设备指针）

```c
static const struct device *const uart = DEVICE_DT_GET(DT_NODELABEL(usart1));
```

---

## 调试设备树

```powershell
# 查看合并后的最终设备树
cat build\zephyr\zephyr.dts

# 查看设备树生成的头文件
cat build\zephyr\include\generated\zephyr\devicetree_generated.h

# 仅运行设备树编译阶段
west build -b pandora_stm32l475 -t devicetree .\stm_ai
```
