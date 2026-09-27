#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h"

/* L298N dual motor driver (left/right wheel, 6 signal wires to the MCU).
 *
 *   ENA -> PA0 (TIM2_CH1)      IN1 -> PA2   IN2 -> PA3      A = left wheel
 *   ENB -> PA1 (TIM2_CH2)      IN3 -> PA4   IN4 -> PA5      B = right wheel
 *
 * Remove the ENA / ENB jumper caps on the L298N module, otherwise PWM speed
 * control is bypassed.  Speeds are percentages: -100 .. 0 .. +100. */

void Motor_Init(void);
void Motor_Set(int16_t left, int16_t right);
void Motor_Brake(void);     /* both channels off (coast) */

#endif /* __MOTOR_H */
