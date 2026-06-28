/*
 * ap3216c_thread.c — AP3216C 环境光/接近传感器采集线程
 *
 * 功能: 每 500ms 采集一次 ALS/IR/PS, 写入共享数据结构供显示线程读取
 *       使用 Zephyr 标准传感器 API
 *
 * 硬件: Liteon AP3216C (I2C, 0x1E)
 * 驱动: drivers/sensor/ap3216c/
 *
 * 日志: LOG_LEVEL_WRN — 仅输出警告和错误, 不打印常规数值
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "sensor_data.h"

LOG_MODULE_REGISTER(ap3216c_thread, LOG_LEVEL_WRN);

#define AP3216C_STACK_SIZE  1024
#define AP3216C_PRIORITY    14
#define AP3216C_SAMPLE_MS   500 /* 采样周期 500ms */

#define AP3216C_DEV_NODE  DT_NODELABEL(ap3216c)

void ap3216c_thread_entry(void *p1, void *p2, void *p3)
{
	const struct device *dev = DEVICE_DT_GET(AP3216C_DEV_NODE);
	struct sensor_value als, ir, ps;
	int ret;

	k_msleep(250);

	if (!device_is_ready(dev)) {
		LOG_ERR("AP3216C: device not ready");
		return;
	}

	LOG_INF("AP3216C Thread started");

	while (1) {
		ret = sensor_sample_fetch(dev);
		if (ret == 0) {
			sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &als);
			sensor_channel_get(dev, SENSOR_CHAN_IR, &ir);
			sensor_channel_get(dev, SENSOR_CHAN_PROX, &ps);

			/* [共享] 写入全局传感器数据结构 */
			k_mutex_lock(&g_sensor_data.lock, K_FOREVER);
			g_sensor_data.als_val1 = als.val1;
			g_sensor_data.als_val2 = als.val2;
			g_sensor_data.ir_val1  = ir.val1;
			g_sensor_data.ir_val2  = ir.val2;
			g_sensor_data.ps_val1  = ps.val1;
			g_sensor_data.ps_val2  = ps.val2;
			g_sensor_data.ap3216c_valid = true;
			k_mutex_unlock(&g_sensor_data.lock);
		} else {
			LOG_ERR("AP3216C: read error (ret=%d)", ret);
		}

		k_msleep(AP3216C_SAMPLE_MS);
	}
}

K_THREAD_DEFINE(ap3216c_tid, AP3216C_STACK_SIZE,
		ap3216c_thread_entry, NULL, NULL, NULL,
		AP3216C_PRIORITY, 0, 0);
