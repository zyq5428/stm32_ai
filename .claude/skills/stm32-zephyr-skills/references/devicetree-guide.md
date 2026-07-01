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

## SPI — 串行外设

```dts
/* SPI3 — LCD ST7789V2 */
&spi3 {
	status = "okay";
	pinctrl-0 = <&spi3_sck_pb3 &spi3_mosi_pb5>;
	pinctrl-names = "default";
	cs-gpios = <&gpiod 7 GPIO_ACTIVE_LOW>;

	display: st7789v@0 {
		compatible = "sitronix,st7789v";
		reg = <0>;
		spi-max-frequency = <40000000>;
		reset-gpios = <&gpiob 6 GPIO_ACTIVE_LOW>;
		dc-gpios = <&gpiob 4 GPIO_ACTIVE_HIGH>;
		backlight-gpios = <&gpiob 7 GPIO_ACTIVE_HIGH>;
		width = <240>;
		height = <240>;
	};
};

/* SPI1 — TF 卡槽 */
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
