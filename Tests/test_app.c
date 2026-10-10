#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stm32f1xx_hal.h"
GPIO_TypeDef gpio_a, gpio_b, gpio_c;
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;
static uint32_t tick;
static uint8_t output1, output2;
static int i2c_fail, transfers;
static char transmitted[2048];
uint32_t HAL_GetTick(void) { return tick; }
void HAL_Delay(uint32_t ms) { tick += ms; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s) {
    if (p == GPIOC && pin == GPIO_PIN_13) output1 = (s == GPIO_PIN_RESET);
    if (p == GPIOB && pin == GPIO_PIN_5) output2 = (s == GPIO_PIN_RESET);
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin) {
    return (p->IDR & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
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

/* ---- stubs for the modules that are not compiled on the host ---- */

/* motor.c (L298N driver): record the last differential command */
static int motor_left, motor_right, motor_brakes, motor_inits;
void Motor_Init(void) { motor_inits++; Motor_Set(0, 0); }
void Motor_Set(int16_t left, int16_t right) { motor_left = left; motor_right = right; }
void Motor_Brake(void) { motor_brakes++; Motor_Set(0, 0); }

/* main.c (USER CODE 4): I2C bus recovery, called by OLED_Init on failure */
static int i2c_recovers;
void I2C1_RecoverBus(void) { i2c_recovers++; }

/* track.c is compiled as a real translation unit: it reads the 5 sensor
   levels straight from gpio_a.IDR / gpio_b.IDR (see sensors() below). */

/* Sensor simulation.  bit0 = L2 (PB13), bit1 = L1 (PB11), bit2 = M (PB10),
   bit3 = R1 (PA8), bit4 = R2 (PB9); a 1 is the level the module drives. */
#define SENSOR_BBITS (GPIO_PIN_13 | GPIO_PIN_11 | GPIO_PIN_10 | GPIO_PIN_9)
static void sensors(uint8_t b) {
    gpio_b.IDR = (uint16_t)((gpio_b.IDR & ~SENSOR_BBITS) |
        ((b & 0x01u) ? GPIO_PIN_13 : 0) | ((b & 0x02u) ? GPIO_PIN_11 : 0) |
        ((b & 0x04u) ? GPIO_PIN_10 : 0) | ((b & 0x10u) ? GPIO_PIN_9 : 0));
    gpio_a.IDR = (uint16_t)((gpio_a.IDR & ~GPIO_PIN_8) | ((b & 0x08u) ? GPIO_PIN_8 : 0));
}

static void press(uint16_t pin) {
    gpio_b.IDR &= (uint16_t)~pin; App_Tick(); tick += 16; App_Tick();
    gpio_b.IDR |= pin; App_Tick(); tick += 16; App_Tick();
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
    gpio_a.IDR = 0xFFFF; gpio_b.IDR = 0xFFFF;    /* keys released ... */
    sensors(0x00);                               /* ... sensors see white */
    App_Init();
    assert(page == 0 && selected == 0 && !output1 && !output2);
    assert(motor_inits == 1);
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
    /* Bus recovery path: a failed transfer takes the panel down, the probe
       then runs I2C1_RecoverBus() once before retrying. The next healthy
       probe must not re-run the recovery. */
    i2c_fail = 1; OLED_Refresh();
    assert(!OLED_IsReady());
    tick += 1000; App_Tick();
    assert(i2c_recovers == 1 && !OLED_IsReady());
    i2c_fail = 0; tick += 1000; App_Tick();
    assert(OLED_IsReady() && i2c_recovers == 1);
    /* Chunked refresh: one page per call, completion on the 8th, aborts on error. */
    i2c_fail = 0; transfers = 0; OLED_RefreshStart();
    for (int i = 0; i < 7; ++i) assert(OLED_RefreshStep() == 0);
    assert(OLED_RefreshStep() == 1);
    assert(transfers == 8 * 4);             /* 3 commands + 1 data burst per page */
    assert(OLED_RefreshStep() == 0 && transfers == 8 * 4);
    i2c_fail = 1; transfers = 0; OLED_RefreshStart();
    assert(OLED_RefreshStep() == 0 && !OLED_IsReady() && transfers == 1);
    OLED_RefreshStep(); assert(transfers == 1);
    i2c_fail = 0; tick += 1000; App_Tick(); assert(OLED_IsReady());
    /* ---- line following: start, steer, stall floor, polarity, loss stop ---- */
    assert(follow_on == 0);
    press(KEY1_Pin); assert(selected == 2);
    press(KEY3_Pin); assert(page == 3 && motor_step == 5u);
    press(KEY2_Pin); assert(follow_on == 1);     /* K2 starts following */
    tick += 300; App_Tick();                     /* repaint window opens */
    int before = transfers;
    tick += 10; App_Tick();
    assert(transfers > before);                  /* frame pushed in chunks, not 23 ms */
    /* middle probe on the line: straight, base speed */
    sensors(0x04);
    tick += 10; App_Tick();
    assert(g_track_mask == 0x04 && g_track_pos == 0 &&
           motor_left == 80 && motor_right == 80);
    /* line under the left-most probe: inner-wheel stall floor */
    sensors(0x01);
    tick += 10; App_Tick();
    assert(g_track_pos == -100 && motor_left == 55 && motor_right == 115);
    /* R1 only: gentle right (pos +50 -> corr 17) */
    sensors(0x08);
    tick += 10; App_Tick();
    assert(g_track_pos == 50 && motor_left == 97 && motor_right == 63);
    /* inverted-polarity modules: line = low level */
    g_track_pol = 0u; sensors(0x00);
    tick += 10; App_Tick();
    assert(g_track_mask == 0x1F && g_track_pos == 0);
    g_track_pol = 1u;
    sensors(0x04);                               /* keep the line visible */
    tick += 10; App_Tick();
    assert(g_track_pos == 0);
    /* A dead panel is NOT probed while following: the two synchronous
       IsDeviceReady timeouts on a stuck bus can each wait for their whole
       timeout (~25 ms), and the chunked-refresh path must stall nothing. */
    i2c_fail = 1; OLED_Refresh(); assert(!OLED_IsReady());
    int rec0 = i2c_recovers;
    int tx0 = transfers;
    for (int i = 0; i < 4; ++i) { tick += 1000; App_Tick(); }   /* 4 probe windows */
    assert(!OLED_IsReady() && i2c_recovers == rec0);            /* probe deferred */
    assert(transfers == tx0);                                   /* zero bus traffic */
    /* Line lost under the probes: keep the last steering, stop after 1.5 s;
       the panel recovers in the very tick following the stop. */
    i2c_fail = 0;
    sensors(0x00);
    tick += 10; App_Tick();
    assert(g_track_pos == TRACK_NO_LINE);
    int brakes = motor_brakes;
    tick += 1660; App_Tick();                       /* line idle >1.5 s: stop */
    assert(motor_brakes > brakes && follow_on == 0);
    assert(OLED_IsReady() && i2c_recovers == rec0); /* panel back after stopping */
    /* ---- serial: error frames and backlogs must not corrupt value_a ---- */
    transmitted[0] = 0;
    rx_byte = 0x01; HAL_UART_RxCpltCallback(&uart);  /* illegal control byte */
    while (rx_head != rx_tail) serial_poll();
    tick += 300; serial_poll();                      /* idle gap closes the bad frame */
    assert(!strcmp(transmitted, "ERR\r\n"));
    transmitted[0] = 0;
    send("4242");                                    /* no CR/LF at all */
    tick += 300; serial_poll();
    assert(value_a == 4242 && !strcmp(transmitted, "OK\r\n"));
    /* Same for an overflow-recovered frame. */
    transmitted[0] = 0;
    for (int i = 0; i < 150; ++i) { rx_byte = '7'; HAL_UART_RxCpltCallback(&uart); }
    while (rx_head != rx_tail) serial_poll();
    tick += 300; serial_poll();
    assert(!strcmp(transmitted, "ERR\r\n"));
    transmitted[0] = 0;
    send("7"); tick += 300; serial_poll();
    assert(value_a == 7 && !strcmp(transmitted, "OK\r\n"));
    /* A >32-byte backlog must drain completely before the idle break fires,
       otherwise the tail is re-parsed as a fresh command (regression: the
       last 8 ones used to become value 11111111 after the ERR). */
    transmitted[0] = 0;
    for (int i = 0; i < 40; ++i) { rx_byte = '1'; HAL_UART_RxCpltCallback(&uart); }
    tick += 300; serial_poll();                      /* 32-byte budget: backlog left */
    assert(transmitted[0] == 0);                     /* no frame closed yet */
    serial_poll();                                   /* drains the rest */
    assert(!strcmp(transmitted, "ERR\r\n"));
    assert(value_a == 7);                            /* tail never became a value */
    transmitted[0] = 0;
    send("314"); tick += 300; serial_poll();
    assert(value_a == 314 && !strcmp(transmitted, "OK\r\n"));
    /* Debounce across the 32-bit millisecond counter wrap. */
    gpio_b.IDR &= (uint16_t)~KEY1_Pin; tick = UINT32_MAX - 8; assert(key_scan() == 0);
    tick = 10; assert(key_scan() == 1); assert(key_scan() == 0);
    puts("PASS: menus, 90s LED simulation, keys, UART bounds/overflow/error-frame/backlog recovery, OLED clipping/recovery/chunked refresh, line following (5-way digital: start/steer/stall-floor/polarity/loss-stop), tick rollover");
    return 0;
}
