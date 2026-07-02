# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

# 查找 Zephyr 构建系统
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# 项目名称
project(stm32_ai)

# 添加应用源文件 — 每类功能对应独立文件
target_sources(app PRIVATE
	src/main.c
	src/led_thread.c
	src/aht10_thread.c
	src/ap3216c_thread.c
	src/icm20608_thread.c
	src/display_thread.c    # ★ LVGL 动画显示
	# src/norflash_thread.c  # ★ NOR Flash 自检 — 已关闭
)

# 添加传感器驱动源文件 — 每个传感器对应独立驱动目录
target_sources(app PRIVATE
	drivers/sensor/aht10/aht10.c
	drivers/sensor/ap3216c/ap3216c.c
	drivers/sensor/icm20608/icm20608.c
)

# 添加头文件搜索路径
target_include_directories(app PRIVATE
	include
	drivers/sensor/aht10
	drivers/sensor/ap3216c
	drivers/sensor/icm20608
)
