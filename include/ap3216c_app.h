/*
 * ap3216c_app.h — AP3216C 应用线程公共接口
 */

#ifndef INCLUDE_AP3216C_APP_H_
#define INCLUDE_AP3216C_APP_H_

#include <zephyr/kernel.h>

/** AP3216C 环境光/接近采集线程入口 */
extern void ap3216c_thread_entry(void *p1, void *p2, void *p3);

/** AP3216C 线程 ID (类型为 const) */
extern const k_tid_t ap3216c_tid;

#endif /* INCLUDE_AP3216C_APP_H_ */
