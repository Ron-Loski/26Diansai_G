#ifndef SIGNAL_ANALYZER_H
#define SIGNAL_ANALYZER_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif
#define ANALYZER_MAX_COMPONENTS 3U
#define ANALYZER_DISPLAY_POINTS 480U

typedef struct
{
  uint8_t harmonic_order;
  float frequency_hz;
  float amplitude_peak_mv;
  float phase_rad;
} Analyzer_Component_t;

typedef struct
{
  uint8_t valid;
  uint32_t frame_id;
  float fundamental_hz;
  float upp_mv;
  float urms_mv;
  uint8_t component_count;
  Analyzer_Component_t component[ANALYZER_MAX_COMPONENTS];
  uint32_t otr_count;
  uint32_t processing_us;
} Analyzer_Result_t;

typedef struct
{
  uint32_t version;
  float mv_per_code[50];
  float phase_correction_rad[50];
  uint32_t crc32;
} Analyzer_Calibration_t;

void SignalAnalyzer_Init(void);
uint8_t SignalAnalyzer_Process(const int16_t *samples,
                               uint32_t frame_id,
                               uint32_t otr_count,
                               Analyzer_Result_t *result);
const int16_t *SignalAnalyzer_GetDisplayWave(void);
void SignalAnalyzer_SetDisplayPeriods(uint8_t periods);
uint8_t SignalAnalyzer_GetDisplayPeriods(void);
const Analyzer_Calibration_t *SignalAnalyzer_GetCalibration(void);
uint8_t SignalAnalyzer_SetCalibration(const Analyzer_Calibration_t *table);
uint8_t SignalAnalyzer_LoadCalibrationFromFlash(void);
uint8_t SignalAnalyzer_SaveCalibrationToFlash(
    const Analyzer_Calibration_t *table);

#ifdef __cplusplus
}
#endif

#endif
