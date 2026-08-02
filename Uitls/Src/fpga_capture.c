#include "fpga_capture.h"

#include <stdio.h>
#include <string.h>

#include "spi.h"

#define FPGA_REQUEST_SYNC 0xA55AU
#define FPGA_RESPONSE_SYNC 0x5AA5U
#define FPGA_PROTOCOL_VERSION 0x0001U
#define FPGA_CMD_ARM 0x0001U
#define FPGA_CMD_STATUS 0x0002U
#define FPGA_CMD_READ 0x0003U

#define FPGA_HEADER_WORDS 8U
#define FPGA_STATUS_RESPONSE_WORDS (FPGA_HEADER_WORDS + 1U)
#define FPGA_READ_RESPONSE_WORDS \
  (FPGA_HEADER_WORDS + FPGA_CAPTURE_SAMPLE_COUNT + 1U)
#define FPGA_READ_TRANSACTION_WORDS (4U + FPGA_READ_RESPONSE_WORDS)

#define FPGA_FLAG_PLL_LOCKED (1U << 0)
#define FPGA_FLAG_ADC_CONFIGURED (1U << 1)
#define FPGA_FLAG_CAPTURE_BUSY (1U << 2)
#define FPGA_FLAG_FRAME_READY (1U << 3)
#define FPGA_FLAG_OTR_SEEN (1U << 4)
#define FPGA_FLAG_SPI_ERROR (1U << 6)

typedef enum
{
  CAPTURE_STARTUP = 0,
  CAPTURE_ARM,
  CAPTURE_WAIT,
  CAPTURE_READ,
  CAPTURE_FRAME_HELD
} CaptureState_t;

static uint16_t TxWords[FPGA_READ_TRANSACTION_WORDS]
    __attribute__((section("DMA_RAM"), aligned(32)));
static uint16_t RxWords[FPGA_READ_TRANSACTION_WORDS]
    __attribute__((section("DMA_RAM"), aligned(32)));

static FPGA_CaptureStatus_t CaptureStatus;
static CaptureState_t CaptureState;
static volatile uint8_t DmaDone;
static volatile uint8_t DmaError;
static uint8_t FrameAvailable;
static uint32_t StateTick;
static uint8_t CaptureActive;
static uint32_t RawDebugTick;

static uint16_t CRC16_Word(uint16_t crc, uint16_t data)
{
  uint32_t bit;
  for (bit = 0U; bit < 16U; ++bit)
  {
    uint16_t input = (uint16_t)((data & 0x8000U) != 0U);
    uint16_t top = (uint16_t)((crc & 0x8000U) != 0U);
    crc <<= 1;
    if ((input ^ top) != 0U)
    {
      crc ^= 0x1021U;
    }
    data <<= 1;
  }
  return crc;
}

static void BuildRequest(uint16_t *words, uint16_t command, uint16_t argument)
{
  uint16_t crc = 0xFFFFU;
  words[0] = FPGA_REQUEST_SYNC;
  words[1] = command;
  words[2] = argument;
  crc = CRC16_Word(crc, words[0]);
  crc = CRC16_Word(crc, words[1]);
  crc = CRC16_Word(crc, words[2]);
  words[3] = crc;
}

static void ChipSelect(uint8_t selected)
{
  HAL_GPIO_WritePin(FPGA_CAPTURE_CS_GPIO_Port, FPGA_CAPTURE_CS_Pin,
                    selected != 0U ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static HAL_StatusTypeDef SendArm(void)
{
  HAL_StatusTypeDef status;
  BuildRequest(TxWords, FPGA_CMD_ARM, 0U);
  ChipSelect(1U);
  status = HAL_SPI_Transmit(&hspi3, (uint8_t *)TxWords, 4U, 20U);
  ChipSelect(0U);
  return status;
}

static uint8_t ReadStatus(void)
{
  uint16_t crc = 0xFFFFU;
  uint32_t i;
  uint16_t flags;

  memset(TxWords, 0, (4U + FPGA_STATUS_RESPONSE_WORDS) * sizeof(uint16_t));
  memset(RxWords, 0, (4U + FPGA_STATUS_RESPONSE_WORDS) * sizeof(uint16_t));
  BuildRequest(TxWords, FPGA_CMD_STATUS, 0U);

  ChipSelect(1U);
  if (HAL_SPI_TransmitReceive(&hspi3, (uint8_t *)TxWords,
                              (uint8_t *)RxWords,
                              4U + FPGA_STATUS_RESPONSE_WORDS, 20U) != HAL_OK)
  {
    ChipSelect(0U);
    CaptureStatus.protocol_errors++;
    if ((HAL_GetTick() - RawDebugTick) >= 1000U)
    {
      RawDebugTick = HAL_GetTick();
      printf("SPI_RAW,hal_error=0x%08lX,state=0x%08lX\r\n",
             (unsigned long)HAL_SPI_GetError(&hspi3),
             (unsigned long)HAL_SPI_GetState(&hspi3));
    }
    return 0U;
  }
  ChipSelect(0U);

  if ((RxWords[4] != FPGA_RESPONSE_SYNC) ||
      (RxWords[5] != FPGA_PROTOCOL_VERSION))
  {
    CaptureStatus.protocol_errors++;
    if ((HAL_GetTick() - RawDebugTick) >= 1000U)
    {
      RawDebugTick = HAL_GetTick();
      printf("SPI_RAW,%04X,%04X,%04X,%04X,%04X,%04X,%04X,%04X,%04X\r\n",
             RxWords[4], RxWords[5], RxWords[6], RxWords[7],
             RxWords[8], RxWords[9], RxWords[10], RxWords[11],
             RxWords[12]);
    }
    return 0U;
  }
  for (i = 4U; i < 4U + FPGA_HEADER_WORDS; ++i)
  {
    crc = CRC16_Word(crc, RxWords[i]);
  }
  if (crc != RxWords[4U + FPGA_HEADER_WORDS])
  {
    CaptureStatus.crc_errors++;
    if ((HAL_GetTick() - RawDebugTick) >= 1000U)
    {
      RawDebugTick = HAL_GetTick();
      printf("SPI_RAW,crc_expected=%04X,crc_received=%04X\r\n",
             crc, RxWords[4U + FPGA_HEADER_WORDS]);
    }
    return 0U;
  }

  flags = RxWords[6];
  CaptureStatus.pll_locked =
      (uint8_t)((flags & FPGA_FLAG_PLL_LOCKED) != 0U);
  CaptureStatus.adc_configured =
      (uint8_t)((flags & FPGA_FLAG_ADC_CONFIGURED) != 0U);
  CaptureStatus.capture_busy =
      (uint8_t)((flags & FPGA_FLAG_CAPTURE_BUSY) != 0U);
  CaptureStatus.frame_ready =
      (uint8_t)((flags & FPGA_FLAG_FRAME_READY) != 0U);
  CaptureStatus.otr_seen =
      (uint8_t)((flags & FPGA_FLAG_OTR_SEEN) != 0U);
  CaptureStatus.spi_error =
      (uint8_t)((flags & FPGA_FLAG_SPI_ERROR) != 0U);
  CaptureStatus.frame_id = RxWords[7];
  CaptureStatus.otr_count =
      (uint32_t)RxWords[10] | ((uint32_t)RxWords[11] << 16);
  return 1U;
}

static HAL_StatusTypeDef StartReadDMA(void)
{
  memset(TxWords, 0, sizeof(TxWords));
  memset(RxWords, 0, sizeof(RxWords));
  BuildRequest(TxWords, FPGA_CMD_READ, 0U);
  DmaDone = 0U;
  DmaError = 0U;
  RawDebugTick = 0U;
  ChipSelect(1U);
  if (HAL_SPI_TransmitReceive_DMA(&hspi3, (uint8_t *)TxWords,
                                  (uint8_t *)RxWords,
                                  FPGA_READ_TRANSACTION_WORDS) != HAL_OK)
  {
    ChipSelect(0U);
    return HAL_ERROR;
  }
  return HAL_OK;
}

static uint8_t ValidateReadFrame(void)
{
  uint16_t crc = 0xFFFFU;
  uint32_t i;
  const uint32_t response = 4U;
  const uint32_t crc_index =
      response + FPGA_HEADER_WORDS + FPGA_CAPTURE_SAMPLE_COUNT;

  if ((RxWords[response] != FPGA_RESPONSE_SYNC) ||
      (RxWords[response + 1U] != FPGA_PROTOCOL_VERSION) ||
      (RxWords[response + 5U] != FPGA_CAPTURE_SAMPLE_COUNT))
  {
    CaptureStatus.protocol_errors++;
    return 0U;
  }

  for (i = response; i < crc_index; ++i)
  {
    crc = CRC16_Word(crc, RxWords[i]);
  }
  if (crc != RxWords[crc_index])
  {
    CaptureStatus.crc_errors++;
    return 0U;
  }

  CaptureStatus.frame_id = RxWords[response + 3U];
  CaptureStatus.otr_count =
      (uint32_t)RxWords[response + 6U] |
      ((uint32_t)RxWords[response + 7U] << 16);
  CaptureStatus.good_frames++;
  return 1U;
}

HAL_StatusTypeDef FPGACapture_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  CaptureActive = 0U;
  memset(&CaptureStatus, 0, sizeof(CaptureStatus));
  FrameAvailable = 0U;
  DmaDone = 0U;
  DmaError = 0U;

  __HAL_RCC_GPIOD_CLK_ENABLE();
  gpio.Pin = FPGA_CAPTURE_CS_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(FPGA_CAPTURE_CS_GPIO_Port, &gpio);
  ChipSelect(0U);

  if (SPI3_ConfigureAnalyzerMode() != HAL_OK)
  {
    return HAL_ERROR;
  }

  CaptureState = CAPTURE_STARTUP;
  StateTick = HAL_GetTick();
  CaptureActive = 1U;
  return HAL_OK;
}

void FPGACapture_Service(void)
{
  if (CaptureActive == 0U)
  {
    return;
  }

  switch (CaptureState)
  {
    case CAPTURE_STARTUP:
      if ((HAL_GetTick() - StateTick) >= 120U)
      {
        if (ReadStatus() != 0U &&
            CaptureStatus.pll_locked != 0U &&
            CaptureStatus.adc_configured != 0U)
        {
          CaptureState = CAPTURE_ARM;
        }
        else
        {
          StateTick = HAL_GetTick();
        }
      }
      break;

    case CAPTURE_ARM:
      if (SendArm() == HAL_OK)
      {
        CaptureState = CAPTURE_WAIT;
        StateTick = HAL_GetTick();
      }
      else
      {
        CaptureStatus.protocol_errors++;
      }
      break;

    case CAPTURE_WAIT:
      if ((HAL_GetTick() - StateTick) >= 3U)
      {
        StateTick = HAL_GetTick();
        if (ReadStatus() != 0U && CaptureStatus.frame_ready != 0U)
        {
          if (StartReadDMA() == HAL_OK)
          {
            CaptureState = CAPTURE_READ;
          }
          else
          {
            CaptureStatus.dma_errors++;
          }
        }
      }
      break;

    case CAPTURE_READ:
      if (DmaError != 0U)
      {
        DmaError = 0U;
        CaptureStatus.dma_errors++;
        CaptureState = CAPTURE_ARM;
      }
      else if (DmaDone != 0U)
      {
        DmaDone = 0U;
        if (ValidateReadFrame() != 0U)
        {
          FrameAvailable = 1U;
          CaptureState = CAPTURE_FRAME_HELD;
        }
        else
        {
          CaptureState = CAPTURE_ARM;
        }
      }
      break;

    case CAPTURE_FRAME_HELD:
    default:
      break;
  }
}

uint8_t FPGACapture_FrameAvailable(void)
{
  return FrameAvailable;
}

const int16_t *FPGACapture_GetSamples(void)
{
  return (const int16_t *)&RxWords[4U + FPGA_HEADER_WORDS];
}

void FPGACapture_ReleaseFrame(void)
{
  FrameAvailable = 0U;
  CaptureState = CAPTURE_ARM;
}

const FPGA_CaptureStatus_t *FPGACapture_GetStatus(void)
{
  return &CaptureStatus;
}

void FPGACapture_SPIComplete(void)
{
  if (CaptureActive != 0U && CaptureState == CAPTURE_READ)
  {
    ChipSelect(0U);
    DmaDone = 1U;
  }
}

void FPGACapture_SPIError(void)
{
  if (CaptureActive != 0U)
  {
    ChipSelect(0U);
    DmaError = 1U;
  }
}

uint8_t FPGACapture_IsActive(void)
{
  return CaptureActive;
}
