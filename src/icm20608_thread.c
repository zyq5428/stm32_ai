/*
 * icm20608_thread.c — ICM20608 六轴 IMU 传感器采集线程
 *
 * 功能: 每 100ms (10Hz) 采集加速度计+陀螺仪并通过日志输出
 * 使用 Zephyr 标准传感器 API
 *
 * 硬件: InvenSense ICM-20608-G (I2C, 0x68)
 * 驱动: drivers/sensor/icm20608/
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(icm20608_thread, LOG_LEVEL_INF);

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

			LOG_INF("ICM20608 | Acc: X%5d.%03d Y%5d.%03d Z%5d.%03d"
				" | Gyro: X%5d.%03d Y%5d.%03d Z%5d.%03d"
				" | T: %d.%03d",
				accel[0].val1, accel[0].val2 / 1000,
				accel[1].val1, accel[1].val2 / 1000,
				accel[2].val1, accel[2].val2 / 1000,
				gyro[0].val1, gyro[0].val2 / 1000,
				gyro[1].val1, gyro[1].val2 / 1000,
				gyro[2].val1, gyro[2].val2 / 1000,
				temp.val1, temp.val2 / 1000);
		} else {
			LOG_ERR("ICM20608: read error (ret=%d)", ret);
		}

		k_msleep(ICM20608_SAMPLE_MS);
	}
}

K_THREAD_DEFINE(icm20608_tid, ICM20608_STACK_SIZE,
		icm20608_thread_entry, NULL, NULL, NULL,
		ICM20608_PRIORITY, 0, 0);
