#ifndef __MY_MATH_H_
#define __MY_MATH_H_                     /* My_Math头文件重复包含保护宏。 */

#include "g_Inc.h"

#define Alpha                          0.1f       /* 一阶低通滤波器的新数据权重。 */
#define ADC_DC_OFFSET              21845.0f       /* 16位ADC原始数据的直流中点偏置。 */
#define ADC_FULL_SCALE_VALUE       65535.0f       /* 16位ADC原始数据的满量程数值。 */
#define ADC_REFERENCE_VOLTAGE          3.3f       /* ADC参考电压，单位为V。 */

#define SIGNAL_COMPONENT_COUNT          3U        /* 结构体保存的最大频率分量数：一个基波和两个谐波。 */
#define SIGNAL_FUNDAMENTAL_INDEX        0U        /* 基波在各分量数组中的下标。 */
#define SIGNAL_HARMONIC_FIRST_INDEX     1U        /* 按频率升序排列的第一个已检测谐波下标。 */
#define SIGNAL_HARMONIC_SECOND_INDEX    2U        /* 按频率升序排列的第二个已检测谐波下标。 */
#define SIGNAL_COMPONENT_MIN_RATIO    0.01f       /* 有效频率分量相对最强频谱峰的最小强度比例。 */

/**
 * @brief 输入信号的时域和频域分析结果。
 * @note 三个数组元素依次表示：基波、第一个已检测谐波、第二个已检测谐波。
 * @note 谐波按频率从低到高保存；只有一个谐波时，第二个谐波槽位保持为0。
 */
typedef struct SignalPara
{
	uint16_t Index[SIGNAL_COMPONENT_COUNT];       /**< 各分量在FFT数组中的频点下标。 */
	uint32_t Freq[SIGNAL_COMPONENT_COUNT];        /**< 各分量的频率，单位为Hz。 */
	float ComponentUpp[SIGNAL_COMPONENT_COUNT];   /**< 各分量对应的峰峰值，单位为V。 */
	float SpectrumAmp[SIGNAL_COMPONENT_COUNT];    /**< 各分量未经归一化的频谱强度。 */
	float Phase[SIGNAL_COMPONENT_COUNT];          /**< 各分量的相位，单位为度。 */
	float Upp;                                    /**< 总输入信号的峰峰值，单位为V。 */
	float Urms;                                   /**< 总输入信号相对0V的有效值，单位为V。 */
} SignalPara_t;

/**
 * @brief 将ADC时域数据转换为复数输入并执行CFFT和正频率幅值计算。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄，fftLen决定FFT点数N。
 * @param[in] ADC_Buff ADC原始采样数组，至少包含N个uint16_t元素。
 * @param[out] ADC_Voltage 去除直流偏置后的电压数组，至少包含N个float元素。
 * @param[out] FFT_IN FFT复数结果数组，容量至少为2N个float；实部和虚部交错保存。
 * @param[out] FFT_OUT 正频率幅值数组，容量至少为N/2个float。
 * @param[in] Sample_Size ADC_Buff中的有效采样点数，必须不小于N。
 * @return 无；句柄或缓冲区无效时不写入输出。
 */
void Math_FFT(const arm_cfft_instance_f32 *Cfft_Handler,
              const uint16_t *ADC_Buff, float *ADC_Voltage,
              float *FFT_IN, float *FFT_OUT, uint16_t Sample_Size);

/**
 * @brief 对浮点数组进行降序冒泡排序。
 * @param[in,out] Buff 待排序数组；函数会原地修改其元素顺序。
 * @param[in] Size Buff中的元素数量。
 * @return 无。
 */
void Math_BubbleSort(float *Buff, uint16_t Size);

/**
 * @brief 从幅值谱和FFT复数结果中提取基波及最多两个谐波分量。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄，fftLen决定FFT点数N。
 * @param[in] FFT_OUT 正频率幅值谱数组，至少包含N/2个float元素。
 * @param[in] FFT_IN FFT复数结果数组，至少包含2N个float元素。
 * @param[in] Complex_Size FFT_IN中的float元素数量，必须不小于2N。
 * @return 返回完整频谱分析结果；输入无效时所有字段为0。
 */
SignalPara_t Math_AnalyzeSpectrum(const arm_cfft_instance_f32 *Cfft_Handler,
                                  const float *FFT_OUT, const float *FFT_IN,
                                  uint16_t Complex_Size);

/**
 * @brief 从正频率幅值谱中寻找幅值最大的两个频点。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄，fftLen决定FFT点数N。
 * @param[in] FFT_OUT 正频率幅值数组，至少包含N/2个float元素。
 * @param[out] pFirst 第一信号参数，其基波频率最终不小于pSecond的基波频率。
 * @param[out] pSecond 第二信号参数。
 * @param[in] Spectrum_Size FFT_OUT中的float元素数量，必须不小于N/2。
 * @return 无；结果通过pFirst和pSecond输出。
 * @note pFirst和pSecond必须指向不同的结构体对象。
 */
void Math_FindMaxAndSecondMaxFreqAndAmp(
    const arm_cfft_instance_f32 *Cfft_Handler, const float *FFT_OUT,
    SignalPara_t *pFirst, SignalPara_t *pSecond, uint16_t Spectrum_Size);

/**
 * @brief 计算总输入信号的峰峰值和有效值。
 * @param[in] ADC_Voltage 已去除直流偏置的电压数组。
 * @param[out] pSignal 分析结果；函数只更新Upp和Urms字段。
 * @param[in] Sample_Size ADC_Voltage中的有效采样点数。
 * @return 无；输入无效时Upp和Urms清零。
 */
void Math_CalculateTimeDomainParameters(const float *ADC_Voltage,
                                        SignalPara_t *pSignal,
                                        uint16_t Sample_Size);

/**
 * @brief 计算指定阶次谐波与基波的频谱强度比值。
 * @param[in] Signal 已完成频谱分析的信号结构体。
 * @param[in] Harmonic_Order 待查找的谐波阶次，必须不小于2。
 * @return 找到对应阶次时返回谐波与基波的频谱强度比，否则返回0。
 */
float Math_GetHarmonicRatio(const SignalPara_t *Signal,
                            uint8_t Harmonic_Order);

/**
 * @brief 对当前值执行一阶低通滤波。
 * @param[in] Last_val 上一次滤波输出值。
 * @param[in] Curr_val 当前输入值。
 * @return 返回按Alpha加权得到的新滤波值。
 */
float Math_LowPassFilter(float Last_val, float Curr_val);

/**
 * @brief 将双通道ADC交错采样数据去除直流偏置后拆分为两路CFFT复数输入数组。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄，fftLen决定每路点数N。
 * @param[in] Mix_Buff 双通道交错ADC数组，布局为通道1、通道2依次交替。
 * @param[out] FeedbackFFT_INFirst 第一通道去直流复数输入，容量至少为2N个float。
 * @param[out] FeedbackFFT_INSecond 第二通道去直流复数输入，容量至少为2N个float。
 * @param[in] Sample_Size Mix_Buff中的uint16_t元素数，必须不小于2N。
 * @return 无。
 */
void Math_Feedback_SeparateChannel(
    const arm_cfft_instance_f32 *Cfft_Handler, const uint16_t *Mix_Buff,
    float *FeedbackFFT_INFirst, float *FeedbackFFT_INSecond,
    uint16_t Sample_Size);

/**
 * @brief 对单路复数输入执行CFFT，并提取基波及最多两个谐波分量。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄，fftLen决定FFT点数N。
 * @param[out] pSignal 统一信号频谱分析结果输出。
 * @param[in,out] FFT_IN 容量至少为2N个float的复数输入；函数会原地覆盖为FFT结果。
 * @param[out] FFT_OUT 正频率幅值数组，容量至少为N/2个float。
 * @param[in] Sample_Size FFT_IN中的float元素数，必须不小于2N。
 * @return 无；输入无效时所有字段清零。
 */
void Math_Feedback_FFTAndExtractPhase(
    const arm_cfft_instance_f32 *Cfft_Handler, SignalPara_t *pSignal,
    float *FFT_IN, float *FFT_OUT, uint16_t Sample_Size);

#endif
