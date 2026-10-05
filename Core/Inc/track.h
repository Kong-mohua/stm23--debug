#ifndef __TRACK_H
#define __TRACK_H

#include <stdint.h>

/* YB-MVX05 8-way grayscale tracking sensor (analog multiplexed).
 *
 *   8 probes X1..X8 -> 3-bit channel select -> single analog output.
 *
 *   AD0 -> PB10      ADC value appears on OUT after a short settle time.
 *   AD1 -> PB11      OUT -> PB1 = ADC1_IN9 (12 bit, 0..4095)
 *   AD2 -> PA6       (PA6 carries the board's display "g" LED -- fine for a
 *                     digital output, unusable as analog input: clamped ~2.3V)
 *
 *   Powered from 3.3 V so the analog output stays inside the MCU range.
 *   A probe reading ABOVE g_track_thr counts as "on the line" when
 *   g_track_pol == 1 (flip to 0 if the polarity turns out inverted).
 *
 *   Fault handling: if any ADC conversion in a scan fails, that scan is
 *   reported as g_track_mask = 0 / g_track_pos = TRACK_NO_LINE instead of
 *   made-up data (a failed sample must never look like "every probe on the
 *   line" in low-active mode).  The follow loop then treats it like a lost
 *   line and stops after its timeout. */

#define TRACK_NO_LINE   999     /* g_track_pos when no probe sees the line */

void Track_Init(void);
void Track_Scan(void);          /* refresh all 8 channels (~1 ms) */
void Track_ApplySettings(void);
uint8_t Track_CalibrateWhite(void);
uint8_t Track_CalibrateBlack(void); /* rejects insufficient or inconsistent contrast */
extern volatile uint8_t g_track_valid;
extern volatile uint8_t g_track_calibrated_white;

extern volatile uint16_t g_track_raw[8];   /* last ADC value per channel */
extern volatile uint8_t  g_track_mask;     /* bit i = channel i on the line */
extern volatile int16_t  g_track_pos;      /* -100..100, or TRACK_NO_LINE */
extern volatile uint16_t g_track_thr;      /* binarisation threshold */
extern volatile uint8_t  g_track_pol;      /* 1: line reads above thr */

#endif /* __TRACK_H */
