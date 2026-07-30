#ifndef HMI_DISPLAY_H
#define HMI_DISPLAY_H

#include "signal_analyzer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HMI_DISPLAY_POINTS 466U
#define HMI_SPECTRUM_MAX_HZ 500000.0f

void HMI_Display_Init(void);
void HMI_Display_Service(void);
void HMI_Display_Update(const Analyzer_Result_t *result,
                        const int16_t *display_wave,
                        uint32_t wave_count);
void HMI_Display_ShowStatus(const char *status);

#ifdef __cplusplus
}
#endif

#endif
