#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef struct { int unused; } GPIO_TypeDef;
extern GPIO_TypeDef gpio_a, gpio_b, gpio_c;
#define GPIOA (&gpio_a)
#define GPIOB (&gpio_b)
#define GPIOC (&gpio_c)
#define GPIO_PIN_1 (1u<<1)
#define GPIO_PIN_5 (1u<<5)
#define GPIO_PIN_6 (1u<<6)
#define GPIO_PIN_8 (1u<<8)
#define GPIO_PIN_9 (1u<<9)
#define GPIO_PIN_10 (1u<<10)
#define GPIO_PIN_11 (1u<<11)
#define GPIO_PIN_12 (1u<<12)
#define GPIO_PIN_13 (1u<<13)
#define GPIO_PIN_14 (1u<<14)
#define GPIO_PIN_15 (1u<<15)
typedef int GPIO_PinState;
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define GPIO_MODE_AF_PP 0
#define GPIO_MODE_INPUT 0
#define GPIO_MODE_OUTPUT_PP 0
#define GPIO_MODE_ANALOG 0
#define GPIO_SPEED_FREQ_HIGH 0
#define GPIO_SPEED_FREQ_LOW 0
#define GPIO_PULLUP 0
#define GPIO_NOPULL 0
typedef struct { uint32_t Pin, Mode, Speed, Pull; } GPIO_InitTypeDef;
typedef struct { int unused; } I2C_HandleTypeDef;
typedef struct {
    void *Instance;
    struct { uint32_t BaudRate, WordLength, StopBits, Parity, Mode, HwFlowCtl, OverSampling; } Init;
    int RxState;
} UART_HandleTypeDef;
typedef struct {
    void *Instance;
    struct { uint32_t ScanConvMode, ContinuousConvMode, DiscontinuousConvMode,
                    ExternalTrigConv, DataAlign, NbrOfConversion; } Init;
} ADC_HandleTypeDef;
typedef struct { uint32_t Channel, Rank, SamplingTime; } ADC_ChannelConfTypeDef;
#define ADC1 ((void*)2)
#define ADC_CHANNEL_9 9
#define ADC_REGULAR_RANK_1 1
#define ADC_SAMPLETIME_55CYCLES_5 0
#define ADC_SCAN_DISABLE 0
#define ADC_SOFTWARE_START 0
#define ADC_DATAALIGN_RIGHT 0
#define DISABLE 0
#define ENABLE 1
#define USART1 ((void*)1)
#define USART1_IRQn 1
#define UART_WORDLENGTH_8B 0
#define UART_STOPBITS_1 0
#define UART_PARITY_NONE 0
#define UART_MODE_TX_RX 0
#define UART_HWCONTROL_NONE 0
#define UART_OVERSAMPLING_16 0
#define HAL_UART_STATE_READY 0
#define HAL_OK 0
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_ADC1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_USART1_CLK_ENABLE() ((void)0)
static inline uint32_t __get_PRIMASK(void) { return 0; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t x) { (void)x; }
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
void HAL_GPIO_WritePin(GPIO_TypeDef *, uint16_t, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *, uint16_t);
void HAL_GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *);
void HAL_NVIC_SetPriority(int, int, int);
void HAL_NVIC_EnableIRQ(int);
int HAL_UART_Init(UART_HandleTypeDef *);
int HAL_UART_Receive_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
int HAL_UART_Transmit(UART_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
void HAL_UART_IRQHandler(UART_HandleTypeDef *);
int HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *, uint16_t, uint32_t, uint32_t);
int HAL_I2C_Master_Transmit(I2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t);
int HAL_ADC_Init(ADC_HandleTypeDef *);
int HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *);
int HAL_ADC_ConfigChannel(ADC_HandleTypeDef *, ADC_ChannelConfTypeDef *);
int HAL_ADC_Start(ADC_HandleTypeDef *);
int HAL_ADC_Stop(ADC_HandleTypeDef *);
int HAL_ADC_PollForConversion(ADC_HandleTypeDef *, uint32_t);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *);
#endif
