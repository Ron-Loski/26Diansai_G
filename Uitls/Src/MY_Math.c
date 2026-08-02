#include "g_Inc.h"

/**
 * @brief 从CFFT句柄中读取并校验FFT点数。
 * @param[in] Cfft_Handler 待读取的CFFT句柄。
 * @return 返回512、1024、2048或4096；句柄为空或点数不受支持时返回0。
 */
static uint16_t Math_GetFFTSize(const arm_cfft_instance_f32 *Cfft_Handler)
{
	if (Cfft_Handler == NULL)
	{
		return 0U;
	}

	switch (Cfft_Handler->fftLen)
	{
		case SampleSize_512:
		case SampleSize_1024:
		case SampleSize_2048:
		case SampleSize_4096:
			return Cfft_Handler->fftLen;
		default:
			return 0U;
	}
}

/**
 * @brief 将频点索引换算为实际频率。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄。
 * @param[in] Index 待换算的频点索引。
 * @return 返回以Hz为单位的频率；句柄无效时返回0。
 */
static uint32_t Math_BinToFrequency(
    const arm_cfft_instance_f32 *Cfft_Handler, uint16_t Index)
{
	uint16_t FFT_Size = Math_GetFFTSize(Cfft_Handler);

	if (FFT_Size == 0U)
	{
		return 0U;
	}

	return (uint32_t)(((uint64_t)ADC1_SAMPLE_RATE_HZ * Index) / FFT_Size);
}

/**
 * @brief 将未经归一化的单边频谱强度换算为对应正弦分量的峰峰值。
 * @param[in] SpectrumAmp CMSIS-DSP输出的频谱强度。
 * @param[in] FFT_Size FFT点数。
 * @return 返回对应分量的峰峰值；FFT_Size为0时返回0。
 */
static float Math_SpectrumAmpToUpp(float SpectrumAmp, uint16_t FFT_Size)
{
	if (FFT_Size == 0U)
	{
		return 0.0f;
	}

	return (4.0f * SpectrumAmp) / (float)FFT_Size;
}

/**
 * @brief 从单边幅值谱中选出最多三个显著局部峰，并按频率升序排列。
 * @param[in] FFT_OUT 正频率幅值谱数组。
 * @param[in] Spectrum_Size FFT_OUT中的有效元素数。
 * @param[out] PeakBins 频点下标数组，容量至少为SIGNAL_COMPONENT_COUNT。
 * @return 返回有效频率分量数，范围为0至SIGNAL_COMPONENT_COUNT。
 * @note 小于最强频谱峰SIGNAL_COMPONENT_MIN_RATIO倍的局部峰会被忽略。
 */
static uint8_t Math_FindDominantPeakBins(const float *FFT_OUT,
                                         uint16_t Spectrum_Size,
                                         uint16_t *PeakBins)
{
	float PeakAmps[SIGNAL_COMPONENT_COUNT] = {0.0f};
	uint8_t PeakCount = 0U;

	if ((FFT_OUT == NULL) || (PeakBins == NULL) || (Spectrum_Size < 2U))
	{
		return 0U;
	}

	for (uint8_t Slot = 0U; Slot < SIGNAL_COMPONENT_COUNT; Slot++)
	{
		PeakBins[Slot] = 0U;
	}

	/* 忽略直流频点，只将不小于相邻频点的幅值视为局部峰。 */
	for (uint16_t Index = 1U; Index < Spectrum_Size; Index++)
	{
		float Amp = FFT_OUT[Index];

		if (((Index > 1U) && (Amp <= FFT_OUT[Index - 1U])) ||
		    (((Index + 1U) < Spectrum_Size) &&
		     (Amp < FFT_OUT[Index + 1U])))
		{
			continue;
		}

		for (uint8_t Rank = 0U; Rank < SIGNAL_COMPONENT_COUNT; Rank++)
		{
			if (Amp > PeakAmps[Rank])
			{
				for (uint8_t Shift = SIGNAL_COMPONENT_COUNT - 1U;
				     Shift > Rank; Shift--)
				{
					PeakAmps[Shift] = PeakAmps[Shift - 1U];
					PeakBins[Shift] = PeakBins[Shift - 1U];
				}

				PeakAmps[Rank] = Amp;
				PeakBins[Rank] = Index;
				break;
			}
		}
	}

	if (PeakAmps[0] <= 0.0f)
	{
		return 0U;
	}

	while ((PeakCount < SIGNAL_COMPONENT_COUNT) &&
	       (PeakBins[PeakCount] != 0U) &&
	       (PeakAmps[PeakCount] >=
	        (PeakAmps[0] * SIGNAL_COMPONENT_MIN_RATIO)))
	{
		PeakCount++;
	}

	/* 最低频率分量作为基波，其余有效分量按频率升序保存为两个谐波。 */
	for (uint8_t Left = 0U; Left < PeakCount; Left++)
	{
		for (uint8_t Right = Left + 1U; Right < PeakCount; Right++)
		{
			if (PeakBins[Left] > PeakBins[Right])
			{
				uint16_t Temp = PeakBins[Left];
				PeakBins[Left] = PeakBins[Right];
				PeakBins[Right] = Temp;
			}
		}
	}

	return PeakCount;
}

/**
 * @brief 将一个频点的频域参数写入统一信号结构体的指定分量位置。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄。
 * @param[in] FFT_OUT 正频率频谱强度数组。
 * @param[in] FFT_IN FFT复数结果数组；传入NULL时不计算相位。
 * @param[in,out] Signal 待写入的统一信号结构体。
 * @param[in] Component_Index 分量数组下标，0至2依次代表基波和两个已检测谐波。
 * @param[in] Bin FFT频点下标。
 * @param[in] Frequency 分量频率，单位为Hz。
 * @return 无。
 */
static void Math_SetSpectrumComponent(
    const arm_cfft_instance_f32 *Cfft_Handler, const float *FFT_OUT,
    const float *FFT_IN, SignalPara_t *Signal, uint8_t Component_Index,
    uint16_t Bin, uint32_t Frequency)
{
	uint16_t FFT_Size = Math_GetFFTSize(Cfft_Handler);

	if ((FFT_OUT == NULL) || (Signal == NULL) ||
	    (Component_Index >= SIGNAL_COMPONENT_COUNT) ||
	    (FFT_Size == 0U) || (Bin >= (FFT_Size / 2U)))
	{
		return;
	}

	Signal->Index[Component_Index] = Bin;
	Signal->Freq[Component_Index] = Frequency;
	Signal->SpectrumAmp[Component_Index] = FFT_OUT[Bin];
	Signal->ComponentUpp[Component_Index] =
	    Math_SpectrumAmpToUpp(FFT_OUT[Bin], FFT_Size);

	if (FFT_IN != NULL)
	{
		float Real = FFT_IN[2U * Bin];
		float Imag = FFT_IN[2U * Bin + 1U];

		Signal->Phase[Component_Index] =
		    atan2f(Imag, Real) * 180.0f / PI;
	}
}

/**
 * @brief 将ADC原始数据转换为去直流的电压数据，并执行复数FFT和幅值计算。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄，fftLen决定FFT点数N。
 * @param[in] ADC_Buff ADC原始采样数组，至少包含N个uint16_t元素。
 * @param[out] ADC_Voltage 去除32768直流偏置后的电压数组，至少包含N个float元素。
 * @param[out] FFT_IN 容量至少为2N个float；调用后保存FFT复数结果。
 * @param[out] FFT_OUT 容量至少为N/2个float；保存正频率幅值。
 * @param[in] Sample_Size ADC_Buff中的有效采样点数，必须不小于N。
 * @return 无；句柄、长度或缓冲区无效时不执行计算。
 */
void Math_FFT(const arm_cfft_instance_f32 *Cfft_Handler,
              const uint16_t *ADC_Buff, float *ADC_Voltage,
              float *FFT_IN, float *FFT_OUT, uint16_t Sample_Size)
{
	uint16_t FFT_Size;

	if ((Cfft_Handler == NULL) || (ADC_Buff == NULL) ||
	    (ADC_Voltage == NULL) || (FFT_IN == NULL) || (FFT_OUT == NULL))
	{
		return;
	}

	FFT_Size = Math_GetFFTSize(Cfft_Handler);
	if ((FFT_Size == 0U) || (Sample_Size < FFT_Size))
	{
		return;
	}

	for (uint16_t Index = 0U; Index < FFT_Size; Index++)
	{
		float Voltage =
		    (((float)ADC_Buff[Index] - ADC_DC_OFFSET) /
		     ADC_FULL_SCALE_VALUE) * ADC_REFERENCE_VOLTAGE;

		ADC_Voltage[Index] = Voltage;
		FFT_IN[2U * Index] = Voltage;
		FFT_IN[2U * Index + 1U] = 0.0f;
	}

	arm_cfft_f32(Cfft_Handler, FFT_IN, 0U, 1U);
	arm_cmplx_mag_f32(FFT_IN, FFT_OUT, FFT_Size / 2U);
}

/**
 * @brief 对浮点数组执行降序冒泡排序。
 * @param[in,out] Buff 待排序数组；函数会原地改变元素顺序。
 * @param[in] Size Buff中的元素数量。
 * @return 无；数组为空或少于两个元素时不执行操作。
 */
void Math_BubbleSort(float *Buff, uint16_t Size)
{
	float temp;

	if ((Buff == NULL) || (Size < 2U))
	{
		return;
	}

	for (uint16_t i = 0U; i < (Size - 1U); i++)
	{
		for (uint16_t j = 0U; j < (Size - 1U - i); j++)
		{
			if (Buff[j] < Buff[j + 1U])
			{
				temp = Buff[j];
				Buff[j] = Buff[j + 1U];
				Buff[j + 1U] = temp;
			}
		}
	}
}

/**
 * @brief 从幅值谱和FFT复数结果中提取基波及最多两个谐波分量。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄，fftLen决定FFT点数N。
 * @param[in] FFT_OUT 正频率幅值谱数组，至少包含N/2个float元素。
 * @param[in] FFT_IN FFT复数结果数组，至少包含2N个float元素。
 * @param[in] Complex_Size FFT_IN中的float元素数量，必须不小于2N。
 * @return 返回频谱分析结果；输入无效或未找到有效分量时各字段为0。
 * @note 最多选择三个显著局部峰并按频率升序保存，最低频率分量作为基波。
 */
SignalPara_t Math_AnalyzeSpectrum(const arm_cfft_instance_f32 *Cfft_Handler,
                                  const float *FFT_OUT, const float *FFT_IN,
                                  uint16_t Complex_Size)
{
	SignalPara_t Signal = {0};
	uint16_t FFT_Size = Math_GetFFTSize(Cfft_Handler);
	uint16_t PeakBins[SIGNAL_COMPONENT_COUNT] = {0U};
	uint8_t PeakCount;

	if ((FFT_OUT == NULL) || (FFT_IN == NULL) || (FFT_Size == 0U) ||
	    (Complex_Size < ((uint32_t)FFT_Size * 2U)))
	{
		return Signal;
	}

	PeakCount = Math_FindDominantPeakBins(
	    FFT_OUT, FFT_Size / 2U, PeakBins);
	for (uint8_t Component = 0U; Component < PeakCount; Component++)
	{
		uint16_t Bin = PeakBins[Component];

		Math_SetSpectrumComponent(
		    Cfft_Handler, FFT_OUT, FFT_IN, &Signal, Component,
		    Bin, Math_BinToFrequency(Cfft_Handler, Bin));
	}

	return Signal;
}

/**
 * @brief 从正频率幅值谱中寻找幅值最大的两个频点。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄，fftLen决定FFT点数N。
 * @param[in] FFT_OUT 正频率幅值数组，至少包含N/2个float元素。
 * @param[out] pFirst 第一信号参数；最终其基波频率不小于pSecond。
 * @param[out] pSecond 第二信号参数。
 * @param[in] Spectrum_Size FFT_OUT中的float元素数量，必须不小于N/2。
 * @return 无；输入无效时两个输出结构体均清零。
 */
void Math_FindMaxAndSecondMaxFreqAndAmp(
    const arm_cfft_instance_f32 *Cfft_Handler, const float *FFT_OUT,
    SignalPara_t *pFirst, SignalPara_t *pSecond, uint16_t Spectrum_Size)
{
	uint16_t FFT_Size = Math_GetFFTSize(Cfft_Handler);
	uint16_t FirstIndex = 0U;
	uint16_t SecondIndex = 0U;
	float FirstAmp = 0.0f;
	float SecondAmp = 0.0f;

	if ((pFirst == NULL) || (pSecond == NULL) || (pFirst == pSecond))
	{
		return;
	}

	*pFirst = (SignalPara_t){0};
	*pSecond = (SignalPara_t){0};

	if ((FFT_OUT == NULL) || (FFT_Size == 0U) ||
	    (Spectrum_Size < (FFT_Size / 2U)))
	{
		return;
	}

	for (uint16_t Index = 1U; Index < (FFT_Size / 2U); Index++)
	{
		if (FFT_OUT[Index] > FirstAmp)
		{
			SecondAmp = FirstAmp;
			SecondIndex = FirstIndex;
			FirstAmp = FFT_OUT[Index];
			FirstIndex = Index;
		}
		else if (FFT_OUT[Index] > SecondAmp)
		{
			SecondAmp = FFT_OUT[Index];
			SecondIndex = Index;
		}
	}

	if (FirstIndex != 0U)
	{
		Math_SetSpectrumComponent(
		    Cfft_Handler, FFT_OUT, NULL, pFirst, SIGNAL_FUNDAMENTAL_INDEX,
		    FirstIndex, Math_BinToFrequency(Cfft_Handler, FirstIndex));
	}
	if (SecondIndex != 0U)
	{
		Math_SetSpectrumComponent(
		    Cfft_Handler, FFT_OUT, NULL, pSecond, SIGNAL_FUNDAMENTAL_INDEX,
		    SecondIndex, Math_BinToFrequency(Cfft_Handler, SecondIndex));
	}

	if (pFirst->Freq[SIGNAL_FUNDAMENTAL_INDEX] <
	    pSecond->Freq[SIGNAL_FUNDAMENTAL_INDEX])
	{
		SignalPara_t Temp = *pFirst;
		*pFirst = *pSecond;
		*pSecond = Temp;
	}
}

/**
 * @brief 由去直流后的电压数据计算总输入信号的峰峰值和有效值。
 * @param[in] ADC_Voltage 已去除32768直流偏置的电压数组。
 * @param[in,out] pSignal 分析结果；函数只更新Upp和Urms字段。
 * @param[in] Sample_Size ADC_Voltage中的有效采样点数。
 * @return 无；输入无效时将Upp和Urms清零。
 */
void Math_CalculateTimeDomainParameters(const float *ADC_Voltage,
                                        SignalPara_t *pSignal,
                                        uint16_t Sample_Size)
{
	float MinVoltage;
	float MaxVoltage;
	float SquareSum = 0.0f;

	if (pSignal == NULL)
	{
		return;
	}

	pSignal->Upp = 0.0f;
	pSignal->Urms = 0.0f;
	if ((ADC_Voltage == NULL) || (Sample_Size == 0U))
	{
		return;
	}

	MinVoltage = ADC_Voltage[0];
	MaxVoltage = ADC_Voltage[0];
	for (uint16_t Index = 0U; Index < Sample_Size; Index++)
	{
		float Voltage = ADC_Voltage[Index];

		if (Voltage < MinVoltage)
		{
			MinVoltage = Voltage;
		}
		if (Voltage > MaxVoltage)
		{
			MaxVoltage = Voltage;
		}
		SquareSum += Voltage * Voltage;
	}

	pSignal->Upp = MaxVoltage - MinVoltage;
	pSignal->Urms = sqrtf(SquareSum / (float)Sample_Size);
}

/**
 * @brief 计算指定阶次谐波与基波的频谱强度比值。
 * @param[in] Signal 已完成频谱分析的信号结构体。
 * @param[in] Harmonic_Order 待查找的谐波阶次，必须不小于2。
 * @return 找到对应阶次时返回谐波与基波的频谱强度比，否则返回0。
 */
float Math_GetHarmonicRatio(const SignalPara_t *Signal,
                            uint8_t Harmonic_Order)
{
	float FundamentalAmp;
	uint32_t FundamentalFreq;

	if ((Signal == NULL) || (Harmonic_Order < 2U))
	{
		return 0.0f;
	}

	FundamentalAmp = Signal->SpectrumAmp[SIGNAL_FUNDAMENTAL_INDEX];
	FundamentalFreq = Signal->Freq[SIGNAL_FUNDAMENTAL_INDEX];
	if ((FundamentalAmp <= 0.0f) || (FundamentalFreq == 0U))
	{
		return 0.0f;
	}

	for (uint8_t Component = SIGNAL_HARMONIC_FIRST_INDEX;
	     Component < SIGNAL_COMPONENT_COUNT; Component++)
	{
		uint32_t Frequency = Signal->Freq[Component];
		uint32_t DetectedOrder;

		if (Frequency == 0U)
		{
			continue;
		}

		DetectedOrder =
		    (Frequency + (FundamentalFreq / 2U)) / FundamentalFreq;
		if (DetectedOrder == Harmonic_Order)
		{
			return Signal->SpectrumAmp[Component] / FundamentalAmp;
		}
	}

	return 0.0f;
}

/**
 * @brief 使用固定权重对当前输入执行一阶低通滤波。
 * @param[in] Last_val 上一次滤波输出值。
 * @param[in] Curr_val 当前输入值。
 * @return 返回按Alpha加权得到的新滤波值。
 */
float Math_LowPassFilter(float Last_val, float Curr_val)
{
	return (1.0f - Alpha) * Last_val + Alpha * Curr_val;
}

/**
 * @brief 从双通道ADC交错缓冲区中分离两路数据，去除直流偏置并生成复数FFT输入。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄，fftLen决定每路点数N。
 * @param[in] Mix_Buff 双通道交错ADC数组，布局为通道1、通道2依次交替。
 * @param[out] FeedbackFFT_INFirst 第一通道复数输入，容量至少为2N个float。
 * @param[out] FeedbackFFT_INSecond 第二通道复数输入，容量至少为2N个float。
 * @param[in] Sample_Size Mix_Buff中的uint16_t元素数，必须不小于2N。
 * @return 无；句柄、长度或缓冲区无效时不写入输出。
 */
void Math_Feedback_SeparateChannel(
    const arm_cfft_instance_f32 *Cfft_Handler, const uint16_t *Mix_Buff,
    float *FeedbackFFT_INFirst, float *FeedbackFFT_INSecond,
    uint16_t Sample_Size)
{
    uint16_t FFT_Size = Math_GetFFTSize(Cfft_Handler);

    if ((Mix_Buff == NULL) || (FeedbackFFT_INFirst == NULL) ||
        (FeedbackFFT_INSecond == NULL) || (FFT_Size == 0U) ||
        (Sample_Size < ((uint32_t)FFT_Size * 2U)))
    {
        return;
    }

	for (uint16_t Index = 0U; Index < FFT_Size; Index++)
	{
		/* 通道1位于交错缓冲区的偶数下标。 */
		FeedbackFFT_INFirst[2U * Index] =
		    (((float)Mix_Buff[2U * Index] - ADC_DC_OFFSET) /
		     ADC_FULL_SCALE_VALUE) * ADC_REFERENCE_VOLTAGE;
		FeedbackFFT_INFirst[2U * Index + 1U] = 0.0f;

		/* 通道2位于交错缓冲区的奇数下标。 */
		FeedbackFFT_INSecond[2U * Index] =
		    (((float)Mix_Buff[2U * Index + 1U] - ADC_DC_OFFSET) /
		     ADC_FULL_SCALE_VALUE) * ADC_REFERENCE_VOLTAGE;
		FeedbackFFT_INSecond[2U * Index + 1U] = 0.0f;
	}
}

/**
 * @brief 对单路复数输入执行FFT，并提取基波及最多两个谐波分量。
 * @param[in] Cfft_Handler 已初始化的CFFT句柄，fftLen决定FFT点数N。
 * @param[out] pSignal 统一的时频域信号参数结构体。
 * @param[in,out] FFT_IN 容量至少为2N个float；调用后被原地覆盖为FFT结果。
 * @param[out] FFT_OUT 容量至少为N/2个float；保存正频率幅值。
 * @param[in] Sample_Size FFT_IN中的float元素数量，必须不小于2N。
 * @return 无；输入无效时将pSignal全部字段清零。
 */
void Math_Feedback_FFTAndExtractPhase(const arm_cfft_instance_f32 *Cfft_Handler,
                                      SignalPara_t *pSignal, float *FFT_IN,
                                      float *FFT_OUT, uint16_t Sample_Size)
{
	uint16_t FFT_Size;

	if (pSignal == NULL)
	{
		return;
	}

	*pSignal = (SignalPara_t){0};

	if ((Cfft_Handler == NULL) || (FFT_IN == NULL) || (FFT_OUT == NULL))
	{
		return;
	}

	FFT_Size = Math_GetFFTSize(Cfft_Handler);
	if ((FFT_Size == 0U) || (Sample_Size < ((uint32_t)FFT_Size * 2U)))
	{
		return;
	}

	/* 原地执行FFT，复数结果覆盖FFT_IN。 */
	arm_cfft_f32(Cfft_Handler, FFT_IN, 0U, 1U);

	/* 计算正频率范围内各频点的幅值。 */
	arm_cmplx_mag_f32(FFT_IN, FFT_OUT, FFT_Size / 2U);

	/* 从FFT结果提取基波到四次谐波的频域参数。 */
	*pSignal = Math_AnalyzeSpectrum(
	    Cfft_Handler, FFT_OUT, FFT_IN, Sample_Size);
}



















