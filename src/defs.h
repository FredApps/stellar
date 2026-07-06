/* defs.h - shared types and constants for SHOOTER (16-bit real-mode DOS/VGA) */
#ifndef DEFS_H
#define DEFS_H

typedef unsigned char  u8;
typedef unsigned int   u16;
typedef unsigned long  u32;
typedef signed   int   i16;
typedef unsigned char  bool;
#define TRUE  1
#define FALSE 0

#define SCRW    320
#define SCRH    200
#define SCRSZ   64000U          /* 320*200 */

/* build a far pointer from segment:offset */
#define MKFP(seg,off) ((u8 __far *)(((u32)(seg) << 16) | (u16)(off)))

/* VGA default-palette colour indices we rely on */
#define C_BLACK    0
#define C_BLUE     1
#define C_GREEN    2
#define C_CYAN     3
#define C_RED      4
#define C_MAGENTA  5
#define C_BROWN    6
#define C_LGRAY    7
#define C_DGRAY    8
#define C_LBLUE    9
#define C_LGREEN   10
#define C_LCYAN    11
#define C_LRED     12
#define C_LMAG     13
#define C_YELLOW   14
#define C_WHITE    15

/* custom palette ramps (set in vga_init), 16 entries each */
#define PAL_FIRE   16    /* 16..31  dark red -> orange -> white   */
#define PAL_NEB    32    /* 32..47  black -> deep blue nebula     */
#define PAL_GLOW   48    /* 48..63  dark cyan -> bright glow      */

/* keyboard scancodes (make codes) */
#define SC_ESC    0x01
#define SC_SPACE  0x39
#define SC_UP     0x48
#define SC_DOWN   0x50
#define SC_LEFT   0x4B
#define SC_RIGHT  0x4D
#define SC_ENTER  0x1C
#define SC_BKSP   0x0E
#define SC_P      0x19
#define SC_M      0x32
#define SC_CTRL   0x1D
#define SC_B      0x30
#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_H      0x23
#define SC_W      0x11
#define SC_A      0x1E
#define SC_S      0x1F
#define SC_D      0x20

/* gun levels (persistent weapon power) */
#define GUN_MIN   1
#define GUN_MAX   4

/* main weapon type */
#define WT_CANNON 0     /* spread bullets (default)    */
#define WT_LASER  1     /* fast piercing beams         */
#define WT_WAVE   2     /* wide slow arc               */
#define WT_COUNT  3

/* difficulty */
#define DIF_EASY   0
#define DIF_NORMAL 1
#define DIF_HARD   2

/* powerup types */
#define PU_GUN     0    /* +1 gun level (persistent)   */
#define PU_RAPID   1    /* timed fast fire             */
#define PU_SHIELD  2    /* absorbs one hit             */
#define PU_LIFE    3    /* extra ship                  */
#define PU_MISSILE 4    /* +4 homing missiles          */
#define PU_LASER   5    /* switch to laser weapon      */
#define PU_WAVE    6    /* switch to wave weapon       */
#define PU_BOMB    7    /* +1 smart bomb               */
#define PU_SCORE   8    /* risky score pickup          */
#define PU_COUNT   9

/* enemy types */
#define E_SCOUT   0   /* straight diver           */
#define E_WEAVER  1   /* sine weaver              */
#define E_SHOOTER 2   /* returns fire             */
#define E_BOSS    3   /* handled separately       */

/* pool sizes */
#define MAX_PBULLET  48
#define MAX_EBULLET  64
#define MAX_ENEMY    28
#define MAX_POWERUP   8
#define MAX_PART    160
#define MAX_STARS    96
#define MAX_MISSILE   6

#define HISCORE_N     8
#define NAME_LEN      8

/* kind: 0 cannon, 1 laser (pierces), 2 wave */
typedef struct { i16 x, y, dx, dy; bool active; u8 kind, grazed, dmg; } Bullet;

typedef struct {
    bool active; u8 type;
    i16 x, y;            /* top-left pixel pos           */
    i16 hp;
    i16 t;               /* per-enemy timer / phase      */
    i16 base;            /* sine base x                  */
    i16 vy;              /* fall speed                   */
    i16 firecd;          /* fire cooldown                */
    u8  elite;           /* rare stronger variant        */
    u8  mode;            /* movement / formation mode    */
    i16 aux;             /* mode timer or direction      */
} Enemy;

typedef struct { bool active; u8 type; i16 x, y, t; } Powerup;

/* col==0 means fire-ramp particle that fades with life */
typedef struct { bool active; i16 x, y, dx, dy, life; u8 col; } Particle;

typedef struct { bool active; i16 x, y, dx; } Missile;

typedef struct { i16 x, y; u8 layer, col; } Star;

typedef struct {
    i16 x, y;
    i16 lives;
    u8  gun;             /* persistent gun level 1..GUN_MAX */
    u8  wtype;           /* main weapon (WT_*)              */
    i16 msl;             /* homing missile ammo             */
    i16 bombs;           /* smart bombs                     */
    i16 shield;          /* frames of shield remaining      */
    i16 invuln;          /* i-frames after hit              */
    i16 firecd;
    i16 rapid;           /* frames of rapid-fire left       */
    i16 boost;           /* boost energy bar                */
    i16 boost_cd;        /* recharge delay after boosting   */
    i16 combo;           /* current kill combo              */
    i16 combo_t;         /* frames until combo resets       */
    i16 max_combo;       /* best combo this run             */
    bool boosting;
    bool alive;
} Player;

typedef struct {
    char name[NAME_LEN + 1];
    u32  score;
} HiEntry;

#endif
