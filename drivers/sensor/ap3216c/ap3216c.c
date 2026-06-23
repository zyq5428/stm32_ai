/*
 * ap3216c.c — AP3216C 环境光/接近传感器 Zephyr 驱动
 *
 * 参照模板: Zephyr drivers/sensor/vishay/vcnl4040/vcnl4040.c
 * 硬件验证: stm32_app/drivers/ap3216c_drv.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT liteon_ap3216c

#include "ap3216c.h"

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ap3216c, CONFIG_SENSOR_LOG_LEVEL);

/**
 * @brief 读取一对寄存器 (小端序 16-bit)
 */
static int ap3216c_read_reg_pair(const struct ap3216c_config *cfg,
				  uint8_t reg, uint16_t *val)
{
	uint8_t buf[2];
	int rc;

	rc = i2c_write_read_dt(&cfg->bus, &reg, 1, buf, 2);
	if (rc < 0) {
		return rc;
	}

	*val = ((uint16_t)buf[1] << 8) | buf[0];
	return 0;
}

/**
 * @brief 读取全部传感器数据 (ALS + IR + PS)
 *
 * 分别读取三个通道，PS 使用 stm32_app 验证过的 10-bit 提取公式
 */
static int ap3216c_read_all(const struct ap3216c_config *cfg,
			     struct ap3216c_data *data)
{
	uint16_t raw;
	int rc;

	/* IR: 0x0A-0x0B (16-bit 小端) */
	rc = ap3216c_read_reg_pair(cfg, AP3216C_REG_IR_DATA_L, &raw);
	if (rc < 0) {
		LOG_ERR("Failed to read IR data.");
		return rc;
	}
	data->ir = raw;

	/* ALS: 0x0C-0x0D (16-bit 小端) */
	rc = ap3216c_read_reg_pair(cfg, AP3216C_REG_ALS_DATA_L, &raw);
	if (rc < 0) {
		LOG_ERR("Failed to read ALS data.");
		return rc;
	}
	data->als = raw;

	/* PS: 0x0E-0x0F (10-bit, 特殊位布局)
	 *
	 * stm32_app 验证的提取公式:
	 *   ps = (raw & 0x000F) | (((raw >> 8) & 0x3F) << 4)
	 *   = low_byte[3:0] | high_byte[5:0] << 4
	 *   = 10-bit 接近传感器原始值
	 */
	rc = ap3216c_read_reg_pair(cfg, AP3216C_REG_PS_DATA_L, &raw);
	if (rc < 0) {
		LOG_ERR("Failed to read PS data.");
		return rc;
	}
	data->ps = (raw & 0x000F) | (((raw >> 8) & 0x3F) << 4);

	return 0;
}

/* ---- Zephyr 传感器 API ---- */

static int ap3216c_sample_fetch(const struct device *dev,
				 enum sensor_channel chan)
{
	struct ap3216c_data *data = dev->data;
	const struct ap3216c_config *cfg = dev->config;

	if (chan != SENSOR_CHAN_ALL &&
	    chan != SENSOR_CHAN_LIGHT &&
	    chan != SENSOR_CHAN_PROX &&
	    chan != SENSOR_CHAN_IR) {
		return -ENOTSUP;
	}

	return ap3216c_read_all(cfg, data);
}

static int ap3216c_channel_get(const struct device *dev,
				enum sensor_channel chan,
				struct sensor_value *val)
{
	const struct ap3216c_data *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_LIGHT:
		/* ALS: 原始 16-bit 环境光值 */
		val->val1 = data->als;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_PROX:
		/* PS: 10-bit 接近传感器值, 值越大物体越近 */
		val->val1 = data->ps;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_IR:
		/* IR: 16-bit 红外值 */
		val->val1 = data->ir;
		val->val2 = 0;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static DEVICE_API(sensor, ap3216c_driver_api) = {
	.sample_fetch = ap3216c_sample_fetch,
	.channel_get = ap3216c_channel_get,
};

/* ---- 初始化 ---- */

static int ap3216c_init(const struct device *dev)
{
	const struct ap3216c_config *cfg = dev->config;
	int rc;

	if (!i2c_is_ready_dt(&cfg->bus)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	/*
	 * 1. 软件复位 — 写入 {SYS_CONF, SW_RESET}
	 * ★ 寄存器型 I2C 设备必须先发送寄存器地址，再发送数据
	 */
	{
		uint8_t buf[2] = { AP3216C_REG_SYS_CONF, AP3216C_MODE_SW_RESET };
		rc = i2c_write_dt(&cfg->bus, buf, sizeof(buf));
		if (rc < 0) {
			LOG_ERR("Reset failed (ret=%d)", rc);
			return rc;
		}
	}

	k_msleep(15);

	/*
	 * 2. 配置为 ALS + PS+IR 同时工作模式 — 写入 {SYS_CONF, ALS_AND_PS}
	 */
	{
		uint8_t buf[2] = { AP3216C_REG_SYS_CONF, AP3216C_MODE_ALS_AND_PS };
		rc = i2c_write_dt(&cfg->bus, buf, sizeof(buf));
		if (rc < 0) {
			LOG_ERR("Config mode failed (ret=%d)", rc);
			return rc;
		}
	}

	/* 3. 等待传感器启动 (参照 stm32_app: 至少 300ms) */
	k_msleep(300);

	LOG_DBG("AP3216C initialized");
	return 0;
}

/* ---- 设备实例化 ---- */

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define AP3216C_DEFINE(n)							\
	static struct ap3216c_data ap3216c_data_##n;				\
										\
	static const struct ap3216c_config ap3216c_config_##n = {		\
		.bus = I2C_DT_SPEC_INST_GET(n),				\
	};									\
										\
	SENSOR_DEVICE_DT_INST_DEFINE(n, ap3216c_init, NULL,			\
				      &ap3216c_data_##n, &ap3216c_config_##n,	\
				      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,\
				      &ap3216c_driver_api);

DT_INST_FOREACH_STATUS_OKAY(AP3216C_DEFINE)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
