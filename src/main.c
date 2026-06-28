/*
 * main.c — STM32 AI 应用主程序
 * 功能: 打印开发板信息，释放 CPU 给工作线程
 * 硬件: Pandora STM32L475 IoT Board
 */

/* [头文件] Zephyr 内核和日志 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "sensor_data.h"

/* [日志] 注册 main 模块 */
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* [全局] 传感器共享数据实例 — 所有传感器线程写入，显示线程读取 */
struct sensor_data g_sensor_data;

/* [入口] 主函数 — 只打印信息，业务逻辑交给独立线程 */
int main(void)
{
	/* [信息] 输出当前开发板型号 */
	LOG_INF("应用启动完成, Board: %s", CONFIG_BOARD);

	/* [休眠] 永久休眠，释放 CPU 和栈空间给工作线程 */
	while (1) {
		k_sleep(K_FOREVER);
	}
	return 0;
}
