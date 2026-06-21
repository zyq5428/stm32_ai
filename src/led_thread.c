/*
 * led_thread.c — 绿色 LED 闪烁独立线程
 * 功能: 500ms 间隔闪烁 Pandora 板绿色 LED
 * 硬件: Pandora STM32L475 IoT Board
 * 引脚: PE8 (green_led) — 共阳极 RGB LED 绿色通道
 */

/* [头文件] Zephyr 内核、GPIO 驱动和日志 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

/* [日志] 注册 LED_TASK 模块 */
LOG_MODULE_REGISTER(LED_TASK, LOG_LEVEL_INF);

/* [设备树] 从别名 green-led 获取 GPIO 规格 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/**
 * @brief LED 线程入口函数
 *
 * 每 500ms 翻转一次绿色 LED，产生 1Hz 闪烁效果。
 *
 * @param p1, p2, p3 线程参数（由 K_THREAD_DEFINE 传入，此处未使用）
 */
void led_thread_entry(void *p1, void *p2, void *p3)
{
	/* [延时] 启动后短暂休眠 100ms，确保系统其他组件初始化完成 */
	k_msleep(100);

	LOG_INF("LED Thread started");

	int ret; /* 函数返回状态码 */

	/* [检查] 验证 GPIO 设备是否已就绪 */
	if (gpio_is_ready_dt(&led)) {
		LOG_DBG("Found LED device %s", led.port->name);
	} else {
		LOG_ERR("LED device %s is not ready", led.port->name);
		return; /* 设备不可用，终止线程 */
	}

	/* [配置] 设置 GPIO 为输出，初始状态为关闭（共阳极：高电平=灭） */
	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Error: Failed to configure LED pin (ret=%d)", ret);
		while (1) { return; }
	}

	/* [LED 周期] 主循环 — 500ms 翻转一次，1Hz 闪烁 */
	while (1) {
		/* [延时] 休眠 500 毫秒 */
		k_msleep(500);

		/* [操作] 翻转 LED 引脚电平 */
		gpio_pin_toggle_dt(&led);
	}
}

/* [线程配置] 栈大小和优先级 */
#define LED_STACK_SIZE 1024  /* 线程栈大小（单位：字节） */
#define LED_PRIORITY 15      /* 线程优先级（数字越大优先级越低） */

/**
 * @brief 定义并自动启动 LED 线程
 *
 * K_THREAD_DEFINE 参数依次为：
 * 线程 ID, 栈大小, 入口函数, 参数1, 参数2, 参数3, 优先级, 选项, 启动延迟
 */
K_THREAD_DEFINE(led_tid, LED_STACK_SIZE,
		led_thread_entry, NULL, NULL, NULL,
		LED_PRIORITY, 0, 0);
