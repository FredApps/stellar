/* input.c - self-contained INT 9 handler (make/break -> key table) */
#include <dos.h>
#include <conio.h>
#include "input.h"

volatile u8 g_key[128];
volatile u8 g_edge[128];

static void (__interrupt __far *old_int9)(void);
static bool installed = FALSE;

static void __interrupt __far kbd_isr(void)
{
    u8 sc = inp(0x60);
    u8 code = sc & 0x7F;
    if (sc & 0x80) {
        g_key[code] = 0;                 /* break */
    } else {
        if (!g_key[code]) g_edge[code] = 1;
        g_key[code] = 1;                 /* make  */
    }
    /* AT/PS2 8042 auto-acks on the port-0x60 read; just signal the PIC.
       (No XT-style port-0x61 toggle - that can make an AT controller
        drop the next scancode.) */
    outp(0x20, 0x20);                    /* EOI to master PIC */
}

void kbd_install(void)
{
    int i;
    if (installed) return;
    for (i = 0; i < 128; i++) { g_key[i] = 0; g_edge[i] = 0; }
    old_int9 = _dos_getvect(0x09);
    _disable();
    _dos_setvect(0x09, kbd_isr);
    _enable();
    installed = TRUE;
}

void kbd_remove(void)
{
    if (!installed) return;
    _disable();
    _dos_setvect(0x09, old_int9);
    _enable();
    installed = FALSE;
}

void kbd_clear(void)
{
    int i;
    _disable();                          /* keep the ISR from racing the wipe */
    for (i = 0; i < 128; i++) { g_key[i] = 0; g_edge[i] = 0; }
    _enable();
}

bool key_pressed(u8 sc) { return g_key[sc & 0x7F] ? TRUE : FALSE; }

bool key_hit(u8 sc)
{
    u8 c = sc & 0x7F;
    if (g_edge[c]) { g_edge[c] = 0; return TRUE; }
    return FALSE;
}

/* scancode -> uppercase ASCII for name entry (letters, digits, space) */
static const char sc_ascii[128] = {
/*00*/ 0,0,'1','2','3','4','5','6','7','8','9','0',0,0,0,0,
/*10*/ 'Q','W','E','R','T','Y','U','I','O','P',0,0,0,0,'A','S',
/*20*/ 'D','F','G','H','J','K','L',0,0,0,0,0,'Z','X','C','V',
/*30*/ 'B','N','M',0,0,0,0,0,0,' ',0,0,0,0,0,0,
0
};

char kbd_getchar(void)
{
    int i;
    for (i = 0; i < 128; i++) {
        /* only consume edges that map to a character, so control keys
           (Enter, Backspace, Esc...) stay available to key_hit() */
        if (g_edge[i] && sc_ascii[i]) {
            g_edge[i] = 0;
            return sc_ascii[i];
        }
    }
    return 0;
}
