#ifndef __MY_FFT_H_
#define __MY_FFT_H_                    /* 自定义FFT头文件重复包含保护宏。 */

#include "g_Inc.h"

#define MY_FFT_MIN_SIZE       32768UL  /* 自定义FFT允许的最小点数。 */
#define MY_FFT_COMPLEX_SCALE      2UL  /* 每个复数由实部和虚部两个float交错保存。 */

/**
 * @brief 自定义FFT函数的执行状态。
 */
typedef enum
{
	MY_FFT_STATUS_OK = 0U,             /**< FFT初始化或运算成功。 */
	MY_FFT_STATUS_NULL_POINTER,         /**< 输入句柄或数组指针为空。 */
	MY_FFT_STATUS_INVALID_SIZE,         /**< FFT点数小于32768或不是2的整数次幂。 */
	MY_FFT_STATUS_BUFFER_TOO_SMALL      /**< 输入或输出数组容量小于要求。 */
} MyFFT_StatusTypeDef;

/**
 * @brief 自定义FFT句柄。
 * @note FFT_Size决定本次FFT处理的点数，必须不小于32768且为2的整数次幂。
 */
typedef struct
{
	uint32_t FFT_Size;                 /**< FFT点数。 */
} MyFFT_HandleTypeDef;

/**
 * @brief 初始化自定义FFT句柄。
 * @param[out] FFT_Handle 待初始化的FFT句柄。
 * @param[in] FFT_Size FFT点数，必须不小于32768且为2的整数次幂。
 * @return 返回MY_FFT_STATUS_OK表示初始化成功，否则返回对应错误状态。
 */
MyFFT_StatusTypeDef MyFFT_Init(MyFFT_HandleTypeDef *FFT_Handle,
                               uint32_t FFT_Size);

/**
 * @brief 对实数电压数组执行自定义点数的正向基2 FFT。
 * @param[in] FFT_Handle 已由MyFFT_Init初始化的FFT句柄。
 * @param[in] Voltage_Input 输入电压数组；函数直接使用其数值，不转换量程、不减直流。
 * @param[in] Input_Count Voltage_Input可用的float元素数，必须不小于FFT_Size。
 * @param[out] Complex_Output FFT复数输出，按实部、虚部交错保存，共需2×FFT_Size个float。
 * @param[in] Output_Count Complex_Output可用的float元素数，必须不小于2×FFT_Size。
 * @return 返回MY_FFT_STATUS_OK表示运算成功，否则返回对应错误状态。
 * @note 正向FFT不进行归一化，计算定义为X[k]=Σx[n]e^(-j2πkn/N)。
 * @note Voltage_Input与Complex_Output可以是同一地址，但禁止部分重叠。
 */
MyFFT_StatusTypeDef MyFFT_ForwardRealF32(
    const MyFFT_HandleTypeDef *FFT_Handle,
    const float *Voltage_Input, uint32_t Input_Count,
    float *Complex_Output, uint32_t Output_Count);

#endif
