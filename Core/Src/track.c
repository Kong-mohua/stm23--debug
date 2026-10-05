#include "track.h"
#include "main.h"
#include "settings.h"

/* YB-MVX05 8-way grayscale tracking sensor, analog multiplexed.
 *
 * One measurement = set the 3 mux address pins, let the analog switch and
 * the probe output settle, then convert OUT (PB1 / ADC1_IN9) to 12 bits.
 * Scan loop reads all 8 probes into g_track_raw[], then derives a 3-bit
 * "on the line" mask and a signed position (-100 .. +100).
 *
 * Pin choice note: PA6 was tried first but is unusable as an analog input
 * on this expansion board -- the on-board 4-digit display hangs its "g"
 * segment LED on PA6 (common anode tied to 3.3V), which clamps the pin
 * around ~2.3V. PB1 is not connected to anything else on the board.
 *
 * Fault handling: a failed ADC conversion is NOT data.  If any conversion
 * in a scan fails, that scan reports mask = 0 / pos = TRACK_NO_LINE; with
 * low-active polarity a "0" sample would otherwise look like every probe
 * sitting on the line (centered) and the stop-on-line-loss logic would
 * never trigger. */

volatile uint16_t g_track_raw[8];
volatile uint8_t  g_track_mask;
volatile int16_t  g_track_pos = TRACK_NO_LINE;
volatile uint16_t g_track_thr = 2048u;
volatile uint8_t  g_track_pol = 1u;     /* 1: line = raw ABOVE thr */
volatile uint8_t g_track_valid, g_track_calibrated_white;
static uint16_t thresholds[8], white[8];

static ADC_HandleTypeDef hadc1;

/* ~50 us settle: mux switch time + probe output RC. Accuracy is not
   critical here, this only needs to be long enough. */
static void settle_delay(void)
{
    volatile uint32_t n = 100u;
    while (n--) { }
}

/* Convert OUT once.  Returns 1 on success, 0 if the ADC never completed
   (in that case *out keeps its previous value). */
static uint8_t adc_read(uint16_t *out)
{
    uint8_t ok = 0u;

    if (HAL_ADC_Start(&hadc1) == HAL_OK) {
        if (HAL_ADC_PollForConversion(&hadc1, 2u) == HAL_OK) {
            *out = (uint16_t)HAL_ADC_GetValue(&hadc1);
            ok = 1u;
        }
        HAL_ADC_Stop(&hadc1);
    }
    return ok;
}

void Track_Init(void)
{
    GPIO_InitTypeDef gi = {0};
    ADC_ChannelConfTypeDef ch = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    /* AD0 (PB10), AD1 (PB11), AD2 (PA6): mux channel select */
    gi.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    gi.Mode = GPIO_MODE_OUTPUT_PP;
    gi.Pull = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gi);
    gi.Pin = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOA, &gi);

    /* OUT (PB1): analog input, ADC1_IN9 */
    gi.Pin = GPIO_PIN_1;
    gi.Mode = GPIO_MODE_ANALOG;
    gi.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &gi);

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) Error_Handler();

    ch.Channel = ADC_CHANNEL_9;
    ch.Rank = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();

    Track_ApplySettings();
    g_track_calibrated_white = 0;
    Track_Scan();
}

void Track_Scan(void)
{
    /* probe weights: channel 0 = leftmost (-100) .. channel 7 (right) */
    static const int16_t weight[8] = { -100, -71, -43, -14, 14, 43, 71, 100 };
    uint8_t i, count = 0, ok = 1u;
    uint8_t mask = 0;
    int32_t sum = 0;
    uint16_t v;

    for (i = 0; i < 8u; ++i) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, (i & 1u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, (i & 2u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6,  (i & 4u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        settle_delay();
        v = 0u;
        if (!adc_read(&v)) { ok = 0u; break; } /* fail fast, not 16 timeouts */
        if (!adc_read(&v)) { ok = 0u; break; }
        g_track_raw[i] = v;
    }

    if (!ok) {
        /* Fault scan: report "no line" (and no probe hits) rather than
           letting zeroed samples masquerade as valid data.  The follow
           loop treats it exactly like a lost line and stops after its
           timeout. */
        g_track_mask = 0u;
        g_track_pos = TRACK_NO_LINE;
        g_track_valid = 0;
        return;
    }

    for (i = 0; i < 8u; ++i) {
        v = g_track_raw[i];
        if (g_track_pol ? (v > thresholds[i]) : (v < thresholds[i])) {
            mask |= (uint8_t)(1u << i);
            sum += weight[i];
            ++count;
        }
    }
    g_track_mask = mask;
    g_track_pos = count ? (int16_t)(sum / (int32_t)count) : TRACK_NO_LINE;
    g_track_valid = 1;
}

void Track_ApplySettings(void)
{
    for (uint8_t i = 0; i < 8; ++i) thresholds[i] = settings.threshold[i];
    g_track_thr = thresholds[0]; /* retained for diagnostic scripts */
    g_track_pol = settings.polarity;
}
uint8_t Track_CalibrateWhite(void)
{
    Track_Scan();
    if (!g_track_valid) { g_track_calibrated_white = 0; return 0; }
    for (uint8_t i = 0; i < 8; ++i) white[i] = g_track_raw[i];
    g_track_calibrated_white = 1;
    return 1;
}
uint8_t Track_CalibrateBlack(void)
{
    uint16_t candidate[8];
    uint8_t polarity;
    if (!g_track_calibrated_white) return 0;
    Track_Scan();
    if (!g_track_valid) return 0;
    polarity = g_track_raw[0] > white[0];
    for (uint8_t i = 0; i < 8; ++i) {
        int32_t delta = (int32_t)g_track_raw[i] - white[i];
        if ((delta > 0) != polarity || (delta > -100 && delta < 100)) return 0;
        candidate[i] = (uint16_t)((g_track_raw[i] + white[i]) / 2u);
    }
    for (uint8_t i = 0; i < 8; ++i) settings.threshold[i] = candidate[i];
    settings.polarity = polarity;
    settings.calibrated = 1;
    Track_ApplySettings();
    g_track_calibrated_white = 0;
    return 1;
}
