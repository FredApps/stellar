/* sprites.c - sprite bitmaps (string-art) + ROM 8x8 font text */
#include <string.h>
#include "sprites.h"
#include "vga.h"

u8 spr_ship[3][SH_SHIP_W * SH_SHIP_H];
u8 spr_enemy[3][SH_EN_W * SH_EN_H];
u8 spr_pbullet[3][SH_PB_W * SH_PB_H];
u8 spr_ebullet[SH_EB_W * SH_EB_H];
u8 spr_powerup[PU_COUNT][SH_PU_W * SH_PU_H];
u8 spr_boss[3][SH_BOSS_W * SH_BOSS_H];
u8 spr_missile[SH_MSL_W * SH_MSL_H];

/* ---- ROM 8x8 font pointer (INT 10h AX=1130h BH=03h -> ES:BP) ---- */
extern u8 __far *get_rom8x8(void);
#pragma aux get_rom8x8 =            \
    "push bp"                       \
    "mov ax,1130h"                  \
    "mov bh,3"                      \
    "int 10h"                       \
    "mov dx,es"                     \
    "mov ax,bp"                     \
    "pop bp"                        \
    value [dx ax]                   \
    modify [ax bx cx dx es];

static u8 __far *rom_font;

static u8 legend(char ch)
{
    switch (ch) {
        case 'o': return C_DGRAY;
        case 'w': return C_WHITE;
        case 'l': return C_LGRAY;
        case 'r': return C_RED;    case 'R': return C_LRED;
        case 'y': return C_YELLOW;
        case 'g': return C_GREEN;  case 'G': return C_LGREEN;
        case 'b': return C_BLUE;   case 'B': return C_LBLUE;
        case 'c': return C_CYAN;   case 'C': return C_LCYAN;
        case 'm': return C_MAGENTA;case 'M': return C_LMAG;
        case 'n': return C_BROWN;
        /* ramp colours for shaded art */
        case 'D': return PAL_FIRE + 3;   /* dark red    */
        case 'O': return PAL_FIRE + 8;   /* orange      */
        case 'F': return PAL_FIRE + 13;  /* bright fire */
        case 'x': return PAL_GLOW + 7;   /* dim glow    */
        case 'X': return PAL_GLOW + 14;  /* bright glow */
        default:  return 0;
    }
}

static void build(u8 *dst, i16 w, i16 h, const char *rows[])
{
    i16 x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            dst[y * w + x] = legend(rows[y][x]);
}

static const char *SHIP[16] = {
".......ww.......",
".......lw.......",
"......olwl......",
"......olwl......",
".....oolwll.....",
".....olxXwl.....",
"....lolxXwll....",
"...llolxXwlll...",
"..lloolwwllwll..",
".ll.oolwwloo.wl.",
"ll..ooolloo...wl",
"l.owooo..ooowo.w",
"..oro......oro..",
"..oro......oro..",
"..OFO......OFO..",
"..F.F......F.F.."
};

static const char *SCOUT[14] = {
"................",
"..rr........rr..",
"..rRl......lRr..",
"..rRRl....lRRr..",
"...rRRllllRRr...",
"...rRRRRRRRRr...",
"....rRXxxXRr....",
"....rRXxxXRr....",
".....rRRRRr.....",
"......lRRl......",
"......oRRo......",
".......FF.......",
".......DD.......",
"................"
};

static const char *WEAVER[14] = {
"................",
"......gGGg......",
"....ggGGGGgg....",
"...gGGGGGGGGg...",
"..gGGXGGGGXGGg..",
"..gGGXGGGGXGGg..",
".gGGGGllllGGGGg.",
"gGGGGGllllGGGGGg",
".gGlGlGGGGlGlGg.",
"..g.l.gGGg.l.g..",
".....gl..lg.....",
"......F..F......",
"................",
"................"
};

static const char *SHOOTER[14] = {
"...m........m...",
"...mm......mm...",
"...mMl....lMm...",
"..mMMmllllmMMm..",
"..mMMMMMMMMMMm..",
"..mMXxMMMMxXMm..",
"..mMXxMwwMxXMm..",
"...mMMMwwMMMm...",
"....mMMMMMMm....",
".....mMMMMm.....",
"......mMMm......",
".......mm.......",
".......FF.......",
"................"
};

static const char *MISSILE[10] = {
"..R..",
".RRR.",
".lwl.",
".lwl.",
".lwl.",
".lwl.",
".olo.",
".olo.",
".OFO.",
"F.F.F"
};

static const char *PBULLET[8] = {
".w.",
"ywy",
"ywy",
"ywy",
"ywy",
"ywy",
"ywy",
".y."
};

static const char *EBULLET[7] = {
"..r..",
".rRr.",
"rRRRr",
"rRRRr",
"rRRRr",
".rRr.",
"..r.."
};

static const char *PU_SHAPE[12] = {
".....oo.....",
"....oFFo....",
"...oFFFFo...",
"..oFFFFFFo..",
".oFFFFFFFFo.",
"oFFFFFFFFFFo",
"oFFFFFFFFFFo",
".oFFFFFFFFo.",
"..oFFFFFFo..",
"...oFFFFo...",
"....oFFo....",
".....oo....."
};

static void build_pu(u8 *dst, u8 fill)
{
    i16 x, y;
    for (y = 0; y < SH_PU_H; y++)
        for (x = 0; x < SH_PU_W; x++) {
            char ch = PU_SHAPE[y][x];
            dst[y * SH_PU_W + x] = (ch == 'F') ? fill : legend(ch);
        }
}

/* boss kinds: 0 grey dreadnought, 1 red warship, 2 green hive */
static void build_boss(u8 *dst, u8 kind)
{
    i16 x, y, hw;
    u8 hull = (kind == 1) ? C_LRED : (kind == 2) ? C_LGREEN : C_LGRAY;
    u8 edge = (kind == 1) ? C_RED  : (kind == 2) ? C_GREEN   : C_DGRAY;
    u8 core = (kind == 2) ? (u8)(PAL_GLOW + 4) : (u8)(PAL_FIRE + 4);
    memset(dst, 0, SH_BOSS_W * SH_BOSS_H);
    for (y = 0; y < SH_BOSS_H; y++) {
        if      (y < 4)  hw = (kind == 2) ? 8 : 4;      /* hive is wider up top */
        else if (y < 10) hw = 6 + (y - 4) * 3;
        else if (y < 24) hw = 22;
        else             hw = 22 - (y - 24) * 2;
        if (hw < 0) hw = 0;
        for (x = 24 - hw; x <= 24 + hw; x++) {
            u8 c = hull;
            if (x < 24 - hw + 2 || x > 24 + hw - 2) c = edge;
            dst[y * SH_BOSS_W + x] = c;
        }
    }
    for (y = 8; y < 16; y++) {
        for (x = 1; x < 7; x++)  dst[y * SH_BOSS_W + x] = (x < 3) ? edge : hull;
        for (x = 41; x < 47; x++) dst[y * SH_BOSS_W + x] = (x > 44) ? edge : hull;
    }
    for (x = 2; x < 6; x++)  dst[15 * SH_BOSS_W + x] = PAL_GLOW + 12;
    for (x = 42; x < 46; x++) dst[15 * SH_BOSS_W + x] = PAL_GLOW + 12;
    for (x = 8; x < 40; x++)
        if (dst[21 * SH_BOSS_W + x]) dst[21 * SH_BOSS_W + x] = (u8)(PAL_GLOW + 8);
    for (y = 12; y < 20; y++) for (x = 20; x < 28; x++) dst[y * SH_BOSS_W + x] = core;
    for (y = 24; y < 30; y++) {
        dst[y * SH_BOSS_W + 14] = PAL_FIRE + 10; dst[y * SH_BOSS_W + 15] = PAL_FIRE + 10;
        dst[y * SH_BOSS_W + 32] = PAL_FIRE + 10; dst[y * SH_BOSS_W + 33] = PAL_FIRE + 10;
    }
}

/* recolour the cannon bullet into laser/wave palettes */
static void build_pbul(u8 *dst, u8 body, u8 core)
{
    i16 x, y;
    for (y = 0; y < SH_PB_H; y++)
        for (x = 0; x < SH_PB_W; x++) {
            char ch = PBULLET[y][x];
            u8 c = 0;
            if (ch == 'w' || ch == 'W') c = core;
            else if (ch == 'y' || ch == 'Y') c = body;
            dst[y * SH_PB_W + x] = c;
        }
}

static void make_banked_ships(void)
{
    i16 x, y;
    build(spr_ship[1], SH_SHIP_W, SH_SHIP_H, SHIP);
    memset(spr_ship[0], 0, SH_SHIP_W * SH_SHIP_H);
    memset(spr_ship[2], 0, SH_SHIP_W * SH_SHIP_H);
    for (y = 0; y < SH_SHIP_H; y++) {
        for (x = 0; x < SH_SHIP_W; x++) {
            u8 c = spr_ship[1][y * SH_SHIP_W + x];
            if (!c) continue;
            if (x > 0) spr_ship[0][y * SH_SHIP_W + x - (y > 6 ? 1 : 0)] = c;
            if (x < SH_SHIP_W - 1) spr_ship[2][y * SH_SHIP_W + x + (y > 6 ? 1 : 0)] = c;
        }
    }
    for (y = 8; y < 13; y++) {
        spr_ship[0][y * SH_SHIP_W + 2] = C_LGRAY;
        spr_ship[2][y * SH_SHIP_W + 13] = C_LGRAY;
    }
}

void sprites_init(void)
{
    make_banked_ships();
    build(spr_enemy[E_SCOUT],   SH_EN_W, SH_EN_H, SCOUT);
    build(spr_enemy[E_WEAVER],  SH_EN_W, SH_EN_H, WEAVER);
    build(spr_enemy[E_SHOOTER], SH_EN_W, SH_EN_H, SHOOTER);
    build(spr_pbullet[WT_CANNON], SH_PB_W, SH_PB_H, PBULLET);
    build_pbul(spr_pbullet[WT_LASER], C_LCYAN, C_WHITE);
    build_pbul(spr_pbullet[WT_WAVE],  C_LMAG,  C_WHITE);
    build(spr_ebullet, SH_EB_W, SH_EB_H, EBULLET);
    build(spr_missile, SH_MSL_W, SH_MSL_H, MISSILE);
    build_pu(spr_powerup[PU_GUN],     PAL_FIRE + 9);   /* orange  */
    build_pu(spr_powerup[PU_RAPID],   C_YELLOW);
    build_pu(spr_powerup[PU_SHIELD],  C_LBLUE);
    build_pu(spr_powerup[PU_LIFE],    C_LGREEN);
    build_pu(spr_powerup[PU_MISSILE], C_LGRAY);
    build_pu(spr_powerup[PU_LASER],   C_LCYAN);
    build_pu(spr_powerup[PU_WAVE],    C_LMAG);
    build_pu(spr_powerup[PU_BOMB],    C_LRED);
    build_pu(spr_powerup[PU_SCORE],   C_WHITE);
    build_boss(spr_boss[0], 0);
    build_boss(spr_boss[1], 1);
    build_boss(spr_boss[2], 2);
    rom_font = get_rom8x8();
}

void text_draw(i16 x, i16 y, const char *s, u8 col)
{
    for (; *s; s++, x += 8) {
        const u8 __far *g = rom_font + (u16)(u8)(*s) * 8;
        i16 r, b;
        if (*s == ' ') continue;
        for (r = 0; r < 8; r++) {
            u8 bits = g[r];
            for (b = 0; b < 8; b++)
                if (bits & (0x80 >> b)) vga_pixel(x + b, y + r, col);
        }
    }
}

void text_center(i16 y, const char *s, u8 col)
{
    i16 len = 0; const char *p = s;
    while (*p++) len++;
    text_draw((SCRW - len * 8) / 2, y, s, col);
}
