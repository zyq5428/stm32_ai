/*
 * ap3216c_thread.c — AP3216C 环境光/接近传感器采集线程
 *
 * 功能: 每 500ms 采集一次 ALS/IR/PS 并通过日志输出
 * 使用 Zephyr 标准传感器 API
 *
 * 硬件: Liteon AP3216C (I2C, 0x1E)
 * 驱动: drivers/sensor/ap3216c/
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ap3216c_thread, LOG_LEVEL_INF);

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

			LOG_INF("AP3216C | ALS: %5d | IR: %5d | PS: %4d",
				als.val1, ir.val1, ps.val1);
		} else {
			LOG_ERR("AP3216C: read error (ret=%d)", ret);
		}

		k_msleep(AP3216C_SAMPLE_MS);
	}
}

K_THREAD_DEFINE(ap3216c_tid, AP3216C_STACK_SIZE,
		ap3216c_thread_entry, NULL, NULL, NULL,
		AP3216C_PRIORITY, 0, 0);
