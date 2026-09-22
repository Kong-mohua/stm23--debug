#include "oled.h"
#include <string.h>
#include "oledfont.h"
#include "cnfont.h"

/* hi2c1 is defined in main.c by CubeMX */
extern I2C_HandleTypeDef hi2c1;

#define OLED_ADDR   0x78   /* 0x3C << 1 */
#define OLED_CMD    0x00
#define OLED_DATA   0x40

/* framebuffer: 8 pages x 128 columns; each byte = 8 VERTICAL pixels, bit0 = top */
static uint8_t fb[8][128];

/* ---------------- low level ---------------- */

static void wr_cmd(uint8_t c)
{
    uint8_t b[2] = { OLED_CMD, c };
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, b, 2, 100);
}

static void wr_data(const uint8_t *d, uint16_t n)
{
    uint8_t b[129];
    b[0] = OLED_DATA;
    while (n) {
        uint16_t m = (n > 128) ? 128 : n;
        memcpy(&b[1], d, m);
        HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, b, (uint16_t)(m + 1), 500);
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

    HAL_Delay(100);
    for (uint16_t i = 0; i < sizeof(seq); i++)
        wr_cmd(seq[i]);

    OLED_Clear();
    OLED_Refresh();
}

void OLED_Clear(void)
{
    memset(fb, 0, sizeof(fb));
}

void OLED_Refresh(void)
{
    for (uint8_t p = 0; p < 8; p++) {
        wr_cmd(0xB0 + p);   /* set page */
        wr_cmd(0x00);       /* low  column */
        wr_cmd(0x10);       /* high column */
        wr_data(fb[p], 128);
    }
}

/* draw one 8x16 ASCII char into row (row*2 = upper page); returns advance in pixels */
static uint8_t draw_ascii(uint8_t row, uint8_t x, char ch)
{
    if (ch < 32 || ch > 126) ch = '?';

    const uint8_t *g = ASCII_FONT[ch - 32];   /* 8 columns x 2 pages, column-major */

    for (uint8_t c = 0; c < 8; c++) {
        uint8_t col = (uint8_t)(x + c);
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

static uint8_t draw_cn(uint8_t row, uint8_t x, uint16_t code)
{
    const uint8_t *g = find_cn(code);

    if (g) {
        for (uint8_t c = 0; c < 16; c++) {
            uint8_t col = (uint8_t)(x + c);
            if (col < 128) {
                fb[row * 2][col]     = g[c * 2];
                fb[row * 2 + 1][col] = g[c * 2 + 1];
            }
        }
    }
    return 16;
}

void OLED_ShowStr(uint8_t row, uint8_t x, const char *s)
{
    while (*s)
        x += draw_ascii(row, x, *s++);
}

void OLED_ShowMix(uint8_t row, uint8_t x, const char *s)
{
    const uint8_t *p = (const uint8_t *)s;

    while (*p) {
        if (*p < 0x80) {
            x += draw_ascii(row, x, (char)*p++);
        } else if ((*p & 0xE0) == 0xC0) {          /* 2-byte UTF-8 */
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {          /* 3-byte UTF-8 (Chinese) */
            uint16_t code = (uint16_t)((*p++ & 0x0F) << 12);
            code |= (uint16_t)((*p++ & 0x3F) << 6);
            code |= (uint16_t)(*p++ & 0x3F);
            x += draw_cn(row, x, code);
        } else {
            p++;                                    /* 4-byte: skip */
        }
    }
}
