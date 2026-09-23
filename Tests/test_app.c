#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stm32f1xx_hal.h"
GPIO_TypeDef gpio_a, gpio_b, gpio_c;
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;
static uint32_t tick;
static uint16_t input = 0xffff;
static uint8_t output1, output2;
static int i2c_fail, transfers;
static char transmitted[2048];
uint32_t HAL_GetTick(void) { return tick; }
void HAL_Delay(uint32_t ms) { tick += ms; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s) {
    if (p == GPIOC && pin == GPIO_PIN_13) output1 = (s == GPIO_PIN_RESET);
    if (p == GPIOB && pin == GPIO_PIN_5) output2 = (s == GPIO_PIN_RESET);
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin) { (void)p; return (input & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET; }
void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *g) { (void)p; (void)g; }
void HAL_NVIC_SetPriority(int a, int b, int c) { (void)a; (void)b; (void)c; }
void HAL_NVIC_EnableIRQ(int a) { (void)a; }
int HAL_UART_Init(UART_HandleTypeDef *p) { (void)p; return HAL_OK; }
int HAL_UART_Receive_IT(UART_HandleTypeDef *p, uint8_t *b, uint16_t n) { (void)p; (void)b; (void)n; return HAL_OK; }
int HAL_UART_Transmit(UART_HandleTypeDef *p, uint8_t *b, uint16_t n, uint32_t timeout) {
    (void)p; (void)timeout;
    size_t used = strlen(transmitted);
    assert(used + n < sizeof(transmitted));
    memcpy(transmitted + used, b, n); transmitted[used + n] = 0;
    return HAL_OK;
}
void HAL_UART_IRQHandler(UART_HandleTypeDef *p) { (void)p; }
int HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *p, uint16_t a, uint32_t n, uint32_t ms) { (void)p; (void)a; (void)n; (void)ms; return i2c_fail; }
int HAL_I2C_Master_Transmit(I2C_HandleTypeDef *p, uint16_t a, uint8_t *b, uint16_t n, uint32_t ms) {
    (void)p; (void)a; (void)b; (void)n; (void)ms; transfers++; return i2c_fail;
}
void Error_Handler(void) { abort(); }
#include "../Core/Src/oled.c"
#include "../Core/Src/app.c"
#include "key_scan.inc"

static void press(uint16_t pin) {
    input &= (uint16_t)~pin; App_Tick(); tick += 16; App_Tick();
    input |= pin; App_Tick(); tick += 16; App_Tick();
}
static void send(const char *s) {
    while (*s) { rx_byte = (uint8_t)*s++; HAL_UART_RxCpltCallback(&uart); }
    while (rx_head != rx_tail) serial_poll();
}
int main(void) {
    int32_t n;
    assert(parse_number("2147483647", &n) && n == INT32_MAX);
    assert(parse_number("-2147483648", &n) && n == INT32_MIN);
    assert(!parse_number("2147483648", &n));
    assert(!parse_number("-2147483649", &n));
    assert(!parse_number("", &n) && !parse_number("-", &n));
    assert(!parse_number("12x", &n));
    App_Init();
    assert(page == 0 && selected == 0 && !output1 && !output2);
    press(KEY1_Pin); assert(selected == 3);
    press(KEY2_Pin); assert(selected == 0);
    press(KEY3_Pin); assert(page == 1);
    press(KEY1_Pin); assert(output1 && output2);
    press(KEY1_Pin); assert(!output1 && !output2);
    press(KEY2_Pin); assert(output1 != output2);
    for (int i = 0; i < 180; ++i) {
        uint8_t previous = output1;
        tick += 500; App_Tick();
        assert(output1 != output2 && output1 != previous);
    }
    press(KEY4_Pin); assert(page == 0 && !output1 && !output2);
    press(KEY2_Pin); press(KEY3_Pin); assert(page == 2);
    send("111\r\nGET A\r\n"); assert(value_a == 111 && !strcmp(transmitted, "OK\r\n111\r\n"));
    transmitted[0] = 0;
    send("0\nGET A\n-99\nGET A\n");
    assert(value_a == -99 && !strcmp(transmitted, "OK\r\n0\r\nOK\r\n-99\r\n"));
    send("999999999999999999999999999999999999\n"); assert(value_a == -99);
    send("2147483648\n"); assert(value_a == -99);
    /* UART overflow must never turn a truncated frame into a valid number. */
    for (int i = 0; i < 150; ++i) { rx_byte = '1'; HAL_UART_RxCpltCallback(&uart); }
    serial_poll(); send("\n25\n"); assert(value_a == 25);
    press(KEY4_Pin); press(KEY2_Pin); press(KEY3_Pin); assert(page == 3);
    press(KEY4_Pin); press(KEY2_Pin); press(KEY3_Pin); assert(page == 4);
    press(KEY4_Pin); assert(page == 0);
    /* Right clipping must not overwrite the start of the row. */
    OLED_Clear(); OLED_ShowStr(0, 0, "A");
    uint8_t first[8]; memcpy(first, fb[0], 8);
    OLED_ShowStr(0, 120, "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
    assert(!memcmp(first, fb[0], 8));
    uint8_t snapshot[sizeof(fb)]; memcpy(snapshot, fb, sizeof(fb));
    OLED_ShowStr(4, 0, "bad"); OLED_ShowMix(255, 0, "bad");
    OLED_ShowStr(0, 250, "bad"); OLED_ShowStr(0, 0, NULL);
    assert(!memcmp(snapshot, fb, sizeof(fb)));
    OLED_ShowMix(0, 0, "\xE4"); OLED_ShowMix(0, 0, "\xE4\xB8");
    OLED_ShowMix(0, 0, "\xF0\x9F\x98\x80");
    OLED_ShowMix(0, 0, "\xED\xA0\x80");
    i2c_fail = 1; transfers = 0; OLED_Refresh();
    assert(!OLED_IsReady() && transfers == 1);
    OLED_Refresh(); assert(transfers == 1);
    i2c_fail = 0; tick += 1000; App_Tick(); assert(OLED_IsReady());
    /* Debounce across the 32-bit millisecond counter wrap. */
    input &= (uint16_t)~KEY1_Pin; tick = UINT32_MAX - 8; assert(key_scan() == 0);
    tick = 10; assert(key_scan() == 1); assert(key_scan() == 0);
    puts("PASS: menus, 90s LED simulation, keys, UART signed bounds/overflow, OLED clipping/recovery, tick rollover");
    return 0;
}
