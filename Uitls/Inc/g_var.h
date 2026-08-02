#ifndef __G_VAR_H_
#define __G_VAR_H_                      /* 全局变量头文件重复包含保护宏。 */

#include "g_Inc.h"

#define SampleSize_512                512U       /* 512点FFT及采样数量。 */
#define SampleSize_1024              1024U       /* 1024点FFT及采样数量。 */
#define SampleSize_2048              2048U       /* 2048点FFT及采样数量。 */
#define SampleSize_4096              4096U       /* 4096点FFT及采样数量。 */

#define TIM3_KERNEL_CLOCK_HZ  240000000UL       /* TIM3内核时钟频率，单位为Hz。 */
#define TIM3_PERIOD_TICKS           117U         /* TIM3一次更新事件包含的计数周期。 */
#define ADC1_SAMPLE_RATE_HZ  		2048000UL /* ADC1采样率，单位为Hz。 */

struct SignalPara;                              /**< 信号分析结构体前置声明，完整定义见My_Math.h。 */

extern volatile uint8_t adc1_done;               /**< ADC1 DMA采满4096点完成标志。 */

extern arm_cfft_instance_f32 cfft_handler_512;   /**< 512点CFFT句柄。 */
extern arm_cfft_instance_f32 cfft_handler_1024;  /**< 1024点CFFT句柄。 */
extern arm_cfft_instance_f32 cfft_handler_2048;  /**< 2048点CFFT句柄。 */
extern arm_cfft_instance_f32 cfft_handler_4096;  /**< 4096点CFFT句柄。 */

/**
 * @brief 初始化512、1024、2048和4096点CFFT句柄。
 * @note 无输入参数。
 * @return ARM_MATH_SUCCESS表示全部初始化成功，否则返回首次出现的CMSIS-DSP错误状态。
 */
arm_status G_Var_InitCfftHandlers(void);

extern uint16_t ADC_RawBuff[SampleSize_4096];   /**< ADC原始采样数据。 */
extern float ADC_Voltage[SampleSize_4096];      /**< 去除直流偏置后的ADC电压数据。 */
extern struct SignalPara InputSignalPara;        /**< Ques1生成的输入信号分析结果。 */

#endif
