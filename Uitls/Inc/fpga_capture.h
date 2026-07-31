#ifndef FPGA_CAPTURE_H
#define FPGA_CAPTURE_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif
#define FPGA_CAPTURE_SAMPLE_COUNT 4096U
#define FPGA_CAPTURE_SAMPLE_RATE_HZ (62500000.0f / 32.0f)

typedef struct
{
  uint8_t pll_locked;
  uint8_t adc_configured;
  uint8_t capture_busy;
  uint8_t frame_ready;
  uint8_t otr_seen;
  uint8_t filter_error;
  uint8_t spi_error;
  uint16_t frame_id;
  uint32_t otr_count;
  uint32_t good_frames;
  uint32_t crc_errors;
  uint32_t protocol_errors;
  uint32_t dma_errors;
} FPGA_CaptureStatus_t;

HAL_StatusTypeDef FPGACapture_Init(void);
void FPGACapture_Service(void);
uint8_t FPGACapture_FrameAvailable(void);
const int16_t *FPGACapture_GetSamples(void);
void FPGACapture_ReleaseFrame(void);
const FPGA_CaptureStatus_t *FPGACapture_GetStatus(void);
void FPGACapture_SPIComplete(void);
void FPGACapture_SPIError(void);
uint8_t FPGACapture_IsActive(void);

#ifdef __cplusplus
}
#endif

#endif
