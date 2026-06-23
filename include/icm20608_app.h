/*
 * icm20608_app.h — ICM20608 应用线程公共接口
 */

#ifndef INCLUDE_ICM20608_APP_H_
#define INCLUDE_ICM20608_APP_H_

#include <zephyr/kernel.h>

/** ICM20608 IMU 采集线程入口 */
extern void icm20608_thread_entry(void *p1, void *p2, void *p3);

/** ICM20608 线程 ID (类型为 const) */
extern const k_tid_t icm20608_tid;

#endif /* INCLUDE_ICM20608_APP_H_ */
