/* vga.c - Mode 13h video for Open Watcom 16-bit real mode */
#include <i86.h>
#include <conio.h>
#include <string.h>
#include <malloc.h>
#include "vga.h"

u8 __far *g_back = 0;
u8 __far *g_bg = 0;
static u8 __far *vga_mem = 0;   /* 0xA000:0000 */
static u8 pal_theme = 0;
static u8 pal_phase = 0;

static void pal_set(u8 i, u8 r, u8 g, u8 b)
{
    outp(0x3C8, i);
    outp(0x3C9, r); outp(0x3C9, g); outp(0x3C9, b);
}

static u8 clamp63(i16 v)
{
    if (v < 0) return 0;
    if (v > 63) return 63;
    return (u8)v;
}

/* custom ramps above the 16 standard colours (values are 6-bit, 0..63) */
static void set_palette(void)
{
    u8 i;
    for (i = 0; i < 16; i++) {                       /* PAL_FIRE */
        u8 flick = (u8)(((i + pal_phase) & 3) == 0 ? 3 : 0);
        pal_set(PAL_FIRE + i, clamp63(20 + i * 3 + flick),
                clamp63((i < 5 ? 0 : (i - 5) * 6) + flick / 2),
                clamp63(i < 11 ? 0 : (i - 11) * 12));
    }
    for (i = 0; i < 16; i++) {                       /* PAL_NEB */
        i16 r, g, b, pulse = ((i + pal_phase) & 7) == 0 ? 2 : 0;
        switch (pal_theme & 3) {
            default:
            case 0: r = i / 2;     g = i / 3;     b = 3 + i * 2; break;  /* blue */
            case 1: r = 2 + i;     g = i / 4;     b = 5 + i * 2; break;  /* violet */
            case 2: r = i / 4;     g = 2 + i;     b = 5 + i;     break;  /* teal */
            case 3: r = 4 + i * 2; g = 1 + i / 2; b = i / 4;     break;  /* ember */
        }
        pal_set(PAL_NEB + i, clamp63(r + pulse), clamp63(g + pulse), clamp63(b + pulse));
    }
    for (i = 0; i < 16; i++) {                       /* PAL_GLOW */
        i16 r, g, b, pulse = (pal_phase & 1) ? 2 : 0;
        if ((pal_theme & 3) == 1)      { r = 8 + i * 2; g = i;         b = 12 + i * 3; }
        else if ((pal_theme & 3) == 2) { r = i / 4;     g = 10 + i*3;  b = 10 + i * 2; }
        else if ((pal_theme & 3) == 3) { r = 14 + i*3;  g = 6 + i * 2; b = i / 3;      }
        else                           { r = i / 4;     g = 8 + i * 3; b = 12 + i * 3; }
        pal_set(PAL_GLOW + i, clamp63(r + pulse), clamp63(g + pulse), clamp63(b + pulse));
    }
}

void vga_set_theme(u8 theme)
{
    pal_theme = theme;
    set_palette();
}

void vga_cycle_palette(void)
{
    pal_phase = (u8)((pal_phase + 1) & 7);
    set_palette();
}

int vga_init(void)
{
    union REGS r;
    g_back = (u8 __far *)_fmalloc(SCRSZ);
    g_bg   = (u8 __far *)_fmalloc(SCRSZ);
    if (!g_back || !g_bg) return 1;
    _fmemset(g_bg, C_BLACK, SCRSZ);
    vga_mem = MKFP(0xA000, 0);
    r.w.ax = 0x0013;            /* set 320x200x256 */
    int86(0x10, &r, &r);
    set_palette();
    vga_clear(C_BLACK);
    vga_present();
    return 0;
}

void vga_shutdown(void)
{
    union REGS r;
    r.w.ax = 0x0003;           /* back to 80x25 text */
    int86(0x10, &r, &r);
    if (g_back) { _ffree(g_back); g_back = 0; }
    if (g_bg)   { _ffree(g_bg);   g_bg = 0; }
}

/* back rows [yoff..] <- bg top, back rows [0..yoff) <- bg bottom (wrap) */
void vga_bg_blit(u16 yoff)
{
    u16 top = (u16)(SCRH - yoff);
    if (yoff == 0) { _fmemcpy(g_back, g_bg, SCRSZ); return; }
    _fmemcpy(g_back + (u16)yoff * SCRW, g_bg, top * SCRW);
    _fmemcpy(g_back, g_bg + top * SCRW, (u16)yoff * SCRW);
}

void vga_wait_vsync(void)
{
    /* Bounded spins: on hardware where port 0x3DA never toggles bit 3 the
       game must not hard-hang, since our INT 9 handler disables Ctrl-Alt-Del.
       ~60000 iterations is far longer than a real ~16 ms retrace period. */
    u16 g;
    for (g = 0; g < 60000 && (inp(0x3DA) & 0x08); g++) ;    /* wait until not in retrace */
    for (g = 0; g < 60000 && !(inp(0x3DA) & 0x08); g++) ;   /* wait for retrace start    */
}

void vga_present(void)
{
    vga_wait_vsync();
    _fmemcpy(vga_mem, g_back, SCRSZ);
}

void vga_present_paced(void)
{
    vga_present();
    vga_wait_vsync();              /* cap gameplay near 35 Hz on fast CPUs */
}

void vga_clear(u8 col)
{
    _fmemset(g_back, col, SCRSZ);
}

void vga_pixel(i16 x, i16 y, u8 col)
{
    if ((u16)x < SCRW && (u16)y < SCRH)
        g_back[(u16)y * SCRW + (u16)x] = col;
}

void vga_hline(i16 x, i16 y, i16 w, u8 col)
{
    i16 x2;
    if (y < 0 || y >= SCRH) return;
    if (x < 0) { w += x; x = 0; }
    x2 = x + w; if (x2 > SCRW) x2 = SCRW;
    if (x2 <= x) return;
    _fmemset(g_back + (u16)y * SCRW + (u16)x, col, (u16)(x2 - x));
}

void vga_rect(i16 x, i16 y, i16 w, i16 h, u8 col)
{
    i16 j;
    for (j = 0; j < h; j++) vga_hline(x, y + j, w, col);
}

void vga_frame(i16 x, i16 y, i16 w, i16 h, u8 col)
{
    i16 j;
    vga_hline(x, y, w, col);
    vga_hline(x, y + h - 1, w, col);
    for (j = 0; j < h; j++) { vga_pixel(x, y + j, col); vga_pixel(x + w - 1, y + j, col); }
}

void vga_sprite(i16 x, i16 y, i16 w, i16 h, const u8 __far *data)
{
    i16 sx, sy;
    for (sy = 0; sy < h; sy++) {
        i16 py = y + sy;
        const u8 __far *row = data + (u16)sy * w;
        if ((u16)py >= SCRH) continue;
        for (sx = 0; sx < w; sx++) {
            u8 c = row[sx];
            i16 px = x + sx;
            if (c && (u16)px < SCRW)
                g_back[(u16)py * SCRW + (u16)px] = c;
        }
    }
}

void vga_read_dac(u8 *pal)
{
    int i;
    outp(0x3C7, 0);
    for (i = 0; i < 768; i++) pal[i] = (u8)(inp(0x3C9) & 0x3F);
}
