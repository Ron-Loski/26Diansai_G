/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "g_Inc.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
  START_SWITCH_WAITING = 0,
  START_SWITCH_START_DEBOUNCE,
  START_SWITCH_RUNNING,
  START_SWITCH_RESET_DEBOUNCE
} StartSwitchState_t;

typedef enum
{
  START_SWITCH_EVENT_NONE = 0,
  START_SWITCH_EVENT_START,
  START_SWITCH_EVENT_RESET
} StartSwitchEvent_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define START_SWITCH_GPIO_PORT       GPIOB
#define START_SWITCH_PIN             GPIO_PIN_12
#define START_SWITCH_DEBOUNCE_MS     50U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static Analyzer_Result_t AnalyzerResult;
static uint32_t AnalyzerStatusTick;
static StartSwitchState_t StartSwitchState;
static uint32_t StartSwitchDebounceTick;
static uint8_t AnalyzerRunning;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
static void StartSwitch_Init(void);
static StartSwitchEvent_t StartSwitch_Service(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void StartSwitch_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  gpio.Pin = START_SWITCH_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(START_SWITCH_GPIO_PORT, &gpio);

  StartSwitchState = START_SWITCH_WAITING;
  StartSwitchDebounceTick = HAL_GetTick();
  AnalyzerRunning = 0U;
}

static StartSwitchEvent_t StartSwitch_Service(void)
{
  GPIO_PinState pin_state =
      HAL_GPIO_ReadPin(START_SWITCH_GPIO_PORT, START_SWITCH_PIN);

  switch (StartSwitchState)
  {
    case START_SWITCH_WAITING:
      if (pin_state == GPIO_PIN_RESET)
      {
        StartSwitchDebounceTick = HAL_GetTick();
        StartSwitchState = START_SWITCH_START_DEBOUNCE;
      }
      break;

    case START_SWITCH_START_DEBOUNCE:
      if (pin_state != GPIO_PIN_RESET)
      {
        StartSwitchState = START_SWITCH_WAITING;
      }
      else if ((HAL_GetTick() - StartSwitchDebounceTick) >=
               START_SWITCH_DEBOUNCE_MS)
      {
        StartSwitchState = START_SWITCH_RUNNING;
        return START_SWITCH_EVENT_START;
      }
      break;

    case START_SWITCH_RUNNING:
      if (pin_state == GPIO_PIN_SET)
      {
        StartSwitchDebounceTick = HAL_GetTick();
        StartSwitchState = START_SWITCH_RESET_DEBOUNCE;
      }
      break;

    case START_SWITCH_RESET_DEBOUNCE:
      if (pin_state != GPIO_PIN_SET)
      {
        StartSwitchState = START_SWITCH_RUNNING;
      }
      else if ((HAL_GetTick() - StartSwitchDebounceTick) >=
               START_SWITCH_DEBOUNCE_MS)
      {
        return START_SWITCH_EVENT_RESET;
      }
      break;

    default:
      StartSwitchState = START_SWITCH_WAITING;
      break;
  }

  return START_SWITCH_EVENT_NONE;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_SPI3_Init();
  MX_TIM3_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  StartSwitch_Init();
  USART3_CommandRx_Init();
  SignalAnalyzer_Init();
  HMI_Display_Init();
  printf("ANALYZER_FW,H743VI_GAIN_CAL,build=20260731_1900\r\n");
  printf("ANALYZER_CAL,profile=adc_direct,version=4,scale=0.977656,"
         "mv_per_code=2.386855\r\n");
  printf("ANALYZER_WAIT_START,pin=PB12,active=low,debounce_ms=%u\r\n",
         (unsigned int)START_SWITCH_DEBOUNCE_MS);
  printf("SPI3_PINCHECK,mode=%lu,pupd=%lu,af=%lu,idr=%lu\r\n",
         (unsigned long)((GPIOC->MODER >> (11U * 2U)) & 3U),
         (unsigned long)((GPIOC->PUPDR >> (11U * 2U)) & 3U),
         (unsigned long)((GPIOC->AFR[1] >> ((11U - 8U) * 4U)) & 15U),
         (unsigned long)((GPIOC->IDR >> 11U) & 1U));
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    StartSwitchEvent_t switch_event;

    Command_Execute();
    HMI_Display_Service();
    switch_event = StartSwitch_Service();

    if (switch_event == START_SWITCH_EVENT_RESET)
    {
      printf("ANALYZER_BUTTON_RESET,pin=PB12\r\n");
      NVIC_SystemReset();
    }

    if (AnalyzerRunning == 0U)
    {
      if (switch_event == START_SWITCH_EVENT_START)
      {
        if (FPGACapture_Init() == HAL_OK)
        {
          AnalyzerRunning = 1U;
          AnalyzerStatusTick = HAL_GetTick();
          printf("ANALYZER_BUTTON_START,pin=PB12\r\n");
          printf("ANALYZER_START,raw_fs=25000000,decimation=13,"
                 "analysis_fs=1923076.923,n=4096,spi=SPI3_12MHz\r\n");
        }
        else
        {
          HMI_Display_ShowStatus("NO SIGNAL");
          printf("ANALYZER_ERROR,fpga_capture_init\r\n");
        }
      }

      if (AnalyzerRunning == 0U)
      {
        continue;
      }
    }

    FPGACapture_Service();

    if ((HAL_GetTick() - AnalyzerStatusTick) >= 1000U)
    {
      const FPGA_CaptureStatus_t *status = FPGACapture_GetStatus();
      AnalyzerStatusTick = HAL_GetTick();
      printf("ANALYZER_STATUS,pll=%u,cfg=%u,busy=%u,ready=%u,"
             "frame=%u,otr=%lu,good=%lu,crc=%lu,protocol=%lu,dma=%lu,"
             "fw=1900,pc11_mode=%lu,pc11_pupd=%lu,pc11_af=%lu,"
             "pc11_idr=%lu\r\n",
             status->pll_locked, status->adc_configured,
             status->capture_busy, status->frame_ready, status->frame_id,
             (unsigned long)status->otr_count,
             (unsigned long)status->good_frames,
             (unsigned long)status->crc_errors,
             (unsigned long)status->protocol_errors,
             (unsigned long)status->dma_errors,
             (unsigned long)((GPIOC->MODER >> (11U * 2U)) & 3U),
             (unsigned long)((GPIOC->PUPDR >> (11U * 2U)) & 3U),
             (unsigned long)((GPIOC->AFR[1] >>
                              ((11U - 8U) * 4U)) & 15U),
             (unsigned long)((GPIOC->IDR >> 11U) & 1U));
    }

    if (FPGACapture_FrameAvailable() != 0U)
    {
      const FPGA_CaptureStatus_t *capture_status =
          FPGACapture_GetStatus();
      const int16_t *capture_samples = FPGACapture_GetSamples();
      uint8_t wave_dump_request = Command_TakeWaveDumpRequest();
      if (SignalAnalyzer_Process(capture_samples,
                                 capture_status->frame_id,
                                 capture_status->otr_count,
                                 &AnalyzerResult) != 0U)
      {
        uint32_t i;
        printf("ANALYZER_RESULT,frame=%lu,f0=%.3f,upp=%.3f,"
               "urms=%.3f,components=%u,otr=%lu,processing_us=%lu\r\n",
               (unsigned long)AnalyzerResult.frame_id,
               AnalyzerResult.fundamental_hz,
               AnalyzerResult.upp_mv,
               AnalyzerResult.urms_mv,
               AnalyzerResult.component_count,
               (unsigned long)AnalyzerResult.otr_count,
               (unsigned long)AnalyzerResult.processing_us);
        for (i = 0U; i < AnalyzerResult.component_count; ++i)
        {
          printf("ANALYZER_COMPONENT,n=%u,f=%.3f,peak_mv=%.3f,"
                 "phase=%.6f\r\n",
                 AnalyzerResult.component[i].harmonic_order,
                 AnalyzerResult.component[i].frequency_hz,
                 AnalyzerResult.component[i].amplitude_peak_mv,
                 AnalyzerResult.component[i].phase_rad);
        }
        HMI_Display_Update(&AnalyzerResult,
                           SignalAnalyzer_GetDisplayWave(),
                           ANALYZER_DISPLAY_POINTS);
      }
      else
      {
        HMI_Display_ShowStatus("NO SIGNAL");
        printf("ANALYZER_NO_VALID_COMPONENT,frame=%u\r\n",
               capture_status->frame_id);
      }

      if (wave_dump_request == 1U)
      {
        uint32_t i;
        printf("WAVE_RAW_BEGIN,frame=%u,fs=1923076.923,count=4096\r\n",
               capture_status->frame_id);
        for (i = 0U; i < FPGA_CAPTURE_SAMPLE_COUNT; ++i)
        {
          printf("%d\r\n", capture_samples[i]);
        }
        printf("WAVE_RAW_END\r\n");
      }
      else if (wave_dump_request == 2U)
      {
        const int16_t *display_wave = SignalAnalyzer_GetDisplayWave();
        uint32_t i;
        printf("WAVE_DISPLAY_BEGIN,frame=%u,periods=%u,count=%u\r\n",
               capture_status->frame_id,
               SignalAnalyzer_GetDisplayPeriods(),
               ANALYZER_DISPLAY_POINTS);
        for (i = 0U; i < ANALYZER_DISPLAY_POINTS; ++i)
        {
          printf("%d\r\n", display_wave[i]);
        }
        printf("WAVE_DISPLAY_END\r\n");
      }
      FPGACapture_ReleaseFrame();
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
