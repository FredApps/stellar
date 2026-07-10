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
u8 spr_bosspod[SH_POD_W * SH_POD_H];
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
static void bvline(u8 *d, i16 w, i16 h, i16 x, i16 y0, i16 y1, u8 c)
{
    i16 y;
    if (y0 < 0) y0 = 0;
    if (y1 > h - 1) y1 = h - 1;
    for (y = y0; y <= y1; y++) bset(d, w, h, x, y, c);
}
static void bdisc(u8 *d, i16 w, i16 h, i16 cx, i16 cy, i16 r, u8 c)
{
    i16 x, y;
    i16 r2 = (i16)(r * r);
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++)
            if (x * x + y * y <= r2) bset(d, w, h, cx + x, cy + y, c);
}
/* annulus: pixels with r0 <= dist <= r1 */
static void bring(u8 *d, i16 w, i16 h, i16 cx, i16 cy, i16 r0, i16 r1, u8 c)
{
    i16 x, y;
    i16 a = (i16)(r0 * r0), b = (i16)(r1 * r1);
    for (y = -r1; y <= r1; y++)
        for (x = -r1; x <= r1; x++) {
            i16 d2 = (i16)(x * x + y * y);
            if (d2 >= a && d2 <= b) bset(d, w, h, cx + x, cy + y, c);
        }
}
/* checkerboard fill for shaded armour */
static void bdither(u8 *d, i16 w, i16 h, i16 x0, i16 y0, i16 x1, i16 y1, u8 c)
{
    i16 x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (((x ^ y) & 1) == 0) bset(d, w, h, x, y, c);
}
/* author the left half, mirror onto the right (symmetric hulls) */
static void bmirror(u8 *d, i16 w, i16 h)
{
    i16 x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w / 2; x++)
            d[y * w + (w - 1 - x)] = d[y * w + x];
}

/* Each roster slot has its own silhouette, footprint and palette so bosses
   read as different ships, not recolours. dims returned via *ow/*oh.
   Several bosses lean on run-time overlays in game.c (KRAKEN tentacles,
   VORTEX orbs, SEEKER/BASILISK tracking pupils, NEXUS pods). */
static void build_boss(u8 *dst, u8 kind, i16 *ow, i16 *oh)
{
    i16 x, y, cx, w, h;
    memset(dst, 0, BOSS_MAXW * BOSS_MAXH);
    switch (kind) {

    case 0: {   /* GORGON - serrated siege wall, exposed fire trench */
        w = 68; h = 30; cx = w / 2;
        for (x = 0; x < w; x += 8) {                       /* serrated crown */
            bhline(dst, w, h, x + 2, x + 5, 2, C_LGRAY);
            bhline(dst, w, h, x + 1, x + 6, 3, C_LGRAY);
            bhline(dst, w, h, x, x + 7, 4, C_LGRAY);
            bset(dst, w, h, (i16)(x + 3), 1, C_YELLOW);    /* sensor tips */
        }
        brect(dst, w, h, 0, 5, w - 1, 12, C_LGRAY);        /* upper armour band */
        brect(dst, w, h, 1, 13, w - 2, 19, C_DGRAY);       /* mid band          */
        bdither(dst, w, h, 2, 20, w - 3, 26, C_LGRAY);     /* lower skirt       */
        bhline(dst, w, h, 0, w - 1, 5, C_WHITE);
        for (x = 10; x < w - 10; x += 12) bvline(dst, w, h, x, 6, 18, C_DGRAY);
        brect(dst, w, h, cx - 11, 8, cx + 10, 17, C_BLACK);      /* core trench */
        brect(dst, w, h, cx - 10, 9, cx + 9, 16, PAL_FIRE + 6);
        brect(dst, w, h, cx - 6, 11, cx + 5, 14, PAL_FIRE + 11);
        bhline(dst, w, h, cx - 11, cx + 10, 8, C_RED);
        bhline(dst, w, h, cx - 11, cx + 10, 17, C_RED);
        for (x = 6; x < w - 6; x += 8) {                   /* gun ports */
            bset(dst, w, h, x, 27, PAL_FIRE + 12);
            bset(dst, w, h, x, 28, PAL_FIRE + 8);
        }
        break; }

    case 1: {   /* REAPER - asymmetric crescent scythe, skull cockpit */
        w = 34; h = 33; cx = w / 2;
        for (y = 0; y < 30; y++) {                         /* curved blade */
            i16 x0 = (i16)(2 + (y * y) / 45);
            i16 x1 = (i16)(x0 + 11 - y / 3);
            if (x1 <= x0) x1 = (i16)(x0 + 1);
            bhline(dst, w, h, x0, x1, y, C_LRED);
            bset(dst, w, h, x0, y, C_WHITE);               /* honed edge */
            bset(dst, w, h, x1, y, C_RED);
        }
        bhline(dst, w, h, 21, 22, 30, C_LRED);
        bset(dst, w, h, 22, 31, C_WHITE);                  /* blade point */
        bvline(dst, w, h, 26, 4, 26, C_DGRAY);             /* shaft */
        bvline(dst, w, h, 27, 3, 27, C_LGRAY);
        bvline(dst, w, h, 28, 4, 26, C_DGRAY);
        brect(dst, w, h, 12, 2, 26, 4, C_RED);             /* crossbar */
        brect(dst, w, h, 23, 6, 31, 13, C_LGRAY);          /* skull */
        bset(dst, w, h, 25, 9, PAL_FIRE + 13); bset(dst, w, h, 29, 9, PAL_FIRE + 13);
        bhline(dst, w, h, 24, 30, 12, C_DGRAY);            /* jaw */
        bset(dst, w, h, 25, 13, C_DGRAY); bset(dst, w, h, 27, 13, C_DGRAY);
        bset(dst, w, h, 29, 13, C_DGRAY);
        brect(dst, w, h, 26, 27, 28, 31, PAL_FIRE + 9);    /* shaft engine */
        break; }

    case 2: {   /* LEVIATHAN - full-width strike carrier, runway deck */
        w = 72; h = 32; cx = w / 2;
        brect(dst, w, h, 2, 8, w - 3, 26, C_LGRAY);        /* deck */
        bhline(dst, w, h, 2, w - 3, 8, C_WHITE);
        brect(dst, w, h, 0, 12, 3, 24, C_DGRAY);           /* prow wedges */
        brect(dst, w, h, w - 4, 12, w - 1, 24, C_DGRAY);
        for (x = 6; x < w - 6; x += 6)                     /* runway stripes */
            bset(dst, w, h, x, 17, (x & 4) ? C_WHITE : C_YELLOW);
        brect(dst, w, h, 8, 11, 24, 23, C_BLACK);          /* twin hangar maws */
        brect(dst, w, h, w - 25, 11, w - 9, 23, C_BLACK);
        for (y = 11; y <= 23; y++) {                       /* green guide lights */
            bset(dst, w, h, 8, y, C_LGREEN);  bset(dst, w, h, 24, y, C_LGREEN);
            bset(dst, w, h, w - 25, y, C_LGREEN); bset(dst, w, h, w - 9, y, C_LGREEN);
        }
        for (x = 10; x < 24; x += 4) bset(dst, w, h, x, 23, C_GREEN);
        for (x = w - 23; x < w - 9; x += 4) bset(dst, w, h, x, 23, C_GREEN);
        brect(dst, w, h, cx - 7, 2, cx + 6, 10, C_DGRAY);  /* bridge tower */
        brect(dst, w, h, cx - 4, 4, cx + 3, 8, PAL_GLOW + 9);
        bhline(dst, w, h, cx - 7, cx + 6, 2, C_LGRAY);
        bhline(dst, w, h, 4, w - 5, 27, C_DGRAY);
        for (x = 8; x < w - 8; x += 8) {                   /* engine row */
            bset(dst, w, h, x, 28, PAL_FIRE + 11);
            bset(dst, w, h, x, 29, PAL_FIRE + 7);
        }
        break; }

    case 3: {   /* SEEKER - concentric glow rings around a watching eye */
        w = 40; h = 40; cx = w / 2;
        bring(dst, w, h, cx, 20, 18, 19, PAL_GLOW + 5);
        bring(dst, w, h, cx, 20, 13, 14, PAL_GLOW + 9);
        bring(dst, w, h, cx, 20, 8, 9, PAL_GLOW + 13);
        bdisc(dst, w, h, cx, 20, 6, C_GREEN);
        bdisc(dst, w, h, cx, 20, 4, C_LGREEN);
        bvline(dst, w, h, cx, 1, 6, C_LGREEN);             /* four ring spokes */
        bvline(dst, w, h, cx, 33, 38, C_LGREEN);
        bhline(dst, w, h, 1, 6, 20, C_LGREEN);
        bhline(dst, w, h, 33, 38, 20, C_LGREEN);
        bset(dst, w, h, cx, 1, C_WHITE); bset(dst, w, h, cx, 38, C_WHITE);
        bset(dst, w, h, 1, 20, C_WHITE); bset(dst, w, h, 38, 20, C_WHITE);
        /* pupil socket: the pupil itself tracks the player at draw time */
        brect(dst, w, h, cx - 2, 18, cx + 1, 21, C_BLACK);
        break; }

    case 4: {   /* MANTIS - oversized serrated claw crescents, thin thorax */
        w = 60; h = 30; cx = w / 2;
        brect(dst, w, h, cx - 4, 6, cx + 3, 25, C_GREEN);  /* thorax */
        brect(dst, w, h, cx - 2, 10, cx + 1, 20, PAL_GLOW + 10);
        bset(dst, w, h, cx - 3, 4, C_LGREEN); bset(dst, w, h, cx - 4, 3, C_LGREEN);
        for (y = 1; y < 27; y++) {                         /* claw crescent */
            i16 dy2 = (i16)(y - 14);
            i16 span = (i16)(17 - (dy2 * dy2) / 12);
            if (span < 3) span = 3;
            bhline(dst, w, h, 2, 2 + span, y, (y & 1) ? C_GREEN : C_LGREEN);
        }
        for (y = 3; y < 25; y += 3) {                      /* serrated teeth */
            i16 dy2 = (i16)(y - 14);
            i16 span = (i16)(17 - (dy2 * dy2) / 12);
            if (span < 3) span = 3;
            bset(dst, w, h, (i16)(3 + span), y, C_LGREEN);
            bset(dst, w, h, (i16)(4 + span), y, C_GREEN);
        }
        bset(dst, w, h, 20, 13, C_YELLOW); bset(dst, w, h, 20, 15, C_YELLOW);
        bset(dst, w, h, 21, 14, C_WHITE);                  /* claw-tip node */
        bmirror(dst, w, h);
        break; }

    case 5: {   /* ANVIL - industrial crush press with hazard chevrons */
        w = 48; h = 30; cx = w / 2;
        brect(dst, w, h, 2, 1, w - 3, 8, C_DGRAY);         /* top girder */
        bhline(dst, w, h, 2, w - 3, 1, C_LGRAY);
        for (x = 5; x < w - 5; x += 6) bset(dst, w, h, x, 4, C_WHITE);
        for (x = 10; x <= w - 12; x += 12) {               /* piston shafts */
            bvline(dst, w, h, x, 9, 20, C_LGRAY);
            bvline(dst, w, h, (i16)(x + 1), 9, 20, C_DGRAY);
        }
        brect(dst, w, h, 4, 21, w - 5, 27, C_RED);         /* crush plate */
        bhline(dst, w, h, 4, w - 5, 21, C_LRED);
        for (x = 4; x <= w - 5; x++)                       /* hazard chevrons */
            if (((x >> 2) & 1) == 0) bset(dst, w, h, x, 24, C_YELLOW);
        bhline(dst, w, h, 6, w - 7, 28, C_DGRAY);
        break; }

    case 6: {   /* SERAPH - haloed sentinel, layered feather ribs */
        w = 56; h = 48; cx = w / 2;
        bhline(dst, w, h, cx - 6, cx + 5, 1, PAL_GLOW + 14);          /* halo */
        bset(dst, w, h, cx - 7, 2, PAL_GLOW + 11); bset(dst, w, h, cx + 6, 2, PAL_GLOW + 11);
        brect(dst, w, h, cx - 3, 4, cx + 2, 9, C_WHITE);              /* head */
        for (y = 10; y < 44; y++) {                                   /* robe body */
            i16 half = (y < 30) ? 5 : (i16)(5 + (y - 30) / 3);
            bhline(dst, w, h, cx - half, cx + half, y, C_LCYAN);
        }
        bvline(dst, w, h, cx - 1, 12, 40, C_WHITE);
        bvline(dst, w, h, cx, 12, 40, C_WHITE);
        brect(dst, w, h, cx - 2, 22, cx + 1, 28, PAL_GLOW + 12);      /* heart core */
        for (x = 0; x < 5; x++) {                                     /* feather ribs */
            i16 rx = (i16)(cx - 9 - x * 4);
            i16 top = (i16)(12 + x * 2);
            i16 bot = (i16)(42 - x * 4);
            bvline(dst, w, h, rx, top, bot, (u8)(PAL_GLOW + 13 - x * 2));
            bvline(dst, w, h, (i16)(rx - 1), top, bot, (u8)(PAL_GLOW + 11 - x * 2));
            bset(dst, w, h, rx, (i16)(bot + 1), C_WHITE);             /* feather tip */
        }
        bmirror(dst, w, h);
        break; }

    case 7: {   /* NEXUS - narrow core spire; fire-pods orbit at run time */
        w = 22; h = 28; cx = w / 2;
        for (y = 0; y < h; y++) {                          /* diamond spire */
            i16 dy2 = (i16)(y - h / 2);
            i16 ay = (dy2 < 0) ? (i16)-dy2 : dy2;
            i16 half = (i16)(9 - (ay * 2) / 3);
            if (half < 1) half = 1;
            bhline(dst, w, h, cx - half, cx + half, y, C_DGRAY);
            bset(dst, w, h, cx - half, y, C_LMAG);
            bset(dst, w, h, cx + half, y, C_LMAG);
        }
        brect(dst, w, h, cx - 3, 10, cx + 2, 17, PAL_GLOW + 8);
        brect(dst, w, h, cx - 1, 12, cx, 15, C_WHITE);
        bset(dst, w, h, cx - 1, 0, C_WHITE); bset(dst, w, h, cx, 0, C_WHITE);
        bset(dst, w, h, cx - 1, h - 1, C_WHITE); bset(dst, w, h, cx, h - 1, C_WHITE);
        break; }

    case 8: {   /* KRAKEN - bulbous mantle + eyes; tentacles animate at run time */
        w = 64; h = 30; cx = w / 2;
        for (y = 0; y < h; y++) {                          /* dome profile */
            i16 half = (y < 4) ? (i16)(14 + y * 4) : (y < 22) ? 30 : (i16)(30 - (y - 22) * 3);
            if (half > 30) half = 30;
            if (half < 6) half = 6;
            bhline(dst, w, h, cx - half, cx + half, y, C_GREEN);
            bset(dst, w, h, cx - half, y, C_LGREEN);
            bset(dst, w, h, cx + half, y, C_LGREEN);
        }
        bdither(dst, w, h, cx - 24, 3, cx + 24, 10, PAL_NEB + 10);   /* sheen */
        bdisc(dst, w, h, cx - 12, 15, 4, C_WHITE);         /* eyes */
        bdisc(dst, w, h, cx + 12, 15, 4, C_WHITE);
        bdisc(dst, w, h, cx - 12, 15, 2, C_BLACK);
        bdisc(dst, w, h, cx + 12, 15, 2, C_BLACK);
        bset(dst, w, h, cx - 13, 14, C_LCYAN); bset(dst, w, h, cx + 11, 14, C_LCYAN);
        brect(dst, w, h, cx - 3, 22, cx + 2, 27, C_BLACK); /* beak */
        bset(dst, w, h, cx - 1, 27, C_YELLOW); bset(dst, w, h, cx, 27, C_YELLOW);
        break; }

    case 9: {   /* PHANTOM - hollow spectre: dashed outline, dissolving tail */
        w = 34; h = 40; cx = w / 2;
        for (y = 0; y < h; y++) {
            i16 half = (y < 10) ? (i16)(3 + y) : (y < 28) ? 13 : (i16)(13 - (y - 28));
            if (half < 2) half = 2;
            if ((y & 3) != 3) {                            /* gaps in the outline */
                bset(dst, w, h, cx - half, y, (y & 1) ? C_LBLUE : C_LCYAN);
                bset(dst, w, h, cx + half, y, (y & 1) ? C_LBLUE : C_LCYAN);
            }
        }
        bvline(dst, w, h, cx - 1, 6, 26, C_WHITE);         /* inner wisp */
        bvline(dst, w, h, cx, 6, 26, C_WHITE);
        bset(dst, w, h, cx - 5, 14, (u8)(PAL_GLOW + 13));  /* hollow eyes */
        bset(dst, w, h, cx + 5, 14, (u8)(PAL_GLOW + 13));
        bset(dst, w, h, cx - 5, 15, (u8)(PAL_GLOW + 13));
        bset(dst, w, h, cx + 5, 15, (u8)(PAL_GLOW + 13));
        bdither(dst, w, h, cx - 8, 30, cx + 8, 38, C_LBLUE); /* dissolving tail */
        break; }

    case 10: {  /* CITADEL - crenellated battlement, three turret towers */
        w = 72; h = 34; cx = w / 2;
        brect(dst, w, h, 1, 14, w - 2, 30, C_DGRAY);       /* main wall */
        bhline(dst, w, h, 1, w - 2, 14, C_LGRAY);
        bdither(dst, w, h, 3, 24, w - 4, 29, C_LGRAY);     /* stonework */
        for (x = 2; x < w - 2; x += 8)                     /* merlons */
            brect(dst, w, h, x, 10, (i16)(x + 4), 14, C_LGRAY);
        for (x = 0; x < 3; x++) {                          /* turret towers */
            i16 tx = (x == 0) ? 8 : (x == 1) ? (i16)(cx - 6) : (i16)(w - 20);
            brect(dst, w, h, tx, 2, (i16)(tx + 11), 16, C_LGRAY);
            bhline(dst, w, h, tx, (i16)(tx + 11), 2, C_WHITE);
            brect(dst, w, h, (i16)(tx + 3), 6, (i16)(tx + 8), 9, C_BLACK);
            brect(dst, w, h, (i16)(tx + 4), 7, (i16)(tx + 7), 8, C_RED);
            bset(dst, w, h, (i16)(tx + 5), 17, PAL_FIRE + 12);
        }
        brect(dst, w, h, cx - 5, 20, cx + 4, 29, C_BLACK); /* gate */
        brect(dst, w, h, cx - 4, 21, cx + 3, 28, PAL_FIRE + 7);
        for (x = 6; x < w - 6; x += 10) bset(dst, w, h, x, 31, PAL_FIRE + 10);
        break; }

    case 11: {  /* VORTEX - split mag/cyan ring; orbs orbit at run time */
        w = 44; h = 44; cx = w / 2;
        bring(dst, w, h, cx, 22, 15, 20, C_LMAG);
        for (y = 0; y < h; y++)                            /* right half cyan */
            for (x = cx; x < w; x++)
                if (dst[y * w + x] == C_LMAG) dst[y * w + x] = C_LCYAN;
        bring(dst, w, h, cx, 22, 6, 8, C_LCYAN);           /* inner ring */
        for (y = 0; y < h; y++)                            /* inner-left magenta */
            for (x = 0; x < cx; x++)
                if (dst[y * w + x] == C_LCYAN) dst[y * w + x] = C_LMAG;
        brect(dst, w, h, cx - 1, 21, cx, 22, C_WHITE);     /* singularity */
        bset(dst, w, h, cx, 3, C_WHITE); bset(dst, w, h, cx, 40, C_WHITE);
        bset(dst, w, h, 3, 22, C_WHITE); bset(dst, w, h, 40, 22, C_WHITE);
        break; }

    case 12: {  /* BASILISK - serpent skull, one huge tracking eye */
        w = 48; h = 32; cx = w / 2;
        for (y = 2; y < 30; y++) {                         /* skull wedge */
            i16 half = (y < 12) ? (i16)(10 + y) : (i16)(22 - (y - 12) / 2);
            if (half > 22) half = 22;
            bhline(dst, w, h, cx - half, cx + half, y, C_GREEN);
            bset(dst, w, h, cx - half, y, C_LGREEN);
            bset(dst, w, h, cx + half, y, C_LGREEN);
        }
        bdither(dst, w, h, cx - 18, 16, cx + 18, 27, C_LGREEN);  /* scales */
        for (x = 0; x < 4; x++) {                          /* crest spikes */
            i16 sx = (i16)(cx - 9 + x * 6);
            bset(dst, w, h, sx, 1, C_LGREEN);
            bset(dst, w, h, sx, 0, C_WHITE);
        }
        bring(dst, w, h, cx, 13, 7, 8, C_YELLOW);          /* eye socket */
        bdisc(dst, w, h, cx, 13, 6, C_YELLOW);
        bdisc(dst, w, h, cx, 13, 4, C_RED);
        brect(dst, w, h, cx - 1, 12, cx, 14, C_BLACK);     /* slit pupil base */
        bset(dst, w, h, cx - 6, 28, C_WHITE); bset(dst, w, h, cx + 5, 28, C_WHITE);
        bset(dst, w, h, cx - 6, 29, C_WHITE); bset(dst, w, h, cx + 5, 29, C_WHITE);
        break; }

    case 13: {  /* TITAN - tri-layer dreadnought; armour breaks per phase */
        w = 72; h = 44; cx = w / 2;
        brect(dst, w, h, 0, 26, w - 1, 38, C_DGRAY);       /* lower hull */
        bdither(dst, w, h, 2, 27, w - 3, 33, C_LGRAY);     /* armour texture */
        brect(dst, w, h, 6, 14, w - 7, 26, C_LGRAY);       /* mid hull */
        brect(dst, w, h, 14, 4, w - 15, 14, C_DGRAY);      /* upper hull */
        bhline(dst, w, h, 14, w - 15, 4, C_WHITE);
        bhline(dst, w, h, 6, w - 7, 14, C_WHITE);
        bhline(dst, w, h, 0, w - 1, 26, C_LGRAY);
        brect(dst, w, h, cx - 9, 0, cx + 8, 24, C_LGRAY);  /* command spine */
        bvline(dst, w, h, cx - 9, 0, 24, C_DGRAY); bvline(dst, w, h, cx + 8, 0, 24, C_DGRAY);
        brect(dst, w, h, cx - 4, 6, cx + 3, 20, PAL_FIRE + 8);   /* reactor slot */
        brect(dst, w, h, cx - 2, 9, cx + 1, 17, PAL_FIRE + 13);
        for (x = 10; x < w - 10; x += 12) {                /* heavy cannon mounts */
            brect(dst, w, h, x, 30, (i16)(x + 5), 36, C_RED);
            bset(dst, w, h, (i16)(x + 2), 37, PAL_FIRE + 12);
        }
        for (x = 4; x < w - 4; x += 10)                    /* engine bank glow */
            brect(dst, w, h, x, 40, (i16)(x + 4), 42, (u8)(PAL_FIRE + 9));
        brect(dst, w, h, 0, 16, 6, 30, C_LGRAY);           /* side pontoons */
        brect(dst, w, h, w - 7, 16, w - 1, 30, C_LGRAY);
        break; }

    default: {  /* OVERLORD - crowned obelisk finale, cycling core */
        w = 64; h = 52; cx = w / 2;
        for (x = 0; x < 5; x++) {                          /* crown spikes */
            i16 sx = (i16)(cx - 12 + x * 6);
            bvline(dst, w, h, sx, (x == 2) ? 0 : 2, 7, C_LMAG);
            bset(dst, w, h, sx, (x == 2) ? 0 : 2, C_WHITE);
        }
        for (y = 7; y < 46; y++) {                         /* obelisk body */
            i16 half = (i16)(8 + (y * 14) / 46);
            bhline(dst, w, h, cx - half, cx + half, y, C_MAGENTA);
            bset(dst, w, h, cx - half, y, C_LMAG);
            bset(dst, w, h, cx + half, y, C_LMAG);
        }
        bdither(dst, w, h, cx - 18, 30, cx + 18, 44, C_LMAG);
        bvline(dst, w, h, cx - 1, 7, 45, C_WHITE);         /* spine */
        bvline(dst, w, h, cx, 7, 45, C_WHITE);
        brect(dst, w, h, cx - 6, 20, cx + 5, 32, (u8)(PAL_GLOW + 4));  /* core */
        brect(dst, w, h, cx - 3, 23, cx + 2, 29, (u8)(PAL_GLOW + 10));
        brect(dst, w, h, 2, 24, 8, 40, C_MAGENTA);         /* emitter pylons */
        brect(dst, w, h, w - 9, 24, w - 3, 40, C_MAGENTA);
        bvline(dst, w, h, 5, 20, 23, C_LMAG); bvline(dst, w, h, w - 6, 20, 23, C_LMAG);
        bset(dst, w, h, 5, 19, C_WHITE); bset(dst, w, h, w - 6, 19, C_WHITE);
        bhline(dst, w, h, 8, cx - 8, 27, C_LMAG);          /* pylon struts */
        bhline(dst, w, h, cx + 7, w - 9, 27, C_LMAG);
        for (x = cx - 16; x <= cx + 16; x += 4)            /* engine flare base */
            bset(dst, w, h, x, 47, (u8)(PAL_FIRE + 10));
        bhline(dst, w, h, cx - 20, cx + 20, 46, C_DGRAY);
        break; }
    }
    *ow = w; *oh = h;
}

/* NEXUS orbiting fire-pod (drawn twice at boss.px[]/py[]) */
static void build_bosspod(u8 *dst)
{
    memset(dst, 0, SH_POD_W * SH_POD_H);
    bring(dst, SH_POD_W, SH_POD_H, 7, 6, 4, 5, C_LMAG);
    bdisc(dst, SH_POD_W, SH_POD_H, 7, 6, 3, (u8)(PAL_GLOW + 8));
    bset(dst, SH_POD_W, SH_POD_H, 7, 6, C_WHITE);
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
    build_bosspod(spr_bosspod);
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
