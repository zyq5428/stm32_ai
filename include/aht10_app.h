/*
 * aht10_app.h — AHT10 应用线程公共接口
 *
 * 仅声明线程入口函数，供外部引用 (如 shell 命令)。
 */

#ifndef INCLUDE_AHT10_APP_H_
#define INCLUDE_AHT10_APP_H_

#include <zephyr/kernel.h>

/** AHT10 温湿度采集线程入口 */
extern void aht10_thread_entry(void *p1, void *p2, void *p3);

/** AHT10 线程 ID (由 K_THREAD_DEFINE 定义, 类型为 const) */
extern const k_tid_t aht10_tid;

#endif /* INCLUDE_AHT10_APP_H_ */
