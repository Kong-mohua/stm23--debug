#include "track.h"
#include "main.h"

/* 5-way DIGITAL tracking module (silk: L2 L1 M R1 R2 + VCC GND).
 *
 * Every channel is one digital level from the module's own comparator; the
 * black/white threshold is set by the trim pots on the module, so firmware
 * has nothing to threshold.  Wiring on the expansion board:
 *
 *   L2 -> PB13    L1 -> PB11    M -> PB10    R1 -> PA8    R2 -> PB9
 *
 * Inputs use the internal pull-up: a missing / unplugged wire then reads a
 * steady 1 instead of a floating mystery level.  (Historical note: with the
 * earlier analog module this file drove a 4051 mux and sampled ADC1_IN9;
 * the whole analog path is gone now.)
 *
 * g_track_pol == 1 means "on the line = level 1"; flip to 0 for low-active
 * modules.  A scan reports mask = 0 / TRACK_NO_LINE when no probe sees the
 * line, and the follow loop treats that as a lost line (stops after 1.5 s). */

volatile uint8_t g_track_bits;
volatile uint8_t g_track_mask;
volatile int16_t g_track_pos = TRACK_NO_LINE;
volatile uint8_t g_track_pol = 1u;      /* 1: line = level 1 */

void Track_Init(void)
{
    GPIO_InitTypeDef gi = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gi.Mode = GPIO_MODE_INPUT;
    gi.Pull = GPIO_PULLUP;
    gi.Pin = GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_13;
    HAL_GPIO_Init(GPIOB, &gi);
    gi.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOA, &gi);

    Track_Scan();
}

void Track_Scan(void)
{
    /* probe weights: channel 0 = L2 (leftmost) .. channel 4 = R2 */
    static const int16_t weight[5] = { -100, -50, 0, 50, 100 };
    uint8_t bits = 0, line, i, count = 0;
    int32_t sum = 0;

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13)) bits |= 0x01u;    /* L2 */
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11)) bits |= 0x02u;    /* L1 */
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10)) bits |= 0x04u;    /* M  */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8))  bits |= 0x08u;    /* R1 */
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9))  bits |= 0x10u;    /* R2 */
    g_track_bits = bits;

    line = g_track_pol ? bits : (uint8_t)(~bits & 0x1Fu);
    g_track_mask = line;

    for (i = 0; i < 5u; ++i) {
        if (line & (1u << i)) {
            sum += weight[i];
            ++count;
        }
    }
    g_track_pos = count ? (int16_t)(sum / (int32_t)count) : TRACK_NO_LINE;
}
