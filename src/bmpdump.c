/* bmpdump.c - write back buffer as an 8-bit BMP for headless verification */
#include <stdio.h>
#include "bmpdump.h"
#include "vga.h"

static void w16(FILE *f, u16 v) { fputc(v & 0xFF, f); fputc(v >> 8, f); }
static void w32(FILE *f, u32 v)
{ fputc((u8)v, f); fputc((u8)(v >> 8), f); fputc((u8)(v >> 16), f); fputc((u8)(v >> 24), f); }

void bmp_dump(const char *path)
{
    u8 pal[768];
    i16 x, y;
    u32 pixoff = 14 + 40 + 256 * 4;
    u32 filesz = pixoff + (u32)SCRW * SCRH;
    FILE *f = fopen(path, "wb");
    if (!f) return;

    vga_read_dac(pal);

    /* BITMAPFILEHEADER */
    fputc('B', f); fputc('M', f);
    w32(f, filesz); w16(f, 0); w16(f, 0); w32(f, pixoff);
    /* BITMAPINFOHEADER */
    w32(f, 40); w32(f, SCRW); w32(f, SCRH);
    w16(f, 1); w16(f, 8); w32(f, 0); w32(f, (u32)SCRW * SCRH);
    w32(f, 0); w32(f, 0); w32(f, 256); w32(f, 0);
    /* palette (BGRA), scale 6-bit DAC -> 8-bit */
    for (x = 0; x < 256; x++) {
        u8 r = (u8)(pal[x * 3 + 0] << 2);
        u8 g = (u8)(pal[x * 3 + 1] << 2);
        u8 b = (u8)(pal[x * 3 + 2] << 2);
        fputc(b, f); fputc(g, f); fputc(r, f); fputc(0, f);
    }
    /* pixels bottom-up; width 320 already 4-aligned */
    for (y = SCRH - 1; y >= 0; y--) {
        const u8 __far *row = g_back + (u16)y * SCRW;
        for (x = 0; x < SCRW; x++) fputc(row[x], f);
    }
    fclose(f);
}
