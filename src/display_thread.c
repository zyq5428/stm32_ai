/*
 * display_thread.c — LVGL 动效验证 (四球公转 + 脉冲中心 + 轨道环)
 *
 * 功能: 验证 LVGL + ST7789V + MIPI-DBI-SPI 显示通路的动画渲染能力
 * 硬件: Pandora STM32L475 IoT Board
 * 显示: ST7789V 240x240 (SPI3)
 * 背光: PB7 GPIO
 *
 * ★ LVGL 通过 CONFIG_LV_Z_AUTO_INIT=y 自动初始化, 本线程不调用 lvgl_init()
 */

/* [头文件] Zephyr 内核、GPIO、显示驱动和日志 */
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

#include <lvgl.h>
#include <math.h>

/* [日志] 注册 display_thread 模块 */
LOG_MODULE_REGISTER(display_thread, LOG_LEVEL_INF);

/* [设备树] 显示设备 */
static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* [常量] 屏幕中心坐标 (240×240 → 中心 = 120,120) */
#define SCR_CX 120
#define SCR_CY 120

/* [对象] 四颗轨道小球指针 */
static lv_obj_t *ball_r;   /* 红色 — 内轨道 (r=40) */
static lv_obj_t *ball_g;   /* 绿色 — 中内轨道 (r=55) */
static lv_obj_t *ball_b;   /* 蓝色 — 中外轨道 (r=70) */
static lv_obj_t *ball_y;   /* 黄色 — 外轨道 (r=85) */

/* [对象] 中心脉冲圆 */
static lv_obj_t *center_dot;

/* [动画] 全局轨道角度变量 (0~3600, 单位 0.1°, 即 0°~360°) */
static int32_t orbit_angle = 0;

/* ================================================================
 * [动画回调] 轨道小球 — 计算四个球的屏幕坐标并实时更新位置
 *
 * 四颗球以相同角速度、不同轨道半径公转。
 * 相位各差 90° (900 单位), 均匀分布在圆周上。
 * ================================================================ */
static void orbit_anim_cb(void *var, int32_t v)
{
	float rad;
	lv_coord_t x, y;

	/* [数组] 四个球的指针、轨道半径、初始相位 */
	lv_obj_t *targets[4] = { ball_r, ball_g, ball_b, ball_y };
	int32_t  radius[4]   = { 40, 55, 70, 85 };
	int32_t  phase[4]    = { 0, 900, 1800, 2700 };  /* 各差 90° */

	orbit_angle = v;

	for (int i = 0; i < 4; i++) {
		/* 将角度换算为弧度，计算球心坐标 */
		rad = (float)((v + phase[i]) % 3600) * 3.14159f / 1800.0f;
		x = SCR_CX + (lv_coord_t)((float)radius[i] * cosf(rad)) - 7;
		y = SCR_CY + (lv_coord_t)((float)radius[i] * sinf(rad)) - 7;
		lv_obj_set_pos(targets[i], x, y);
	}
}

/* ================================================================
 * [动画回调] 脉冲中心 — 改变圆心控件大小 (10↔24 往返)
 * ================================================================ */
static void pulse_cb(void *var, int32_t v)
{
	lv_obj_set_size(center_dot, (lv_coord_t)v, (lv_coord_t)v);
}

/**
 * @brief 显示线程入口函数
 *
 * 流程 (参照 stm32_app):
 *   1. 等待驱动就绪 → 2. 检查显示设备 → 3. 开启显示
 *   → 4. 开启背光 → 5. 构建动画场景 → 6. 进入 LVGL 主循环
 *
 * ★ 不调用 lvgl_init() — LVGL 已通过 CONFIG_LV_Z_AUTO_INIT=y 自动初始化
 *
 * @param p1, p2, p3 线程参数（由 K_THREAD_DEFINE 传入，此处未使用）
 */
void display_thread_entry(void *p1, void *p2, void *p3)
{
	int ret;

	/* [延时] 等待 SPI/MIPI-DBI/ST7789V 驱动完全就绪 */
	k_msleep(500);

	LOG_INF("Display Thread started");

	/* [检查] 验证显示设备是否已就绪 */
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device %s is not ready", display_dev->name);
		return;
	}
	LOG_INF("Found display device: %s", display_dev->name);

	/* [操作] 开启显示面板 */
	ret = display_blanking_off(display_dev);
	if (ret < 0) {
		LOG_ERR("Failed to turn display on (ret=%d)", ret);
		return;
	}
	LOG_INF("Display panel ON");

	/* [背光] PB7 GPIO 使能 LCD 背光 */
	const struct device *gpiob = DEVICE_DT_GET(DT_NODELABEL(gpiob));
	if (device_is_ready(gpiob)) {
		ret = gpio_pin_configure(gpiob, 7, GPIO_OUTPUT_ACTIVE);
		if (ret == 0) {
			gpio_pin_set_raw(gpiob, 7, 1);
			LOG_INF("Backlight ON (PB7)");
		}
	} else {
		LOG_WRN("GPIOB not ready, backlight skipped");
	}

	/* ================================================================
	 * [场景构建] 创建所有动画元素
	 * ================================================================ */

	/* [屏幕] 深色背景 */
	lv_obj_t *scr = lv_scr_act();
	lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0A18), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

	/* [控件] 静态轨道参考圆环 — 仅显示边框，透明填充，作视觉引导 */
	lv_obj_t *guide = lv_obj_create(scr);
	lv_obj_set_size(guide, 180, 180);
	lv_obj_set_style_radius(guide, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_opa(guide, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(guide, 1, 0);
	lv_obj_set_style_border_color(guide, lv_color_hex(0x1A1A44), 0);
	lv_obj_set_style_border_opa(guide, LV_OPA_60, 0);
	lv_obj_center(guide);

	/* [控件] 红色小球 — 内轨道，半径 40px */
	ball_r = lv_obj_create(scr);
	lv_obj_set_size(ball_r, 14, 14);
	lv_obj_set_style_radius(ball_r, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(ball_r, lv_color_hex(0xFF3355), 0);
	lv_obj_set_style_border_width(ball_r, 0, 0);
	lv_obj_set_style_bg_opa(ball_r, LV_OPA_COVER, 0);

	/* [控件] 绿色小球 — 中内轨道，半径 55px */
	ball_g = lv_obj_create(scr);
	lv_obj_set_size(ball_g, 14, 14);
	lv_obj_set_style_radius(ball_g, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(ball_g, lv_color_hex(0x33FF77), 0);
	lv_obj_set_style_border_width(ball_g, 0, 0);
	lv_obj_set_style_bg_opa(ball_g, LV_OPA_COVER, 0);

	/* [控件] 蓝色小球 — 中外轨道，半径 70px */
	ball_b = lv_obj_create(scr);
	lv_obj_set_size(ball_b, 14, 14);
	lv_obj_set_style_radius(ball_b, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(ball_b, lv_color_hex(0x3399FF), 0);
	lv_obj_set_style_border_width(ball_b, 0, 0);
	lv_obj_set_style_bg_opa(ball_b, LV_OPA_COVER, 0);

	/* [控件] 黄色小球 — 外轨道，半径 85px */
	ball_y = lv_obj_create(scr);
	lv_obj_set_size(ball_y, 14, 14);
	lv_obj_set_style_radius(ball_y, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(ball_y, lv_color_hex(0xFFCC33), 0);
	lv_obj_set_style_border_width(ball_y, 0, 0);
	lv_obj_set_style_bg_opa(ball_y, LV_OPA_COVER, 0);

	/* [控件] 中心脉冲圆点 — 半透明白色 */
	center_dot = lv_obj_create(scr);
	lv_obj_set_size(center_dot, 16, 16);
	lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(center_dot, lv_color_hex(0xFFFFFF), 0);
	lv_obj_set_style_border_width(center_dot, 0, 0);
	lv_obj_set_style_bg_opa(center_dot, LV_OPA_70, 0);
	lv_obj_center(center_dot);

	/* ================================================================
	 * [动画启动] 两个独立动画同时运行
	 * ================================================================ */

	/* [动画] 轨道小球 — 5 秒匀速公转一圈，无限循环 */
	lv_anim_t anim_orbit;
	lv_anim_init(&anim_orbit);
	lv_anim_set_var(&anim_orbit, &orbit_angle);
	lv_anim_set_exec_cb(&anim_orbit, orbit_anim_cb);
	lv_anim_set_values(&anim_orbit, 0, 3600);
	lv_anim_set_duration(&anim_orbit, 5000);
	lv_anim_set_repeat_count(&anim_orbit, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&anim_orbit);

	/* [动画] 脉冲中心 — 0.7s 放大 → 0.7s 缩小，无限往返 */
	lv_anim_t anim_pulse;
	lv_anim_init(&anim_pulse);
	lv_anim_set_var(&anim_pulse, &anim_pulse);  /* var 仅作占位符,实际在回调中操作 center_dot */
	lv_anim_set_exec_cb(&anim_pulse, pulse_cb);
	lv_anim_set_values(&anim_pulse, 10, 24);
	lv_anim_set_duration(&anim_pulse, 700);
	lv_anim_set_playback_duration(&anim_pulse, 700);
	lv_anim_set_repeat_count(&anim_pulse, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&anim_pulse);

	LOG_INF("LVGL animation started — 4-layer orbiting balls + pulse center");

	/* [主循环] 驱动 LVGL timer handler, 50 FPS */
	while (1) {
		lv_timer_handler();
		k_msleep(20);
	}
}

/* [线程配置] 栈大小和优先级 */
#define DISPLAY_STACK_SIZE 8192  /* 线程栈大小（单位：字节） */
#define DISPLAY_PRIORITY    10   /* 线程优先级（数字越大优先级越低） */

/**
 * @brief 定义并自动启动显示线程
 *
 * K_THREAD_DEFINE 参数依次为：
 * 线程 ID, 栈大小, 入口函数, 参数1, 参数2, 参数3, 优先级, 选项, 启动延迟
 */
K_THREAD_DEFINE(display_thread_tid, DISPLAY_STACK_SIZE,
		display_thread_entry, NULL, NULL, NULL,
		DISPLAY_PRIORITY, 0, 0);
