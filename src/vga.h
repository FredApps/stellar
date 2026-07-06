/* vga.h - Mode 13h video, double buffer, drawing primitives */
#ifndef VGA_H
#define VGA_H
#include "defs.h"

extern u8 __far *g_back;      /* 64000-byte back buffer               */
extern u8 __far *g_bg;        /* 64000-byte static background (nebula) */

int  vga_init(void);          /* set mode 13h, alloc buffers; 0 = ok */
void vga_shutdown(void);      /* restore text mode, free buffers     */
void vga_present(void);       /* wait vsync, blit back buffer -> VGA */
void vga_present_paced(void); /* present, then skip one retrace      */
void vga_wait_vsync(void);

/* copy background into back buffer with vertical wrap offset (scroll) */
void vga_bg_blit(u16 yoff);

/* colour themes/cycling for the custom ramps */
void vga_set_theme(u8 theme);
void vga_cycle_palette(void);

void vga_clear(u8 col);
void vga_pixel(i16 x, i16 y, u8 col);
void vga_hline(i16 x, i16 y, i16 w, u8 col);
void vga_rect(i16 x, i16 y, i16 w, i16 h, u8 col);       /* filled */
void vga_frame(i16 x, i16 y, i16 w, i16 h, u8 col);      /* outline */

/* sprite: w*h bytes, colour 0 = transparent, clipped to screen */
void vga_sprite(i16 x, i16 y, i16 w, i16 h, const u8 __far *data);

/* read the current 256-colour DAC into pal[768] (0..63 range) */
void vga_read_dac(u8 *pal768);

#endif
