/* sprites.h - sprite bitmaps + text rendering */
#ifndef SPRITES_H
#define SPRITES_H
#include "defs.h"

/* sprite dimensions */
#define SH_SHIP_W 16
#define SH_SHIP_H 16
#define SH_EN_W   16
#define SH_EN_H   14
#define SH_PB_W    3
#define SH_PB_H    8
#define SH_EB_W    5
#define SH_EB_H    7
#define SH_PU_W   12
#define SH_PU_H   12
#define SH_BOSS_W 48                 /* legacy default / title-screen draw only */
#define SH_BOSS_H 34
#define BOSS_MAXW 64                 /* max boss footprint; storage stride       */
#define BOSS_MAXH 44
#define SH_MSL_W   5
#define SH_MSL_H  10

#define NSTAGE 4                     /* enemy skins, one per 4-wave block */
#define NBOSS  15                    /* one authored boss per campaign boss wave */

extern u8 spr_ship[3][SH_SHIP_W * SH_SHIP_H];      /* left / straight / right */
extern u8 spr_enemy[NSTAGE][3][SH_EN_W * SH_EN_H]; /* [stage][scout/weaver/shooter] */
extern u8 spr_pbullet[3][SH_PB_W * SH_PB_H]; /* cannon / laser / wave  */
extern u8 spr_ebullet[SH_EB_W * SH_EB_H];
extern u8 spr_powerup[PU_COUNT][SH_PU_W * SH_PU_H];
extern u8 spr_boss[NBOSS][BOSS_MAXW * BOSS_MAXH];
extern i16 spr_boss_w[NBOSS], spr_boss_h[NBOSS];
extern u8 spr_missile[SH_MSL_W * SH_MSL_H];

void sprites_init(void);            /* build bitmaps + fetch ROM font */

/* text: 8x8 ROM font, colour 0 skips background (transparent) */
void text_draw(i16 x, i16 y, const char *s, u8 col);
void text_center(i16 y, const char *s, u8 col);

#endif
