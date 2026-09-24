/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body -- UART WIRING MONITOR (diagnostic build)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  *
  *  TEMPORARY DIAGNOSTIC BUILD -- not the competition firmware.
  *
  *  1) LED1 mirrors PA10 (RX) and LED2 mirrors PA9 (TX).
  *     LEDs are active-low, so the pin level goes straight through:
  *     pin LOW -> LED lit, pin HIGH -> LED dark.
  *     => touch a GND jumper to a breadboard hole; when LED1 lights, that
  *        hole is electrically PA10.
  *  2) OLED shows live levels plus how many bytes USART1 has received.
  *     A rising RX count proves the whole CH340 -> board link works.
  *  3) USART1 = 9600 8N1, polled (no interrupts, so it cannot clash with
  *     the callbacks app.c already defines).
  *  4) KEY1 resets the counters.
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static volatile uint32_t rx_count = 0;
static volatile uint32_t rx_garbage = 0;
static volatile uint8_t  rx_last = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define KEY_NUM     4
#define DEBOUNCE_MS 15

static GPIO_TypeDef* const key_port[KEY_NUM] = {KEY1_GPIO_Port, KEY2_GPIO_Port, KEY3_GPIO_Port, KEY4_GPIO_Port};
static const uint16_t      key_pin [KEY_NUM] = {KEY1_Pin, KEY2_Pin, KEY3_Pin, KEY4_Pin};

/* Scan 4 keys with software debounce. Non-blocking.
   Returns a bitmask: bit0=KEY1 bit1=KEY2 bit2=KEY3 bit3=KEY4.
   (Kept because app.c calls it; here KEY1 is reused to clear the counters.) */
uint8_t key_scan(void)
{
    static uint8_t  last_raw[KEY_NUM] = {1,1,1,1};
    static uint8_t  stable  [KEY_NUM] = {1,1,1,1};
    static uint32_t t_change[KEY_NUM] = {0,0,0,0};
    uint8_t events = 0;
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < KEY_NUM; i++)
    {
        uint8_t raw = (HAL_GPIO_ReadPin(key_port[i], key_pin[i]) == GPIO_PIN_RESET) ? 0 : 1;

        if (raw != last_raw[i]) {
            last_raw[i] = raw;
            t_change[i] = now;
        }
        else if ((now - t_change[i]) >= DEBOUNCE_MS && raw != stable[i]) {
            stable[i] = raw;
            if (stable[i] == 0)
                events |= (1u << i);
        }
    }
    return events;
}

/* Polled USART1 receive: no interrupt, no callback, no clash with app.c.
   Any activity on PA10 -- even a wrong-baud byte -- bumps rx_garbage, so the
   counter alone tells you the wire is live. */
static void serial_poll(void)
{
    uint32_t sr = USART1->SR;

    if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        uint8_t b = (uint8_t)USART1->DR;   /* reading DR clears RXNE and the error flags */
        if (sr & USART_SR_RXNE) { rx_last = b; rx_count++; }
        else                    { rx_garbage++; }
    }
}

static void draw_monitor(void)
{
    char t[24];
    unsigned a9  = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9)  == GPIO_PIN_SET) ? 1u : 0u;
    unsigned a10 = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET) ? 1u : 0u;
    uint32_t c = rx_count;
    uint32_t g = rx_garbage;
    unsigned l = rx_last;

    OLED_Clear();
    OLED_ShowStr(0, 0, "UART MONITOR");
    snprintf(t, sizeof(t), "PA9=%u PA10=%u", a9, a10);
    OLED_ShowStr(1, 0, t);
    snprintf(t, sizeof(t), "RX=%lu", (unsigned long)c);
    OLED_ShowStr(2, 0, t);
    snprintf(t, sizeof(t), "ERR=%lu L=%02X", (unsigned long)g, l);
    OLED_ShowStr(3, 0, t);
    OLED_Refresh();
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  GPIO_PinState shown_a9, shown_a10;
  uint32_t last_draw, probe_tick;
  uint32_t shown_count = 0xFFFFFFFFu;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  OLED_Init();
  last_draw  = HAL_GetTick();
  probe_tick = last_draw;
  shown_a9   = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9);
  shown_a10  = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10);
  draw_monitor();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    GPIO_PinState pa9  = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9);
    GPIO_PinState pa10 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10);
    uint32_t now = HAL_GetTick();
    uint8_t  keys;

    serial_poll();

    /* LED1 follows PA10, LED2 follows PA9.  LEDs are active-low, so the
       electrical level is written straight through: LOW pin -> LED lit. */
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, pa10);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, pa9);

    keys = key_scan();
    if (keys & 0x01u) { rx_count = 0; rx_garbage = 0; rx_last = 0; }

    if (pa9 != shown_a9 || pa10 != shown_a10 || rx_count != shown_count ||
        (uint32_t)(now - last_draw) >= 250u)
    {
        shown_a9    = pa9;
        shown_a10   = pa10;
        shown_count = rx_count;
        last_draw   = now;
        draw_monitor();
    }
    if (!OLED_IsReady() && (uint32_t)(now - probe_tick) >= 1000u) {
        probe_tick = now;
        OLED_Init();
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */
  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */
  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LED1_Pin */
  GPIO_InitStruct.Pin = LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY1_Pin KEY3_Pin KEY4_Pin KEY2_Pin */
  GPIO_InitStruct.Pin = KEY1_Pin|KEY3_Pin|KEY4_Pin|KEY2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LED2_Pin */
  GPIO_InitStruct.Pin = LED2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED2_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
