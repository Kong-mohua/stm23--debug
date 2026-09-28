#ifndef __TRACK_H
#define __TRACK_H

#include <stdint.h>

/* YB-MVX05 8-way grayscale tracking sensor (analog multiplexed).
 *
 *   8 probes X1..X8 -> 3-bit channel select -> single analog output.
 *
 *   AD0 -> PB10      ADC value appears on OUT after a short settle time.
 *   AD1 -> PB11      OUT -> PA6 = ADC1_IN6 (12 bit, 0..4095)
 *   AD2 -> PB1
 *
 *   Powered from 3.3 V so the analog output stays inside the MCU range.
 *   A probe reading ABOVE g_track_thr counts as "on the line" when
 *   g_track_pol == 1 (flip to 0 if the polarity turns out inverted). */

#define TRACK_NO_LINE   999     /* g_track_pos when no probe sees the line */

void Track_Init(void);
void Track_Scan(void);          /* refresh all 8 channels (~1 ms) */

extern volatile uint16_t g_track_raw[8];   /* last ADC value per channel */
extern volatile uint8_t  g_track_mask;     /* bit i = channel i on the line */
extern volatile int16_t  g_track_pos;      /* -100..100, or TRACK_NO_LINE */
extern volatile uint16_t g_track_thr;      /* binarisation threshold */
extern volatile uint8_t  g_track_pol;      /* 1: line reads above thr */

#endif /* __TRACK_H */
