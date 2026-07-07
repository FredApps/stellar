/* sprites.c - sprite bitmaps (string-art) + ROM 8x8 font text */
#include <string.h>
#include "sprites.h"
#include "vga.h"

u8 spr_ship[3][SH_SHIP_W * SH_SHIP_H];
u8 spr_enemy[NSTAGE][3][SH_EN_W * SH_EN_H];
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

/* per-stage enemy skins: keep the type's body hue (so scout/weaver/shooter
   stay readable) but retint the outline + glow and stamp escalating accent
   pixels (nubs -> fins -> spikes) so each 4-wave block looks distinct. Stage 0
   is the untouched original. Themes track the nebula: violet / teal / ember. */
static const u8 stage_trim[NSTAGE]   = { C_DGRAY, C_MAGENTA, C_CYAN, PAL_FIRE + 8 };
static const u8 stage_glow[NSTAGE]   = { PAL_GLOW + 10, C_LMAG, C_LCYAN, PAL_FIRE + 12 };
static const u8 stage_accent[NSTAGE] = { PAL_GLOW + 12, C_LMAG, C_LCYAN, PAL_FIRE + 13 };

static void build_enemy_stage(u8 *dst, const char *rows[], u8 stage)
{
    i16 i;
    u8 trim = stage_trim[stage], glow = stage_glow[stage], acc = stage_accent[stage];
    build(dst, SH_EN_W, SH_EN_H, rows);
    if (stage == 0) return;                       /* original look */
    for (i = 0; i < SH_EN_W * SH_EN_H; i++) {     /* retint outline + glow */
        u8 c = dst[i];
        if (c == C_DGRAY) dst[i] = trim;
        else if (c >= PAL_GLOW && c < PAL_GLOW + 16) dst[i] = glow;
    }
#define EP(x,y) dst[(y) * SH_EN_W + (x)] = acc
    if (stage >= 1) { EP(2,7); EP(13,7); }                    /* side nubs  */
    if (stage >= 2) { EP(1,6); EP(14,6); EP(1,8); EP(14,8); } /* side fins  */
    if (stage >= 3) { EP(3,0); EP(12,0); EP(2,1); EP(13,1); } /* top spikes */
#undef EP
}

void sprites_init(void)
{
    int s;
    make_banked_ships();
    for (s = 0; s < NSTAGE; s++) {
        build_enemy_stage(spr_enemy[s][E_SCOUT],   SCOUT,   (u8)s);
        build_enemy_stage(spr_enemy[s][E_WEAVER],  WEAVER,  (u8)s);
        build_enemy_stage(spr_enemy[s][E_SHOOTER], SHOOTER, (u8)s);
    }
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
        /* fast path: whole glyph on-screen -> write rows unrolled, no per-
           pixel clipping (this is the hot cost on the help/score screens). */
        if ((u16)x <= SCRW - 8 && (u16)y <= SCRH - 8) {
            u8 __far *p = g_back + (u16)y * SCRW + (u16)x;
            for (r = 0; r < 8; r++, p += SCRW) {
                u8 bits = g[r];
                if (!bits) continue;
                if (bits & 0x80) p[0] = col;
                if (bits & 0x40) p[1] = col;
                if (bits & 0x20) p[2] = col;
                if (bits & 0x10) p[3] = col;
                if (bits & 0x08) p[4] = col;
                if (bits & 0x04) p[5] = col;
                if (bits & 0x02) p[6] = col;
                if (bits & 0x01) p[7] = col;
            }
        } else {                                  /* clipped path */
            for (r = 0; r < 8; r++) {
                u8 bits = g[r];
                for (b = 0; b < 8; b++)
                    if (bits & (0x80 >> b)) vga_pixel(x + b, y + r, col);
            }
        }
    }
}

void text_center(i16 y, const char *s, u8 col)
{
    i16 len = 0; const char *p = s;
    while (*p++) len++;
    text_draw((SCRW - len * 8) / 2, y, s, col);
}
