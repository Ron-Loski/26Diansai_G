#include "hmi_display.h"

#include <stdio.h>
#include <string.h>

#include "usart.h"

#define HMI_UART                 huart2
#define HMI_TX_TIMEOUT_MS        80U
#define HMI_TOUCH_PACKET_SIZE    9U
#define HMI_RX_RING_SIZE         64U

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

#define HMI_FONT_UI              2U

#define HMI_TEXT_TITLE           "\xD6\xDC\xC6\xDA\xD0\xC5\xBA\xC5\xB2\xE2\xC1\xBF\xB7\xD6\xCE\xF6\xD7\xB0\xD6\xC3"
#define HMI_TEXT_SUBTITLE        "2026 \xC8\xAB\xB9\xFA\xB4\xF3\xD1\xA7\xC9\xFA\xB5\xE7\xD7\xD3\xC9\xE8\xBC\xC6\xBE\xBA\xC8\xFC - G\xCC\xE2"
#define HMI_TEXT_REAL_TIME       "\xCA\xB5\xCA\xB1"
#define HMI_TEXT_TIME_DOMAIN     "\xCA\xB1\xD3\xF2\xB2\xA8\xD0\xCE"
#define HMI_TEXT_SPECTRUM        "\xB5\xE7\xD1\xB9\xC6\xB5\xC6\xD7"
#define HMI_TEXT_MEASUREMENTS    "\xB2\xE2\xC1\xBF\xB2\xCE\xCA\xFD"
#define HMI_TEXT_COMPONENTS      "\xC6\xB5\xC2\xCA\xB7\xD6\xC1\xBF"
#define HMI_TEXT_PERIOD_1        "1\xD6\xDC\xC6\xDA"
#define HMI_TEXT_PERIOD_3        "3\xD6\xDC\xC6\xDA"
#define HMI_TEXT_PEAK_TO_PEAK    "\xB7\xE5\xB7\xE5\xD6\xB5"
#define HMI_TEXT_TRUE_RMS        "\xD5\xE6\xD3\xD0\xD0\xA7\xD6\xB5"
#define HMI_TEXT_FUND_FREQ       "\xBB\xF9\xB2\xA8\xC6\xB5\xC2\xCA"
#define HMI_TEXT_ORDER           "\xB4\xCE\xCA\xFD"
#define HMI_TEXT_FREQUENCY       "\xC6\xB5\xC2\xCA"
#define HMI_TEXT_PEAK            "\xB7\xE5\xD6\xB5"
#define HMI_TEXT_READY           "\xBE\xCD\xD0\xF7"
#define HMI_TEXT_VALID           "\xD3\xD0\xD0\xA7"
#define HMI_TEXT_NO_SIGNAL       "\xCE\xDE\xD0\xC5\xBA\xC5"

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
static uint8_t hmi_rx_irq_byte;
static volatile uint8_t hmi_rx_ring[HMI_RX_RING_SIZE];
static volatile uint8_t hmi_rx_head;
static volatile uint8_t hmi_rx_tail;
static uint8_t hmi_rx_irq_enabled;

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
  uint16_t background_1 =
      (hmi_periods == 1U) ? HMI_COLOR_CARD : HMI_COLOR_PANEL;
  uint16_t background_3 =
      (hmi_periods == 3U) ? HMI_COLOR_CARD : HMI_COLOR_PANEL;

  HMI_Fill(430U, 103U, 96U, 34U, background_1);
  HMI_Fill(536U, 103U, 96U, 34U, background_3);
  HMI_DrawRect(430U, 103U, 96U, 34U,
               (hmi_periods == 1U) ? selected : normal);
  HMI_DrawRect(536U, 103U, 96U, 34U,
               (hmi_periods == 3U) ? selected : normal);
  HMI_DrawText(430U, 107U, 96U, 24U, HMI_FONT_UI,
               (hmi_periods == 1U) ? HMI_COLOR_TEXT : HMI_COLOR_MUTED,
               background_1, 1U, HMI_TEXT_PERIOD_1);
  HMI_DrawText(536U, 107U, 96U, 24U, HMI_FONT_UI,
               (hmi_periods == 3U) ? HMI_COLOR_TEXT : HMI_COLOR_MUTED,
               background_3, 1U, HMI_TEXT_PERIOD_3);
}

static void HMI_DrawStaticUi(void)
{
  uint16_t i;

  HMI_SendCommand("cls 164");
  HMI_Fill(0U, 0U, 1024U, 72U, HMI_COLOR_HEADER);
  HMI_DrawLine(0U, 71U, 1023U, 71U, HMI_COLOR_BORDER);
  HMI_DrawCircle(31U, 35U, 14U, HMI_COLOR_WAVE);
  HMI_DrawText(55U, 12U, 500U, 38U, HMI_FONT_UI, HMI_COLOR_TEXT,
               HMI_COLOR_HEADER, 0U, HMI_TEXT_TITLE);
  HMI_DrawText(55U, 44U, 500U, 24U, HMI_FONT_UI, HMI_COLOR_MUTED,
               HMI_COLOR_HEADER, 0U, HMI_TEXT_SUBTITLE);

  HMI_Fill(858U, 18U, 146U, 38U, HMI_COLOR_PANEL);
  HMI_DrawRect(858U, 18U, 146U, 38U, HMI_COLOR_WAVE);
  HMI_DrawCircle(876U, 37U, 5U, HMI_COLOR_OK);
  HMI_DrawText(890U, 25U, 110U, 24U, HMI_FONT_UI, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, HMI_TEXT_REAL_TIME);

  HMI_Fill(20U, 92U, 620U, 270U, HMI_COLOR_PANEL);
  HMI_DrawRect(20U, 92U, 620U, 270U, HMI_COLOR_BORDER);
  HMI_Fill(660U, 92U, 344U, 270U, HMI_COLOR_PANEL);
  HMI_DrawRect(660U, 92U, 344U, 270U, HMI_COLOR_BORDER);
  HMI_Fill(20U, 382U, 620U, 198U, HMI_COLOR_PANEL);
  HMI_DrawRect(20U, 382U, 620U, 198U, HMI_COLOR_BORDER);
  HMI_Fill(660U, 382U, 344U, 198U, HMI_COLOR_PANEL);
  HMI_DrawRect(660U, 382U, 344U, 198U, HMI_COLOR_BORDER);

  HMI_DrawText(40U, 103U, 240U, 30U, HMI_FONT_UI, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, HMI_TEXT_TIME_DOMAIN);
  HMI_DrawText(40U, 393U, 300U, 30U, HMI_FONT_UI, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, HMI_TEXT_SPECTRUM);
  HMI_DrawText(680U, 103U, 280U, 30U, HMI_FONT_UI, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, HMI_TEXT_MEASUREMENTS);
  HMI_DrawText(680U, 393U, 280U, 30U, HMI_FONT_UI, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, HMI_TEXT_COMPONENTS);

  HMI_DrawTimeGrid();
  HMI_DrawSpectrumGrid();
  HMI_DrawPeriodButtons();

  HMI_DrawText(42U, 335U, 80U, 24U, HMI_FONT_UI, HMI_COLOR_MUTED,
               HMI_COLOR_PANEL, 0U, "-U");
  HMI_DrawText(575U, 335U, 42U, 24U, HMI_FONT_UI, HMI_COLOR_MUTED,
               HMI_COLOR_PANEL, 2U, "t");

  for (i = 0U; i <= 5U; ++i)
  {
    char label[16];
    (void)snprintf(label, sizeof(label), "%uk",
                   (unsigned int)(i * 100U));
    HMI_DrawText((uint16_t)(35U + i * 116U), 556U, 50U, 24U,
                 HMI_FONT_UI, HMI_COLOR_MUTED, HMI_COLOR_PANEL, 1U, label);
  }

  HMI_Fill(680U, 140U, 304U, 58U, HMI_COLOR_CARD);
  HMI_Fill(680U, 210U, 304U, 58U, HMI_COLOR_CARD);
  HMI_Fill(680U, 280U, 304U, 58U, HMI_COLOR_CARD);
  HMI_DrawText(694U, 146U, 116U, 24U, HMI_FONT_UI, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, HMI_TEXT_PEAK_TO_PEAK);
  HMI_DrawText(694U, 216U, 116U, 24U, HMI_FONT_UI, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, HMI_TEXT_TRUE_RMS);
  HMI_DrawText(694U, 286U, 116U, 24U, HMI_FONT_UI, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, HMI_TEXT_FUND_FREQ);

  HMI_Fill(680U, 430U, 304U, 30U, HMI_COLOR_CARD);
  HMI_DrawText(688U, 433U, 60U, 24U, HMI_FONT_UI, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, HMI_TEXT_ORDER);
  HMI_DrawText(768U, 433U, 110U, 24U, HMI_FONT_UI, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, HMI_TEXT_FREQUENCY);
  HMI_DrawText(906U, 433U, 66U, 24U, HMI_FONT_UI, HMI_COLOR_MUTED,
               HMI_COLOR_CARD, 0U, HMI_TEXT_PEAK);
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
    if ((x >= 425U) && (x <= 531U))
    {
      HMI_SelectPeriods(1U);
    }
    else if ((x >= 532U) && (x <= 637U))
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
  HMI_SendCommand("recmod=0");
  HMI_SendCommand("page page0");
  HAL_Delay(40U);
  HMI_SendCommand("sendxy=1");

  hmi_rx_head = 0U;
  hmi_rx_tail = 0U;
  hmi_rx_irq_enabled =
      (NVIC_GetEnableIRQ(USART2_IRQn) != 0U) ? 1U : 0U;
  if (hmi_rx_irq_enabled != 0U)
  {
    (void)HAL_UART_Receive_IT(&HMI_UART, &hmi_rx_irq_byte, 1U);
  }

  HMI_HideExampleObjects();
  HMI_DrawStaticUi();
  HMI_Display_ShowStatus(HMI_TEXT_READY);
}

void HMI_Display_Service(void)
{
  uint8_t byte;

  if (hmi_rx_irq_enabled != 0U)
  {
    while (hmi_rx_tail != hmi_rx_head)
    {
      byte = hmi_rx_ring[hmi_rx_tail];
      hmi_rx_tail =
          (uint8_t)((hmi_rx_tail + 1U) % HMI_RX_RING_SIZE);
      HMI_HandleRxByte(byte);
    }
    return;
  }

  while (HAL_UART_Receive(&HMI_UART, &byte, 1U, 0U) == HAL_OK)
  {
    HMI_HandleRxByte(byte);
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    uint8_t next =
        (uint8_t)((hmi_rx_head + 1U) % HMI_RX_RING_SIZE);

    if (next == hmi_rx_tail)
    {
      hmi_rx_tail =
          (uint8_t)((hmi_rx_tail + 1U) % HMI_RX_RING_SIZE);
    }
    hmi_rx_ring[hmi_rx_head] = hmi_rx_irq_byte;
    hmi_rx_head = next;
    (void)HAL_UART_Receive_IT(&HMI_UART, &hmi_rx_irq_byte, 1U);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    __HAL_UART_CLEAR_OREFLAG(huart);
    (void)HAL_UART_Receive_IT(&HMI_UART, &hmi_rx_irq_byte, 1U);
  }
}

void HMI_Display_ShowStatus(const char *status)
{
  const char *display_text = status;
  uint16_t color = HMI_COLOR_OK;

  if ((strcmp(status, "NO SIGNAL") == 0) ||
      (strcmp(status, HMI_TEXT_NO_SIGNAL) == 0))
  {
    display_text = HMI_TEXT_NO_SIGNAL;
    color = HMI_COLOR_ERROR;
  }
  else if (strcmp(status, "READY") == 0)
  {
    display_text = HMI_TEXT_READY;
  }
  else if (strcmp(status, "VALID") == 0)
  {
    display_text = HMI_TEXT_VALID;
  }

  HMI_Fill(858U, 18U, 146U, 38U, HMI_COLOR_PANEL);
  HMI_DrawRect(858U, 18U, 146U, 38U, color);
  HMI_DrawCircle(876U, 37U, 5U, color);
  HMI_DrawText(890U, 25U, 110U, 24U, HMI_FONT_UI, HMI_COLOR_TEXT,
               HMI_COLOR_PANEL, 0U, display_text);
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

  HMI_Fill(810U, 152U, 160U, 38U, HMI_COLOR_CARD);
  (void)snprintf(text, sizeof(text), "%.1f mV", result->upp_mv);
  HMI_DrawText(810U, 151U, 160U, 40U, HMI_FONT_UI, HMI_COLOR_WAVE,
               HMI_COLOR_CARD, 2U, text);

  HMI_Fill(810U, 222U, 160U, 38U, HMI_COLOR_CARD);
  (void)snprintf(text, sizeof(text), "%.1f mV", result->urms_mv);
  HMI_DrawText(810U, 221U, 160U, 40U, HMI_FONT_UI, HMI_COLOR_RMS,
               HMI_COLOR_CARD, 2U, text);

  HMI_Fill(810U, 292U, 160U, 38U, HMI_COLOR_CARD);
  (void)snprintf(text, sizeof(text), "%.3f kHz",
                 result->fundamental_hz / 1000.0f);
  HMI_DrawText(810U, 291U, 160U, 40U, HMI_FONT_UI, HMI_COLOR_FREQ,
               HMI_COLOR_CARD, 2U, text);

  HMI_Fill(680U, 463U, 304U, 105U, HMI_COLOR_PANEL);
  for (i = 0U; i < ANALYZER_MAX_COMPONENTS; ++i)
  {
    uint16_t y = (uint16_t)(466U + i * 34U);
    if (i < result->component_count)
    {
      (void)snprintf(text, sizeof(text), "%u\xB4\xCE",
                     result->component[i].harmonic_order);
      HMI_DrawText(690U, y, 52U, 28U, HMI_FONT_UI, HMI_COLOR_TEXT,
                   HMI_COLOR_PANEL, 1U, text);
      (void)snprintf(text, sizeof(text), "%.2f kHz",
                     result->component[i].frequency_hz / 1000.0f);
      HMI_DrawText(758U, y, 126U, 28U, HMI_FONT_UI, HMI_COLOR_RMS,
                   HMI_COLOR_PANEL, 1U, text);
      (void)snprintf(text, sizeof(text), "%.1f mV",
                     result->component[i].amplitude_peak_mv);
      HMI_DrawText(892U, y, 88U, 28U, HMI_FONT_UI, HMI_COLOR_FREQ,
                   HMI_COLOR_PANEL, 2U, text);
    }
    else
    {
      HMI_DrawText(690U, y, 290U, 28U, HMI_FONT_UI, HMI_COLOR_MUTED,
                   HMI_COLOR_PANEL, 1U, "--");
    }
  }

  HMI_DrawWaveform(display_wave, wave_count);
  HMI_DrawSpectrum(result);
  HMI_Display_ShowStatus(HMI_TEXT_VALID);
}
