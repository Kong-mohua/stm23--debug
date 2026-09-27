#include "motor.h"

/* TIM2 on APB1 (8 MHz with HSI): 8 MHz / 800 = 10 kHz PWM */
#define MOTOR_PERIOD   799u

static TIM_HandleTypeDef htim2;

static void dir_pins(GPIO_PinState a1, GPIO_PinState a2,
                     GPIO_PinState b1, GPIO_PinState b2)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, a1);   /* IN1 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, a2);   /* IN2 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, b1);   /* IN3 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, b2);   /* IN4 */
}

void Motor_Init(void)
{
    GPIO_InitTypeDef gi = {0};
    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();

    /* PA0/PA1 = TIM2_CH1/CH2, wired to ENA/ENB on the L298N */
    gi.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gi.Mode = GPIO_MODE_AF_PP;
    gi.Pull = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gi);

    /* PA2..PA5 = IN1..IN4 */
    gi.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    gi.Mode = GPIO_MODE_OUTPUT_PP;
    gi.Pull = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gi);

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = MOTOR_PERIOD;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_2) != HAL_OK) Error_Handler();

    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

    Motor_Brake();      /* both channels off */
}

void Motor_Set(int16_t left, int16_t right)
{
    uint32_t pulse_l, pulse_r;

    /* direction first, then magnitude (ENA/ENB carry the PWM) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, (left  >= 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, (left  >= 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, (right >= 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, (right >= 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    if (left  < 0) left  = (int16_t)-left;
    if (right < 0) right = (int16_t)-right;
    if (left  > 100) left  = 100;
    if (right > 100) right = 100;

    pulse_l = (uint32_t)left  * (MOTOR_PERIOD + 1u) / 100u;
    pulse_r = (uint32_t)right * (MOTOR_PERIOD + 1u) / 100u;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_l);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulse_r);
}

void Motor_Brake(void)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);
    dir_pins(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
}
