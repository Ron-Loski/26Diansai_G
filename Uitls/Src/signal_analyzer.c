#include "signal_analyzer.h"

#include <math.h>
#include <string.h>

#include "arm_math.h"
#include "fpga_capture.h"

#define FFT_SIZE FPGA_CAPTURE_SAMPLE_COUNT
#define FFT_BINS (FFT_SIZE / 2U)
#define PI_F 3.14159265358979323846f
#define TWO_PI_F (2.0f * PI_F)
#define MIN_ANALYSIS_HZ 10000.0f
#define MAX_ANALYSIS_HZ 500000.0f
#define MIN_COMPONENT_PEAK_MV 2.5f
#define MIN_COMPONENT_RATIO 0.02f
#define COMPONENT_GATE_MARGIN 0.8f
#define NOISE_THRESHOLD_MULTIPLIER 6.0f
#define ADC_DIRECT_GAIN_CORRECTION 0.977656f
#define DEFAULT_INPUT_MV_PER_CODE \
  ((10000.0f / 4096.0f) * ADC_DIRECT_GAIN_CORRECTION)
#define CALIBRATION_VERSION 4U
#define CALIBRATION_FLASH_ADDRESS 0x081E0000UL
#define CALIBRATION_FLASH_WORDS 13U

typedef struct
{
  float frequency;
  float amplitude_code;
  uint8_t order;
} Peak_t;

static arm_cfft_instance_f32 FFT;
static float FFTBuffer[FFT_SIZE * 2U];
static float Magnitude[FFT_BINS];
static float Centered[FFT_SIZE];
static int16_t DisplayWave[ANALYZER_DISPLAY_POINTS];
static Analyzer_Calibration_t Calibration;
static uint8_t DisplayPeriods = 1U;

static uint32_t CRC32_Calculate(const uint8_t *data, uint32_t length)
{
  uint32_t crc = 0xFFFFFFFFU;
  uint32_t i;
  uint32_t bit;
  for (i = 0U; i < length; ++i)
  {
    crc ^= data[i];
    for (bit = 0U; bit < 8U; ++bit)
    {
      crc = (crc >> 1) ^ ((crc & 1U) != 0U ? 0xEDB88320U : 0U);
    }
  }
  return ~crc;
}

static float CalibrationInterpolate(const float *values, float frequency)
{
  float position;
  uint32_t lower;
  float fraction;

  if (frequency <= 10000.0f)
  {
    return values[0];
  }
  if (frequency >= 500000.0f)
  {
    return values[49];
  }
  position = frequency / 10000.0f - 1.0f;
  lower = (uint32_t)position;
  fraction = position - (float)lower;
  return values[lower] +
         fraction * (values[lower + 1U] - values[lower]);
}

static uint8_t SolveLinear(float matrix[7][8], uint32_t size,
                           float solution[7])
{
  uint32_t column;
  uint32_t row;
  uint32_t pivot;
  uint32_t j;

  for (column = 0U; column < size; ++column)
  {
    float largest = fabsf(matrix[column][column]);
    pivot = column;
    for (row = column + 1U; row < size; ++row)
    {
      float value = fabsf(matrix[row][column]);
      if (value > largest)
      {
        largest = value;
        pivot = row;
      }
    }
    if (largest < 1.0e-8f)
    {
      return 0U;
    }
    if (pivot != column)
    {
      for (j = column; j <= size; ++j)
      {
        float temporary = matrix[column][j];
        matrix[column][j] = matrix[pivot][j];
        matrix[pivot][j] = temporary;
      }
    }

    {
      float divisor = matrix[column][column];
      for (j = column; j <= size; ++j)
      {
        matrix[column][j] /= divisor;
      }
    }
    for (row = 0U; row < size; ++row)
    {
      if (row != column)
      {
        float factor = matrix[row][column];
        for (j = column; j <= size; ++j)
        {
          matrix[row][j] -= factor * matrix[column][j];
        }
      }
    }
  }

  for (row = 0U; row < size; ++row)
  {
    solution[row] = matrix[row][size];
  }
  return 1U;
}

static uint8_t JointFit(float fundamental, Peak_t *peaks, uint32_t count,
                        float coefficients[7])
{
  float normal[7][8];
  float basis[7];
  uint32_t size = 1U + 2U * count;
  uint32_t n;
  uint32_t row;
  uint32_t column;

  memset(normal, 0, sizeof(normal));
  for (n = 0U; n < FFT_SIZE; ++n)
  {
    float time = (float)n / FPGA_CAPTURE_SAMPLE_RATE_HZ;
    basis[0] = 1.0f;
    for (row = 0U; row < count; ++row)
    {
      float angle = TWO_PI_F * fundamental *
                    (float)peaks[row].order * time;
      basis[1U + 2U * row] = cosf(angle);
      basis[2U + 2U * row] = sinf(angle);
    }
    for (row = 0U; row < size; ++row)
    {
      normal[row][size] += basis[row] * Centered[n];
      for (column = 0U; column < size; ++column)
      {
        normal[row][column] += basis[row] * basis[column];
      }
    }
  }
  return SolveLinear(normal, size, coefficients);
}

static void SortPeaksByFrequency(Peak_t *peaks, uint32_t count)
{
  uint32_t i;
  uint32_t j;
  for (i = 0U; i < count; ++i)
  {
    for (j = i + 1U; j < count; ++j)
    {
      if (peaks[j].frequency < peaks[i].frequency)
      {
        Peak_t temporary = peaks[i];
        peaks[i] = peaks[j];
        peaks[j] = temporary;
      }
    }
  }
}

static uint32_t DetectPeaks(Peak_t peaks[ANALYZER_MAX_COMPONENTS])
{
  typedef struct
  {
    Peak_t peak;
    uint32_t bin;
  } Candidate_t;
  Candidate_t candidates[32];
  const float bin_hz = FPGA_CAPTURE_SAMPLE_RATE_HZ / (float)FFT_SIZE;
  uint32_t first_bin = (uint32_t)floorf(MIN_ANALYSIS_HZ / bin_hz);
  uint32_t last_bin = (uint32_t)ceilf(MAX_ANALYSIS_HZ / bin_hz);
  uint32_t bin;
  uint32_t count = 0U;
  uint32_t candidate_count = 0U;
  float noise_sum = 0.0f;
  uint32_t noise_count = 0U;
  float noise_amplitude;
  float threshold;
  float strongest = 0.0f;

  for (bin = first_bin; bin <= last_bin; ++bin)
  {
    noise_sum += Magnitude[bin];
    noise_count++;
  }
  noise_amplitude =
      (noise_count != 0U) ? (4.0f * noise_sum /
       ((float)noise_count * (float)FFT_SIZE)) : 0.0f;
  /*
   * Keep a 20% acquisition margin below the nominal 2.5 mV peak
   * component limit. A component exactly at the nominal limit otherwise
   * flickers as FFT interpolation and ADC noise move it across the gate.
   * The independent 6x noise gate still rejects an elevated noise floor.
   */
  threshold = fmaxf(
      MIN_COMPONENT_PEAK_MV * COMPONENT_GATE_MARGIN /
          DEFAULT_INPUT_MV_PER_CODE,
                    NOISE_THRESHOLD_MULTIPLIER * noise_amplitude);

  if (first_bin < 1U)
    first_bin = 1U;
  if (last_bin > FFT_BINS - 2U)
    last_bin = FFT_BINS - 2U;
  for (bin = first_bin; bin <= last_bin; ++bin)
  {
    float y0;
    float y1;
    float y2;
    float delta;
    float amplitude;
    uint32_t position;

    if (!(Magnitude[bin] > Magnitude[bin - 1U] &&
          Magnitude[bin] >= Magnitude[bin + 1U]))
    {
      continue;
    }
    y0 = Magnitude[bin - 1U];
    y1 = Magnitude[bin];
    y2 = Magnitude[bin + 1U];
    {
      float denominator = y0 - 2.0f * y1 + y2;
      delta = (fabsf(denominator) > 1.0e-20f) ?
          (0.5f * (y0 - y2) / denominator) : 0.0f;
    }
    if (delta < -0.5f)
      delta = -0.5f;
    if (delta > 0.5f)
      delta = 0.5f;
    amplitude = 4.0f * (y1 - 0.25f * (y0 - y2) * delta) /
                (float)FFT_SIZE;
    position = candidate_count;
    if (position > 31U)
      position = 31U;
    while (position > 0U &&
           candidates[position - 1U].peak.amplitude_code < amplitude)
    {
      if (position < 32U)
        candidates[position] = candidates[position - 1U];
      position--;
    }
    if (position < 32U)
    {
      candidates[position].peak.frequency =
          ((float)bin + delta) * bin_hz;
      candidates[position].peak.amplitude_code = amplitude;
      candidates[position].peak.order = 1U;
      candidates[position].bin = bin;
      if (candidate_count < 32U)
        candidate_count++;
    }
  }

  if (candidate_count != 0U)
    strongest = candidates[0].peak.amplitude_code;
  threshold = fmaxf(
      threshold,
      strongest * MIN_COMPONENT_RATIO * COMPONENT_GATE_MARGIN);
  for (bin = 0U; bin < candidate_count &&
                count < ANALYZER_MAX_COMPONENTS; ++bin)
  {
    uint8_t separated = 1U;
    uint32_t selected;
    if (candidates[bin].peak.amplitude_code < threshold)
      break;
    for (selected = 0U; selected < count; ++selected)
    {
      float difference = fabsf(candidates[bin].peak.frequency -
                               peaks[selected].frequency);
      if (difference < 8.0f * bin_hz)
      {
        separated = 0U;
        break;
      }
    }
    if (separated != 0U)
      peaks[count++] = candidates[bin].peak;
  }

  SortPeaksByFrequency(peaks, count);
  return count;
}

static float EstimateFundamental(Peak_t *peaks, uint32_t count)
{
  float fundamental = peaks[0].frequency;
  uint32_t iteration;
  uint32_t i;

  for (iteration = 0U; iteration < 3U; ++iteration)
  {
    float numerator = 0.0f;
    float denominator = 0.0f;
    for (i = 0U; i < count; ++i)
    {
      uint32_t order =
          (uint32_t)floorf(peaks[i].frequency / fundamental + 0.5f);
      if (order < 1U)
        order = 1U;
      if (order > 50U)
        order = 50U;
      peaks[i].order = (uint8_t)order;
      numerator += (float)order * peaks[i].frequency *
                   peaks[i].amplitude_code;
      denominator += (float)(order * order) * peaks[i].amplitude_code;
    }
    if (denominator > 0.0f)
      fundamental = numerator / denominator;
  }
  return fundamental;
}

void SignalAnalyzer_Init(void)
{
  uint32_t i;
  (void)arm_cfft_init_f32(&FFT, FFT_SIZE);
  memset(&Calibration, 0, sizeof(Calibration));
  Calibration.version = CALIBRATION_VERSION;
  for (i = 0U; i < 50U; ++i)
  {
    Calibration.mv_per_code[i] = DEFAULT_INPUT_MV_PER_CODE;
    Calibration.phase_correction_rad[i] = 0.0f;
  }
  Calibration.crc32 = CRC32_Calculate(
      (const uint8_t *)&Calibration,
      sizeof(Calibration) - sizeof(Calibration.crc32));
  (void)SignalAnalyzer_LoadCalibrationFromFlash();

  if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0U)
  {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  }
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint8_t SignalAnalyzer_Process(const int16_t *samples,
                               uint32_t frame_id,
                               uint32_t otr_count,
                               Analyzer_Result_t *result)
{
  Peak_t peaks[ANALYZER_MAX_COMPONENTS];
  float coefficients[7];
  float mean = 0.0f;
  float fundamental;
  float reconstructed_min = 1.0e30f;
  float reconstructed_max = -1.0e30f;
  float sum_square = 0.0f;
  const float frequency_margin_hz =
      FPGA_CAPTURE_SAMPLE_RATE_HZ / (float)FFT_SIZE;
  uint32_t start_cycles;
  uint32_t i;
  uint32_t component;
  uint32_t count;

  if (samples == NULL || result == NULL)
    return 0U;
  start_cycles = DWT->CYCCNT;
  memset(result, 0, sizeof(*result));
  memset(peaks, 0, sizeof(peaks));

  for (i = 0U; i < FFT_SIZE; ++i)
    mean += (float)samples[i];
  mean /= (float)FFT_SIZE;

  for (i = 0U; i < FFT_SIZE; ++i)
  {
    float window = 0.5f -
        0.5f * arm_cos_f32(TWO_PI_F * (float)i / (float)(FFT_SIZE - 1U));
    Centered[i] = (float)samples[i] - mean;
    FFTBuffer[2U * i] = Centered[i] * window;
    FFTBuffer[2U * i + 1U] = 0.0f;
  }

  arm_cfft_f32(&FFT, FFTBuffer, 0U, 1U);
  arm_cmplx_mag_f32(FFTBuffer, Magnitude, FFT_BINS);
  count = DetectPeaks(peaks);
  if (count == 0U)
    return 0U;

  fundamental = EstimateFundamental(peaks, count);
  if (fundamental < MIN_ANALYSIS_HZ - frequency_margin_hz ||
      fundamental > MAX_ANALYSIS_HZ + frequency_margin_hz)
    return 0U;
  if (fundamental < MIN_ANALYSIS_HZ)
    fundamental = MIN_ANALYSIS_HZ;
  /*
   * A valid 10 kHz input is biased slightly below the lower boundary by
   * FFT peak interpolation, so retain the 10 kHz clamp on that side.  At
   * 500 kHz, however, clamping a slightly high estimate before JointFit
   * introduces a frequency mismatch and frame-dependent amplitude error.
   * Keep the upper-bound estimate while the one-bin guard above continues
   * to reject signals that are genuinely outside the analysis band.
   */
  if (JointFit(fundamental, peaks, count, coefficients) == 0U)
    return 0U;

  result->frame_id = frame_id;
  result->fundamental_hz = fundamental;
  result->component_count = (uint8_t)count;
  result->otr_count = otr_count;

  for (component = 0U; component < count; ++component)
  {
    float cosine = coefficients[1U + 2U * component];
    float sine = coefficients[2U + 2U * component];
    float frequency = fundamental * (float)peaks[component].order;
    float scale = CalibrationInterpolate(Calibration.mv_per_code, frequency);
    float phase_correction = CalibrationInterpolate(
        Calibration.phase_correction_rad, frequency);
    float amplitude = sqrtf(cosine * cosine + sine * sine) * scale;
    float phase = atan2f(-sine, cosine) + phase_correction;

    result->component[component].harmonic_order = peaks[component].order;
    result->component[component].frequency_hz = frequency;
    result->component[component].amplitude_peak_mv = amplitude;
    result->component[component].phase_rad = phase;
    sum_square += 0.5f * amplitude * amplitude;
  }
  result->urms_mv = sqrtf(sum_square);

  for (i = 0U; i < FFT_SIZE; ++i)
  {
    float phase_position = TWO_PI_F * (float)i / (float)FFT_SIZE;
    float value = 0.0f;
    for (component = 0U; component < count; ++component)
    {
      value += result->component[component].amplitude_peak_mv *
          cosf((float)result->component[component].harmonic_order *
               phase_position + result->component[component].phase_rad);
    }
    if (value < reconstructed_min)
      reconstructed_min = value;
    if (value > reconstructed_max)
      reconstructed_max = value;
  }
  result->upp_mv = reconstructed_max - reconstructed_min;

  for (i = 0U; i < ANALYZER_DISPLAY_POINTS; ++i)
  {
    float phase_position = TWO_PI_F * (float)DisplayPeriods *
                           (float)i /
                           (float)(ANALYZER_DISPLAY_POINTS - 1U);
    float value = 0.0f;
    for (component = 0U; component < count; ++component)
    {
      value += result->component[component].amplitude_peak_mv *
          cosf((float)result->component[component].harmonic_order *
               phase_position + result->component[component].phase_rad);
    }
    if (value > 32767.0f)
      value = 32767.0f;
    if (value < -32768.0f)
      value = -32768.0f;
    DisplayWave[i] = (int16_t)value;
  }

  result->processing_us =
      (DWT->CYCCNT - start_cycles) / (SystemCoreClock / 1000000U);
  result->valid = 1U;
  return 1U;
}

const int16_t *SignalAnalyzer_GetDisplayWave(void)
{
  return DisplayWave;
}

void SignalAnalyzer_SetDisplayPeriods(uint8_t periods)
{
  DisplayPeriods = (periods == 3U) ? 3U : 1U;
}

uint8_t SignalAnalyzer_GetDisplayPeriods(void)
{
  return DisplayPeriods;
}

const Analyzer_Calibration_t *SignalAnalyzer_GetCalibration(void)
{
  return &Calibration;
}

uint8_t SignalAnalyzer_SetCalibration(const Analyzer_Calibration_t *table)
{
  uint32_t crc;
  uint32_t i;
  if (table == NULL || table->version != CALIBRATION_VERSION)
    return 0U;
  crc = CRC32_Calculate((const uint8_t *)table,
                       sizeof(*table) - sizeof(table->crc32));
  if (crc != table->crc32)
    return 0U;
  for (i = 0U; i < 50U; ++i)
  {
    if (!isfinite(table->mv_per_code[i]) ||
        table->mv_per_code[i] <= 0.0f ||
        !isfinite(table->phase_correction_rad[i]))
      return 0U;
  }
  Calibration = *table;
  return 1U;
}

uint8_t SignalAnalyzer_LoadCalibrationFromFlash(void)
{
  const Analyzer_Calibration_t *stored =
      (const Analyzer_Calibration_t *)CALIBRATION_FLASH_ADDRESS;
  return SignalAnalyzer_SetCalibration(stored);
}

uint8_t SignalAnalyzer_SaveCalibrationToFlash(
    const Analyzer_Calibration_t *table)
{
  FLASH_EraseInitTypeDef erase;
  uint32_t sector_error = 0U;
  uint32_t offset;
  static uint32_t flash_words[CALIBRATION_FLASH_WORDS][8]
      __attribute__((aligned(32)));

  if (SignalAnalyzer_SetCalibration(table) == 0U)
    return 0U;

  memset(flash_words, 0xFF, sizeof(flash_words));
  memcpy(flash_words, table, sizeof(*table));
  memset(&erase, 0, sizeof(erase));
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_2;
  erase.Sector = FLASH_SECTOR_7;
  erase.NbSectors = 1U;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

  if (HAL_FLASH_Unlock() != HAL_OK)
    return 0U;
  if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
  {
    (void)HAL_FLASH_Lock();
    return 0U;
  }
  for (offset = 0U; offset < CALIBRATION_FLASH_WORDS; ++offset)
  {
    if (HAL_FLASH_Program(
            FLASH_TYPEPROGRAM_FLASHWORD,
            CALIBRATION_FLASH_ADDRESS + offset * 32U,
            (uint32_t)&flash_words[offset][0]) != HAL_OK)
    {
      (void)HAL_FLASH_Lock();
      return 0U;
    }
  }
  (void)HAL_FLASH_Lock();
  return SignalAnalyzer_LoadCalibrationFromFlash();
}
