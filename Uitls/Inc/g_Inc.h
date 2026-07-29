#ifndef __G_INC_H_
#define __G_INC_H_                      /* 用户模块总头文件重复包含保护宏。 */

/* C标准库 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* CMSIS-DSP数字信号处理库 */
#include "arm_math.h"

/* CubeMX生成的外设接口 */
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "gpio.h"
#include "spi.h"
#include "stm32h7xx_hal_conf.h"
#include "stm32h7xx_it.h"
#include "tim.h"
#include "usart.h"

/* 用户功能模块 */
#include "g_var.h"
#include "Command.h"
#include "My_Math.h"
#include "My_FFT.h"
#include "CallBack.h"
#include "Justfloat.h"
#include "fpga_capture.h"
#include "signal_analyzer.h"

#endif
