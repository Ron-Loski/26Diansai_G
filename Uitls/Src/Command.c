#include "g_Inc.h"

#define COMMAND_LINE_SIZE       64U     /* 单条文本命令的最大存储字节数。 */
#define COMMAND_RX_DMA_SIZE    128U     /* USART3单次DMA接收缓冲区字节数。 */
#define COMMAND_FFT_SIZE SampleSize_4096 /* Ques1命令使用的FFT点数。 */
#define COMMAND_FFT_COMPLEX_SIZE (COMMAND_FFT_SIZE * 2U) /* FFT复数数组的float元素数。 */
#define COMMAND_FFT_SPECTRUM_SIZE (COMMAND_FFT_SIZE / 2U) /* 单边幅值谱的float元素数。 */

static volatile Commandtypdef Command = Command_None;
static char CommandLine[COMMAND_LINE_SIZE];
static uint16_t CommandLineIndex = 0U;
static uint8_t CommandRxBuffer[COMMAND_RX_DMA_SIZE];
static float CommandFFTInput[COMMAND_FFT_COMPLEX_SIZE];
static float CommandFFTOutput[COMMAND_FFT_SPECTRUM_SIZE];

/**
 * @brief 启动一次USART3 Receive-to-Idle DMA接收。
 * @note 无输入参数。
 * @return 返回HAL_UARTEx_ReceiveToIdle_DMA的执行状态。
 */
static HAL_StatusTypeDef Command_RxStart(void)
{
	HAL_StatusTypeDef Status;

	Status = HAL_UARTEx_ReceiveToIdle_DMA(&huart3,
	                                     CommandRxBuffer,
	                                     sizeof(CommandRxBuffer));
	if (Status == HAL_OK)
	{
		__HAL_DMA_DISABLE_IT(huart3.hdmarx, DMA_IT_HT);
	}

	return Status;
}

/**
 * @brief 清除当前内部命令状态。
 * @note 无输入参数。
 * @return 无；内部变量Command被置为Command_None。
 */
static void Command_Clear(void)
{
	Command = Command_None;
}

/**
 * @brief 将一行完整命令交给命令判断函数。
 * @param[in] Line 以空字符结尾的完整命令字符串。
 * @return 无；解析结果写入命令模块内部状态。
 */
static void Command_HandleLine(char *Line)
{
	Command_Judege(Line);
}

/**
 * @brief 启动ADC1 DMA和TIM3，阻塞等待4096点采样完成。
 * @note ADC1必须配置为TIM3 TRGO触发，DMA必须配置为Normal模式。
 * @return HAL_OK表示采样完成；ADC或定时器启动失败时返回对应HAL状态。
 */
static HAL_StatusTypeDef Command_AcquireADC(void)
{
	HAL_StatusTypeDef Status;

	adc1_done = 0U;
	__HAL_TIM_SET_COUNTER(&htim3, 0U);

	/* 必须先启动ADC DMA，再启动TIM3，避免丢失第一个定时器触发。 */
	Status = HAL_ADC_Start_DMA(
	    &hadc1, (uint32_t *)ADC_RawBuff, COMMAND_FFT_SIZE);
	HAL_TIM_Base_Start(&htim3);
	/* DMA完成中断会在HAL_ADC_ConvCpltCallback中置位adc1_done。 */
	while (adc1_done == 0U)
	{
	}

	/* Normal模式采满4096点后停止触发源和ADC DMA，为下次命令复位状态。 */
	(void)HAL_TIM_Base_Stop(&htim3);
	(void)HAL_ADC_Stop_DMA(&hadc1);
	adc1_done = 0U; /* 本次完成事件已处理，清零标志以等待下一次采集。 */
	return HAL_OK;
}

/**
 * @brief 执行Ques1对应的4096点采集、FFT、参数计算和原始数据输出。
 * @note 先由TIM3触发ADC1完成4096点DMA采样，再执行FFT和参数计算。
 * @note ADC原始值写入ADC_RawBuff，去直流电压写入ADC_Voltage。
 * @note 使用printf逐行输出ADC_RawBuff中的4096个原始采样值。
 * @return 无；FFT句柄初始化或ADC采样启动失败时不发送数据。
 */
static void Command_ProcessQues1(void)
{
	if (cfft_handler_4096.fftLen != COMMAND_FFT_SIZE)
	{
		if (G_Var_InitCfftHandlers() != ARM_MATH_SUCCESS)
		{
			return;
		}
	}

	if (Command_AcquireADC() != HAL_OK)
	{
		return;
	}

	Math_FFT(
	    &cfft_handler_4096, ADC_RawBuff, ADC_Voltage, CommandFFTInput,
	    CommandFFTOutput, COMMAND_FFT_SIZE);
	InputSignalPara = Math_AnalyzeSpectrum(
	    &cfft_handler_4096, CommandFFTOutput, CommandFFTInput,
	    COMMAND_FFT_COMPLEX_SIZE);
	Math_CalculateTimeDomainParameters(
	    ADC_Voltage, &InputSignalPara, COMMAND_FFT_SIZE);

	for (uint16_t SampleIndex = 0U;
	     SampleIndex < COMMAND_FFT_SIZE; SampleIndex++)
	{
		printf("%u\r\n", (unsigned int)ADC_RawBuff[SampleIndex]);
	}
}

/**
 * @brief 按字节解析接收数据并组合以换行符结尾的命令行。
 * @param[in] Data 接收到的原始字节数组。
 * @param[in] Length Data中的有效字节数。
 * @return 无；完整命令行将更新内部命令状态。
 */
static void Command_ParseReceivedData(const uint8_t *Data, uint16_t Length)
{
	uint16_t Index;

	for (Index = 0U; Index < Length; Index++)
	{
		char Character = (char)Data[Index];

		if ((Character == '\n') || (Character == '\r'))
		{
			if (CommandLineIndex != 0U)
			{
				CommandLine[CommandLineIndex] = '\0';
				Command_HandleLine(CommandLine);
				CommandLineIndex = 0U;
			}
		}
		else if (CommandLineIndex < (COMMAND_LINE_SIZE - 1U))
		{
			CommandLine[CommandLineIndex] = Character;
			CommandLineIndex++;
		}
		else
		{
			CommandLineIndex = 0U;
			memset(CommandLine, 0, sizeof(CommandLine));
		}
	}
}

void USART3_CommandRx_Init(void)
{
	memset(CommandRxBuffer, 0, sizeof(CommandRxBuffer));
	memset(CommandLine, 0, sizeof(CommandLine));
	CommandLineIndex = 0U;
	(void)Command_RxStart();
}

void USART3_CommandRx_Event(uint16_t Size)
{
	if (Size > COMMAND_RX_DMA_SIZE)
	{
		Size = COMMAND_RX_DMA_SIZE;
	}

	Command_ParseReceivedData(CommandRxBuffer, Size);

	/* 普通模式下每次空闲接收事件结束后都需要重新启动DMA。 */
	(void)Command_RxStart();
}

void Command_Judege(char *Line)
{
	if (Line == NULL)
	{
		return;
	}

	Command_Clear();
	if (strcmp(Line, "Ques1") == 0)
	{
		Command = Ques1;
	}
}

void Command_Execute(void)
{
	switch (Command)
	{
		case Ques1:
			Command_Clear();
			Command_ProcessQues1();
			break;

		case Command_None:
		default:
			break;
	}
}

void Command_Study(void)
{
	/* main.c属于系统生成文件，因此此预留函数不在其中添加逻辑。 */
}
