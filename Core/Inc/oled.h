#ifndef __OLED_H
#define __OLED_H

#include "main.h"

/* 0.96" SSD1306 128x64, I2C, address 0x3C (8-bit 0x78) */

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Refresh(void);
uint8_t OLED_IsReady(void);

/* Chunked refresh (one page per call) for real-time screens:
   start with OLED_RefreshStart(), then call OLED_RefreshStep() once per
   loop pass; returns 1 when the whole frame has been pushed. */
void OLED_RefreshStart(void);
uint8_t OLED_RefreshStep(void);

/* row: 0~3 (each row is 16 pixels tall, uses 2 pages), x: column 0~127 */
void OLED_ShowStr(uint8_t row, uint8_t x, const char *s);   /* ASCII only, 8x16 */
void OLED_ShowMix(uint8_t row, uint8_t x, const char *s);   /* ASCII 8x16 + Chinese 16x16, UTF-8 */

#endif /* __OLED_H */
