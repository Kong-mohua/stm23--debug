#include "app.h"
#include "oled.h"
#include "motor.h"
#include "track.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern uint8_t key_scan(void);
extern UART_HandleTypeDef huart1;
#define uart huart1
static uint8_t rx_byte;
#define RX_SIZE 128u
static volatile uint8_t rx_buf[RX_SIZE];
static volatile uint16_t rx_head, rx_tail;
static volatile uint8_t rx_lost;
static char line[32];
static uint8_t line_len, discard_line;
static volatile uint32_t rx_tick;
static int32_t value_a;
static uint8_t page, selected, led_mode, led1, led2, dirty;
static uint32_t blink_tick, paint_tick, probe_tick;
static uint32_t motor_tick;
static uint8_t motor_step;

/* Line following: differential steering; the position error shifts speed
   between the two wheels (both stay forward). */
#define FOLLOW_BASE 80      /* cruise speed, percent (moves fine on 6 V) */
#define FOLLOW_K    35      /* steering gain: corr = pos * K / 100       */
static uint8_t follow_on;
static int16_t follow_corr;
static uint32_t scan_tick, lost_tick;

static void set_leds(uint8_t one, uint8_t two)
{
    led1 = one;
    led2 = two;
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, one ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, two ? GPIO_PIN_RESET : GPIO_PIN_SET);
    dirty = 1;
}

/* Explicit signed 32-bit bounds, including INT32_MIN. */
static uint8_t parse_number(const char *s, int32_t *result)
{
    uint8_t negative = 0;
    uint32_t n = 0, limit;
    if (*s == '-' || *s == '+') negative = (*s++ == '-');
    if (!*s) return 0;
    limit = negative ? 2147483648u : 2147483647u;
    while (*s) {
        uint32_t digit;
        if (*s < '0' || *s > '9') return 0;
        digit = (uint32_t)(*s++ - '0');
        if (n > (limit - digit) / 10u) return 0;
        n = n * 10u + digit;
    }
    *result = negative ? (n == 2147483648u ? INT32_MIN : -(int32_t)n) : (int32_t)n;
    return 1;
}

static void reply(const char *s)
{
    HAL_UART_Transmit(&uart, (uint8_t *)s, (uint16_t)strlen(s), 30);
}

static uint8_t same_ci(char a, char b)
{
    if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
    if (b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
    return (uint8_t)(a == b);
}

static uint8_t is_get_a(const char *s)
{
    return (uint8_t)(same_ci(s[0], 'G') && same_ci(s[1], 'E') && same_ci(s[2], 'T') &&
                     s[3] == ' ' && same_ci(s[4], 'A') && s[5] == '\0');
}

static void command(void)
{
    int32_t parsed;
    char out[20];
    const char *s = line;

    line[line_len] = '\0';
    while (*s == ' ') ++s;                                       /* tolerate padding */
    while (line_len && line[line_len - 1] == ' ') line[--line_len] = '\0';

    if (is_get_a(s)) {
        snprintf(out, sizeof(out), "%ld\r\n", (long)value_a);
        reply(out);
    } else if (parse_number(s, &parsed)) {
        value_a = parsed;
        if (page == 2) dirty = 1;
        reply("OK\r\n");
    } else reply("ERR\r\n");
}

static void serial_poll(void)
{
    /* Bound work per iteration even if a host continuously floods commands. */
    for (uint8_t budget = 0; budget < 32; ++budget) {
        uint8_t c;
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        if (rx_lost) {
            rx_tail = rx_head;
            rx_lost = 0;
            line_len = 0;
            discard_line = 1;
        }
        if (rx_tail == rx_head) {
            __set_PRIMASK(mask);
            break;
        }
        c = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1u) % RX_SIZE;
        __set_PRIMASK(mask);
        if (c == '\r' || c == '\n') {
            if (discard_line) reply("ERR\r\n");
            else if (line_len) command();
            line_len = discard_line = 0;
        } else if (!discard_line) {
            if (c < 32 || c > 126 || line_len >= sizeof(line) - 1u)
                discard_line = 1;
            else line[line_len++] = (char)c;
        }
    }
    /* Host tools that send without any line ending: an idle gap ends the
       frame.  A frame already flagged as garbage (illegal byte, overlong
       line, receive overflow) must be closed here as well -- otherwise
       discard_line would stay set and the next command (sent without a
       line ending) would be swallowed without any reply. */
    if ((uint32_t)(HAL_GetTick() - rx_tick) >= 200u) {
        if (discard_line) {
            reply("ERR\r\n");
            discard_line = 0;
            line_len = 0;
        } else if (line_len) {
            command();
            line_len = 0;
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    uint16_t next;
    if (h != &uart) return;
    next = (rx_head + 1u) % RX_SIZE;
    if (next == rx_tail) rx_lost = 1;
    else { rx_buf[rx_head] = rx_byte; rx_head = next; }
    rx_tick = HAL_GetTick();
    HAL_UART_Receive_IT(&uart, &rx_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    if (h != &uart) return;
    rx_lost = 1;
    if (h->RxState == HAL_UART_STATE_READY)
        HAL_UART_Receive_IT(&uart, &rx_byte, 1);
}

/* Motor self test: forward, stop, reverse, turn left, turn right, done. */
static const char *const motor_step_text[] = {
    "1 FORWARD", "2 STOP", "3 REVERSE", "4 TURN LEFT", "5 TURN RIGHT", "6 DONE"
};

static void motor_test_apply(uint8_t step)
{
    switch (step) {
    case 0: Motor_Set(85, 85);   break;   /* strong duty: 6V - L298N drop needs it */
    case 1: Motor_Set(0, 0);     break;
    case 2: Motor_Set(-85, -85); break;
    case 3: Motor_Set(-75, 75);  break;   /* left back / right ahead  */
    case 4: Motor_Set(75, -75);  break;   /* left ahead / right back  */
    default: Motor_Brake();      break;
    }
}

/* Draw the current screen into the framebuffer (no bus traffic). */
static void paint_text(void)
{
    static const char *const items[] = {
        "LED 控制", "信息显示", "巡线功能", "拓展功能"
    };
    char text[20];
    OLED_Clear();
    if (page == 0) {
        for (uint8_t i = 0; i < 4; ++i) {
            OLED_ShowStr(i, 0, i == selected ? ">" : " ");
            OLED_ShowMix(i, 8, items[i]);
        }
    } else if (page == 1) {
        OLED_ShowMix(0, 0, "LED 控制");
        snprintf(text, sizeof(text), "1:%s 2:%s", led1 ? "ON" : "OFF", led2 ? "ON" : "OFF");
        OLED_ShowStr(1, 0, text);
        OLED_ShowStr(2, 0, "K1:ON/OFF K2:ALT");
        OLED_ShowStr(3, 0, "K4:BACK");
    } else if (page == 2) {
        OLED_ShowStr(0, 0, STUDENT_NAME);
        OLED_ShowStr(1, 0, STUDENT_ID);
        snprintf(text, sizeof(text), "a=%ld", (long)value_a);
        OLED_ShowStr(2, 0, text);
        OLED_ShowStr(3, 0, "K4:BACK");
    } else if (page == 3) {
        OLED_ShowMix(0, 0, items[page - 1]);
        if (motor_step < 5u) {
            OLED_ShowStr(1, 0, "MOTOR SELF TEST");
            OLED_ShowStr(2, 0, motor_step_text[motor_step]);
            OLED_ShowStr(3, 0, "K4:STOP+BACK");
        } else {
            char bits[9];
            for (uint8_t i = 0; i < 8u; ++i) bits[i] = (g_track_mask & (1u << i)) ? '1' : '0';
            bits[8] = '\0';
            snprintf(text, sizeof(text), "S:%s %s", bits, follow_on ? "RUN" : "STOP");
            OLED_ShowStr(1, 0, text);
            if (g_track_pos == TRACK_NO_LINE) OLED_ShowStr(2, 0, "POS: NO LINE");
            else {
                snprintf(text, sizeof(text), "POS:%+d", (int)g_track_pos);
                OLED_ShowStr(2, 0, text);
            }
            OLED_ShowStr(3, 0, "K1:TEST K2:GO");
        }
    } else {
        OLED_ShowMix(0, 0, items[page - 1]);
        OLED_ShowStr(1, 0, "NOT IMPLEMENTED");
        OLED_ShowStr(2, 0, "NEEDS HARDWARE");
        OLED_ShowStr(3, 0, "K4:BACK");
    }
}

/* Draw and push in one blocking go (menus, page transitions). */
static void paint(void)
{
    paint_text();
    OLED_Refresh();
}

void App_Init(void)
{
    page = selected = led_mode = 0;
    value_a = 0;
    follow_on = 0;
    follow_corr = 0;
    motor_step = 5u;                /* page 3 idles until K1 / K2 */
    set_leds(0, 0);
    Motor_Init();   /* driver standby: wheels stay off until the self test runs */
    Track_Init();   /* 8-way grayscale sensor + ADC on PB1 */
    HAL_Delay(100); /* power-on settling only; never used for LED flashing */
    OLED_Init();
    if (HAL_UART_Receive_IT(&uart, &rx_byte, 1) != HAL_OK) Error_Handler();
    probe_tick = paint_tick = HAL_GetTick();
    paint();
    dirty = 0;
}

void App_Tick(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t keys = key_scan();
    serial_poll();
    if (page && (keys & 8u)) {
        page = led_mode = 0;
        follow_on = 0;
        set_leds(0, 0);
        Motor_Brake();
    } else if (!page) {
        if (keys & 1u) { selected = (selected + 3u) % 4u; dirty = 1; }
        else if (keys & 2u) { selected = (selected + 1u) % 4u; dirty = 1; }
        else if (keys & 4u) {
            page = selected + 1u;
            dirty = 1;
            if (page == 3u) {            /* tracking page: idle until asked */
                motor_step = 5u;
            }
        }
    } else if (page == 1) {
        if (keys & 1u) {
            led_mode = (led_mode == 1u) ? 0u : 1u;
            set_leds(led_mode == 1u, led_mode == 1u);
        } else if (keys & 2u) {
            led_mode = 2;
            blink_tick = now;
            set_leds(1, 0);
        }
    } else if (page == 3) {
        if (keys & 1u) {                 /* K1: replay the motor self test */
            follow_on = 0;
            motor_step = 0u;
            motor_tick = now;
            motor_test_apply(0);
            dirty = 1;
        } else if (keys & 2u) {          /* K2: start / stop line following */
            motor_step = 5u;             /* abort the self test if it runs */
            follow_on = !follow_on;
            if (follow_on) { lost_tick = now; follow_corr = 0; }
            else Motor_Brake();
            dirty = 1;
        }
    }
    if (page == 1 && led_mode == 2 && (uint32_t)(now - blink_tick) >= 500u) {
        blink_tick = now;
        set_leds(!led1, !led2);
    }
    if (page == 3u && motor_step < 5u && (uint32_t)(now - motor_tick) >= 1500u) {
        motor_tick = now;
        ++motor_step;
        motor_test_apply(motor_step);
        dirty = 1;
    }
    if ((uint32_t)(now - scan_tick) >= (follow_on ? 5u : 50u)) {
        scan_tick = now;
        Track_Scan();                    /* always live: calibration from any page */
        if (page == 3u && motor_step >= 5u) dirty = 1;
        if (follow_on && page == 3u) {   /* line following control */
            int16_t pos = g_track_pos;
            if (pos != TRACK_NO_LINE) {
                lost_tick = now;
                follow_corr = (int16_t)((int32_t)pos * FOLLOW_K / 100);
            }
            if ((uint32_t)(now - lost_tick) <= 1500u) {
                int16_t l = (int16_t)(FOLLOW_BASE + follow_corr);
                int16_t r = (int16_t)(FOLLOW_BASE - follow_corr);
                /* keep the inner wheel above the stall threshold (6 V + L298N) */
                if (l < 55) l = 55;
                if (r < 55) r = 55;
                Motor_Set(l, r);
            } else {
                Motor_Brake();           /* line lost for 1.5 s: stop safely */
                follow_on = 0;
                dirty = 1;
            }
        }
    }
    if (!OLED_IsReady() && (uint32_t)(now - probe_tick) >= 1000u) {
        probe_tick = now;
        OLED_Init();
        dirty = 1;
    }
    /* Display update.  While line following, a full blocking refresh costs
       ~23 ms of I2C time and would stall the 5 ms steering pass; instead the
       frame is pushed one page (~3 ms) per loop pass between control
       updates.  Menus keep the simple blocking refresh. */
    if (page == 3u && follow_on) {
        if (dirty && (uint32_t)(now - paint_tick) >= 250u) {
            paint_tick = now;
            paint_text();
            dirty = 0;
            OLED_RefreshStart();
        }
        OLED_RefreshStep();              /* idle cycle: returns immediately */
    } else if (dirty && (uint32_t)(now - paint_tick) >= 50u) {
        paint_tick = now;
        paint();
        dirty = 0;
    }
}
