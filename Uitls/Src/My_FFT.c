#include "g_Inc.h"

#define MY_FFT_TWO_PI                         6.28318530717958647692f /* 2π常量，用于计算旋转因子。 */
#define MY_FFT_TWIDDLE_RENORMALIZE_INTERVAL 256UL /* 旋转因子递推时的单位模长校正间隔。 */

/**
 * @brief 判断FFT点数是否为有效的2的整数次幂。
 * @param[in] FFT_Size 待校验的FFT点数。
 * @return 有效时返回1，无效时返回0。
 */
static uint8_t MyFFT_IsValidSize(uint32_t FFT_Size)
{
	if ((FFT_Size < MY_FFT_MIN_SIZE) ||
	    (FFT_Size > (UINT32_MAX / MY_FFT_COMPLEX_SCALE)))
	{
		return 0U;
	}

	return ((FFT_Size & (FFT_Size - 1UL)) == 0UL) ? 1U : 0U;
}

/**
 * @brief 对实部、虚部交错数组执行原地位倒序排列。
 * @param[in,out] Complex_Data 待重排的复数数组，容量至少为2×FFT_Size个float。
 * @param[in] FFT_Size FFT点数，必须为2的整数次幂。
 * @return 无；结果直接写回Complex_Data。
 */
static void MyFFT_BitReverse(float *Complex_Data, uint32_t FFT_Size)
{
	uint32_t Reversed = 0UL;

	for (uint32_t Index = 1UL; Index < FFT_Size; Index++)
	{
		uint32_t Bit = FFT_Size >> 1U;

		while ((Reversed & Bit) != 0UL)
		{
			Reversed ^= Bit;
			Bit >>= 1U;
		}
		Reversed ^= Bit;

		if (Index < Reversed)
		{
			uint32_t IndexOffset = Index * MY_FFT_COMPLEX_SCALE;
			uint32_t ReversedOffset = Reversed * MY_FFT_COMPLEX_SCALE;
			float TempReal = Complex_Data[IndexOffset];
			float TempImag = Complex_Data[IndexOffset + 1UL];

			Complex_Data[IndexOffset] = Complex_Data[ReversedOffset];
			Complex_Data[IndexOffset + 1UL] =
			    Complex_Data[ReversedOffset + 1UL];
			Complex_Data[ReversedOffset] = TempReal;
			Complex_Data[ReversedOffset + 1UL] = TempImag;
		}
	}
}

/**
 * @brief 对已完成位倒序排列的复数数组执行原地正向基2蝶形运算。
 * @param[in,out] Complex_Data 复数交错数组，输入和FFT结果使用同一缓冲区。
 * @param[in] FFT_Size FFT点数，必须为2的整数次幂。
 * @return 无；未归一化的正向FFT结果直接写回Complex_Data。
 */
static void MyFFT_ExecuteButterflies(float *Complex_Data, uint32_t FFT_Size)
{
	for (uint32_t StageSize = 2UL; StageSize <= FFT_Size;
	     StageSize <<= 1U)
	{
		uint32_t HalfSize = StageSize >> 1U;
		float AngleStep = -MY_FFT_TWO_PI / (float)StageSize;
		float StepReal = cosf(AngleStep);
		float StepImag = sinf(AngleStep);

		for (uint32_t Block = 0UL; Block < FFT_Size; Block += StageSize)
		{
			float TwiddleReal = 1.0f;
			float TwiddleImag = 0.0f;

			for (uint32_t Butterfly = 0UL; Butterfly < HalfSize;
			     Butterfly++)
			{
				uint32_t EvenOffset =
				    (Block + Butterfly) * MY_FFT_COMPLEX_SCALE;
				uint32_t OddOffset =
				    (Block + Butterfly + HalfSize) *
				    MY_FFT_COMPLEX_SCALE;
				float OddReal = Complex_Data[OddOffset];
				float OddImag = Complex_Data[OddOffset + 1UL];
				float ProductReal =
				    TwiddleReal * OddReal - TwiddleImag * OddImag;
				float ProductImag =
				    TwiddleReal * OddImag + TwiddleImag * OddReal;
				float EvenReal = Complex_Data[EvenOffset];
				float EvenImag = Complex_Data[EvenOffset + 1UL];
				float NextTwiddleReal =
				    TwiddleReal * StepReal - TwiddleImag * StepImag;

				Complex_Data[EvenOffset] = EvenReal + ProductReal;
				Complex_Data[EvenOffset + 1UL] =
				    EvenImag + ProductImag;
				Complex_Data[OddOffset] = EvenReal - ProductReal;
				Complex_Data[OddOffset + 1UL] =
				    EvenImag - ProductImag;

				TwiddleImag =
				    TwiddleReal * StepImag + TwiddleImag * StepReal;
				TwiddleReal = NextTwiddleReal;

				if ((((Butterfly + 1UL) %
				      MY_FFT_TWIDDLE_RENORMALIZE_INTERVAL) == 0UL) &&
				    ((Butterfly + 1UL) < HalfSize))
				{
					float NormSquared =
					    TwiddleReal * TwiddleReal +
					    TwiddleImag * TwiddleImag;
					float InverseNorm = 1.0f / sqrtf(NormSquared);

					TwiddleReal *= InverseNorm;
					TwiddleImag *= InverseNorm;
				}
			}
		}

		/* 当前级已处理完整长度，直接结束以避免无符号左移溢出。 */
		if (StageSize == FFT_Size)
		{
			break;
		}
	}
}

/**
 * @brief 初始化自定义FFT句柄。
 * @param[out] FFT_Handle 待初始化的FFT句柄。
 * @param[in] FFT_Size FFT点数，必须不小于32768且为2的整数次幂。
 * @return 返回MY_FFT_STATUS_OK表示初始化成功，否则返回对应错误状态。
 */
MyFFT_StatusTypeDef MyFFT_Init(MyFFT_HandleTypeDef *FFT_Handle,
                               uint32_t FFT_Size)
{
	if (FFT_Handle == NULL)
	{
		return MY_FFT_STATUS_NULL_POINTER;
	}

	FFT_Handle->FFT_Size = 0UL;
	if (MyFFT_IsValidSize(FFT_Size) == 0U)
	{
		return MY_FFT_STATUS_INVALID_SIZE;
	}

	FFT_Handle->FFT_Size = FFT_Size;
	return MY_FFT_STATUS_OK;
}

/**
 * @brief 对实数电压数组执行自定义点数的正向基2 FFT。
 * @param[in] FFT_Handle 已由MyFFT_Init初始化的FFT句柄。
 * @param[in] Voltage_Input 输入电压数组；函数直接使用其数值，不转换量程、不减直流。
 * @param[in] Input_Count Voltage_Input可用的float元素数，必须不小于FFT_Size。
 * @param[out] Complex_Output FFT复数输出，按实部、虚部交错保存，共需2×FFT_Size个float。
 * @param[in] Output_Count Complex_Output可用的float元素数，必须不小于2×FFT_Size。
 * @return 返回MY_FFT_STATUS_OK表示运算成功，否则返回对应错误状态。
 * @note Voltage_Input与Complex_Output地址相同时从尾部展开实数输入，避免覆盖未读取数据。
 */
MyFFT_StatusTypeDef MyFFT_ForwardRealF32(
    const MyFFT_HandleTypeDef *FFT_Handle,
    const float *Voltage_Input, uint32_t Input_Count,
    float *Complex_Output, uint32_t Output_Count)
{
	uint32_t FFT_Size;
	uint32_t RequiredOutputCount;

	if ((FFT_Handle == NULL) || (Voltage_Input == NULL) ||
	    (Complex_Output == NULL))
	{
		return MY_FFT_STATUS_NULL_POINTER;
	}

	FFT_Size = FFT_Handle->FFT_Size;
	if (MyFFT_IsValidSize(FFT_Size) == 0U)
	{
		return MY_FFT_STATUS_INVALID_SIZE;
	}

	RequiredOutputCount = FFT_Size * MY_FFT_COMPLEX_SCALE;
	if ((Input_Count < FFT_Size) ||
	    (Output_Count < RequiredOutputCount))
	{
		return MY_FFT_STATUS_BUFFER_TOO_SMALL;
	}

	if (Voltage_Input == Complex_Output)
	{
		/* 同一缓冲区时从末尾向前扩展，保留尚未读取的实数输入。 */
		for (uint32_t Count = FFT_Size; Count > 0UL; Count--)
		{
			uint32_t Index = Count - 1UL;
			float Voltage = Voltage_Input[Index];

			Complex_Output[Index * MY_FFT_COMPLEX_SCALE] = Voltage;
			Complex_Output[Index * MY_FFT_COMPLEX_SCALE + 1UL] = 0.0f;
		}
	}
	else
	{
		for (uint32_t Index = 0UL; Index < FFT_Size; Index++)
		{
			Complex_Output[Index * MY_FFT_COMPLEX_SCALE] =
			    Voltage_Input[Index];
			Complex_Output[Index * MY_FFT_COMPLEX_SCALE + 1UL] = 0.0f;
		}
	}

	MyFFT_BitReverse(Complex_Output, FFT_Size);
	MyFFT_ExecuteButterflies(Complex_Output, FFT_Size);
	return MY_FFT_STATUS_OK;
}

