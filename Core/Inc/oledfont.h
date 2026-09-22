#ifndef __OLEDFONT_H
#define __OLEDFONT_H
#include <stdint.h>

/* ASCII 8x16 点阵, 索引=字符码-32, 每字16字节(列主序) */
#define ASCII_FONT_W 8
#define ASCII_FONT_H 16
extern const uint8_t ASCII_FONT[95][16];

#endif
