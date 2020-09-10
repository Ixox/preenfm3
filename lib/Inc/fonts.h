/* vim: set ai et ts=4 sw=4: */
#ifndef __FONTS_H__
#define __FONTS_H__

#include <stdint.h>

typedef struct {
    const uint8_t width;
    uint8_t height;
    const uint16_t *data;
} FontDef;


extern const uint8_t segmentFont[];
extern FontDef Font_11x18;
extern FontDef Font_7x10;


#endif // __FONTS_H__
