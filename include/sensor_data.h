/*
 * sensor_data.h — 传感器共享数据结构
 *
 * 所有传感器线程将最新读数写入此结构 (mutex保护)
 * display_thread 通过 lv_timer 回调读取并更新 UI
 */

#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>

struct sensor_data {
	struct k_mutex lock;

	/* AHT10 温湿度 */
	int32_t temp_val1, temp_val2;   /* 温度: val1.val2 (带1位小数) */
	int32_t hum_val1, hum_val2;     /* 湿度: val1.val2 */
	bool aht10_valid;

	/* AP3216C 环境光/接近/红外 */
	int32_t als_val1, als_val2;     /* 环境光 lux */
	int32_t ir_val1, ir_val2;       /* 红外 */
	int32_t ps_val1, ps_val2;       /* 接近 */
	bool ap3216c_valid;

	/* ICM20608 六轴 IMU */
	int32_t accel_x_v1, accel_x_v2; /* 加速度 X */
	int32_t accel_y_v1, accel_y_v2; /* 加速度 Y */
	int32_t accel_z_v1, accel_z_v2; /* 加速度 Z */
	int32_t gyro_x_v1, gyro_x_v2;   /* 陀螺仪 X */
	int32_t gyro_y_v1, gyro_y_v2;   /* 陀螺仪 Y */
	int32_t gyro_z_v1, gyro_z_v2;   /* 陀螺仪 Z */
	bool icm20608_valid;
};

/* 全局传感器数据实例 (main.c 中定义) */
extern struct sensor_data g_sensor_data;

#endif /* SENSOR_DATA_H */
