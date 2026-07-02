# Pandora STM32L475 外设代码示例

> 所有示例基于设备树，不硬编码引脚号。注释格式：`/* [类型] 说明 */`

---

## 1. GPIO — LED 控制

### 单线程 LED 闪烁

```c
/*
 * [LED 闪烁] 控制 Pandora 绿色 LED (PE8) 周期性闪烁
 *
 * 硬件：PE8，共阳极（GPIO_ACTIVE_HIGH = 逻辑点亮）
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_blink, LOG_LEVEL_INF);

/* [设备树] 通过节点标签获取绿色 LED */
#define GREEN_LED_NODE DT_NODELABEL(green_led)
static const struct gpio_dt_spec green_led =
	GPIO_DT_SPEC_GET(GREEN_LED_NODE, gpios);

#define SLEEP_TIME_MS 1000

int main(void)
{
	if (!gpio_is_ready_dt(&green_led)) {
		LOG_ERR("绿色 LED GPIO 未就绪");
		return -ENODEV;
	}

	/* [配置] 输出模式，初始灭 */
	gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_INACTIVE);
	LOG_INF("绿色 LED 初始化完成");

	while (1) {
		/* [操作] 翻转 LED */
		gpio_pin_toggle_dt(&green_led);
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
```

### 多线程 LED 控制

```c
/*
 * [多线程 LED] 每个任务独立线程
 *   - led_tid:    绿色 LED 闪烁（500ms）
 *   - status_tid: 系统状态上报（2000ms）
 *   - main:       初始化后休眠
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(multi_led, LOG_LEVEL_INF);

#define GREEN_LED_NODE DT_NODELABEL(green_led)
static const struct gpio_dt_spec green_led =
	GPIO_DT_SPEC_GET(GREEN_LED_NODE, gpios);

#define LED_STACK      1024
#define LED_PRIO       5
#define STATUS_STACK   2048
#define STATUS_PRIO    7
#define LED_BLINK_MS   500
#define STATUS_MS      2000

/* [LED 线程] 周期性翻转 LED */
static void led_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	LOG_INF("[LED] 启动");
	while (1) {
		gpio_pin_toggle_dt(&green_led);
		k_msleep(LED_BLINK_MS);
	}
}
K_THREAD_DEFINE(led_tid, LED_STACK, led_thread_fn,
		NULL, NULL, NULL, LED_PRIO, 0, 0);

/* [状态线程] 定期上报 */
static void status_thread_fn(void *a, void *b, void *c)
{
	uint32_t n = 0;
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	LOG_INF("[状态] 启动");
	while (1) {
		n++;
		LOG_INF("[状态:%u] uptime=%lld ms", n, k_uptime_get());
		k_msleep(STATUS_MS);
	}
}
K_THREAD_DEFINE(status_tid, STATUS_STACK, status_thread_fn,
		NULL, NULL, NULL, STATUS_PRIO, 0, 0);

/* [主线程] */
int main(void)
{
	if (!gpio_is_ready_dt(&green_led)) {
		LOG_ERR("GPIO 未就绪");
		return -ENODEV;
	}
	gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_INACTIVE);
	LOG_INF("硬件初始化完成");
	while (1) { k_sleep(K_FOREVER); }
	return 0;
}
```

---

## 2. 按键输入

```c
/*
 * [按键检测] 轮询检测 K0 (PD10, 低电平)
 *
 * Pandora 按键：
 *   - K0 → PD10 (joy_right), K1 → PD9 (joy_down)
 *   - K2 → PD8 (joy_left), WK_UP → PC13 (joy_up, 高电平)
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(keys, LOG_LEVEL_INF);

/* [设备树] K0 = joy_right */
#define KEY_NODE DT_NODELABEL(joy_right)
static const struct gpio_dt_spec key =
	GPIO_DT_SPEC_GET(KEY_NODE, gpios);

int main(void)
{
	if (!gpio_is_ready_dt(&key)) {
		LOG_ERR("按键 GPIO 未就绪");
		return -ENODEV;
	}
	gpio_pin_configure_dt(&key, GPIO_INPUT);

	LOG_INF("按键检测启动（按下 K0 查看输出）");

	while (1) {
		if (gpio_pin_get_dt(&key) == 0) {
			LOG_INF("[按键] K0 被按下!");
		}
		k_msleep(50);  /* 50ms 轮询去抖 */
	}
	return 0;
}
```

---

## 3. UART 串口

```c
/*
 * [串口回显] USART1 接收并回显字符
 *
 * 硬件：PA9(TX) / PA10(RX), ST-Link VCP, 115200-8N1
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uart_echo, LOG_LEVEL_INF);

static const struct device *const uart = DEVICE_DT_GET(DT_NODELABEL(usart1));

int main(void)
{
	uint8_t c;

	if (!device_is_ready(uart)) {
		LOG_ERR("USART1 未就绪");
		return -ENODEV;
	}
	LOG_INF("USART1 回显启动, 115200");

	while (1) {
		if (uart_poll_in(uart, &c) == 0) {
			uart_poll_out(uart, c);
		}
		k_msleep(10);
	}
	return 0;
}
```

---

## 4. I2C 传感器扫描

```c
/*
 * [I2C 扫描] 探测 Pandora I2C 总线上的设备
 *
 * 已知设备地址：
 *   ICM20608(IMU)=0x68, AHT10(温湿度)=0x38
 *   AP3216C(光感)=0x1E, ES8388(音频)=0x10/0x11
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_scan, LOG_LEVEL_INF);

static const struct device *const i2c = DEVICE_DT_GET(DT_NODELABEL(i2c3));

int main(void)
{
	uint8_t dummy;

	if (!device_is_ready(i2c)) {
		LOG_ERR("I2C3 未就绪");
		return -ENODEV;
	}
	LOG_INF("I2C 扫描开始...");

	for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
		if (i2c_read(i2c, &dummy, 1, addr) == 0) {
			LOG_INF("[I2C] 设备: 0x%02X", addr);
		}
		k_msleep(1);
	}
	LOG_INF("扫描完成");
	return 0;
}
```

---

## 5. PWM 呼吸灯

```c
/*
 * [PWM 呼吸灯] 蓝色 LED (PE9, TIM1_CH1) 呼吸效果
 *
 * 需要 overlay 启用 timers1 + pwm 节点
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pwm_breath, LOG_LEVEL_INF);

static const struct device *const pwm = DEVICE_DT_GET(DT_NODELABEL(pwm1));

#define PERIOD_NS   20000   /* 50kHz */
#define STEP_MS     10

int main(void)
{
	uint32_t pulse = 0;
	bool up = true;

	if (!device_is_ready(pwm)) {
		LOG_ERR("PWM 未就绪");
		return -ENODEV;
	}
	LOG_INF("蓝色 LED 呼吸灯启动");

	while (1) {
		pwm_set_dt(&(struct pwm_dt_spec){
			.dev = pwm, .channel = 1,
			.period = PERIOD_NS, .flags = 0,
		}, PERIOD_NS, pulse);

		if (up) {
			pulse += PERIOD_NS / 100;
			if (pulse >= PERIOD_NS) up = false;
		} else {
			pulse -= PERIOD_NS / 100;
			if (pulse == 0) up = true;
		}
		k_msleep(STEP_MS);
	}
	return 0;
}
```

---

## LVGL 图形库 + ST7789V LCD (240x240)

> **架构**: LVGL → Zephyr display_write() → ST7789V → MIPI-DBI-SPI → SPI3
>
> **关键 Kconfig**:
> - `CONFIG_LV_COLOR_16_SWAP=y` ★ STM32(小端)→屏幕(大端), 不开颜色异常
> - Zephyr 4.x `lv_conf.h` 已正确桥接此选项, 无需 CMake 额外定义
> - 推荐 auto-init (默认), 简化代码结构

```c
/* === LVGL auto-init 模式 (推荐) === */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>

/* LVGL timer 回调: 更新传感器数据 (在 lv_timer_handler 上下文执行, 单线程安全) */
static void upd_timer_cb(lv_timer_t *t) {
	/* 读取传感器共享数据 → lv_label_set_text() */
}

void display_thread_entry(void *p1, void *p2, void *p3)
{
	/* 1. 背光 PB7=HIGH */
	const struct device *gb = DEVICE_DT_GET(DT_NODELABEL(gpiob));
	k_msleep(300);
	gpio_pin_configure(gb, 7, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_raw(gb, 7, 1);

	/* 2. 显示设备 (auto-init 已完成 lv_init + display_create) */
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(dev)) { LOG_ERR("disp not ready"); return; }
	display_blanking_off(dev);

	/* 3. 构建 UI */
	lv_obj_t *lbl = lv_label_create(lv_scr_act());
	lv_label_set_text(lbl, "Hello LVGL!");
	lv_obj_center(lbl);

	/* 4. 注册刷新 timer */
	lv_timer_create(upd_timer_cb, 500, NULL);

	/* 5. 主循环: 驱动 LVGL (所有操作在同一线程, 无需加锁) */
	while (1) {
		lv_timer_handler();   /* LVGL v9 API */
		k_msleep(30);
	}
}
K_THREAD_DEFINE(display_tid, 6144, display_thread_entry, NULL, NULL, NULL, 10, 0, 0);
```

### LVGL 常见坑

| 问题 | 原因 | 解决 |
|------|------|------|
| 颜色异常/杂色花屏 | 缺少 `LV_COLOR_16_SWAP` | `CONFIG_LV_COLOR_16_SWAP=y` (Zephyr 4.x 已桥接) |
| heap corruption | 栈太小或内存池不足 | 栈≥6144, `LV_Z_MEM_POOL_SIZE=16384` |
| display not found | auto-init 运行早于驱动就绪 | 检查 `zephyr,display` chosen 节点, 验证驱动编译 |
| UI 操作崩溃 | 多线程竞争 LVGL (手动模式) | 单线程调用 lv_timer_handler, 或包裹 `lvgl_lock()/unlock()` |
| 背光不亮 | 未驱动背光 GPIO | `gpio_pin_set(gpio_b, 7, 1)` (PB7 高电平) |

### prj.conf (LCD+LVGL 最小)

```ini
CONFIG_SPI=y
CONFIG_SPI_STM32=y
CONFIG_DISPLAY=y
CONFIG_ST7789V=y
CONFIG_MIPI_DBI=y
CONFIG_LVGL=y
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_COLOR_16_SWAP=y        # ★ RGB565 字节交换
CONFIG_LV_Z_BITS_PER_PIXEL=16
CONFIG_LV_Z_VDB_SIZE=10
CONFIG_LV_Z_MEM_POOL_SIZE=16384
CONFIG_LV_USE_THEME_DEFAULT=y
CONFIG_LV_THEME_DEFAULT_DARK=y
CONFIG_LV_FONT_MONTSERRAT_14=y
CONFIG_LV_FONT_MONTSERRAT_16=y
```

---

## 外设依赖配置速查

| 外设 | prj.conf | overlay 节点 |
|------|----------|-------------|
| GPIO | `CONFIG_GPIO=y` | 板级已启用 |
| UART | `CONFIG_SERIAL=y` | 板级已启用 |
| I2C | `CONFIG_I2C=y` | gpio-i2c 或 &i2c3 |
| SPI | `CONFIG_SPI=y` + `CONFIG_SPI_STM32=y` | `&spi1{}` / `&spi3{}` |
| LCD ST7789V | `CONFIG_DISPLAY=y` `CONFIG_ST7789V=y` `CONFIG_MIPI_DBI=y` | mipi-dbi桥 + st7789v |
| LVGL | `CONFIG_LVGL=y` + LCD 配置 | 同 LCD |
| PWM | `CONFIG_PWM=y` | `&timersX{}` |
| 传感器 | `CONFIG_SENSOR=y` | I2C 总线 + 子节点 |
| LED API | `CONFIG_LED=y` `CONFIG_LED_GPIO=y` | 板级已定义 `leds {}` |
| 日志 | `CONFIG_LOG=y` | 无需 overlay |
| Shell | `CONFIG_SHELL=y` | 配置 `zephyr,shell-uart` |
