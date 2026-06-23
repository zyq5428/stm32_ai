/*
 * ap3216c.h — AP3216C 环境光/接近传感器驱动私有头文件
 *
 * 参照: Zephyr drivers/sensor/vishay/vcnl4040/vcnl4040.h
 *       stm32_app/drivers/include/ap3216c.h (已验证硬件)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_AP3216C_AP3216C_H_
#define ZEPHYR_DRIVERS_SENSOR_AP3216C_AP3216C_H_

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

/* 寄存器地址 (与 stm32_app 一致) */
#define AP3216C_REG_SYS_CONF      0x00 /* 系统配置 */
#define AP3216C_REG_IR_DATA_L     0x0A /* IR 数据低字节 */
#define AP3216C_REG_IR_DATA_H     0x0B /* IR 数据高字节 */
#define AP3216C_REG_ALS_DATA_L    0x0C /* ALS 数据低字节 */
#define AP3216C_REG_ALS_DATA_H    0x0D /* ALS 数据高字节 */
#define AP3216C_REG_PS_DATA_L     0x0E /* PS 数据低字节 */
#define AP3216C_REG_PS_DATA_H     0x0F /* PS 数据高字节 */

/* 工作模式 (与 stm32_app 一致) */
#define AP3216C_MODE_POWER_DOWN   0x00 /* 掉电 */
#define AP3216C_MODE_ALS          0x01 /* 仅 ALS */
#define AP3216C_MODE_PS_IR        0x02 /* 仅 PS+IR */
#define AP3216C_MODE_ALS_AND_PS   0x03 /* ALS + PS+IR 同时工作 */
#define AP3216C_MODE_SW_RESET     0x04 /* 软件复位 */

/* 驱动配置 */
struct ap3216c_config {
	struct i2c_dt_spec bus;
};

/* 驱动运行时数据 */
struct ap3216c_data {
	uint16_t als; /* 环境光强度 (16-bit) */
	uint16_t ir;  /* 红外强度 (16-bit) */
	uint16_t ps;  /* 接近距离 (10-bit) */
};

#endif /* ZEPHYR_DRIVERS_SENSOR_AP3216C_AP3216C_H_ */
