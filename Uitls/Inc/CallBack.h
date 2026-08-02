#ifndef __CALLBACK_H_
#define __CALLBACK_H_                   /* 回调模块头文件重复包含保护宏。 */

#include "g_Inc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将单个字符通过USART3发送，用作printf的底层重定向函数。
 * @param[in] ch 待发送字符的整数表示。
 * @param[in] stream 标准库文件流指针；当前实现不使用该参数。
 * @return 返回输入字符ch。
 */
int fputc(int ch, FILE *stream);

/**
 * @brief HAL定时器周期到达回调函数。
 * @param[in] htim 触发回调的定时器句柄；当前仅识别TIM3。
 * @return 无。
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

/**
 * @brief HAL ADC DMA传输完成回调函数。
 * @param[in] hadc 完成DMA传输的ADC句柄；当其为ADC1时将adc1_done置1。
 * @return 无；输出通过全局标志adc1_done体现。
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);

/**
 * @brief HAL UART空闲接收事件回调函数。
 * @param[in] huart 产生接收事件的UART句柄；当前仅处理USART3。
 * @param[in] size 本次DMA接收到的有效字节数。
 * @return 无；USART3数据将交给命令接收模块处理。
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif
