#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>
/* Active buzzer module, signal only. Confirm board wiring before use. */
#define BUZZER_ENABLED 1
#define BUZZER_ACTIVE_HIGH 1
void Board_Init(void);
void Board_Beep(uint32_t now);
void Board_Tick(uint32_t now);
#endif
