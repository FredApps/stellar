/* sprites.c - sprite bitmaps (string-art) + ROM 8x8 font text */
#include <string.h>
#include "sprites.h"
#include "vga.h"

u8 spr_ship[3][SH_SHIP_W * SH_SHIP_H];
u8 spr_enemy[NSTAGE][3][SH_EN_W * SH_EN_H];
u8 spr_pbullet[3][SH_PB_W * SH_PB_H];
u8 spr_ebullet[SH_EB_W * SH_EB_H];
u8 spr_powerup[PU_COUNT][SH_PU_W * SH_PU_H];
u8 spr_boss[NBOSS][BOSS_MAXW * BOSS_MAXH];
i16 spr_boss_w[NBOSS], spr_boss_h[NBOSS];
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

/* --- tiny tightly-packed (stride = w) sprite primitives for the bosses --- */
static void bset(u8 *d, i16 w, i16 h, i16 x, i16 y, u8 c)
{
    if ((u16)x < (u16)w && (u16)y < (u16)h) d[(i16)(y * w + x)] = c;
}
static void bhline(u8 *d, i16 w, i16 h, i16 x0, i16 x1, i16 y, u8 c)
{
    i16 x;
    if (x0 < 0) x0 = 0;
    if (x1 > w - 1) x1 = w - 1;
    for (x = x0; x <= x1; x++) bset(d, w, h, x, y, c);
}
static void brect(u8 *d, i16 w, i16 h, i16 x0, i16 y0, i16 x1, i16 y1, u8 c)
{
    i16 y;
    for (y = y0; y <= y1; y++) bhline(d, w, h, x0, x1, y, c);
}

/* Each roster slot has its own silhouette, footprint and palette so bosses
   read as different ships, not recolours. dims returned via *ow/*oh. */
static void build_boss(u8 *dst, u8 kind, i16 *ow, i16 *oh)
{
    i16 x, y, cx, w, h;
    switch (kind) {

    case 0: {   /* GORGON - wide low grey battle-slab, red core, gun ports */
        w = 56; h = 28; cx = w / 2;
        memset(dst, 0, w * h);
        for (y = 0; y < h; y++) {
            i16 half = (y < 4) ? 16 + y : (y < 22) ? 20 + (y >> 3) : 22 - (y - 22);
            bhline(dst, w, h, cx - half, cx + half, y, C_LGRAY);
            bset(dst, w, h, cx - half, y, C_DGRAY);
            bset(dst, w, h, cx + half, y, C_DGRAY);
        }
        bhline(dst, w, h, 4, w - 5, 6, C_DGRAY);          /* armour seams */
        bhline(dst, w, h, 3, w - 4, 19, C_DGRAY);
        brect(dst, w, h, cx - 6, 8, cx + 5, 18, PAL_FIRE + 6);  /* core */
        brect(dst, w, h, cx - 6, 8, cx + 5, 8, C_RED);
        brect(dst, w, h, cx - 6, 18, cx + 5, 18, C_RED);
        for (x = 6; x < w - 6; x += 6) bset(dst, w, h, x, h - 3, PAL_FIRE + 12); /* gun ports */
        bset(dst, w, h, 8, 4, C_YELLOW);  bset(dst, w, h, w - 9, 4, C_YELLOW);
        break; }

    case 1: {   /* REAPER - small crimson dagger pointing DOWN, swept wings */
        w = 32; h = 28; cx = w / 2;
        memset(dst, 0, w * h);
        for (y = 0; y < h; y++) {
            i16 half = 13 - (i16)((y * 11) / h);          /* wide top -> point */
            if (half < 1) half = 1;
            bhline(dst, w, h, cx - half, cx + half, y, C_LRED);
            bset(dst, w, h, cx - half, y, C_RED);
            bset(dst, w, h, cx + half, y, C_RED);
        }
        for (y = 5; y < 12; y++) {                        /* swept-back wings */
            i16 s = 14 - (y - 5);
            bhline(dst, w, h, cx - s, cx - s + 2, y, C_RED);
            bhline(dst, w, h, cx + s - 2, cx + s, y, C_RED);
        }
        brect(dst, w, h, cx - 2, 3, cx + 1, 7, C_YELLOW); /* cockpit eye */
        bset(dst, w, h, cx - 1, 5, PAL_FIRE + 14);
        bset(dst, w, h, cx, 5, PAL_FIRE + 14);
        bset(dst, w, h, cx - 1, h - 1, C_WHITE);          /* piercing tip */
        break; }

    case 2: {   /* SEEKER - green orbiter disc with a glowing eye + spokes */
        w = 40; h = 26; cx = w / 2;
        memset(dst, 0, w * h);
        for (y = 0; y < h; y++) {
            i16 dy = y - (h / 2);
            i16 ay = dy < 0 ? -dy : dy;
            i16 half = (ay < 4) ? 18 : (ay < 8) ? 15 : (ay < 11) ? 9 : 3;
            bhline(dst, w, h, cx - half, cx + half, y, C_LGREEN);
            bset(dst, w, h, cx - half, y, C_GREEN);
            bset(dst, w, h, cx + half, y, C_GREEN);
        }
        for (x = 4; x < w - 4; x += 5) {                  /* rotating-spoke hint */
            bset(dst, w, h, x, h / 2, PAL_GLOW + 12);
        }
        brect(dst, w, h, cx - 4, h / 2 - 3, cx + 3, h / 2 + 2, PAL_GLOW + 8);
        brect(dst, w, h, cx - 2, h / 2 - 1, cx + 1, h / 2, C_WHITE);   /* eye */
        break; }

    case 3: {   /* LEVIATHAN - wide grey carrier with two open hangar bays */
        w = 64; h = 30; cx = w / 2;
        memset(dst, 0, w * h);
        brect(dst, w, h, 3, 3, w - 4, h - 5, C_LGRAY);
        brect(dst, w, h, 3, 3, w - 4, 3, C_DGRAY);        /* top/bottom edges */
        brect(dst, w, h, 3, h - 5, w - 4, h - 5, C_DGRAY);
        brect(dst, w, h, cx - 7, 1, cx + 6, 12, C_LGRAY); /* bridge tower */
        brect(dst, w, h, cx - 5, 3, cx + 4, 9, PAL_GLOW + 6);
        brect(dst, w, h, 8, 11, 20, h - 8, C_BLACK);      /* left hangar bay */
        brect(dst, w, h, w - 21, 11, w - 9, h - 8, C_BLACK); /* right hangar bay */
        for (y = 11; y <= h - 8; y++) {                   /* bay frame lights */
            bset(dst, w, h, 8, y, C_LGREEN);  bset(dst, w, h, 20, y, C_LGREEN);
            bset(dst, w, h, w - 21, y, C_LGREEN); bset(dst, w, h, w - 9, y, C_LGREEN);
        }
        for (x = 6; x < w - 6; x += 8) bset(dst, w, h, x, h - 3, PAL_FIRE + 10); /* thrusters */
        break; }

    default: {  /* OVERLORD - tall magenta finale with white spine + emitters */
        w = 48; h = 40; cx = w / 2;
        memset(dst, 0, w * h);
        for (y = 0; y < h; y++) {
            i16 half = (y < 8) ? 2 + y * 2 : (y < 26) ? 20 : (y < 34) ? 26 - (y - 26) : 14;
            bhline(dst, w, h, cx - half, cx + half, y, C_LMAG);
            bset(dst, w, h, cx - half, y, C_MAGENTA);
            bset(dst, w, h, cx + half, y, C_MAGENTA);
        }
        for (y = 6; y < h - 6; y++) {                     /* white spine */
            bset(dst, w, h, cx - 1, y, C_WHITE);
            bset(dst, w, h, cx, y, C_WHITE);
        }
        brect(dst, w, h, cx - 5, 16, cx + 4, 26, PAL_GLOW + 4);  /* core */
        for (x = 8; x < w - 8; x += 4) {                  /* emitter studs */
            bset(dst, w, h, x, 8, PAL_GLOW + 12);
            bset(dst, w, h, x, h - 8, PAL_GLOW + 10);
        }
        for (y = 12; y < 28; y++) { bset(dst, w, h, 9, y, C_MAGENTA); bset(dst, w, h, w - 10, y, C_MAGENTA); }
        break; }
    }
    *ow = w; *oh = h;
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
    {
        i16 b;
        for (b = 0; b < NBOSS; b++)
            build_boss(spr_boss[b], (u8)b, &spr_boss_w[b], &spr_boss_h[b]);
    }
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
