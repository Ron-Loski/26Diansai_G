#include "g_Inc.h"

int fputc(int ch, FILE *stream)
{
	uint8_t value = (uint8_t)ch;

	(void)stream;
	(void)HAL_UART_Transmit(&huart3, &value, 1U, 10U);
	return ch;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim == &htim3)
	{
		
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc == &hadc1)
	{
		adc1_done = 1U;
	}

}


void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
	if (huart == &huart3)
	{
		USART3_CommandRx_Event(size);
	}
}
