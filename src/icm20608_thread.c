/*
 * icm20608_thread.c — ICM20608 六轴 IMU 传感器采集线程
 *
 * 功能: 每 100ms (10Hz) 采集加速度计+陀螺仪, 写入共享数据结构供显示线程读取
 *       使用 Zephyr 标准传感器 API
 *
 * 硬件: InvenSense ICM-20608-G (I2C, 0x68)
 * 驱动: drivers/sensor/icm20608/
 *
 * 日志: LOG_LEVEL_WRN — 仅输出警告和错误, 不打印常规数值
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "sensor_data.h"

LOG_MODULE_REGISTER(icm20608_thread, LOG_LEVEL_WRN);

#define ICM20608_STACK_SIZE  1024
#define ICM20608_PRIORITY    14
#define ICM20608_SAMPLE_MS   100 /* 采样周期 100ms (10Hz) */

#define ICM20608_DEV_NODE  DT_NODELABEL(icm20608)

void icm20608_thread_entry(void *p1, void *p2, void *p3)
{
	const struct device *dev = DEVICE_DT_GET(ICM20608_DEV_NODE);
	struct sensor_value accel[3], gyro[3], temp;
	int ret;

	k_msleep(300);

	if (!device_is_ready(dev)) {
		LOG_ERR("ICM20608: device not ready");
		return;
	}

	LOG_INF("ICM20608 Thread started");

	while (1) {
		ret = sensor_sample_fetch(dev);
		if (ret == 0) {
			sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel);
			sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, gyro);
			sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &temp);

			/* [共享] 写入全局传感器数据结构 */
			k_mutex_lock(&g_sensor_data.lock, K_FOREVER);
			g_sensor_data.accel_x_v1 = accel[0].val1;
			g_sensor_data.accel_x_v2 = accel[0].val2;
			g_sensor_data.accel_y_v1 = accel[1].val1;
			g_sensor_data.accel_y_v2 = accel[1].val2;
			g_sensor_data.accel_z_v1 = accel[2].val1;
			g_sensor_data.accel_z_v2 = accel[2].val2;
			g_sensor_data.gyro_x_v1  = gyro[0].val1;
			g_sensor_data.gyro_x_v2  = gyro[0].val2;
			g_sensor_data.gyro_y_v1  = gyro[1].val1;
			g_sensor_data.gyro_y_v2  = gyro[1].val2;
			g_sensor_data.gyro_z_v1  = gyro[2].val1;
			g_sensor_data.gyro_z_v2  = gyro[2].val2;
			g_sensor_data.icm20608_valid = true;
			k_mutex_unlock(&g_sensor_data.lock);
		} else {
			LOG_ERR("ICM20608: read error (ret=%d)", ret);
		}

		k_msleep(ICM20608_SAMPLE_MS);
	}
}

K_THREAD_DEFINE(icm20608_tid, ICM20608_STACK_SIZE,
		icm20608_thread_entry, NULL, NULL, NULL,
		ICM20608_PRIORITY, 0, 0);
