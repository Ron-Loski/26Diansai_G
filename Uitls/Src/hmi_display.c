#include "hmi_display.h"

#include <stdio.h>
#include <string.h>

#include "usart.h"

#define HMI_UART                 huart2
#define HMI_TX_TIMEOUT_MS        80U
#define HMI_TOUCH_PACKET_SIZE    9U

#define HMI_COLOR_BG             164U
#define HMI_COLOR_HEADER         2310U
#define HMI_COLOR_PANEL          2310U
#define HMI_COLOR_CARD           2245U
#define HMI_COLOR_PLOT           196U
#define HMI_COLOR_BORDER         6602U
#define HMI_COLOR_GRID           3370U
#define HMI_COLOR_TEXT           65535U
#define HMI_COLOR_MUTED          38066U
#define HMI_COLOR_WAVE           9880U
#define HMI_COLOR_RMS            23967U
#define HMI_COLOR_FREQ           63019U
#define HMI_COLOR_OK             14136U
#define HMI_COLOR_ERROR          64237U

#define HMI_TIME_X               40U
#define HMI_TIME_Y               145U
#define HMI_TIME_W               580U
#define HMI_TIME_H               190U
#define HMI_SPECTRUM_X           40U
#define HMI_SPECTRUM_Y           435U
#define HMI_SPECTRUM_W           580U
#define HMI_SPECTRUM_H           120U

static uint8_t hmi_rx_packet[HMI_TOUCH_PACKET_SIZE];
static uint8_t hmi_rx_index;
static uint8_t hmi_ascii_prefix;
static uint8_t hmi_periods = 1U;

static void HMI_SetMcuBaudRate(uint32_t baud_rate)
{
  (void)HAL_UART_DeInit(&HMI_UART);
  HMI_UART.Init.BaudRate = baud_rate;
  (void)HAL_UART_Init(&HMI_UART);
}

static void HMI_SendRaw(const uint8_t *data, uint16_t length)
{
  (void)HAL_UART_Transmit(&HMI_UART, (uint8_t *)data, length,
                          HMI_TX_TIMEOUT_MS);
}

static void HMI_SendCommand(const char *command)
{
  static const uint8_t tail[3] = {0xFFU, 0xFFU, 0xFFU};

  HMI_SendRaw((const uint8_t *)command, (uint16_t)strlen(command));
  HMI_SendRaw(tail, sizeof(tail));
}

static void HMI_Fill(uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height,
                     uint16_t color)
{
  char command[48];

  (void)snprintf(command, sizeof(command), "fill %u,%u,%u,%u,%u",
                 x, y, width, height, color);
  HMI_SendCommand(command);
}

static void HMI_DrawRect(uint16_t x, uint16_t y,
                         uint16_t width, uint16_t height,
                         uint16_t color)
{
  char command[48];

  (void)snprintf(command, sizeof(command), "draw %u,%u,%u,%u,%u",
                 x, y, (uint16_t)(x + width - 1U),
                 (uint16_t)(y + height - 1U), color);
  HMI_SendCommand(command);
}

static void HMI_DrawLine(uint16_t x0, uint16_t y0,
                         uint16_t x1, uint16_t y1,
                         uint16_t color)
{
  char command[48];

  (void)snprintf(command, sizeof(command), "line %u,%u,%u,%u,%u",
                 x0, y0, x1, y1, color);
  HMI_SendCommand(command);
}

static void HMI_DrawCircle(uint16_t x, uint16_t y,
                           uint16_t radius, uint16_t color)
{
  char command[40];

  (void)snprintf(command, sizeof(command), "cirs %u,%u,%u,%u",
                 x, y, radius, color);
  HMI_SendCommand(command);
}

static void HMI_DrawText(uint16_t x, uint16_t y,
                         uint16_t width, uint16_t height,
                         uint8_t font, uint16_t color,
                         uint16_t background,
                         uint8_t horizontal_alignment,
                         const char *text)
{
  char command[176];

  (void)snprintf(command, sizeof(command),
                 "xstr %u,%u,%u,%u,%u,%u,%u,%u,1,1,\"%s\"",
                 x, y, width, height, font, color, background,
                 horizontal_alignment, text);
  HMI_SendCommand(command);
}

static void HMI_HideExampleObjects(void)
{
  static const char *const objects[] = {
    "s0", "s1", "b0", "b1", "tUpp", "tUrms",
    "tVrms", "tFund", "tPeriod", "tStatus", "tPeaks"
  };
  char command[32];
  uint32_t i;

  for (i = 0U; i < (sizeof(objects) / sizeof(objects[0])); ++i)
  {
    (void)snprintf(command, sizeof(command), "vis %s,0", objects[i]);
    HMI_SendCommand(command);
  }
}

static void HMI_DrawTimeGrid(void)
{
  uint16_t i;

  HMI_Fill(HMI_TIME_X, HMI_TIME_Y, HMI_TIME_W, HMI_TIME_H,
           HMI_COLOR_PLOT);
  HMI_DrawRect(HMI_TIME_X, HMI_TIME_Y, HMI_TIME_W, HMI_TIME_H,
               HMI_COLOR_BORDER);
  for (i = 1U; i < 8U; ++i)
  {
    uint16_t x = (uint16_t)(HMI_TIME_X + i * HMI_TIME_W / 8U);
    HMI_DrawLine(x, HMI_TIME_Y, x,
                 (uint16_t)(HMI_TIME_Y + HMI_TIME_H - 1U),
                 HMI_COLOR_GRID);
  }
  for (i = 1U; i < 6U; ++i)
  {
    uint16_t y = (uint16_t)(HMI_TIME_Y + i * HMI_TIME_H / 6U);
    HMI_DrawLine(HMI_TIME_X, y,
                 (uint16_t)(HMI_TIME_X + HMI_TIME_W - 1U), y,
                 HMI_COLOR_GRID);
  }
  HMI_DrawLine(HMI_TIME_X,
               (uint16_t)(HMI_TIME_Y + HMI_TIME_H / 2U),
               (uint16_t)(HMI_TIME_X + HMI_TIME_W - 1U),
               (uint16_t)(HMI_TIME_Y + HMI_TIME_H / 2U),
               HMI_COLOR_BORDER);
}

static void HMI_DrawSpectrumGrid(void)
{
  uint16_t i;

  HMI_Fill(HMI_SPECTRUM_X, HMI_SPECTRUM_Y,
           HMI_SPECTRUM_W, HMI_SPECTRUM_H, HMI_COLOR_PLOT);
  HMI_DrawRect(HMI_SPECTRUM_X, HMI_SPECTRUM_Y,
               HMI_SPECTRUM_W, HMI_SPECTRUM_H, HMI_COLOR_BORDER);
  for (i = 1U; i < 5U; ++i)
  {
    uint16_t x = (uint16_t)(HMI_SPECTRUM_X +
                            i * HMI_SPECTRUM_W / 5U);
    HMI_DrawLine(x, HMI_SPECTRUM_Y, x,
                 (uint16_t)(HMI_SPECTRUM_Y + HMI_SPECTRUM_H - 1U),
                 HMI_COLOR_GRID);
  }
  for (i = 1U; i < 4U; ++i)
  {
    uint16_t y = (uint16_t)(HMI_SPECTRUM_Y +
                            i * HMI_SPECTRUM_H / 4U);
    HMI_DrawLine(HMI_SPECTRUM_X, y,
                 (uint16_t)(HMI_SPECTRUM_X + HMI_SPECTRUM_W - 1U),
                 y, HMI_COLOR_GRID);
  }
}

static void HMI_DrawPeriodButtons(void)
{
  uint16_t selected = HMI_COLOR_WAVE;
  uint16_t normal = HMI_COLOR_BORDER;

  HMI_Fill(470U, 103U, 78U, 34U, HMI_COLOR_PANEL);
  HMI_Fill(558U, 103U, 78U, 34U, HMI_COLOR_PANEL);
  HMI_DrawRect(470U, 103U, 78U, 34U,
               (hmi_periods == 1U) ? selected : normal);
  HMI_DrawRect(558U, 103U, 78U, 34U,
               (hmi_periods == 3U) ? selected : normal);
  HMI_DrawText(470U, 107U, 78U, 24U, 0U,
               (hmi_periods == 1U) ? HMI_COLOR_TEXT : HMI_COLOR_MUTED,
               HMI_COLOR_PANEL, 1U, "1 PERIOD");
  HMI_DrawText(558U, 107U, 78U, 24U, 0U,
               (hmi_periods == 3U) ? HMI_COLOR_TEXT : HMI_COLOR_MUTED,
               HMI_COLOR_PANEL, 1U, "3 PERIOD");
}

static void HMI_DrawStaticUi(void)
{
  uint16_t i;

  HMI_SendCommand("cls 164");
  HMI_Fill(0U, 0U, 1024U, 72U, HMI_COLOR_HEADER);
  HMI_DrawLine(0U, 71U, 1023U, 71U, HMI_COLOR_BORDER);
  HMI_DrawCircle(31U, 35U, 14U, HMI_COLOR_WAVE);
  HMI_DrawText(55U, 12U, 500U, 38U, 1U, HMI_COLOR_TEXT,
               HMI_COLOR_HEADER, 0U, "PERIODIC SIGNAL ANALYZER");
  HMI_DrawText(55U, 46U, 500U, 20U, 0U, HMI_COLOR_MUTED,
               HMI_COLOR_HEADER, 0U, "2026 TI CUP - PROBLEM G");

  HMI_Fill(858U, 18U, 146U, 38U, HMI_COLOR_PANEL);
  HMI_DrawRect(858U, 18U, 146U, 38U, HMI_COLOR_WAVE);
  HMI_DrawCircle(876U, 37U, 5U, HMI_COLOR_OK);
  HMI_DrawText(890U, 25U, 102U, 24U, 0U, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, "REAL-TIME");

  HMI_Fill(20U, 92U, 620U, 270U, HMI_COLOR_PANEL);
  HMI_DrawRect(20U, 92U, 620U, 270U, HMI_COLOR_BORDER);
  HMI_Fill(660U, 92U, 344U, 270U, HMI_COLOR_PANEL);
  HMI_DrawRect(660U, 92U, 344U, 270U, HMI_COLOR_BORDER);
  HMI_Fill(20U, 382U, 620U, 198U, HMI_COLOR_PANEL);
  HMI_DrawRect(20U, 382U, 620U, 198U, HMI_COLOR_BORDER);
  HMI_Fill(660U, 382U, 344U, 198U, HMI_COLOR_PANEL);
  HMI_DrawRect(660U, 382U, 344U, 198U, HMI_COLOR_BORDER);

  HMI_DrawText(40U, 103U, 240U, 30U, 1U, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, "TIME DOMAIN");
  HMI_DrawText(40U, 393U, 300U, 30U, 1U, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, "VOLTAGE SPECTRUM");
  HMI_DrawText(680U, 103U, 280U, 30U, 1U, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, "MEASUREMENTS");
  HMI_DrawText(680U, 393U, 280U, 30U, 1U, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, "FREQUENCY COMPONENTS");

  HMI_DrawTimeGrid();
  HMI_DrawSpectrumGrid();
  HMI_DrawPeriodButtons();

  HMI_DrawText(42U, 337U, 80U, 18U, 0U, HMI_COLOR_MUTED,
               HMI_COLOR_PANEL, 0U, "-U");
  HMI_DrawText(575U, 337U, 42U, 18U, 0U, HMI_COLOR_MUTED,
               HMI_COLOR_PANEL, 2U, "t");

  for (i = 0U; i <= 5U; ++i)
  {
    char label[16];
    (void)snprintf(label, sizeof(label), "%uk",
                   (unsigned int)(i * 100U));
    HMI_DrawText((uint16_t)(35U + i * 116U), 558U, 50U, 18U,
                 0U, HMI_COLOR_MUTED, HMI_COLOR_PANEL, 1U, label);
  }

  HMI_Fill(680U, 140U, 304U, 58U, HMI_COLOR_CARD);
  HMI_Fill(680U, 210U, 304U, 58U, HMI_COLOR_CARD);
  HMI_Fill(680U, 280U, 304U, 58U, HMI_COLOR_CARD);
  HMI_DrawText(694U, 146U, 90U, 22U, 0U, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, "Upp");
  HMI_DrawText(694U, 216U, 90U, 22U, 0U, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, "TRUE RMS");
  HMI_DrawText(694U, 286U, 90U, 22U, 0U, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, "FUNDAMENTAL");

  HMI_Fill(680U, 430U, 304U, 30U, HMI_COLOR_CARD);
  HMI_DrawText(688U, 434U, 60U, 20U, 0U, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, "ORDER");
  HMI_DrawText(768U, 434U, 110U, 20U, 0U, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, "FREQUENCY");
  HMI_DrawText(906U, 434U, 66U, 20U, 0U, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, "PEAK");
}

static int16_t HMI_MapWaveY(int16_t sample, int16_t minimum,
                            int32_t span)
{
  int32_t scaled = ((int32_t)sample - minimum) *
                   (HMI_TIME_H - 24U) / span;
  return (int16_t)(HMI_TIME_Y + HMI_TIME_H - 12U - scaled);
}

static void HMI_DrawWaveform(const int16_t *wave, uint32_t count)
{
  int16_t minimum = wave[0];
  int16_t maximum = wave[0];
  int32_t span;
  uint32_t i;
  uint16_t previous_x = HMI_TIME_X;
  int16_t previous_y;

  for (i = 1U; i < count; ++i)
  {
    if (wave[i] < minimum)
    {
      minimum = wave[i];
    }
    if (wave[i] > maximum)
    {
      maximum = wave[i];
    }
  }

  HMI_DrawTimeGrid();
  span = (int32_t)maximum - minimum;
  if (span < 16)
  {
    HMI_DrawLine(HMI_TIME_X,
                 (uint16_t)(HMI_TIME_Y + HMI_TIME_H / 2U),
                 (uint16_t)(HMI_TIME_X + HMI_TIME_W - 1U),
                 (uint16_t)(HMI_TIME_Y + HMI_TIME_H / 2U),
                 HMI_COLOR_WAVE);
    return;
  }

  previous_y = HMI_MapWaveY(wave[0], minimum, span);
  for (i = 5U; i < HMI_TIME_W; i += 5U)
  {
    uint32_t source = i * (count - 1U) / (HMI_TIME_W - 1U);
    uint16_t x = (uint16_t)(HMI_TIME_X + i);
    int16_t y = HMI_MapWaveY(wave[source], minimum, span);
    HMI_DrawLine(previous_x, (uint16_t)previous_y,
                 x, (uint16_t)y, HMI_COLOR_WAVE);
    previous_x = x;
    previous_y = y;
  }
}

static void HMI_DrawSpectrum(const Analyzer_Result_t *result)
{
  float maximum = 0.0f;
  uint32_t i;

  HMI_DrawSpectrumGrid();
  for (i = 0U; i < result->component_count; ++i)
  {
    if (result->component[i].amplitude_peak_mv > maximum)
    {
      maximum = result->component[i].amplitude_peak_mv;
    }
  }
  if (maximum <= 0.0f)
  {
    return;
  }

  for (i = 0U; i < result->component_count; ++i)
  {
    float frequency = result->component[i].frequency_hz;
    uint16_t x;
    uint16_t top;
    uint16_t bottom = (uint16_t)(HMI_SPECTRUM_Y +
                                 HMI_SPECTRUM_H - 12U);

    if ((frequency < 0.0f) || (frequency > HMI_SPECTRUM_MAX_HZ))
    {
      continue;
    }

    x = (uint16_t)(HMI_SPECTRUM_X +
                   frequency * (HMI_SPECTRUM_W - 1U) /
                   HMI_SPECTRUM_MAX_HZ);
    top = (uint16_t)(bottom -
                     (HMI_SPECTRUM_H - 28U) *
                     result->component[i].amplitude_peak_mv / maximum);
    HMI_DrawLine((uint16_t)(x - 2U), bottom,
                 (uint16_t)(x - 2U), top, HMI_COLOR_FREQ);
    HMI_DrawLine((uint16_t)(x - 1U), bottom,
                 (uint16_t)(x - 1U), top, HMI_COLOR_FREQ);
    HMI_DrawLine(x, bottom, x, top, HMI_COLOR_FREQ);
    HMI_DrawLine((uint16_t)(x + 1U), bottom,
                 (uint16_t)(x + 1U), top, HMI_COLOR_FREQ);
    HMI_DrawLine((uint16_t)(x + 2U), bottom,
                 (uint16_t)(x + 2U), top, HMI_COLOR_FREQ);
  }
}

static void HMI_SelectPeriods(uint8_t periods)
{
  if ((periods == 1U) || (periods == 3U))
  {
    hmi_periods = periods;
    SignalAnalyzer_SetDisplayPeriods(periods);
    HMI_DrawPeriodButtons();
  }
}

static void HMI_HandleTouchPacket(void)
{
  uint16_t x;
  uint16_t y;

  if ((hmi_rx_packet[0] != 0x67U) ||
      (hmi_rx_packet[6] != 0xFFU) ||
      (hmi_rx_packet[7] != 0xFFU) ||
      (hmi_rx_packet[8] != 0xFFU))
  {
    return;
  }

  x = (uint16_t)(((uint16_t)hmi_rx_packet[1] << 8U) |
                 hmi_rx_packet[2]);
  y = (uint16_t)(((uint16_t)hmi_rx_packet[3] << 8U) |
                 hmi_rx_packet[4]);

  if ((y >= 100U) && (y <= 140U))
  {
    if ((x >= 465U) && (x <= 552U))
    {
      HMI_SelectPeriods(1U);
    }
    else if ((x >= 553U) && (x <= 642U))
    {
      HMI_SelectPeriods(3U);
    }
  }
}

static void HMI_HandleRxByte(uint8_t byte)
{
  if (hmi_ascii_prefix != 0U)
  {
    hmi_ascii_prefix = 0U;
    if (byte == (uint8_t)'1')
    {
      HMI_SelectPeriods(1U);
      return;
    }
    if (byte == (uint8_t)'3')
    {
      HMI_SelectPeriods(3U);
      return;
    }
  }
  if (byte == (uint8_t)'A')
  {
    hmi_ascii_prefix = 1U;
    return;
  }

  if (hmi_rx_index == 0U)
  {
    if (byte == 0x67U)
    {
      hmi_rx_packet[hmi_rx_index++] = byte;
    }
    return;
  }

  hmi_rx_packet[hmi_rx_index++] = byte;
  if (hmi_rx_index == HMI_TOUCH_PACKET_SIZE)
  {
    HMI_HandleTouchPacket();
    hmi_rx_index = 0U;
  }
}

void HMI_Display_Init(void)
{
  HAL_Delay(500U);
  HMI_SetMcuBaudRate(9600U);
  HMI_SendCommand("baud=115200");
  HAL_Delay(80U);
  HMI_SetMcuBaudRate(115200U);
  HAL_Delay(80U);

  HMI_SendCommand("bkcmd=0");
  HMI_SendCommand("sendxy=1");
  HMI_SendCommand("page page0");
  HAL_Delay(40U);
  HMI_HideExampleObjects();
  HMI_DrawStaticUi();
  HMI_Display_ShowStatus("READY");
}

void HMI_Display_Service(void)
{
  uint8_t byte;

  while (HAL_UART_Receive(&HMI_UART, &byte, 1U, 0U) == HAL_OK)
  {
    HMI_HandleRxByte(byte);
  }
}

void HMI_Display_ShowStatus(const char *status)
{
  uint16_t color = (strcmp(status, "NO SIGNAL") == 0) ?
                   HMI_COLOR_ERROR : HMI_COLOR_OK;

  HMI_Fill(858U, 18U, 146U, 38U, HMI_COLOR_PANEL);
  HMI_DrawRect(858U, 18U, 146U, 38U, color);
  HMI_DrawCircle(876U, 37U, 5U, color);
  HMI_DrawText(890U, 25U, 102U, 24U, 0U, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, status);
}

void HMI_Display_Update(const Analyzer_Result_t *result,
                        const int16_t *display_wave,
                        uint32_t wave_count)
{
  char text[48];
  uint32_t i;

  if ((result == NULL) || (display_wave == NULL) || (wave_count < 2U))
  {
    return;
  }

  HMI_Fill(784U, 152U, 186U, 38U, HMI_COLOR_CARD);
  (void)snprintf(text, sizeof(text), "%.1f mV", result->upp_mv);
  HMI_DrawText(784U, 151U, 186U, 40U, 1U, HMI_COLOR_WAVE,
               HMI_COLOR_CARD, 2U, text);

  HMI_Fill(784U, 222U, 186U, 38U, HMI_COLOR_CARD);
  (void)snprintf(text, sizeof(text), "%.1f mV", result->urms_mv);
  HMI_DrawText(784U, 221U, 186U, 40U, 1U, HMI_COLOR_RMS,
               HMI_COLOR_CARD, 2U, text);

  HMI_Fill(784U, 292U, 186U, 38U, HMI_COLOR_CARD);
  (void)snprintf(text, sizeof(text), "%.3f kHz",
                 result->fundamental_hz / 1000.0f);
  HMI_DrawText(784U, 291U, 186U, 40U, 1U, HMI_COLOR_FREQ,
               HMI_COLOR_CARD, 2U, text);

  HMI_Fill(680U, 463U, 304U, 105U, HMI_COLOR_PANEL);
  for (i = 0U; i < ANALYZER_MAX_COMPONENTS; ++i)
  {
    uint16_t y = (uint16_t)(466U + i * 34U);
    if (i < result->component_count)
    {
      (void)snprintf(text, sizeof(text), "H%u",
                     result->component[i].harmonic_order);
      HMI_DrawText(690U, y, 52U, 28U, 0U, HMI_COLOR_TEXT,
                   HMI_COLOR_PANEL, 1U, text);
      (void)snprintf(text, sizeof(text), "%.2f kHz",
                     result->component[i].frequency_hz / 1000.0f);
      HMI_DrawText(758U, y, 126U, 28U, 0U, HMI_COLOR_RMS,
                   HMI_COLOR_PANEL, 1U, text);
      (void)snprintf(text, sizeof(text), "%.1f mV",
                     result->component[i].amplitude_peak_mv);
      HMI_DrawText(892U, y, 88U, 28U, 0U, HMI_COLOR_FREQ,
                   HMI_COLOR_PANEL, 2U, text);
    }
    else
    {
      HMI_DrawText(690U, y, 290U, 28U, 0U, HMI_COLOR_MUTED,
                   HMI_COLOR_PANEL, 1U, "--");
    }
  }

  HMI_DrawWaveform(display_wave, wave_count);
  HMI_DrawSpectrum(result);
  HMI_Display_ShowStatus("VALID");
}
