#include "g_Inc.h"
#include <rt_sys.h>

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
__asm(".global __use_no_semihosting\n");
#endif

FILEHANDLE _sys_open(const char *name, int openmode)
{
	(void)name;
	(void)openmode;
	return 0;
}

int _sys_close(FILEHANDLE fh)
{
	(void)fh;
	return 0;
}

int _sys_write(FILEHANDLE fh, const unsigned char *buffer, unsigned length, int mode)
{
	unsigned remaining = length;

	(void)fh;
	(void)mode;

	while (remaining > 0U)
	{
		uint16_t chunk = (remaining > 65535U) ? 65535U : (uint16_t)remaining;

		if (HAL_UART_Transmit(&huart3, (uint8_t *)buffer, chunk, 100U) != HAL_OK)
		{
			break;
		}

		buffer += chunk;
		remaining -= chunk;
	}

	return (int)remaining;
}

int _sys_read(FILEHANDLE fh, unsigned char *buffer, unsigned length, int mode)
{
	(void)fh;
	(void)buffer;
	(void)mode;
	return (int)length;
}

int _sys_istty(FILEHANDLE fh)
{
	(void)fh;
	return 1;
}

int _sys_seek(FILEHANDLE fh, long position)
{
	(void)fh;
	(void)position;
	return -1;
}

int _sys_ensure(FILEHANDLE fh)
{
	(void)fh;
	return 0;
}

long _sys_flen(FILEHANDLE fh)
{
	(void)fh;
	return 0L;
}

void _ttywrch(int ch)
{
	(void)ch;
}

void _sys_exit(int return_code)
{
	(void)return_code;

	for (;;)
	{
		__NOP();
	}
}

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

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi == &hspi3)
	{
		FPGACapture_SPIComplete();
	}
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi == &hspi3)
	{
		FPGACapture_SPIError();
	}
}
