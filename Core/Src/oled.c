#include "oled.h"
#include <string.h>
#include "oledfont.h"
#include "cnfont.h"

/* hi2c1 is defined in main.c by CubeMX */
extern I2C_HandleTypeDef hi2c1;

/* Implemented in main.c (USER CODE 4): frees a stuck bus and restarts I2C1. */
extern void I2C1_RecoverBus(void);

#define OLED_ADDR   0x78   /* 0x3C << 1 */
#define OLED_CMD    0x00
#define OLED_DATA   0x40

/* framebuffer: 8 pages x 128 columns; each byte = 8 VERTICAL pixels, bit0 = top */
static uint8_t fb[8][128];

/* Stop the current refresh after the first bus error. App_Tick retries panel
   initialization once per second, while keys and LEDs continue to work. */
static uint8_t oled_ok = 0;

/* Chunked-refresh cursor: -1 = idle, 0..7 = next page to push.
   Used by OLED_RefreshStart()/OLED_RefreshStep() for real-time screens. */
static int8_t refresh_page = -1;

/* ---------------- low level ---------------- */

static void wr_cmd(uint8_t c)
{
    uint8_t b[2] = { OLED_CMD, c };

    if (!oled_ok)
        return;

    if (HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, b, 2, 10) != HAL_OK)
        oled_ok = 0;
}

static void wr_data(const uint8_t *d, uint16_t n)
{
    uint8_t b[129];

    if (!oled_ok)
        return;

    b[0] = OLED_DATA;
    while (n) {
        uint16_t m = (n > 128) ? 128 : n;
        memcpy(&b[1], d, m);
        if (HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, b, (uint16_t)(m + 1), 50) != HAL_OK) {
            oled_ok = 0;
            return;
        }
        d += m;
        n -= m;
    }
}

/* ---------------- public API ---------------- */

void OLED_Init(void)
{
    static const uint8_t seq[] = {
        0xAE,           /* display off            */
        0x20, 0x02,     /* page addressing mode   */
        0xB0,           /* page start 0           */
        0xC8,           /* COM scan remapped      */
        0x00, 0x10,     /* column 0               */
        0x40,           /* start line 0           */
        0x81, 0xCF,     /* contrast               */
        0xA1,           /* segment remap          */
        0xA6,           /* normal (not inverted)  */
        0xA8, 0x3F,     /* multiplex 64           */
        0xA4,           /* output follows RAM     */
        0xD3, 0x00,     /* display offset 0       */
        0xD5, 0x80,     /* clock divide           */
        0xD9, 0xF1,     /* pre-charge             */
        0xDA, 0x12,     /* COM pins               */
        0xDB, 0x40,     /* VCOMH deselect         */
        0x8D, 0x14,     /* charge pump on         */
        0xAF            /* display on             */
    };

    /* The caller allows power-on settling and retries once per second. */
    if (HAL_I2C_IsDeviceReady(&hi2c1, OLED_ADDR, 1, 10) != HAL_OK)
    {
        /* A timeout can leave the bus (or the I2C peripheral's BUSY flag)
           wedged, and then every later transfer fails instantly: free the bus
           and restart the peripheral before retrying once. */
        I2C1_RecoverBus();
        if (HAL_I2C_IsDeviceReady(&hi2c1, OLED_ADDR, 1, 10) != HAL_OK)
        {
            oled_ok = 0;
            return;
        }
    }
    oled_ok = 1;

    for (uint16_t i = 0; i < sizeof(seq); i++)
        wr_cmd(seq[i]);

    OLED_Clear();
    /* Chunked instead of a blocking full frame: OLED_Init also runs as a
       recovery path from App_Tick while line following, and ~23 ms of
       synchronous I2C there would stall the 5 ms steering loop. */
    OLED_RefreshStart();
}

void OLED_Clear(void)
{
    memset(fb, 0, sizeof(fb));
}

uint8_t OLED_IsReady(void) { return oled_ok; }

void OLED_Refresh(void)
{
    refresh_page = -1;      /* a full refresh supersedes any chunked cycle */

    if (!oled_ok)
        return;

    for (uint8_t p = 0; p < 8; p++) {
        wr_cmd(0xB0 + p);   /* set page */
        wr_cmd(0x00);       /* low  column */
        wr_cmd(0x10);       /* high column */
        wr_data(fb[p], 128);
    }
}

/* Split refresh for latency-sensitive screens (line following): one page
   (~3 ms of bus time) per call, so the caller's control loop keeps running
   between chunks instead of blocking ~23 ms for the whole frame.
   Begin a cycle with OLED_RefreshStart(), then call OLED_RefreshStep() once
   per loop pass; an idle step returns 0 immediately. */
void OLED_RefreshStart(void)
{
    refresh_page = oled_ok ? 0 : -1;
}

uint8_t OLED_RefreshStep(void)
{
    uint8_t p;

    if (refresh_page < 0 || !oled_ok) {
        refresh_page = -1;
        return 0u;
    }

    p = (uint8_t)refresh_page;
    wr_cmd((uint8_t)(0xB0u + p));     /* set page      */
    wr_cmd(0x00);                     /* low  column   */
    wr_cmd(0x10);                     /* high column   */
    wr_data(fb[p], 128);

    if (!oled_ok) {                   /* bus error: abort the cycle */
        refresh_page = -1;
        return 0u;
    }
    if (p >= 7u) {                    /* last page: frame complete  */
        refresh_page = -1;
        return 1u;
    }
    refresh_page = (int8_t)(p + 1u);
    return 0u;
}

/* draw one 8x16 ASCII char into row (row*2 = upper page); returns advance in pixels */
static uint8_t draw_ascii(uint8_t row, uint16_t x, char ch)
{
    if (ch < 32 || ch > 126) ch = '?';

    const uint8_t *g = ASCII_FONT[ch - 32];   /* 8 columns x 2 pages, column-major */

    for (uint8_t c = 0; c < 8; c++) {
        uint16_t col = x + c;
        if (col < 128) {
            fb[row * 2][col]     = g[c * 2];
            fb[row * 2 + 1][col] = g[c * 2 + 1];
        }
    }
    return 8;
}

/* binary search the Chinese font by Unicode code point; returns 16x16 glyph or NULL */
static const uint8_t *find_cn(uint16_t code)
{
    int lo = 0, hi = CN_FONT_COUNT - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (CN_FONT[mid].code == code)     return CN_FONT[mid].data;
        if (CN_FONT[mid].code < code)      lo = mid + 1;
        else                               hi = mid - 1;
    }
    return NULL;
}

static uint8_t draw_cn(uint8_t row, uint16_t x, uint16_t code)
{
    const uint8_t *g = find_cn(code);

    if (g) {
        for (uint8_t c = 0; c < 16; c++) {
            uint16_t col = x + c;
            if (col < 128) {
                fb[row * 2][col]     = g[c * 2];
                fb[row * 2 + 1][col] = g[c * 2 + 1];
            }
        }
    } else {
        draw_ascii(row, x, '?');
        draw_ascii(row, x + 8u, ' ');
    }
    return 16;
}

void OLED_ShowStr(uint8_t row, uint8_t x, const char *s)
{
    uint16_t cursor = x;
    if (row >= 4 || !s) return;
    while (*s && cursor < 128)
        cursor += draw_ascii(row, cursor, *s++);
}

void OLED_ShowMix(uint8_t row, uint8_t x, const char *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t cursor = x;
    if (row >= 4 || !p) return;
    while (*p && cursor < 128) {
        if (*p < 0x80) {
            cursor += draw_ascii(row, cursor, (char)*p++);
        } else {
            uint8_t count = (*p >= 0xC2 && *p <= 0xDF) ? 2 :
                            (*p >= 0xE0 && *p <= 0xEF) ? 3 :
                            (*p >= 0xF0 && *p <= 0xF4) ? 4 : 0;
            uint32_t code = count ? (*p & ((1u << (7u - count)) - 1u)) : 0;
            uint8_t valid = (count != 0);
            for (uint8_t i = 1; valid && i < count; ++i) {
                /* Check each byte before looking at the next (including NUL). */
                if ((p[i] & 0xC0) != 0x80) valid = 0;
                else code = (code << 6) | (p[i] & 0x3F);
            }
            if (valid && ((count == 2 && code < 0x80) ||
                          (count == 3 && code < 0x800) ||
                          (count == 4 && code < 0x10000) ||
                          (code >= 0xD800 && code <= 0xDFFF) || code > 0x10FFFF))
                valid = 0;
            if (!valid) {
                cursor += draw_ascii(row, cursor, '?');
                ++p;
            } else {
                if (count == 3) cursor += draw_cn(row, cursor, (uint16_t)code);
                else cursor += draw_ascii(row, cursor, '?');
                p += count;
            }
        }
    }
}
