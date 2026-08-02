#include "g_Inc.h"

volatile uint8_t adc1_done = 0U; /* ADC1 DMA采满4096点完成标志。 */

arm_cfft_instance_f32 cfft_handler_512;   /* 512点FFT句柄 */
arm_cfft_instance_f32 cfft_handler_1024;  /* 1024点FFT句柄 */
arm_cfft_instance_f32 cfft_handler_2048;  /* 2048点FFT句柄 */
arm_cfft_instance_f32 cfft_handler_4096;  /* 4096点FFT句柄 */

arm_status G_Var_InitCfftHandlers(void)
{
	arm_status Status;

	Status = arm_cfft_init_f32(&cfft_handler_512, SampleSize_512);
	if (Status != ARM_MATH_SUCCESS)
	{
		return Status;
	}

	Status = arm_cfft_init_f32(&cfft_handler_1024, SampleSize_1024);
	if (Status != ARM_MATH_SUCCESS)
	{
		return Status;
	}

	Status = arm_cfft_init_f32(&cfft_handler_2048, SampleSize_2048);
	if (Status != ARM_MATH_SUCCESS)
	{
		return Status;
	}

	return arm_cfft_init_f32(&cfft_handler_4096, SampleSize_4096);
}

uint16_t ADC_RawBuff[SampleSize_4096] = {0U}; /* ADC原始采样值，32768为直流中点。 */
float ADC_Voltage[SampleSize_4096] = {0.0f}; /* 去除直流偏置后的电压值，单位为V。 */
SignalPara_t InputSignalPara = {0}; /* Ques1生成的输入信号时域和频域分析结果。 */
