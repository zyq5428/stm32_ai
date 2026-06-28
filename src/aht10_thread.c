/*
 * aht10_thread.c — AHT10 温湿度传感器采集线程
 *
 * 功能: 每 2 秒采集一次温湿度, 写入共享数据结构供显示线程读取
 * 使用 Zephyr 标准传感器 API (sensor_sample_fetch + sensor_channel_get)
 *
 * 硬件: Aosong AHT10 (I2C, 0x38)
 * 驱动: drivers/sensor/aht10/
 *
 * 日志: LOG_LEVEL_WRN — 仅输出警告和错误, 不打印常规数值
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "sensor_data.h"

LOG_MODULE_REGISTER(aht10_thread, LOG_LEVEL_WRN);

/* 线程参数 */
#define AHT10_STACK_SIZE  1024
#define AHT10_PRIORITY    14
#define AHT10_SAMPLE_MS   2000 /* 采样周期 2s */

/* 从设备树获取传感器设备 */
#define AHT10_DEV_NODE  DT_NODELABEL(aht10)

void aht10_thread_entry(void *p1, void *p2, void *p3)
{
	const struct device *dev = DEVICE_DT_GET(AHT10_DEV_NODE);
	struct sensor_value temp, humidity;
	int ret;

	k_msleep(200);

	if (!device_is_ready(dev)) {
		LOG_ERR("AHT10: device not ready");
		return;
	}

	LOG_INF("AHT10 Thread started");

	while (1) {
		ret = sensor_sample_fetch(dev);
		if (ret == 0) {
			sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
			sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &humidity);

			/* [共享] 写入全局传感器数据结构 */
			k_mutex_lock(&g_sensor_data.lock, K_FOREVER);
			g_sensor_data.temp_val1 = temp.val1;
			g_sensor_data.temp_val2 = temp.val2;
			g_sensor_data.hum_val1  = humidity.val1;
			g_sensor_data.hum_val2  = humidity.val2;
			g_sensor_data.aht10_valid = true;
			k_mutex_unlock(&g_sensor_data.lock);
		} else if (ret == -EBUSY) {
			LOG_WRN("AHT10: busy");
		} else {
			LOG_ERR("AHT10: fetch error (ret=%d)", ret);
		}

		k_msleep(AHT10_SAMPLE_MS);
	}
}

K_THREAD_DEFINE(aht10_tid, AHT10_STACK_SIZE,
		aht10_thread_entry, NULL, NULL, NULL,
		AHT10_PRIORITY, 0, 0);
