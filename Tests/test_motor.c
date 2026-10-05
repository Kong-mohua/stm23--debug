#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "motor.h"
GPIO_TypeDef gpio_a, gpio_b, gpio_c;
static uint32_t pwm[2];
static uint16_t pins;
void Test_SetCompare(int channel, uint32_t value) { pwm[channel - 1] = value; }
void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *g) { (void)p; (void)g; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState state)
{
    assert(p == GPIOA);
    if (state) pins |= pin; else pins &= (uint16_t)~pin;
    /* Direction reversal may briefly assert both IN lines, but EN must
       already be off. Check real motor.c writes, not a motor mock. */
    if ((pins & (GPIO_PIN_2 | GPIO_PIN_3)) == (GPIO_PIN_2 | GPIO_PIN_3)) assert(!pwm[0]);
    if ((pins & (GPIO_PIN_4 | GPIO_PIN_5)) == (GPIO_PIN_4 | GPIO_PIN_5)) assert(!pwm[1]);
}
int HAL_TIM_PWM_Init(TIM_HandleTypeDef *h) { assert(h->Init.Period == 799); return HAL_OK; }
int HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *h, TIM_OC_InitTypeDef *o, int c)
{ (void)h; assert(o->Pulse == 0); (void)c; return HAL_OK; }
int HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, int c) { (void)h; (void)c; return HAL_OK; }
void Error_Handler(void) { abort(); }
int main(void)
{
    Motor_Init(); assert(!pwm[0] && !pwm[1] && !pins);
    Motor_Set(80, 80); assert(pwm[0] == 640 && pwm[1] == 640);
    Motor_Set(-80, -80); assert(pwm[0] == 640 && pwm[1] == 640);
    Motor_Set(100, 100); assert(pwm[0] == 800 && pwm[1] == 800);
    Motor_Set(INT16_MIN, INT16_MAX); assert(pwm[0] == 800 && pwm[1] == 800);
    assert((pins & GPIO_PIN_3) && (pins & GPIO_PIN_4));
    Motor_Set(0, 50); assert(pwm[0] == 0 && pwm[1] == 400);
    Motor_Brake(); assert(!pwm[0] && !pwm[1] && !pins);
    puts("PASS: real motor saturation/INT16_MIN/reversal sequencing/stop");
    return 0;
}
