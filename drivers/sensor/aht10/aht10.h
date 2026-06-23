/*
 * aht10.h — AHT10 温湿度传感器驱动私有头文件
 *
 * 参照: Zephyr drivers/sensor/aosong/dht20/dht20.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_AHT10_AHT10_H_
#define ZEPHYR_DRIVERS_SENSOR_AHT10_AHT10_H_

#include <zephyr/drivers/i2c.h>

/* 初始化命令: 传感器校准 */
#define AHT10_INIT_CMD      0xE1
#define AHT10_INIT_DATA0    0x08
#define AHT10_INIT_DATA1    0x00

/* 触发测量命令 */
#define AHT10_MEASURE_CMD   0xAC
#define AHT10_MEASURE_DATA0 0x33
#define AHT10_MEASURE_DATA1 0x00

/* 状态字节忙标志 (bit 7) */
#define AHT10_STATUS_BUSY   BIT(7)

/* 测量等待时间 (手册规定: 最长 80ms) */
#define AHT10_MEASURE_WAIT_MS  80

/* 上电等待时间 */
#define AHT10_POWER_ON_WAIT_MS 100

/* 数据帧长度 */
#define AHT10_FRAME_LEN     6

/* 驱动配置 (从设备树获取) */
struct aht10_config {
	struct i2c_dt_spec bus;
};

/* 驱动运行时数据 */
struct aht10_data {
	uint32_t t_sample;   /* 原始温度采样 (20-bit) */
	uint32_t rh_sample;  /* 原始湿度采样 (20-bit) */
};

#endif /* ZEPHYR_DRIVERS_SENSOR_AHT10_AHT10_H_ */
