#ifndef __TRACK_H
#define __TRACK_H

#include <stdint.h>

/* 5-way DIGITAL tracking module (silk: L2 L1 M R1 R2 + VCC GND).
 *
 * Every channel is one digital level from the module's own comparator; the
 * black/white threshold is set by the trim pots on the module, so firmware
 * has nothing to threshold.  Wiring on the expansion board:
 *
 *   L2 -> PB13    L1 -> PB11    M -> PB10    R1 -> PA8    R2 -> PB9
 *
 * Inputs use the internal pull-up: a missing / unplugged wire then reads a
 * steady 1 instead of a floating mystery level.
 *
 *   g_track_pol == 1  ->  "on the line = level 1" (the usual black->high
 *                         module behaviour); set to 0 if it reads inverted.
 *
 *   g_track_pos: weighted average of the probes at -100 / -50 / 0 / +50 /
 *   +100, or TRACK_NO_LINE when no probe sees the line (the follow loop
 *   treats that as a lost line and stops after its timeout). */

#define TRACK_NO_LINE   999     /* g_track_pos when no probe sees the line */

void Track_Init(void);
void Track_Scan(void);          /* refresh all 5 channels (plain GPIO reads) */

extern volatile uint8_t g_track_bits;  /* raw 5-bit level pattern, bit0 = L2 */
extern volatile uint8_t g_track_mask;  /* line-detect pattern (after g_track_pol) */
extern volatile int16_t g_track_pos;   /* -100..100, or TRACK_NO_LINE */
extern volatile uint8_t g_track_pol;   /* 1: line = high level */

#endif /* __TRACK_H */
