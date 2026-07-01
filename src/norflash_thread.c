/*
 * norflash_thread.c — W25Q128 NOR Flash 自检（QSPI indirect 模式）
 *
 * 验证 QSPI 间接模式驱动正常 → 擦除/写入/API读取测试通过 → 永久休眠
 *
 * ★ 重要: 不开 MEMMAP, 使用 QSPI indirect 模式（普通读/写/擦除指令）
 *   STM32L4 QUADSPI 只有一套控制逻辑, indirect 和 memory-mapped 模式互斥
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(NOR_TASK, LOG_LEVEL_INF);

/* [设备树] W25Q128 NOR Flash 节点 */
#define FLASH_NODE       DT_NODELABEL(w25q128jv)
/* [配置] 测试偏移地址 — 选靠后的扇区, 避免覆盖 slot1 分区数据 */
#define TEST_OFFSET      0x00803000
/* [配置] W25Q128 扇区大小 4KB */
#define TEST_SECTOR_SIZE 4096

static const struct device *flash_dev = DEVICE_DT_GET(FLASH_NODE);

void norflash_thread_entry(void *p1, void *p2, void *p3)
{
	int ret;
	uint8_t write_data[] = "Hello Pandora! W25Q128 Indirect Mode OK.";
	uint8_t read_data[sizeof(write_data)];

	LOG_INF("Nor Flash 测试线程已启动 (QSPI indirect 模式)");

	/*
	 * ★ 绕过 Zephyr 设备框架限制:
	 *   MCUboot 先初始化 QSPI 后, App 的设备框架跳过本驱动的 init 调用
	 *   解决: 通过 device 结构体中的函数指针手动调用 init
	 */
	if (!device_is_ready(flash_dev)) {
		LOG_WRN("QSPI not ready, calling init manually...");

		/* device.ops.init 在 struct device 偏移 20 字节处 */
		int (*init_fn)(const struct device *) =
			*(int (**)(const struct device *))(
				(char *)flash_dev + 20);

		int rc = init_fn(flash_dev);
		if (rc < 0) {
			LOG_ERR("Manual QSPI init failed: %d", rc);
			return;
		}

		if (!device_is_ready(flash_dev)) {
			LOG_ERR("Still not ready after manual init!");
			return;
		}
		LOG_INF("Manual QSPI init succeeded!");
	}
	LOG_INF("找到设备: %s", flash_dev->name);

	while (1) {
		LOG_INF("========== 开始新一轮 Nor Flash 测试 (indirect 模式) ==========");

		/* [操作] 擦除扇区 — QSPI indirect 擦除指令 */
		LOG_INF("1. 正在擦除偏移地址 0x%x 处的扇区 (indirect 模式)...", TEST_OFFSET);
		ret = flash_erase(flash_dev, TEST_OFFSET, TEST_SECTOR_SIZE);
		if (ret != 0) {
			LOG_ERR("擦除失败! 错误码: %d", ret);
			k_msleep(5000);
			continue;
		}
		LOG_INF("擦除成功！");

		/* [操作] 写入测试数据 — QSPI indirect 页编程指令 */
		LOG_INF("2. 正在写入数据: \"%s\" (indirect 模式)...", write_data);
		ret = flash_write(flash_dev, TEST_OFFSET, write_data, sizeof(write_data));
		if (ret != 0) {
			LOG_ERR("写入失败! 错误码: %d", ret);
			k_msleep(5000);
			continue;
		}
		LOG_INF("写入成功！");

		/* [操作] API 读取校验 — QSPI indirect 读指令 */
		LOG_INF("3. 正在通过 flash_read() API 读取校验 (indirect 模式)...");
		ret = flash_read(flash_dev, TEST_OFFSET, read_data, sizeof(read_data));
		if (ret != 0) {
			LOG_ERR("读取失败! 错误码: %d", ret);
			k_msleep(5000);
			continue;
		}
		LOG_INF("API 读取到的内容: \"%s\"", read_data);

		/* [校验] 比较写入和读取的数据 — 纯 API 方式, 不依赖 MEMMAP */
		if (memcmp(read_data, write_data, sizeof(write_data)) == 0) {
			LOG_INF("成功：API 读取数据与写入数据完全一致！");
			LOG_INF(">>> QSPI indirect 模式验证通过，线程永久休眠 <<<");
			k_sleep(K_FOREVER);
		} else {
			LOG_WRN("警告：校验不一致，5秒后重试...");
			k_msleep(5000);
		}
	}
}

K_THREAD_DEFINE(nor_tid, 2048,
		norflash_thread_entry, NULL, NULL, NULL,
		17, 0, 0);
