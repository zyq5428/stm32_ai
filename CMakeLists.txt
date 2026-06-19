# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

# 查找 Zephyr 构建系统
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# 项目名称
project(stm32_ai)

# 添加源文件 — 每类功能对应独立文件
target_sources(app PRIVATE
	src/main.c
	src/led_thread.c
)
