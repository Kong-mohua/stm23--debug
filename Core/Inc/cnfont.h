#ifndef __CNFONT_H
#define __CNFONT_H
#include <stdint.h>

/* 中文 16x16, 按 Unicode 码点索引, 每字 32 字节(列主序) */
typedef struct { uint16_t code; uint8_t data[32]; } cn_glyph_t;
#define CN_FONT_COUNT 82
extern const cn_glyph_t CN_FONT[CN_FONT_COUNT];

#endif
