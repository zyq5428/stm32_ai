/*
 * display_thread.c — LVGL LCD 传感器数据显示
 *
 * 架构: LVGL auto-init + 单线程驱动
 * CONFIG_LV_COLOR_16_SWAP=y → lv_conf.h 桥接 → LV_COLOR_16_SWAP
 * 所有 LVGL API 调用都在本线程上下文, 无需加锁
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

#include <lvgl.h>
#include "sensor_data.h"

LOG_MODULE_REGISTER(display_thread, LOG_LEVEL_INF);

/* 线程栈/优先级/刷新周期 */
#define STACK   6144
#define PRIO    10
#define REFRESH 500

/* UI控件句柄 */
static lv_obj_t *lt, *lh, *la, *li, *lp, *lax[3], *lgx[3];

/* ---- 传感器数值格式化 ---- */
static void fv(char *b, size_t sz, int32_t v1, int32_t v2) {
	int32_t d = v2 / 100000; if (d < 0) d = -d;
	snprintf(b, sz, (v1 < 0 || (v1 == 0 && v2 < 0)) ? "-%d.%01ld" : "%d.%01ld",
		v1 < 0 ? -(int)v1 : (int)v1, (long)d);
}
static void fi(char *b, size_t sz, int32_t v1) { snprintf(b, sz, "%d", v1); }

/* ---- UI构建函数 ---- */
static lv_obj_t *mk_t(lv_obj_t *p) {
	lv_obj_t *b = lv_obj_create(p);
	lv_obj_set_size(b, LV_PCT(100), 26);
	lv_obj_set_style_bg_color(b, lv_color_hex(0x102040), 0);
	lv_obj_set_style_border_width(b, 0, 0);
	lv_obj_set_style_radius(b, 0, 0);
	lv_obj_set_style_pad_all(b, 0, 0);
	lv_obj_t *t = lv_label_create(b);
	lv_label_set_text(t, "PANDORA SENSORS");
	lv_obj_set_style_text_color(t, lv_color_white(), 0);
	lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
	lv_obj_center(t);
	return b;
}

static lv_obj_t *mk_c(lv_obj_t *p, const char *tt, lv_color_t ac) {
	lv_obj_t *c = lv_obj_create(p);
	lv_obj_set_size(c, LV_PCT(100), LV_SIZE_CONTENT);
	lv_obj_set_style_bg_color(c, lv_color_hex(0x293050), 0);
	lv_obj_set_style_border_width(c, 0, 0);
	lv_obj_set_style_radius(c, 4, 0);
	lv_obj_set_style_pad_all(c, 4, 0);
	lv_obj_t *ln = lv_obj_create(c);
	lv_obj_set_size(ln, 3, LV_PCT(100));
	lv_obj_align(ln, LV_ALIGN_LEFT_MID, 0, 0);
	lv_obj_set_style_bg_color(ln, ac, 0);
	lv_obj_set_style_border_width(ln, 0, 0);
	lv_obj_set_style_radius(ln, 0, 0);
	lv_obj_t *h = lv_label_create(c);
	lv_label_set_text(h, tt);
	lv_obj_set_style_text_color(h, lv_color_white(), 0);
	lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
	lv_obj_align_to(h, ln, LV_ALIGN_OUT_RIGHT_TOP, 8, 0);
	return c;
}

static lv_obj_t *mk_r(lv_obj_t *c, const char *k, const char *u, lv_color_t vc) {
	lv_obj_t *r = lv_obj_create(c);
	lv_obj_set_size(r, LV_PCT(100), 22);
	lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(r, 0, 0);
	lv_obj_set_style_pad_all(r, 0, 0);
	lv_obj_set_style_pad_left(r, 6, 0);
	lv_obj_t *kl = lv_label_create(r);
	lv_label_set_text(kl, k);
	lv_obj_set_style_text_color(kl, lv_color_hex(0x808080), 0);
	lv_obj_set_style_text_font(kl, &lv_font_montserrat_14, 0);
	lv_obj_align(kl, LV_ALIGN_LEFT_MID, 0, 0);
	lv_obj_t *vl = lv_label_create(r);
	lv_label_set_text(vl, "---");
	lv_obj_set_style_text_color(vl, vc, 0);
	lv_obj_set_style_text_font(vl, &lv_font_montserrat_14, 0);
	lv_obj_align_to(vl, kl, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
	lv_obj_t *ul = lv_label_create(r);
	lv_label_set_text(ul, u);
	lv_obj_set_style_text_color(ul, lv_color_hex(0x606060), 0);
	lv_obj_set_style_text_font(ul, &lv_font_montserrat_14, 0);
	lv_obj_align_to(ul, vl, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
	return vl;
}

static void make_ui(void) {
	lv_obj_t *s = lv_scr_act();
	lv_obj_set_style_bg_color(s, lv_color_hex(0x1A1A2E), 0);

	lv_obj_t *tb = mk_t(s);
	lv_obj_align(tb, LV_ALIGN_TOP_MID, 0, 0);

	lv_obj_t *cs = lv_obj_create(s);
	lv_obj_set_size(cs, LV_PCT(100), 212);
	lv_obj_set_style_bg_opa(cs, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(cs, 0, 0);
	lv_obj_set_style_pad_all(cs, 2, 0);
	lv_obj_align(cs, LV_ALIGN_TOP_MID, 0, 26);

	/* AHT10 卡片 (青色) */
	lv_obj_t *c1 = mk_c(cs, "AHT10", lv_color_hex(0x00FFFF));
	lt = mk_r(c1, "Temp:", "C",   lv_color_white());
	lh = mk_r(c1, "Hum :", "%",   lv_color_white());

	/* AP3216C 卡片 (黄色) */
	lv_obj_t *c2 = mk_c(cs, "AP3216C", lv_color_hex(0xFFFF00));
	la = mk_r(c2, "ALS:", "lux", lv_color_hex(0xFFE000));
	li = mk_r(c2, "IR :", "",    lv_color_hex(0xFF8800));
	lp = mk_r(c2, "PS :", "",    lv_color_hex(0x00FF00));

	/* ICM20608 卡片 (绿色) */
	lv_obj_t *c3 = mk_c(cs, "ICM20608", lv_color_hex(0x00FF00));
	lax[0] = mk_r(c3, "AccX:", "g",   lv_color_hex(0x00FFFF));
	lax[1] = mk_r(c3, "AccY:", "g",   lv_color_hex(0x00FFFF));
	lax[2] = mk_r(c3, "AccZ:", "g",   lv_color_hex(0x00FFFF));
	lgx[0] = mk_r(c3, "GyrX:", "dps", lv_color_hex(0xFF8800));
	lgx[1] = mk_r(c3, "GyrY:", "dps", lv_color_hex(0xFF8800));
	lgx[2] = mk_r(c3, "GyrZ:", "dps", lv_color_hex(0xFF8800));

	/* 底部信息 */
	lv_obj_t *st = lv_label_create(s);
	lv_label_set_text(st, "STM32L475 @ 80MHz");
	lv_obj_set_style_text_color(st, lv_color_hex(0x606060), 0);
	lv_obj_set_style_text_font(st, &lv_font_montserrat_14, 0);
	lv_obj_align(st, LV_ALIGN_BOTTOM_MID, 0, -2);
}

/* ---- 传感器数据刷新 (LVGL timer 回调, 在 lv_timer_handler 上下文执行) ---- */
static void upd_timer_cb(lv_timer_t *t) {
	char b[32];
	struct sensor_data d;

	/* 拷贝共享数据 (mutex保护, 无需LVGL锁 — 本回调运行在lv_timer_handler同一线程) */
	k_mutex_lock(&g_sensor_data.lock, K_FOREVER);
	memcpy(&d, &g_sensor_data, sizeof(d));
	k_mutex_unlock(&g_sensor_data.lock);

	fv(b, sizeof(b), d.temp_val1, d.temp_val2);   lv_label_set_text(lt, b);
	fv(b, sizeof(b), d.hum_val1, d.hum_val2);     lv_label_set_text(lh, b);
	fi(b, sizeof(b), d.als_val1);                 lv_label_set_text(la, b);
	fi(b, sizeof(b), d.ir_val1);                  lv_label_set_text(li, b);
	fi(b, sizeof(b), d.ps_val1);                  lv_label_set_text(lp, b);
	fv(b, sizeof(b), d.accel_x_v1, d.accel_x_v2); lv_label_set_text(lax[0], b);
	fv(b, sizeof(b), d.accel_y_v1, d.accel_y_v2); lv_label_set_text(lax[1], b);
	fv(b, sizeof(b), d.accel_z_v1, d.accel_z_v2); lv_label_set_text(lax[2], b);
	fv(b, sizeof(b), d.gyro_x_v1, d.gyro_x_v2);  lv_label_set_text(lgx[0], b);
	fv(b, sizeof(b), d.gyro_y_v1, d.gyro_y_v2);  lv_label_set_text(lgx[1], b);
	fv(b, sizeof(b), d.gyro_z_v1, d.gyro_z_v2);  lv_label_set_text(lgx[2], b);
}

/* ---- 显示线程入口 ---- */
void display_thread_entry(void *p1, void *p2, void *p3)
{
	/* 1. 背光: PB7 GPIO */
	const struct device *gb = DEVICE_DT_GET(DT_NODELABEL(gpiob));
	k_msleep(300);
	if (device_is_ready(gb)) {
		gpio_pin_configure(gb, 7, GPIO_OUTPUT_ACTIVE);
		gpio_pin_set_raw(gb, 7, 1);
		LOG_INF("BL ON (PB7)");
	}

	/* 2. 获取显示设备 (lvgl_init 已在 SYS_INIT 完成 auto-init) */
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(dev)) {
		LOG_ERR("Display device not ready!");
		return;
	}

	/* 3. 开启显示 */
	display_blanking_off(dev);
	LOG_INF("Display on: %s", dev->name);

	/* 4. 创建 UI */
	make_ui();

	/* 5. 创建传感器刷新 timer (运行在 lv_timer_handler 上下文, 单线程安全) */
	lv_timer_create(upd_timer_cb, REFRESH, NULL);

	LOG_INF("Display started");

	/* 6. 主循环: 驱动 LVGL */
	while (1) {
		lv_timer_handler();
		k_msleep(30);
	}
}

K_THREAD_DEFINE(display_tid, STACK, display_thread_entry, NULL, NULL, NULL, PRIO, 0, 0);
