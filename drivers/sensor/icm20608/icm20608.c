/*
 * icm20608.c — ICM20608 六轴 IMU Zephyr 驱动
 *
 * 参照模板: Zephyr drivers/sensor/tdk/mpu6050/mpu6050.c
 * 硬件验证: stm32_app/drivers/icm20608_drv.c (WHO_AM_I=0xAF/0xAE)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT invensense_icm20608

#include "icm20608.h"

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(icm20608, CONFIG_SENSOR_LOG_LEVEL);

/**
 * @brief 加速度计转换: raw → struct sensor_value (m/s^2)
 *
 * 参照 MPU6050 的 mpu6050_convert_accel()
 */
static void icm20608_convert_accel(struct sensor_value *val, int16_t raw_val,
				    uint16_t sensitivity_shift)
{
	int64_t conv_val;

	conv_val = ((int64_t)raw_val * SENSOR_G) >> sensitivity_shift;
	val->val1 = conv_val / 1000000;
	val->val2 = conv_val % 1000000;
}

/**
 * @brief 陀螺仪转换: raw → struct sensor_value (rad/s)
 *
 * 参照 MPU6050 的 mpu6050_convert_gyro()
 */
static void icm20608_convert_gyro(struct sensor_value *val, int16_t raw_val,
				   uint16_t sensitivity_x10)
{
	int64_t conv_val;

	conv_val = ((int64_t)raw_val * SENSOR_PI * 10) /
		   (sensitivity_x10 * 180U);
	val->val1 = conv_val / 1000000;
	val->val2 = conv_val % 1000000;
}

/**
 * @brief 温度转换: raw → struct sensor_value (°C)
 *
 * ICM-20608 温度公式: T(°C) = raw / 326.8 + 25.0
 * (不同于 MPU6050 的 raw/340 + 36.53)
 */
static void icm20608_convert_temp(struct sensor_value *val, int16_t raw_val)
{
	int64_t tmp_val;

	/* micro_C = raw * 1000000 / 326.8 + 25.0 * 1000000
	 *        = raw * 10000000 / 3268 + 25000000
	 */
	tmp_val = ((int64_t)raw_val * 10000000) / 3268 + 25000000;
	val->val1 = tmp_val / 1000000;
	val->val2 = tmp_val % 1000000;
}

/* ---- Zephyr 传感器 API ---- */

static int icm20608_sample_fetch(const struct device *dev,
				  enum sensor_channel chan)
{
	struct icm20608_data *drv_data = dev->data;
	const struct icm20608_config *cfg = dev->config;
	int16_t buf[7];

	if (i2c_burst_read_dt(&cfg->bus, ICM20608_REG_ACCEL_XOUT_H,
			      (uint8_t *)buf, 14) < 0) {
		LOG_ERR("Failed to read data sample.");
		return -EIO;
	}

	drv_data->accel_x = sys_be16_to_cpu(buf[0]);
	drv_data->accel_y = sys_be16_to_cpu(buf[1]);
	drv_data->accel_z = sys_be16_to_cpu(buf[2]);
	drv_data->temp    = sys_be16_to_cpu(buf[3]);
	drv_data->gyro_x  = sys_be16_to_cpu(buf[4]);
	drv_data->gyro_y  = sys_be16_to_cpu(buf[5]);
	drv_data->gyro_z  = sys_be16_to_cpu(buf[6]);

	return 0;
}

static int icm20608_channel_get(const struct device *dev,
				 enum sensor_channel chan,
				 struct sensor_value *val)
{
	struct icm20608_data *drv_data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_ACCEL_XYZ:
		icm20608_convert_accel(val, drv_data->accel_x,
					drv_data->accel_sensitivity_shift);
		icm20608_convert_accel(val + 1, drv_data->accel_y,
					drv_data->accel_sensitivity_shift);
		icm20608_convert_accel(val + 2, drv_data->accel_z,
					drv_data->accel_sensitivity_shift);
		break;
	case SENSOR_CHAN_ACCEL_X:
		icm20608_convert_accel(val, drv_data->accel_x,
					drv_data->accel_sensitivity_shift);
		break;
	case SENSOR_CHAN_ACCEL_Y:
		icm20608_convert_accel(val, drv_data->accel_y,
					drv_data->accel_sensitivity_shift);
		break;
	case SENSOR_CHAN_ACCEL_Z:
		icm20608_convert_accel(val, drv_data->accel_z,
					drv_data->accel_sensitivity_shift);
		break;
	case SENSOR_CHAN_GYRO_XYZ:
		icm20608_convert_gyro(val, drv_data->gyro_x,
				       drv_data->gyro_sensitivity_x10);
		icm20608_convert_gyro(val + 1, drv_data->gyro_y,
				       drv_data->gyro_sensitivity_x10);
		icm20608_convert_gyro(val + 2, drv_data->gyro_z,
				       drv_data->gyro_sensitivity_x10);
		break;
	case SENSOR_CHAN_GYRO_X:
		icm20608_convert_gyro(val, drv_data->gyro_x,
				       drv_data->gyro_sensitivity_x10);
		break;
	case SENSOR_CHAN_GYRO_Y:
		icm20608_convert_gyro(val, drv_data->gyro_y,
				       drv_data->gyro_sensitivity_x10);
		break;
	case SENSOR_CHAN_GYRO_Z:
		icm20608_convert_gyro(val, drv_data->gyro_z,
				       drv_data->gyro_sensitivity_x10);
		break;
	case SENSOR_CHAN_DIE_TEMP:
		icm20608_convert_temp(val, drv_data->temp);
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static DEVICE_API(sensor, icm20608_driver_api) = {
	.sample_fetch = icm20608_sample_fetch,
	.channel_get = icm20608_channel_get,
};

/* ---- 初始化 (参照 stm32_app 已验证的初始化序列) ---- */

int icm20608_init(const struct device *dev)
{
	struct icm20608_data *drv_data = dev->data;
	const struct icm20608_config *cfg = dev->config;
	uint8_t whoami;
	uint8_t i;

	if (!device_is_ready(cfg->bus.bus)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	/* 1. 先读取 WHO_AM_I 验证芯片身份 (复位前读取!) */
	if (i2c_reg_read_byte_dt(&cfg->bus, ICM20608_REG_WHO_AM_I,
				 &whoami) < 0) {
		LOG_ERR("Failed to read WHO_AM_I.");
		return -EIO;
	}

	if (whoami != ICM20608_G_CHIP_ID && whoami != ICM20608_D_CHIP_ID) {
		LOG_ERR("Invalid WHO_AM_I: 0x%02X (expected 0xAF or 0xAE)", whoami);
		return -EINVAL;
	}

	/* 2. 设备复位 */
	if (i2c_reg_write_byte_dt(&cfg->bus, ICM20608_REG_PWR_MGMT_1,
				  ICM20608_RESET_BIT) < 0) {
		LOG_ERR("Failed to reset device.");
		return -EIO;
	}

	k_msleep(100);

	/* 3. 唤醒设备: 自动选择最佳时钟源 (0x01) */
	if (i2c_reg_write_byte_dt(&cfg->bus, ICM20608_REG_PWR_MGMT_1,
				  ICM20608_CLOCK_AUTO) < 0) {
		LOG_ERR("Failed to wake device.");
		return -EIO;
	}

	k_msleep(10);

	/* 4. 启用加速度计和陀螺仪所有轴 */
	if (i2c_reg_write_byte_dt(&cfg->bus, ICM20608_REG_PWR_MGMT_2,
				  0x00) < 0) {
		LOG_ERR("Failed to enable axes.");
		return -EIO;
	}

	/* 5. 配置加速度计量程 (参照 MPU6050 init 逻辑) */
	for (i = 0U; i < 4; i++) {
		if (BIT(i + 1) == cfg->accel_fs) {
			break;
		}
	}

	if (i == 4U) {
		LOG_ERR("Invalid accel full-scale range.");
		return -EINVAL;
	}

	if (i2c_reg_write_byte_dt(&cfg->bus, ICM20608_REG_ACCEL_CONFIG,
				  i << ICM20608_ACCEL_FS_SHIFT) < 0) {
		LOG_ERR("Failed to write accel config.");
		return -EIO;
	}

	drv_data->accel_sensitivity_shift = 14 - i;

	/* 6. 配置陀螺仪量程 */
	for (i = 0U; i < 4; i++) {
		if (BIT(i) * 250 == cfg->gyro_fs) {
			break;
		}
	}

	if (i == 4U) {
		LOG_ERR("Invalid gyro full-scale range.");
		return -EINVAL;
	}

	if (i2c_reg_write_byte_dt(&cfg->bus, ICM20608_REG_GYRO_CONFIG,
				  i << ICM20608_GYRO_FS_SHIFT) < 0) {
		LOG_ERR("Failed to write gyro config.");
		return -EIO;
	}

	drv_data->gyro_sensitivity_x10 = icm20608_gyro_sensitivity_x10[i];

	/* 7. 设置采样率分频 (默认 0x09 → 100Hz, 参照 stm32_app) */
	if (i2c_reg_write_byte_dt(&cfg->bus, ICM20608_REG_SMPLRT_DIV,
				  cfg->smplrt_div) < 0) {
		LOG_ERR("Failed to write sample rate divider.");
		return -EIO;
	}

	k_msleep(100); /* 等待稳定 */

	LOG_DBG("ICM20608 initialized (WHO_AM_I=0x%02X)", whoami);
	return 0;
}

/* ---- 设备实例化 (参照 MPU6050 驱动模式) ---- */

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define ICM20608_DEFINE(inst)								\
	static struct icm20608_data icm20608_data_##inst;				\
											\
	static const struct icm20608_config icm20608_config_##inst = {			\
		.bus = I2C_DT_SPEC_INST_GET(inst),					\
		.accel_fs = DT_INST_PROP(inst, accel_fs),				\
		.gyro_fs = DT_INST_PROP(inst, gyro_fs),				\
		.smplrt_div = DT_INST_PROP(inst, smplrt_div),				\
	};										\
											\
	SENSOR_DEVICE_DT_INST_DEFINE(inst, icm20608_init, NULL,				\
				      &icm20608_data_##inst, &icm20608_config_##inst,	\
				      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,		\
				      &icm20608_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ICM20608_DEFINE)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
