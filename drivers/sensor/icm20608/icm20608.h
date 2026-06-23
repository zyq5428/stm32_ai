/*
 * icm20608.h — ICM20608 六轴 IMU 驱动私有头文件
 *
 * 参照: Zephyr drivers/sensor/tdk/mpu6050/mpu6050.h
 *       stm32_app/drivers/include/icm20608.h (已验证硬件)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_ICM20608_ICM20608_H_
#define ZEPHYR_DRIVERS_SENSOR_ICM20608_ICM20608_H_

#include <zephyr/drivers/i2c.h>

/* ---- 寄存器地址 ---- */
#define ICM20608_REG_WHO_AM_I      0x75 /* 身份寄存器 */
#define ICM20608_REG_SMPLRT_DIV    0x19 /* 采样率分频 */
#define ICM20608_REG_GYRO_CONFIG   0x1B /* 陀螺仪配置 */
#define ICM20608_REG_ACCEL_CONFIG  0x1C /* 加速度计配置 */
#define ICM20608_REG_PWR_MGMT_1    0x6B /* 电源管理 1 */
#define ICM20608_REG_PWR_MGMT_2    0x6C /* 电源管理 2 */
#define ICM20608_REG_ACCEL_XOUT_H  0x3B /* 加速度计 X 高字节 (起始) */

/* WHO_AM_I — ICM-20608-G = 0xAF, ICM-20608-D = 0xAE */
#define ICM20608_G_CHIP_ID         0xAF
#define ICM20608_D_CHIP_ID         0xAE

/* 电源管理位 */
#define ICM20608_RESET_BIT         0x80 /* 设备复位 (bit 7) */
#define ICM20608_CLOCK_AUTO        0x01 /* 自动选择最佳时钟源 */
#define ICM20608_WAKE_VALUE        0x00 /* 唤醒 (清除 SLEEP) */

/* 量程移位 (与 MPU6050 兼容) */
#define ICM20608_ACCEL_FS_SHIFT    3
#define ICM20608_GYRO_FS_SHIFT     3

/* 陀螺仪灵敏度 x10 (dps / LSB) — 用于避免浮点 */
static const uint16_t icm20608_gyro_sensitivity_x10[] = {
	1310, /* +/-250dps  */
	655,  /* +/-500dps  */
	328,  /* +/-1000dps */
	164   /* +/-2000dps */
};

/* 驱动配置 */
struct icm20608_config {
	struct i2c_dt_spec bus;
	uint8_t accel_fs;     /* 加速度计量程 (g) */
	uint16_t gyro_fs;     /* 陀螺仪量程 (dps) */
	uint8_t smplrt_div;   /* 采样率分频 */
};

/* 驱动运行时数据 */
struct icm20608_data {
	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	uint16_t accel_sensitivity_shift;

	int16_t temp;

	int16_t gyro_x;
	int16_t gyro_y;
	int16_t gyro_z;
	uint16_t gyro_sensitivity_x10;
};

#endif /* ZEPHYR_DRIVERS_SENSOR_ICM20608_ICM20608_H_ */
