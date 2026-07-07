/* input.h - INT 9 keyboard state tracker */
#ifndef INPUT_H
#define INPUT_H
#include "defs.h"

extern volatile u8 g_key[128];      /* 1 while scancode held         */
extern volatile u8 g_edge[128];     /* set on fresh press, cleared by reader */

void kbd_install(void);
void kbd_remove(void);
void kbd_clear(void);               /* drop all held/edge state (anti-stuck-key) */
bool key_pressed(u8 sc);            /* held */
bool key_hit(u8 sc);                /* fresh press (consumes edge)   */
char kbd_getchar(void);             /* fresh A-Z/0-9 as ASCII, or 0  */

#endif
