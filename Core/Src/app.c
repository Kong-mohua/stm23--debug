#include "app.h"
#include "oled.h"
#include "motor.h"
#include "track.h"
#include "settings.h"
#include "race.h"
#include "board.h"
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
static uint8_t follow_on;
static uint32_t scan_tick;
static uint8_t remote_motion, setting_item;
static uint32_t remote_tick;
static const char *notice = "UNSAVED";
static const char *const remote_names[] = {"STOP", "FORWARD", "BACK", "LEFT", "RIGHT"};
#define TX_COUNT 8u
#define TX_LENGTH 128u
static char tx_buf[TX_COUNT][TX_LENGTH];
static volatile uint8_t tx_head, tx_tail, tx_busy;
static volatile uint32_t tx_dropped;

static void stop_motion(uint32_t now)
{
    Race_Stop(now); follow_on = remote_motion = 0; motor_step = 5u;
    Motor_Brake(); dirty = 1;
}

static uint8_t motors_active(void)
{
    return follow_on || remote_motion || (page == 3u && motor_step < 5u);
}

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

static void tx_kick(void)
{
    uint32_t pmask = __get_PRIMASK();
    __disable_irq();
    if (!tx_busy && tx_tail != tx_head) {
        tx_busy = 1;
        if (HAL_UART_Transmit_IT(&uart, (uint8_t *)tx_buf[tx_tail],
              (uint16_t)strlen(tx_buf[tx_tail])) != HAL_OK) tx_busy = 0;
    }
    __set_PRIMASK(pmask);
}

static void reply(const char *s)
{
    uint8_t next = (uint8_t)((tx_head + 1u) % TX_COUNT);
    size_t len = strlen(s);
    if (next == tx_tail || len >= TX_LENGTH) { ++tx_dropped; return; }
    memcpy(tx_buf[tx_head], s, len + 1u);
    tx_head = next;
    tx_kick();
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h)
{
    if (h != &uart || !tx_busy) return;
    tx_tail = (uint8_t)((tx_tail + 1u) % TX_COUNT);
    tx_busy = 0; /* next buffer is started by the main loop */
}

static void status_reply(void)
{
    char out[TX_LENGTH];
    snprintf(out, sizeof(out), "STATE %s RACE %s LAP %u/%u TIME %lu TXDROP %lu\r\n",
        remote_motion ? remote_names[remote_motion] : (follow_on ? "FOLLOW" : "STOP"),
        Race_StateName(), race.laps, settings.laps, (unsigned long)race.elapsed,
        (unsigned long)tx_dropped);
    reply(out);
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

    for (char *p = line; *p; ++p)
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');

    if (!strcmp(s, "STOP") || !strcmp(s, "MOVE STOP")) {
        stop_motion(HAL_GetTick()); status_reply();
    } else if (!strcmp(s, "GET STATUS")) {
        status_reply();
    } else if (!strncmp(s, "MOVE ", 5)) {
        uint8_t move = 0;
        for (uint8_t i = 1; i < 5; ++i)
            if (!strcmp(s + 5, remote_names[i])) move = i;
        if (!move) { reply("ERR\r\n"); return; }
        stop_motion(HAL_GetTick()); remote_motion = move; remote_tick = HAL_GetTick();
        int16_t speed = (int16_t)settings.speed;
        if (move == 1) Motor_Set(speed, speed);
        else if (move == 2) Motor_Set(-speed, -speed);
        else if (move == 3) Motor_Set(0, speed);
        else Motor_Set(speed, 0);
        status_reply();
    } else if (!strcmp(s, "SAVE")) {
        if (motors_active()) reply("ERR BUSY\r\n");
        else { notice = Settings_Save() ? "SAVED" : "SAVE FAILED"; reply(notice); reply("\r\n"); dirty = 1; }
    } else if (!strcmp(s, "CAL WHITE") || !strcmp(s, "CAL BLACK")) {
        if (motors_active()) reply("ERR BUSY\r\n");
        else {
            uint8_t ok = s[4] == 'W' ? Track_CalibrateWhite() : Track_CalibrateBlack();
            notice = ok ? "CAL OK: SAVE" : "CAL FAILED";
            reply(ok ? "OK\r\n" : "ERR CAL\r\n"); dirty = 1;
        }
    } else if (!strcmp(s, "GET RAW")) {
        char raw[TX_LENGTH];
        snprintf(raw, sizeof(raw), "RAW %u %u %u %u %u %u %u %u VALID %u\r\n",
            g_track_raw[0], g_track_raw[1], g_track_raw[2], g_track_raw[3],
            g_track_raw[4], g_track_raw[5], g_track_raw[6], g_track_raw[7], g_track_valid);
        reply(raw);
    } else if (is_get_a(s)) {
        snprintf(out, sizeof(out), "%ld\r\n", (long)value_a);
        reply(out);
    } else if (parse_number(s, &parsed)) {
        value_a = parsed;
        if (page == 2) dirty = 1;
        reply("OK\r\n");
    } else reply("ERR\r\n");
}

/* Settings page: K1 selects; K2 increases/actions; K3 decreases/actions. */
static const char *const setting_names[] = {
    "SPEED", "GAIN", "LAPS", "GAP MS", "MIN LAP MS", "BAR HOLD MS",
    "POLARITY", "REVERSE", "BAR FINISH", "SAVE", "CAL WHITE", "CAL BLACK", "DEFAULTS"
};
#define SETTING_COUNT 13u
static uint16_t setting_value(void)
{
    switch (setting_item) {
    case 0: return settings.speed; case 1: return settings.gain;
    case 2: return settings.laps; case 3: return settings.gap_ms;
    case 4: return settings.min_lap_ms; case 5: return settings.marker_ms;
    case 6: return settings.polarity; case 7: return settings.reverse;
    case 8: return settings.marker_enabled; default: return 0;
    }
}
static uint16_t adjust(uint16_t v, int dir, uint16_t step, uint16_t low, uint16_t high)
{
    int32_t n = (int32_t)v + dir * (int32_t)step;
    return n < low ? low : (n > high ? high : (uint16_t)n);
}
static void settings_key(int dir)
{
    notice = "UNSAVED";
    switch (setting_item) {
    case 0: settings.speed = adjust(settings.speed, dir, 5, 30, 100); break;
    case 1: settings.gain = adjust(settings.gain, dir, 5, 0, 100); break;
    case 2: settings.laps = settings.laps == 1 ? 2 : 1; break;
    case 3: settings.gap_ms = adjust(settings.gap_ms, dir, 50, 50, 1000); break;
    case 4: settings.min_lap_ms = adjust(settings.min_lap_ms, dir, 1000, 2000, 60000); break;
    case 5: settings.marker_ms = adjust(settings.marker_ms, dir, 10, 10, 500); break;
    case 6: settings.polarity ^= 1u; break;
    case 7: settings.reverse ^= 1u; break;
    case 8: settings.marker_enabled ^= 1u; break;
    case 9: notice = Settings_Save() ? "SAVED" : "SAVE FAILED"; break;
    case 10: notice = Track_CalibrateWhite() ? "WHITE CAPTURED" : "CAL FAILED"; break;
    case 11: notice = Track_CalibrateBlack() ? "CAL OK: SAVE" : "CAL FAILED"; break;
    case 12: Settings_Defaults(); notice = "DEFAULT: SAVE"; break;
    default: break;
    }
    Track_ApplySettings(); dirty = 1;
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
       line ending) would be swallowed without any reply.

       Two more rules matter here:
       - Only fire when every received byte has been consumed.  The loop
         above stops after a 32-byte budget, so a longer burst is still in
         the queue: closing the frame now would re-parse its tail as a new
         command and could write garbage into value_a.
       - Sample time and receive state under a masked-interrupt snapshot:
         a byte landing between the two reads would make the unsigned
         difference wrap and split "123" into "12" + "3". */
    if (rx_tail != rx_head)
        return;                                  /* backlog: keep draining */
    {
        uint32_t pmask = __get_PRIMASK();
        uint32_t now, last;
        uint8_t  drained;
        __disable_irq();
        now = HAL_GetTick();
        last = rx_tick;
        drained = (uint8_t)(rx_tail == rx_head);
        __set_PRIMASK(pmask);
        if (drained && (uint32_t)(now - last) >= 200u) {
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
            snprintf(text, sizeof(text), "L%u/%u %lu.%lus", race.laps, settings.laps,
                (unsigned long)(race.elapsed / 1000u), (unsigned long)(race.elapsed / 100u % 10u));
            OLED_ShowStr(2, 0, text);
            if (!g_track_valid) OLED_ShowStr(3, 0, "ADC FAULT");
            else if (!settings.calibrated) OLED_ShowStr(3, 0, "CAL NEEDED");
            else {
                snprintf(text, sizeof(text), "%s%s", Race_StateName(), settings.marker_enabled ? "" : " BAR OFF");
                OLED_ShowStr(3, 0, text);
            }
        }
    } else {
        OLED_ShowMix(0, 0, items[page - 1]);
        snprintf(text, sizeof(text), "%s:%u", setting_names[setting_item], setting_value());
        OLED_ShowStr(1, 0, text);
        OLED_ShowStr(2, 0, notice);
        OLED_ShowStr(3, 0, "1:NEXT 2:+ 3:-");
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
    remote_motion = setting_item = 0;
    tx_head = tx_tail = tx_busy = 0; tx_dropped = 0;
    rx_head = rx_tail = 0; rx_lost = line_len = discard_line = 0;
    memset(&race, 0, sizeof(race));
    Settings_Load(); Board_Init();
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
    Board_Tick(now); tx_kick();
    serial_poll();
    now = HAL_GetTick(); /* serial/calibration work can consume time */
    if (remote_motion && now - remote_tick >= 700u) {
        stop_motion(now); status_reply(); /* dead-man timeout */
    }
    if (keys & 8u) {
        page = led_mode = 0;
        stop_motion(now);
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
            stop_motion(now);
            motor_step = 0u;
            motor_tick = now;
            motor_test_apply(0);
            dirty = 1;
        } else if (keys & 2u) {          /* K2: start / stop line following */
            uint8_t was_running = follow_on;
            stop_motion(now);
            if (!was_running) {
                Track_Scan();
                now = HAL_GetTick();
                follow_on = Race_Start(now, g_track_valid, g_track_mask);
                if (race.beep) { Board_Beep(now); race.beep = 0; }
            }
            dirty = 1;
        } else if ((keys & 4u) && !motors_active()) {
            settings.laps = settings.laps == 1 ? 2 : 1; dirty = 1;
        }
    } else if (page == 4u && !motors_active()) {
        if (keys & 1u) { setting_item = (setting_item + 1u) % SETTING_COUNT; dirty = 1; }
        else if (keys & 2u) settings_key(1);
        else if (keys & 4u) settings_key(-1);
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
            now = HAL_GetTick();
            Race_Update(now, g_track_valid, g_track_mask);
            follow_on = race.running;
            if (follow_on) Motor_Set(race.left, race.right);
            else { Motor_Brake(); dirty = 1; }
            if (race.beep) { Board_Beep(now); race.beep = 0; }
        }
    }
    /* Panel re-probe.  While line following this is deferred: the two
       synchronous HAL_I2C_IsDeviceReady probes inside OLED_Init can each
       wait for their whole timeout (~25 ms on a stuck BUSY bus), which
       would stall the steering loop.  The moment following stops (K2, K4,
       line lost) the panel is re-initialized again within one tick. */
    if (!OLED_IsReady() && !follow_on && (uint32_t)(now - probe_tick) >= 1000u) {
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
