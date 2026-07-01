# Pandora STM32L4 IoT Board — 完整硬件信息（原理图校正版）

> 数据来源:
> - Pandora_STM32L4_Board_V2.4_SCH.pdf (原理图 — 主要依据)
> - STM32L475xx Datasheet.pdf (DS10969 Rev 4, LQFP100 引脚编号)
> - RT-Thread BSP 驱动源码 (MSP/DRV 交叉验证)
> - 提取日期: 2026-06-19
> - 最后校正: 2026-06-19 (根据原理图实际连接纠正 GPIO 和 PIN 序号)

---

## 1. 主控 MCU

| 参数 | 规格 |
|------|------|
| **型号** | STM32L475VET6 |
| **封装** | LQFP100 (14×14mm, 0.5mm pitch) |
| **内核** | ARM Cortex-M4 + FPU + DSP, 80MHz |
| **Flash** | 512 KB (双Bank, RWW) |
| **SRAM** | 128 KB (含32KB硬件奇偶校验) |
| **供电** | 1.71V ~ 3.6V |

---

## 2. 完整外设引脚分配

> **格式**: `功能信号 = GPIO端口引脚 (LQFP100封装PIN号)`

---

### 2.1 QSPI Flash — W25Q128 (16MB NOR Flash)

| 信号 | GPIO | LQFP100 PIN | AF | 说明 |
|------|------|-----|-----|------|
| QSPI_CLK | PE10 | 41 | AF10 | 时钟 |
| QSPI_NCS | PE11 | 42 | AF10 | 片选 |
| QSPI_BK1_IO0 | PE12 | 43 | AF10 | 数据线0 |
| QSPI_BK1_IO1 | PE13 | 44 | AF10 | 数据线1 |
| QSPI_BK1_IO2 | PE14 | 45 | AF10 | 数据线2 |
| QSPI_BK1_IO3 | PE15 | 46 | AF10 | 数据线3 |

### 2.2 MicroSD / TF 卡槽 (SPI1 驱动)

| 信号 | GPIO | LQFP100 PIN | AF |
|------|------|------------|-----|
| SPI1_SCK | PA5 | 30 | AF5 |
| SPI1_MISO | PA6 | 31 | AF5 |
| SPI1_MOSI | PA7 | 32 | AF5 |
| **SD_CS** | **PC3** | **18** | GPIO OUT |

### 2.3 WiFi — AP6181 (SDMMC1)

| 信号 | GPIO | LQFP100 PIN | AF | 说明 |
|------|------|------------|-----|------|
| **SDMMC1_D0** | **PC8** | **65** | AF12 | 数据线0 |
| **SDMMC1_D1** | **PC9** | **66** | AF12 | 数据线1 |
| **SDMMC1_D2** | **PC10** | **78** | AF12 | 数据线2 |
| **SDMMC1_D3** | **PC11** | **79** | AF12 | 数据线3 |
| **SDMMC1_CK** | **PC12** | **80** | AF12 | 时钟 |
| **SDMMC1_CMD** | **PD2** | **83** | AF12 | 命令 |
| **WIFI_REG_ON** | **PD1** | **82** | GPIO OUT | 模块电源使能 (高有效) |
| **WIFI_INT** | **PC5** | **34** | GPIO IN | Host 唤醒中断 |

### 2.4 音频系统 — ES8388 编解码器

#### SAI1 数字音频接口 (I2S)

| 信号 | GPIO | AF | 说明 |
|------|------|-----|------|
| SAI1_MCLK_A | PE2 | AF13 | 主时钟 |
| SAI1_SD_B | PE3 | AF13 | ADC 数据输入 |
| SAI1_FS_A (LRCK) | PE4 | AF13 | 帧同步 / 左右时钟 |
| SAI1_SCK_A (SCLK) | PE5 | AF13 | 位时钟 |
| SAI1_SD_A | PE6 | AF13 | DAC 数据输出 |

#### I2C3 配置接口

| 信号 | GPIO | LQFP100 PIN | 说明 |
|------|------|------------|------|
| **I2C3_SCL** | **PC0** | **15** | ES8388 寄存器配置时钟 |
| **I2C3_SDA** | **PC1** | **16** | ES8388 寄存器配置数据 |

#### 电源控制

| 信号 | GPIO | LQFP100 PIN | 说明 |
|------|------|------------|------|
| **AUDIO_PWR** | **PA15** | **77** | ES8388 PA 模拟部分电源使能 |

#### 模拟音频信号

| 信号 | 连接 | 说明 |
|------|------|------|
| MIC_P / MIC_N | ES8388 LIN1/RIN1 | 板载 6022 咪头差分输入 |
| LOUT1 / ROUT1 | 3.5mm 耳机座 | 立体声耳机输出 |

---

### 2.5 传感器子系统

> **关键**: 多个传感器共用 I2C 总线，且 AHT10 的 SDA 与其它设备重叠。
> **强烈建议**: 使用 GPIO 软件模拟 I2C (bit-bang)，为每组设备创建独立的软件 I2C 总线。

#### 2.5.1 6轴 IMU — ICM20608 (I2C, 地址 0x68)

| 信号 | GPIO | LQFP100 PIN | 说明 |
|------|------|------------|------|
| I2C_SCL | **PC0** | **15** | 共享 I2C 时钟 |
| I2C_SDA | **PC1** | **16** | 共享 I2C 数据 |
| **ICM_INT** | **PD0** | **81** | 传感器中断输出 |

#### 2.5.2 温湿度传感器 — AHT10 (I2C, 地址 0x38)

| 信号 | GPIO | LQFP100 PIN | 说明 |
|------|------|------------|------|
| **I2C_SCL** | **PD6** | **87** | AHT10 独立 SCL |
| I2C_SDA | **PC1** | **16** | ⚠️ 与 I2C3_SDA 重叠 |

#### 2.5.3 环境光/接近传感器 — AP3216C (I2C, 地址 0x1E)

| 信号 | GPIO | LQFP100 PIN | 说明 |
|------|------|------------|------|
| I2C_SCL | **PC0** | **15** | 共享 I2C 时钟 |
| I2C_SDA | **PC1** | **16** | 共享 I2C 数据 |

---

### 2.6 电机驱动 — TC214B / L9110S

> **注**: TC214B 是马达驱动 IC (非触摸控制器)。

| 信号 | GPIO | LQFP100 PIN | 说明 |
|------|------|------------|------|
| **MOTOR_A** | **PA1** | **24** | PWM 驱动 A 相 |
| **MOTOR_B** | **PA0** | **23** | PWM 驱动 B 相 |

---

### 2.7 TFT LCD 显示屏 (1.3" 240×240, ST7789V2 驱动)

| 信号 | GPIO | 功能 | 说明 |
|------|------|------|------|
| SPI3_SCK | PB3 | AF6 | SPI 时钟 |
| SPI3_MOSI | PB5 | AF6 | SPI 数据 (仅发送) |
| LCD_CS | PD7 | GPIO OUT | 片选 |
| LCD_DC (RS) | PB4 | GPIO OUT | 命令/数据选择 |
| LCD_RESET | PB6 | GPIO OUT | 复位 |
| LCD_PWR | PB7 | TIM4_CH2 | 背光 PWM 控制 |

---

### 2.8 RGB LED (共阳极)

| 颜色 | GPIO | 说明 |
|------|------|------|
| LED_R (红) | PE7 | 低电平亮 (共阳极) |
| LED_G (绿) | PE8 | 低电平亮 |
| LED_B (蓝) | PE9 | 低电平亮, TIM1_CH1 支持 PWM |

---

### 2.9 按键

| 按键 | GPIO | LQFP100 PIN | 触发方式 |
|------|------|------------|---------|
| KEY0 (K0) | PD10 | — | 低电平 |
| KEY1 (K1) | PD9 | — | 低电平 |
| KEY2 (K2) | PD8 | — | 低电平 |
| WK_UP | PC13 | 7 | 高电平 (带唤醒功能) |
| RESET | NRST | 14 | 低电平系统复位 |

---

### 2.10 蜂鸣器 (有源)

| 信号 | GPIO | LQFP100 PIN | 说明 |
|------|------|------------|------|
| **BEEP** | PB2 | **37** | 高电平触发，三极管驱动 |

---

### 2.11 红外

| 信号 | GPIO | 说明 |
|------|------|------|
| IR_TX (发射) | PB0 | 红外发射管 |
| IR_RX (接收) | PB1 | LF0038GKLL-1 接收头 |

---

### 2.12 USB OTG FS

| 信号 | GPIO | AF | 说明 |
|------|------|-----|------|
| OTG_FS_DM | PA11 | AF10 | USB 数据- |
| OTG_FS_DP | PA12 | AF10 | USB 数据+ |
| OTG_FS_ID | PA10 | — | 主/从模式识别 |

> **说明**: PA10给了USART1_RX，所以连接器实际未连接。

---

### 2.13 调试与串口

#### ST-Link V2.1 (STM32F103C8T6)

| 信号 | GPIO (STM32L475) | 功能 |
|------|-----------------|------|
| SWCLK | PA14 | SWD 时钟 |
| SWDIO | PA13 | SWD 数据 |
| NRST | NRST (PIN14) | 系统复位 |

#### USART1 (ST-Link 虚拟串口)

| 信号 | GPIO | AF |
|------|------|-----|
| USART1_TX | PA9 | AF7 |
| USART1_RX | PA10 | AF7 |

#### USART2 (扩展串口)

| 信号 | GPIO | AF |
|------|------|-----|
| USART2_TX | PA2 | AF7 |
| USART2_RX | PA3 | AF7 |

---

### 2.14 扩展模块接口

#### NRF24L01 无线模块 (与 ICM20608 共享 SPI2)

| 信号 | GPIO | 说明 |
|------|------|------|
| SPI2_SCK | PB13 | 时钟 (AF5) |
| SPI2_MISO | PB14 | 数据输入 (AF5) |
| SPI2_MOSI | PB15 | 数据输出 (AF5) |
| NRF_CS | 排针引出 | 片选 |
| NRF_CE | 排针引出 | 芯片使能 |
| NRF_IRQ | 排针引出 | 中断 |

#### ESP8266 WiFi 模块接口

通过排针引出 UART，AT 指令控制。

#### ENC28J60 以太网模块接口

通过排针引出 SPI 接口。

#### 独立 IO 扩展排针

部分不与板载外设冲突的独立 IO:
PA4, PA8, PB10-PB15, PC2, PC4, PC6-PC7, PD12-PD15 等。

---

## 3. I2C 总线拓扑与地址

由于 SDA 存在重叠 (PC1 被多个设备共用)，**强烈推荐使用 GPIO 软件模拟 I2C**：

| 逻辑总线 | 设备 | SCL | SDA | 7-bit 地址 |
|---------|------|-----|-----|-----------|
| I2C-A | ES8388 (音频) | PC0 (15) | PC1 (16) | 0x10 (播放) / 0x11 (录音) |
| I2C-A | ICM20608 (IMU) | PC0 (15) | PC1 (16) | 0x68 |
| I2C-A | AP3216C (光感) | PC0 (15) | PC1 (16) | 0x1E |
| I2C-B | AHT10 (温湿度) | PD6 (87) | PC1 (16) | 0x38 |

> **说明**: I2C-A 组 (ES8388 + ICM20608 + AP3216C) 共享 PC0/PC1。I2C-B (AHT10) 使用独立 SCL (PD6)，但 SDA 仍与 PC1 重叠。采用 GPIO 模拟 I2C 后，可以为每组创建独立的虚拟 I2C 总线实例，彻底解决 SDA 冲突问题。

---

## 4. 时钟系统

| 时钟 | 频率 | LQFP100 PIN | 用途 |
|------|------|------------|------|
| HSE 晶振 | 8 MHz | PH0 (PIN12) / PH1 (PIN13) | 主系统时钟源 |
| LSE 晶振 | 32.768 kHz | PC14 (PIN8) / PC15 (PIN9) | RTC 实时时钟 |
| HSI RC | 16 MHz (±1%) | 内部 | 快速启动 |
| MSI RC | 100kHz ~ 48MHz | 内部 | 低功耗运行 |

**PLL 配置**:
- PLL 主: 系统时钟 80MHz
- PLLSAI1: ADC 时钟, USB 48MHz

---

## 5. 电源架构

| 电源域 | 说明 |
|------|------|
| VDD 数字供电 | 3.3V, 多个 VDD/VSS 引脚 |
| VDDA 模拟供电 | 3.3V, PIN22 |
| VREF+ / VREF- | ADC/DAC 参考电压, PIN21/PIN20 |
| VBAT 电池备份 | RTC + 备份寄存器, PIN6 |
| VDDUSB | USB 收发器独立供电 |
| VCC_3V3A | 音频模拟部分独立供电 |
| VCC_WIFI | WiFi 模块独立供电 |

**板载 LDO**:
| 芯片 | 输出 | 电流 |
|------|------|------|
| RT9193-3.3 | 3.3V | 500mA |
| RT9013-3.3 | 3.3V | 500mA |

**输入**: USB Micro 5V 或外部 5V 排针。

---

## 6. 板级信息

| 项目 | 值 |
|------|------|
| 开发板名称 | Pandora IoT Board (潘多拉) |
| 制造商 | 正点原子 (ALIENTEK) |
| PCB 版本 | V2.4 |
| 主控 | STM32L475VET6 (LQFP100) |
| 调试器 | STM32F103C8T6 (ST-Link V2.1) |
| 默认 RTOS | RT-Thread |

---

## 7. 快速引脚速查卡

```
=== 存储 ===
QSPI Flash:    PE10(CLK)  PE11(NCS)  PE12(IO0)  PE13(IO1)  PE14(IO2)  PE15(IO3)
TF卡 SPI1:     PA5(SCK)   PA6(MISO)  PA7(MOSI)  PC3/18(CS)

=== WiFi (AP6181 @ SDMMC1) ===
SDIO 数据:     PC8/65(D0) PC9/66(D1) PC10/67(D2) PC11/68(D3)
SDIO 控制:     PC12/69(CK) PD2/70(CMD)
WiFi 管理:     PD1/82(REG_ON) PC5/34(INT)

=== 音频 (ES8388 @ SAI1 + I2C3) ===
SAI1 音频:     PE2(MCLK)  PE3(SD_B)  PE4(FS/LRCK)  PE5(SCK)  PE6(SD_A)
I2C3 配置:     PC0/15(SCL)  PC1/16(SDA)
电源控制:      PA15/77(AUDIO_PWR)

=== 传感器 (共用 I2C, 推荐 GPIO 模拟 I2C) ===
ICM20608 IMU:  PC0/15(SCL)  PC1/16(SDA)  PD0/81(INT)
AHT10 温湿度:  PD6/87(SCL)  PC1/16(SDA 重叠!)
AP3216C 光感:  PC0/15(SCL)  PC1/16(SDA)

=== LCD (ST7789V2 @ SPI3) ===
显示接口:      PB3(SCK)  PB5(MOSI)  PD7(CS)  PB4(DC)  PB6(RESET)  PB7(BL/PWM)

=== 用户交互 ===
RGB LED:       PE7(R) PE8(G) PE9(B/BL)   [共阳极, 0=亮]
按键:          PD10(K0)  PD9(K1)  PD8(K2)  PC13/7(WK_UP)  NRST/14(RESET)
蜂鸣器:        PE7/37   (与LED_R共用)
红外:          PB0(TX)  PB1(RX)
电机:          PA1/24(MOTOR_A)  PA0/23(MOTOR_B)

=== 通信接口 ===
USART1(VCP):   PA9(TX)  PA10(RX)
USART2(扩展):  PA2(TX)  PA3(RX)
USB OTG:       PA11(DM)  PA12(DP)  PA10(ID)
SPI2(IMU):     PB13(SCK)  PB14(MISO)  PB15(MOSI)

=== 调试与时钟 ===
SWD:           PA14(SWCLK)  PA13(SWDIO)
晶振:          PH0/12(HSE_IN)  PH1/13(HSE_OUT)  [8MHz]
               PC14/8(LSE_IN)  PC15/9(LSE_OUT)  [32.768KHz]
```
