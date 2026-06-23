/*
 * aht10.c — AHT10 温湿度传感器 Zephyr 驱动
 *
 * 参照模板: Zephyr drivers/sensor/aosong/dht20/dht20.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT aosong_aht10

#include "aht10.h"

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(aht10, CONFIG_SENSOR_LOG_LEVEL);

/**
 * @brief 触发测量并读取原始数据帧
 */
static int aht10_read_sample(const struct aht10_config *cfg,
			      struct aht10_data *data)
{
	uint8_t tx_buf[3] = { AHT10_MEASURE_CMD, AHT10_MEASURE_DATA0, AHT10_MEASURE_DATA1 };
	uint8_t rx_buf[AHT10_FRAME_LEN];
	int rc;

	rc = i2c_write_dt(&cfg->bus, tx_buf, sizeof(tx_buf));
	if (rc < 0) {
		LOG_ERR("Failed to start measurement.");
		return rc;
	}

	k_msleep(AHT10_MEASURE_WAIT_MS);

	rc = i2c_read_dt(&cfg->bus, rx_buf, sizeof(rx_buf));
	if (rc < 0) {
		LOG_ERR("Failed to read data.");
		return rc;
	}

	if (rx_buf[0] & AHT10_STATUS_BUSY) {
		LOG_WRN("Sensor measurement not ready.");
		return -EBUSY;
	}

	/*
	 * 解析数据帧 (大端序)
	 * [0]:State [1]:Hum[19:12] [2]:Hum[11:4]
	 * [3]:Hum[3:0]|Temp[19:16] [4]:Temp[15:8] [5]:Temp[7:0]
	 */
	data->rh_sample = ((uint32_t)rx_buf[1] << 12) |
			  ((uint32_t)rx_buf[2] << 4)  |
			  ((uint32_t)rx_buf[3] >> 4);

	data->t_sample = (((uint32_t)rx_buf[3] & 0x0F) << 16) |
			 ((uint32_t)rx_buf[4] << 8) |
			  (uint32_t)rx_buf[5];

	return 0;
}

/**
 * @brief 温度转换: raw → struct sensor_value
 *
 * T(°C) = (raw / 2^20) * 200 - 50
 */
static void aht10_temp_convert(struct sensor_value *val, uint32_t raw)
{
	int32_t micro_c;

	micro_c = ((int64_t)raw * 1000000 * 200) / BIT(20) - 50 * 1000000;

	val->val1 = micro_c / 1000000;
	val->val2 = micro_c % 1000000;
}

/**
 * @brief 湿度转换: raw → struct sensor_value
 *
 * H(%) = (raw / 2^20) * 100
 */
static void aht10_rh_convert(struct sensor_value *val, uint32_t raw)
{
	int32_t micro_rh;

	micro_rh = ((int64_t)raw * 1000000 * 100) / BIT(20);

	val->val1 = micro_rh / 1000000;
	val->val2 = micro_rh % 1000000;
}

/* ---- Zephyr 传感器 API ---- */

static int aht10_sample_fetch(const struct device *dev,
			       enum sensor_channel chan)
{
	struct aht10_data *data = dev->data;
	const struct aht10_config *cfg = dev->config;

	if (chan != SENSOR_CHAN_ALL &&
	    chan != SENSOR_CHAN_AMBIENT_TEMP &&
	    chan != SENSOR_CHAN_HUMIDITY) {
		return -ENOTSUP;
	}

	return aht10_read_sample(cfg, data);
}

static int aht10_channel_get(const struct device *dev,
			      enum sensor_channel chan,
			      struct sensor_value *val)
{
	const struct aht10_data *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_AMBIENT_TEMP:
		aht10_temp_convert(val, data->t_sample);
		break;
	case SENSOR_CHAN_HUMIDITY:
		aht10_rh_convert(val, data->rh_sample);
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static DEVICE_API(sensor, aht10_driver_api) = {
	.sample_fetch = aht10_sample_fetch,
	.channel_get = aht10_channel_get,
};

/* ---- 初始化 ---- */

static int aht10_init(const struct device *dev)
{
	const struct aht10_config *cfg = dev->config;
	uint8_t cmd[3] = { AHT10_INIT_CMD, AHT10_INIT_DATA0, AHT10_INIT_DATA1 };
	int rc;

	if (!i2c_is_ready_dt(&cfg->bus)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	k_msleep(AHT10_POWER_ON_WAIT_MS);

	rc = i2c_write_dt(&cfg->bus, cmd, sizeof(cmd));
	if (rc < 0) {
		LOG_ERR("Init command failed (ret=%d)", rc);
		return rc;
	}

	LOG_DBG("AHT10 initialized");
	return 0;
}

/* ---- 设备实例化 (参照 DHT20 驱动模式) ---- */

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define AHT10_DEFINE(n)								\
	static struct aht10_data aht10_data_##n;				\
										\
	static const struct aht10_config aht10_config_##n = {			\
		.bus = I2C_DT_SPEC_INST_GET(n),				\
	};									\
										\
	SENSOR_DEVICE_DT_INST_DEFINE(n, aht10_init, NULL,			\
				      &aht10_data_##n, &aht10_config_##n,	\
				      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,\
				      &aht10_driver_api);

DT_INST_FOREACH_STATUS_OKAY(AHT10_DEFINE)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
