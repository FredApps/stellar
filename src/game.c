/* game.c - entities, waves, boss, powerups, scoring, state machine */
#include <string.h>
#include <stdio.h>
#include "defs.h"
#include "vga.h"
#include "input.h"
#include "sound.h"
#include "sprites.h"
#include "hiscore.h"
#include "bmpdump.h"
#include "game.h"

/* ---------------- state ---------------- */
enum { ST_TITLE, ST_PLAY, ST_OVER, ST_ENTRY, ST_SCORES, ST_HELP, ST_WIN, ST_QUIT };

static Player   player;
static Bullet   pbul[MAX_PBULLET];
static Bullet   ebul[MAX_EBULLET];
static Enemy    enemy[MAX_ENEMY];
static Powerup  powr[MAX_POWERUP];
static Particle part[MAX_PART];
static Star     star[MAX_STARS];
static Missile  msl[MAX_MISSILE];

/* movement archetypes and attack scripts: decoupled from the roster slot so a
   boss's silhouette, size, motion, and gunnery are all independent axes. */
enum { MV_CARRIER, MV_DIVER, MV_ORBITER, MV_WALL, MV_FINALE };
enum { AK_CARRIER, AK_LANCE, AK_SPIRAL, AK_WALL, AK_FINAL };

/* roster slots (footprint/hitbox is owned by the sprite: spr_boss_w/h[spr]). */
typedef struct { u8 spr, mvt, atkset; i16 hpbonus; } BossDef;
/* GORGON, REAPER, SEEKER, LEVIATHAN, OVERLORD (finale) */
static const BossDef BOSSDEF[NBOSS] = {
    /* spr mvt         atkset      hpbonus */
    {  0, MV_WALL,     AK_WALL,     40 },   /* 0 GORGON    - slow low tank       */
    {  1, MV_DIVER,    AK_LANCE,   -10 },   /* 1 REAPER    - small diving striker */
    {  2, MV_ORBITER,  AK_SPIRAL,    0 },   /* 2 SEEKER    - mid-screen strafer   */
    {  3, MV_CARRIER,  AK_CARRIER,  20 },   /* 3 LEVIATHAN - fighter carrier      */
    {  4, MV_FINALE,   AK_FINAL,    90 },   /* 4 OVERLORD  - wave-60 finale       */
};

static struct {
    bool active, entering;
    i16  x, y, hp, maxhp, dir, t, firecd, charge;
    i16  tx, ty, mv_t;       /* tx target-x; ty reused as diver sub-state    */
    i16  w, h;               /* per-boss footprint = hitbox                  */
    u8   kind;               /* roster slot 0..NBOSS-1 (name + finale test)  */
    u8   spr;                /* sprite index into spr_boss[]                 */
    u8   mvt;                /* movement archetype MV_*                      */
    u8   atkset;             /* attack script AK_*                           */
    u8   phase;              /* 0 full, 1 <66%, 2 <33% (enraged) */
    u8   last_phase, summons;
    u8   atk;                /* current attack pattern within the script     */
    i16  atk_t;              /* frames until the next pattern switch    */
    i16  spin;              /* rotating-spiral angle (sintab index)    */
    i16  launch_t;           /* carrier squad-launch countdown               */
    u8   squads;             /* squads launched this phase (bay throttle)    */
} boss;

/* expanding-ring explosion animation */
typedef struct { bool active; i16 x, y, t, big; } Blast;
static Blast    blast[10];

/* sparse foreground nebula sparks: a cheap second parallax layer */
typedef struct { i16 x, y; u8 layer, col; } Dust;
static Dust     dust[36];

static u32  score;
static i16  wave;
static i16  to_spawn;        /* enemies left to spawn this wave */
static i16  spawn_cd;
static u8   wave_mix;        /* rolling enemy-type bias for this wave */
static i16  flash;           /* screen flash frames */
static i16  shk;             /* screen shake frames */
static i16  shx, shy;        /* current shake offset */
static i16  wave_banner;     /* frames left for wave/boss banner */
static i16  msg_timer;        /* short score/event banner */
static char msg_text[32];
static u32  frame;
static u8   g_diff = DIF_NORMAL;
static u32  last_death_score; /* for pity mechanic */
static i16  wave_kills, wave_missed, wave_hit, combo_broken;
static i16  risk_spawned;
static i16  bosses_defeated, last_wave, last_combo, last_bosses;
static u8   ship_bank;        /* 0 left, 1 straight, 2 right */
static u8   campaign_won, win_pending;

/* integer sine table, one full period, amplitude +-46 (no FPU needed) */
static const i16 sintab[64] = {
      0,  5,  9, 13, 18, 22, 26, 29,
     33, 36, 38, 41, 42, 44, 45, 46,
     46, 46, 45, 44, 42, 41, 38, 36,
     33, 29, 26, 22, 18, 13,  9,  5,
      0, -5, -9,-13,-18,-22,-26,-29,
    -33,-36,-38,-41,-42,-44,-45,-46,
    -46,-46,-45,-44,-42,-41,-38,-36,
    -33,-29,-26,-22,-18,-13, -9, -5
};
static u16  rng = 0x1234;

#define BOOST_MAX       140
#define BOOST_MIN_START  12
#define BOOST_DRAIN       2
#define BOOST_RECHARGE    1
#define BOOST_RECHARGE_CD 25

/* forward declarations */
static void kill_enemy(Enemy *e);
static void boss_die(void);
static void set_msg(const char *s);
static void summon_escort(void);
static void apply_boss_damage(i16 dmg);

/* ---------------- helpers ---------------- */
static u16 rnd(void)
{
    rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8;
    return rng;
}
static i16 rrange(i16 lo, i16 hi) { return lo + (i16)(rnd() % (u16)(hi - lo + 1)); }

static bool overlap(i16 ax, i16 ay, i16 aw, i16 ah,
                    i16 bx, i16 by, i16 bw, i16 bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static Bullet *free_bullet(Bullet *arr, int n)
{ int i; for (i = 0; i < n; i++) if (!arr[i].active) return &arr[i]; return 0; }

static void set_msg(const char *s)
{
    strncpy(msg_text, s, sizeof(msg_text) - 1);
    msg_text[sizeof(msg_text) - 1] = 0;
    msg_timer = 120;
}

static u32 score_scaled(u32 raw)
{
    if (g_diff == DIF_HARD) return (raw * 160UL + 50UL) / 100UL;
    if (g_diff == DIF_NORMAL) return (raw * 125UL + 50UL) / 100UL;
    return raw;
}

static void score_add(u32 raw)
{
    score += score_scaled(raw);
}

static i16 diff_spawn_cd_min(void)
{ return (g_diff == DIF_EASY) ? 22 : (g_diff == DIF_HARD) ? 13 : 16; }

static i16 diff_spawn_cd_max(void)
{ return (g_diff == DIF_EASY) ? 36 : (g_diff == DIF_HARD) ? 24 : 30; }

static i16 diff_enemy_fire_adjust(void)
{ return (g_diff == DIF_EASY) ? 15 : (g_diff == DIF_HARD) ? -18 : 0; }

static i16 diff_boss_hp_mul(void)
{ return (g_diff == DIF_EASY) ? 7 : (g_diff == DIF_HARD) ? 14 : 10; }

static i16 diff_boss_fire_cd(void)
{
    i16 d = (g_diff == DIF_EASY) ? 8 : (g_diff == DIF_HARD) ? -8 : 0;
    switch (boss.atkset) {
    case AK_FINAL:   return (i16)(34 + d - boss.phase * 6);
    case AK_LANCE:   return (i16)(30 + d - boss.phase * 6);  /* fast striker    */
    case AK_WALL:    return (i16)(54 + d - boss.phase * 9);  /* slow heavy      */
    case AK_CARRIER: return (i16)(70 + d - boss.phase * 8);  /* thin suppress   */
    default:         return (i16)(46 + d - boss.phase * 10); /* AK_SPIRAL       */
    }
}

static i16 boss_attack_count(void)
{
    return (boss.atkset == AK_FINAL) ? 4 : (boss.atkset == AK_CARRIER) ? 2 : 3;
}

static i16 boss_atk_time(void)
{
    i16 t = (boss.atkset == AK_FINAL)   ? 112 :
            (boss.atkset == AK_LANCE)   ?  84 :
            (boss.atkset == AK_WALL)    ? 140 :
            (boss.atkset == AK_CARRIER) ? 150 : 126;
    t -= boss.phase * ((boss.atkset == AK_WALL) ? 22 : 26);
    return (t < 48) ? 48 : t;
}

static i16 boss_pct_damage(i16 div)
{
    i16 d = (i16)((boss.maxhp + div - 1) / div);
    return (d < 1) ? 1 : d;
}

static u32 enemy_score(const Enemy *e)
{
    if (e->elite) {
        if (e->type == E_SCOUT) return 180;
        if (e->type == E_WEAVER) return 240;
        return 375;
    }
    return (e->type == E_SCOUT) ? 100 : (e->type == E_WEAVER) ? 150 : 250;
}

static void spawn_part(i16 x, i16 y, u8 col)
{
    int i;
    for (i = 0; i < MAX_PART; i++) if (!part[i].active) {
        part[i].active = TRUE;
        part[i].x = x; part[i].y = y;
        part[i].dx = rrange(-4, 4); part[i].dy = rrange(-4, 4);
        part[i].life = rrange(10, 22);
        part[i].col = col;              /* 0 = fire-ramp fade */
        return;
    }
}
static void burst(i16 x, i16 y, int n, u8 c1, u8 c2)
{ int i; for (i = 0; i < n; i++) spawn_part(x, y, (i & 1) ? c1 : c2); }

/* fiery explosion: particles fade white->orange->dark red via PAL_FIRE */
static void fireburst(i16 x, i16 y, int n)
{ int i; for (i = 0; i < n; i++) spawn_part(x, y, 0); }

static void typeburst(i16 x, i16 y, u8 type, u8 elite)
{
    if (type == E_SCOUT) burst(x, y, 10, (u8)(PAL_FIRE + 12), C_YELLOW);
    else if (type == E_WEAVER) burst(x, y, 14, C_LGREEN, (u8)(PAL_GLOW + 10));
    else burst(x, y, 16, C_LMAG, C_WHITE);
    if (elite) burst(x, y, 8, C_WHITE, C_LCYAN);
}

/* particle colour: fire particles brighten with remaining life */
static u8 part_col(const Particle *p)
{
    i16 l;
    if (p->col) return p->col;
    l = p->life; if (l > 15) l = 15;
    return (u8)(PAL_FIRE + l);
}

static void remember_run(void)
{
    last_wave = wave;
    last_combo = player.max_combo;
    last_bosses = bosses_defeated;
}

/* ---------------- world sprite draw with shake ---------------- */
#define DS(x,y,w,h,d)  vga_sprite((x)+shx,(y)+shy,(w),(h),(d))

/* ---------------- init / reset ---------------- */

/* paint soft blue nebula blobs + dust into the static background buffer */
static void gen_nebula(void)
{
    int b;
    i16 x, y;
    u16 i;
    u8 __far *bg = g_bg;
    _fmemset(bg, C_BLACK, SCRSZ);
    for (b = 0; b < 9; b++) {
        i16 cx = rrange(20, SCRW - 20);
        i16 cy = rrange(10, SCRH - 10);
        i16 r  = rrange(24, 55);
        long r2 = (long)r * r;
        for (y = cy - r; y <= cy + r; y++) {
            i16 wy = (i16)((y + SCRH) % SCRH);       /* wrap for seamless scroll */
            for (x = cx - r; x <= cx + r; x++) {
                long d2;
                i16 wx;
                u8 v, cur;
                if (x < 0 || x >= SCRW) continue;
                wx = x;
                d2 = (long)(x - cx) * (x - cx) + (long)(y - cy) * (y - cy);
                if (d2 >= r2) continue;
                /* intensity 0..10, dithered for a gaseous look */
                v = (u8)(((r2 - d2) * 10) / r2);
                if (v > 2 && ((wx ^ wy) & 1)) v -= 2;
                cur = bg[(u16)wy * SCRW + (u16)wx];
                if (cur >= PAL_NEB) cur = (u8)(cur - PAL_NEB); else cur = 0;
                v = (u8)(v + cur); if (v > 13) v = 13;
                if (v) bg[(u16)wy * SCRW + (u16)wx] = (u8)(PAL_NEB + v);
            }
        }
    }
    /* faint static dust between the blobs */
    for (i = 0; i < 260; i++) {
        u16 o = (u16)((rnd() % 200) * SCRW + rnd() % SCRW);
        if (bg[o] == C_BLACK) bg[o] = (u8)(PAL_NEB + 2 + rnd() % 3);
    }
}

static void reset_game(void)
{
    memset(pbul, 0, sizeof(pbul));
    memset(ebul, 0, sizeof(ebul));
    memset(enemy, 0, sizeof(enemy));
    memset(powr, 0, sizeof(powr));
    memset(part, 0, sizeof(part));
    memset(msl, 0, sizeof(msl));
    memset(blast, 0, sizeof(blast));
    boss.active = FALSE;
    player.x = 152; player.y = 168;
    player.lives = (g_diff == DIF_EASY) ? 5 : (g_diff == DIF_HARD) ? 2 : 3;
    player.gun = GUN_MIN; player.wtype = WT_CANNON;
    player.msl = 5; player.bombs = (g_diff == DIF_HARD) ? 1 : 2;
    player.shield = (g_diff == DIF_EASY) ? 200 : 0;   /* easy: brief invuln head-start */
    player.invuln = 0; player.firecd = 0; player.rapid = 0; player.wave_boost = 0;
    player.boost = BOOST_MAX; player.boost_cd = 0; player.boosting = FALSE;
    player.combo = 0; player.combo_t = 0; player.max_combo = 0; player.alive = TRUE;
    score = 0; wave = 0; flash = 0; shk = 0;
    wave_banner = 0; msg_timer = 0; msg_text[0] = 0; ship_bank = 1;
    wave_kills = wave_missed = wave_hit = combo_broken = 0;
    risk_spawned = 0; bosses_defeated = 0; campaign_won = 0; win_pending = 0;
    to_spawn = 0; spawn_cd = 30; last_death_score = 0;
}

static void init_stars(void)
{
    int i;
    for (i = 0; i < MAX_STARS; i++) {
        star[i].x = rrange(0, SCRW - 1);
        star[i].y = rrange(0, SCRH - 1);
        star[i].layer = (u8)(i % 3);
        star[i].col = (star[i].layer == 0) ? C_DGRAY : (star[i].layer == 1) ? C_LGRAY : C_WHITE;
    }
}

static void init_dust(void)
{
    int i;
    for (i = 0; i < 36; i++) {
        dust[i].x = rrange(0, SCRW - 1);
        dust[i].y = rrange(0, SCRH - 1);
        dust[i].layer = (u8)(1 + (i % 2));
        dust[i].col = (u8)(PAL_NEB + 6 + rnd() % 7);
    }
}

/* ---------------- wave / spawn ---------------- */
static void finish_wave(void)
{
    u32 bonus = 0;
    char b[32];
    if (wave <= 0) return;
    if (!wave_hit) bonus += 1000;
    if (!wave_missed) bonus += 750;
    if (!combo_broken && player.combo >= 3) bonus += 500;
    if (wave % 4 == 0 && player.bombs > 0) bonus += (u32)player.bombs * 400UL;
    if (bonus > 0) {
        u32 award = score_scaled(bonus);
        score += award;
        sprintf(b, "WAVE BONUS %lu", award);
        set_msg(b);
    }
}

static void start_wave(void)
{
    wave++;
    wave_banner = 110;
    wave_kills = wave_missed = wave_hit = combo_broken = 0;
    risk_spawned = 0;
    vga_set_theme((u8)((wave - 1) / 4));
    if (wave % 10 == 0) {
        u32 award = score_scaled(2000);
        score += award;
        sprintf(msg_text, "ENDURANCE +%lu", award);
        msg_timer = 120;
    }
    if (wave == 60 || wave % 4 == 0) {              /* boss wave */
        static const u8 boss_order[4] = { 1, 0, 3, 2 };  /* reaper, gorgon, leviathan, seeker */
        i16 boss_index = (i16)((wave / 4) - 1);
        const BossDef *bd;
        boss.active = TRUE; boss.entering = TRUE;
        boss.kind = (wave == 60 && !campaign_won) ? (u8)(NBOSS - 1)
                                                  : boss_order[boss_index % 4];
        bd = &BOSSDEF[boss.kind];
        boss.spr = bd->spr; boss.mvt = bd->mvt; boss.atkset = bd->atkset;
        boss.w = spr_boss_w[boss.spr]; boss.h = spr_boss_h[boss.spr];
        boss.phase = 0; boss.last_phase = 0; boss.summons = 0;
        boss.atk = 0; boss.atk_t = boss_atk_time(); boss.spin = 0;
        boss.x = SCRW / 2 - boss.w / 2; boss.y = -boss.h;
        boss.maxhp = boss.hp = (i16)(36 + wave * diff_boss_hp_mul()
                                     + boss_index * 8 + bd->hpbonus);
        if (boss.maxhp < 20) boss.maxhp = boss.hp = 20;
        boss.dir = 1; boss.t = 0; boss.firecd = 60; boss.charge = 0;
        boss.tx = boss.x; boss.ty = 0; boss.mv_t = 50;
        boss.launch_t = 90; boss.squads = 0;
        to_spawn = 0;
        snd_sfx(SFX_BOSS);
    } else {
        to_spawn = 5 + wave;
        if (to_spawn > 18) to_spawn = 18;
        if (g_diff == DIF_HARD) to_spawn += 4;
        spawn_cd = (g_diff == DIF_EASY) ? 100 : (g_diff == DIF_HARD) ? 56 : 82;
        /* four-wave blocks: scout rush, weaver maze, shooter crossfire, mixed */
        wave_mix = (u8)(((wave - 1) / 4) & 3);
    }
}

/* pick an enemy type weighted by four-wave block personality */
static u8 pick_type(void)
{
    u16 r = rnd() % 100;
    if (wave < 3) return (r < 70) ? E_SCOUT : (r < 90) ? E_WEAVER : E_SHOOTER;
    if (wave_mix == 0) return (r < 62) ? E_SCOUT : (r < 82) ? E_WEAVER : E_SHOOTER;
    if (wave_mix == 1) return (r < 25) ? E_SCOUT : (r < 74) ? E_WEAVER : E_SHOOTER;
    if (wave_mix == 2) return (r < 24) ? E_SCOUT : (r < 50) ? E_WEAVER : E_SHOOTER;
    return (r < 34) ? E_SCOUT : (r < 67) ? E_WEAVER : E_SHOOTER;
}

static void init_enemy(Enemy *e, u8 type, i16 x)
{
    e->active = TRUE; e->type = type;
    e->base = x; e->x = x; e->y = -SH_EN_H;
    e->t = rrange(0, 63);
    e->vy = 1 + (wave > 6 ? 1 : 0) + (g_diff == DIF_HARD ? 1 : 0);
    e->firecd = rrange(40, 90) + diff_enemy_fire_adjust();
    {
        i16 elite_chance = (wave >= 13 ? 18 : 12);
        if (g_diff == DIF_EASY) elite_chance -= 4;
        if (g_diff == DIF_HARD) elite_chance += 5;
        e->elite = (wave >= 7 && (rnd() % 100) < elite_chance) ? 1 : 0;
    }
    e->mode = 0; e->aux = 0;
    if (type == E_SCOUT)   { e->hp = 1; e->vy += 1; }
    if (type == E_WEAVER)  { e->hp = 2; }
    if (type == E_SHOOTER) { e->hp = 3; if (rnd() % 100 < 35) { e->mode = 1; e->aux = (rnd() & 1) ? 1 : -1; } }
    if (e->elite) {
        if (type == E_SCOUT) { e->hp = 2; e->vy++; }
        else if (type == E_WEAVER) e->hp = 3;
        else e->hp = 4;
    }
}

static bool spawn_one(u8 type, i16 x, i16 y, u8 mode)
{
    int i;
    if (x < 4) x = 4;
    if (x > SCRW - SH_EN_W - 4) x = SCRW - SH_EN_W - 4;
    for (i = 0; i < MAX_ENEMY; i++) if (!enemy[i].active) {
        init_enemy(&enemy[i], type, x);
        enemy[i].y = y;
        enemy[i].mode = mode;
        return TRUE;
    }
    return FALSE;
}

static i16 free_enemy_slots(void)
{
    int i; i16 n = 0;
    for (i = 0; i < MAX_ENEMY; i++) if (!enemy[i].active) n++;
    return n;
}

static void summon_escort(void)
{
    int n;
    u8 type = (boss.atkset == AK_SPIRAL) ? E_WEAVER : E_SCOUT;
    i16 x0 = (boss.x < 80) ? boss.x + 36 : boss.x - 28;
    for (n = 0; n < 3; n++) {
        spawn_one(type, x0 + n * 20, -SH_EN_H - n * 12, 0);
    }
    set_msg("ESCORTS INBOUND");
}

/* carrier: launch a fighter squad from the two hangar bays, but only if the
   shared enemy[] pool has room for the whole squad (never partial/overflow). */
static void launch_squad(void)
{
    i16 size = (i16)(2 + (boss.phase >= 1 ? 1 : 0));
    i16 lx = boss.x + 6;
    i16 rx = boss.x + boss.w - 6 - SH_EN_W;
    int n;
    if (free_enemy_slots() < size) return;
    for (n = 0; n < size; n++) {
        i16 bx = (n & 1) ? rx : lx;
        spawn_one(E_SCOUT, bx, -SH_EN_H - n * 10, 0);
    }
    boss.squads++;
    set_msg("FIGHTERS LAUNCHED");
}

/* spawn one drip, or occasionally a whole formation (counts as several) */
static void spawn_enemy(void)
{
    int i, n;
    /* ~1 in 4 drips becomes a formation */
    if (to_spawn >= 3 && (rnd() % 100) < 26) {
        u8 type = pick_type();
        i16 count = (i16)(3 + rnd() % 3);           /* 3..5 */
        i16 form = (i16)(rnd() % 4);
        i16 x0 = rrange(28, SCRW - 92);
        if (x0 < 8) x0 = 8;
        for (n = 0; n < count && to_spawn > 0; n++) {
            i16 x = x0, y = -SH_EN_H - n * 16;
            u8 t = type;
            if (form == 1) { x = x0 + n * 22; y = -SH_EN_H; }                  /* row */
            else if (form == 2) { x = x0 + (n - count / 2) * 18; y = -SH_EN_H - (count / 2 - (n > count / 2 ? count - n - 1 : n)) * 14; }
            else if (form == 3) { t = (n & 1) ? E_WEAVER : type; x = x0 + n * 18; }
            else x = x0 + n * 20;                                             /* staggered column */
            for (i = 0; i < MAX_ENEMY; i++) if (!enemy[i].active) {
                init_enemy(&enemy[i], t, x);
                enemy[i].y = y;
                enemy[i].t = (form == 3) ? (n * 12) : 16;
                to_spawn--;
                break;
            }
        }
        return;
    }
    for (i = 0; i < MAX_ENEMY; i++) if (!enemy[i].active) {
        init_enemy(&enemy[i], pick_type(), rrange(8, SCRW - SH_EN_W - 8));
        to_spawn--;
        return;
    }
}

/* ---------------- firing ---------------- */
static void add_pbullet(i16 x, i16 y, i16 dx, i16 dy, u8 kind)
{
    Bullet *b = free_bullet(pbul, MAX_PBULLET);
    if (b) {
        b->active = TRUE; b->x = x; b->y = y; b->dx = dx; b->dy = dy; b->kind = kind;
        b->grazed = 0; b->dmg = (kind == WT_LASER) ? 2 : 1;
    }
}

/* fire pattern scales with gun level; weapon type sets the projectile feel */
static void player_fire(void)
{
    i16 cx = player.x + SH_SHIP_W / 2 - SH_PB_W / 2;
    i16 cy = player.y - 4;
    i16 g  = player.gun;
    i16 cd;
    switch (player.wtype) {
    case WT_LASER:                                /* fast, straight, pierces */
        add_pbullet(cx, cy - 2, 0, -12, WT_LASER);
        if (g >= 3) { add_pbullet(cx - 6, cy, 0, -12, WT_LASER);
                      add_pbullet(cx + 6, cy, 0, -12, WT_LASER); }
        cd = 7;
        break;
    case WT_WAVE: {
        i16 spread = g + 1, k;
        if (player.wave_boost > 0) {              /* boosted: piercing beams + centre lane */
            for (k = -spread; k <= spread; k++)
                add_pbullet(cx + k * 2, cy - 2, k, -8, WT_LASER);
            add_pbullet(cx, cy - 4, 0, -11, WT_LASER);   /* extra fast centre lane */
            cd = 11;
        } else {                                  /* base: narrower slow arc */
            for (k = -spread; k <= spread; k++)
                add_pbullet(cx + k * 2, cy, k, -5, WT_WAVE);
            cd = 15;
        }
        break; }
    default:                                      /* WT_CANNON */
        switch (g) {
        case 1:  add_pbullet(cx - 3, cy, 0, -7, 0); add_pbullet(cx + 3, cy, 0, -7, 0); break;
        case 2:  add_pbullet(cx, cy - 2, 0, -7, 0);
                 add_pbullet(cx - 5, cy, 0, -7, 0); add_pbullet(cx + 5, cy, 0, -7, 0); break;
        case 3:  add_pbullet(cx, cy - 2, 0, -7, 0);
                 add_pbullet(cx - 5, cy, -1, -7, 0); add_pbullet(cx + 5, cy, 1, -7, 0); break;
        default: add_pbullet(cx, cy - 2, 0, -7, 0);
                 add_pbullet(cx - 5, cy, 0, -7, 0); add_pbullet(cx + 5, cy, 0, -7, 0);
                 add_pbullet(cx - 7, cy + 2, -2, -7, 0); add_pbullet(cx + 7, cy + 2, 2, -7, 0); break;
        }
        cd = 9;
        break;
    }
    snd_sfx(SFX_FIRE);
    player.firecd = (player.rapid > 0) ? (cd > 4 ? cd - 4 : 2) : cd;
}

static void fire_missile(void)
{
    int i;
    if (player.msl <= 0) return;
    for (i = 0; i < MAX_MISSILE; i++) if (!msl[i].active) {
        msl[i].active = TRUE;
        msl[i].x = player.x + SH_SHIP_W / 2 - SH_MSL_W / 2;
        msl[i].y = player.y - SH_MSL_H;
        msl[i].dx = 0;
        player.msl--;
        snd_sfx(SFX_MISSILE);
        return;
    }
}

static void enemy_fire(i16 ex, i16 ey)
{
    Bullet *b = free_bullet(ebul, MAX_EBULLET);
    i16 dx;
    if (!b) return;
    b->active = TRUE;
    b->x = ex; b->y = ey;
    dx = (player.x + 8 > ex) ? 1 : -1;
    b->dx = dx; b->dy = 3; b->kind = 0; b->grazed = 0; b->dmg = 1;
}

static void add_ebullet(i16 x, i16 y, i16 dx, i16 dy)
{
    Bullet *b = free_bullet(ebul, MAX_EBULLET);
    if (!b) return;
    b->active = TRUE; b->x = x; b->y = y; b->dx = dx; b->dy = dy;
    b->kind = 0; b->grazed = 0; b->dmg = 1;
}

/* Boss attacks: each attack script (boss.atkset) has its own personality and
   cycles a few patterns (boss.atk), intensified by the enrage phase. Volleys
   stay small (<= ~7) so variety comes from pattern shape, not bullet count. */
static void boss_fire(void)
{
    i16 bx = boss.x + boss.w / 2, by = boss.y + boss.h - 6;
    i16 dir = (player.x + 8 > bx) ? 1 : -1;
    u8  hard = (g_diff == DIF_HARD);
    i16 k;
    switch (boss.atkset) {
    case AK_FINAL:                            /* ---- OVERLORD finale ---- */
        switch (boss.atk) {
        case 1:                               /* crossing lances */
            add_ebullet(bx - 18, by - 2, 2, 4); add_ebullet(bx + 18, by - 2, -2, 4);
            add_ebullet(bx - 8, by, 1, 5);     add_ebullet(bx + 8, by, -1, 5);
            if (boss.phase >= 1) add_ebullet(bx, by, 0, 6);
            break;
        case 2:                               /* rotating spokes */
            { i16 a, arms = (boss.phase >= 2) ? 5 : 4;
              for (a = 0; a < arms; a++) {
                  i16 ang = (i16)((boss.spin + a * 16) & 63);
                  add_ebullet(bx, by, sintab[ang] / 14,
                              (i16)(3 + (sintab[(ang + 16) & 63] > 0 ? 1 : 0)));
              }
              boss.spin = (i16)((boss.spin + 7) & 63); }
            break;
        case 3:                               /* aimed split burst */
            add_ebullet(bx, by, dir * 2, 3); add_ebullet(bx, by, dir, 4);
            add_ebullet(bx - 12, by, -1, 4); add_ebullet(bx + 12, by, 1, 4);
            if (hard || boss.phase >= 2) add_ebullet(bx, by, 0, 5);
            break;
        default:                              /* curtain with a moving center gap */
            for (k = -4; k <= 4; k++)
                if (!((boss.t / 18 + k + 8) % 5 == 0))
                    add_ebullet(bx + k * 8, by, k / 3, 3);
            break;
        }
        break;
    case AK_LANCE:                            /* ---- REAPER: sparse, fast, aimed ---- */
        switch (boss.atk) {
        case 1:                               /* light scatter on the retreat */
            add_ebullet(bx - 6, by, -1, 4); add_ebullet(bx + 6, by, 1, 4);
            break;
        case 2:                               /* quick aimed pair */
            add_ebullet(bx - 2, by, dir, 5); add_ebullet(bx + 2, by, dir, 5);
            break;
        default:                              /* tight aimed 3-lance */
            add_ebullet(bx, by, dir, 6);
            add_ebullet(bx, by, dir, 5); add_ebullet(bx, by, dir * 2, 5);
            break;
        }
        break;
    case AK_SPIRAL:                           /* ---- SEEKER: rotating hazard ---- */
        switch (boss.atk) {
        case 1:                               /* rotating spiral */
            { i16 a, arms = (boss.phase >= 1) ? 3 : 2;
              for (a = 0; a < arms; a++) {
                  i16 ang = (i16)((boss.spin + a * 21) & 63);
                  add_ebullet(bx, by, sintab[ang] / 12,
                              (i16)(2 + (sintab[(ang + 16) & 63] > 0 ? 2 : 1)));
              }
              boss.spin = (i16)((boss.spin + 5) & 63); }
            break;
        case 2:                               /* leading side-shots */
            add_ebullet(bx - 10, by, boss.dir, 3); add_ebullet(bx + 10, by, boss.dir, 3);
            if (boss.phase >= 1) add_ebullet(bx, by, dir, 4);
            break;
        default:                              /* radial ring */
            for (k = (hard ? -3 : -2); k <= (hard ? 3 : 2); k++) add_ebullet(bx, by, k, 3);
            break;
        }
        break;
    case AK_CARRIER:                          /* ---- LEVIATHAN: thin suppressive ---- */
        if (boss.atk == 1) {
            add_ebullet(bx, by, dir, 3);      /* single aimed lob */
        } else {
            add_ebullet(bx - 5, by, dir, 3);  /* aimed pair */
            add_ebullet(bx + 5, by, dir, 3);
        }
        break;
    default:                                  /* ---- GORGON wall (AK_WALL) ---- */
        switch (boss.atk) {
        case 1:                               /* wall with a dodge gap under the player */
            { i16 pc = (i16)((player.x + 8 - (bx - 30)) / 10);
              for (k = -3; k <= 3; k++)
                  if (k != pc && k != pc - 1) add_ebullet(bx + k * 10, by, 0, 3); }
            break;
        case 2:                               /* aimed burst + flank */
            add_ebullet(bx, by, dir, 4); add_ebullet(bx, by, 0, 4);
            add_ebullet(bx - 12, by, -1, 3); add_ebullet(bx + 12, by, 1, 3);
            break;
        default:                              /* sweeping fan */
            for (k = -2; k <= 2; k++) add_ebullet(bx, by, k, 3);
            if (boss.phase >= 2) add_ebullet(bx, by, 0, 5);
            break;
        }
        break;
    }
    snd_sfx(SFX_HIT);
}

static void move_toward(i16 *v, i16 target, i16 step)
{
    if (*v < target) { *v += step; if (*v > target) *v = target; }
    else if (*v > target) { *v -= step; if (*v < target) *v = target; }
}

/* Each movement archetype owns its own vertical band (there is no shared top
   band any more); generic clamps below just keep every boss on-screen and
   above the player's home row. */
static void boss_move(void)
{
    i16 cx = (i16)(SCRW / 2 - boss.w / 2);
    switch (boss.mvt) {
    case MV_CARRIER:                          /* high slow hover; launches fighters */
        if (--boss.mv_t <= 0) {
            boss.mv_t = 70;
            boss.tx = (rnd() & 1) ? 8 : (i16)(SCRW - boss.w - 8);
        }
        move_toward(&boss.x, boss.tx, 1);
        boss.y = (i16)(8 + (sintab[boss.t & 63] + 46) / 40);
        if (--boss.launch_t <= 0) {
            launch_squad();
            boss.launch_t = (i16)(180 - boss.phase * 40);
        }
        break;
    case MV_DIVER:                            /* swoop toward the player, retreat */
        if (boss.ty == 0) {                   /* rest / drift high */
            boss.y = (i16)(18 + (sintab[boss.t & 63] + 46) / 30);
            move_toward(&boss.x, boss.tx, 1);
            if (--boss.mv_t <= 0) { boss.ty = 1; boss.tx = (i16)(player.x + 8 - boss.w / 2); }
        } else if (boss.ty == 1) {            /* diving */
            move_toward(&boss.x, boss.tx, 3);
            move_toward(&boss.y, 96, 4);
            if (boss.y >= 96) boss.ty = 2;
        } else {                              /* retreat */
            move_toward(&boss.y, 18, 3);
            if (boss.y <= 18) {
                boss.ty = 0;
                boss.mv_t = (i16)(44 - boss.phase * 10);
                boss.tx = rrange(8, (i16)(SCRW - boss.w - 8));
            }
        }
        break;
    case MV_ORBITER:                          /* fast mid-screen strafe */
        boss.x = (i16)(cx + sintab[(boss.t * 2) & 63] * (3 + boss.phase) / 2);
        boss.y = (i16)(46 + sintab[(boss.t + 16) & 63] / 6);
        boss.dir = (sintab[(boss.t * 2 + 16) & 63] >= 0) ? 1 : -1;
        break;
    case MV_FINALE:                           /* figure-eight core, wide y range */
        boss.x = (i16)(cx + sintab[(boss.t * 2) & 63] * 3 / 2);
        boss.y = (i16)(16 + (sintab[(boss.t * 3 + 16) & 63] + 46) / 4);
        if ((boss.t & 127) == 0) boss.charge = 18;
        break;
    default:                                  /* MV_WALL: low slow bounce, boxes player in */
        boss.x += boss.dir;
        boss.y = (i16)(84 + ((boss.t >> 4) & 1) * 2);
        break;
    }
    if (boss.x < 4) { boss.x = 4; boss.dir = 1; }
    if (boss.x > SCRW - boss.w - 4) { boss.x = (i16)(SCRW - boss.w - 4); boss.dir = -1; }
    if (boss.y < 6) boss.y = 6;
    if (boss.y > SCRH - boss.h - 70) boss.y = (i16)(SCRH - boss.h - 70);
}

/* nominal resting height per archetype, so the slide-in stops where the boss
   will actually hover instead of snapping after the entrance. */
static i16 boss_rest_y(void)
{
    switch (boss.mvt) {
    case MV_CARRIER: return 9;
    case MV_ORBITER: return 46;
    case MV_WALL:    return 84;
    case MV_FINALE:  return 16;
    default:         return 18;   /* MV_DIVER */
    }
}

/* expanding-ring explosion animation */
static void spawn_blast(i16 x, i16 y, i16 big)
{
    int i;
    for (i = 0; i < 10; i++) if (!blast[i].active) {
        blast[i].active = TRUE; blast[i].x = x; blast[i].y = y;
        blast[i].t = 0; blast[i].big = big;
        return;
    }
}

/* ---------------- powerups ---------------- */
static bool drop_powerup(i16 x, i16 y, u8 type)
{
    int i;
    for (i = 0; i < MAX_POWERUP; i++) if (!powr[i].active) {
        powr[i].active = TRUE; powr[i].type = type;
        powr[i].x = x; powr[i].y = y; powr[i].t = 0;
        return TRUE;
    }
    return FALSE;
}

static void force_powerup(i16 x, i16 y, u8 type)
{
    int i, best = 0;
    i16 oldest = -1;
    if (drop_powerup(x, y, type)) return;
    for (i = 0; i < MAX_POWERUP; i++) {
        if (powr[i].type == PU_SCORE) { best = i; break; }
        if (powr[i].t > oldest) { oldest = powr[i].t; best = i; }
    }
    powr[best].active = TRUE; powr[best].type = type;
    powr[best].x = x; powr[best].y = y; powr[best].t = 0;
}
static void apply_powerup(u8 type)
{
    switch (type) {
        case PU_GUN:     if (player.gun < GUN_MAX) player.gun++; break;
        case PU_RAPID:   player.rapid  = 700; break;
        case PU_SHIELD:  player.shield = 350; break;   /* ~10 s of invulnerability at 35 FPS */
        case PU_LIFE:    if (player.lives < 9) player.lives++; break;
        case PU_MISSILE: player.msl += 4; if (player.msl > 30) player.msl = 30; break;
        /* Laser while flying the Wave gun doesn't swap weapons - it super-
           charges the Wave for a while (piercing beams + a centre lane). */
        case PU_LASER:   if (player.wtype == WT_WAVE) player.wave_boost = 350; /* ~10 s at 35 FPS */
                         else player.wtype = WT_LASER; break;
        case PU_WAVE:    player.wtype = WT_WAVE; break;
        case PU_BOMB:    if (player.bombs < 10) player.bombs++; break;
        case PU_SCORE:   score_add(500 + (u32)wave * 50UL); break;
    }
    if (type == PU_LIFE || type == PU_SHIELD) snd_sfx(SFX_PICK1);
    else if (type == PU_SCORE) snd_sfx(SFX_COMBO);
    else snd_sfx(SFX_PICK2);
    score_add(50);
}

/* smart bomb: clear enemy fire and damage everything on screen */
static void apply_boss_damage(i16 dmg)
{
    if (!boss.active) return;
    boss.hp -= dmg;
    burst(boss.x + boss.w / 2, boss.y + boss.h / 2, 8, C_WHITE, C_YELLOW);
    if (boss.hp <= 0) boss_die();
}

static void smart_bomb(void)
{
    int i, cleared = 0;
    if (player.bombs <= 0 || !player.alive) return;
    player.bombs--;
    flash = 8; shk = 18;
    for (i = 0; i < MAX_EBULLET; i++) if (ebul[i].active) { ebul[i].active = FALSE; cleared++; }
    score_add((u32)cleared * 25UL);
    spawn_blast(SCRW / 2, SCRH / 2, 3);
    for (i = 0; i < MAX_ENEMY; i++) if (enemy[i].active) {
        enemy[i].hp -= 5;
        if (enemy[i].hp <= 0) { score_add(100); kill_enemy(&enemy[i]); }
    }
    if (boss.active) apply_boss_damage(boss_pct_damage(3));
    snd_sfx(SFX_EXPLODE);
}

/* ---------------- damage to player ---------------- */
static void hurt_player(void)
{
    if (player.invuln > 0) return;
    /* Active shield = full invulnerability for its whole duration: absorb the
       hit without popping the shield or breaking the combo. Brief i-frames
       keep it to one spark per incoming shot. */
    if (player.shield > 0) { player.invuln = 8;
        burst(player.x + 8, player.y + 8, 10, C_LBLUE, C_WHITE); snd_sfx(SFX_HIT); return; }
    wave_hit = 1; combo_broken = 1;
    player.combo = 0; player.combo_t = 0;
    player.lives--;
    player.invuln = 90;
    player.rapid = 0;
    if (player.gun > GUN_MIN) player.gun--;   /* losing a ship costs a gun level */
    player.wtype = WT_CANNON;
    fireburst(player.x + 8, player.y + 8, 24);
    spawn_blast(player.x + 8, player.y + 8, 2);
    flash = 4; shk = 12;
    snd_sfx(SFX_EXPLODE);
    if (player.lives <= 0) player.alive = FALSE;
    else {
        /* pity: if this life earned little, grant a brief shield (not on HARD).
           Kept short since a shield is now full invulnerability while active. */
        if (g_diff != DIF_HARD && score - last_death_score < 800)
            player.shield = (g_diff == DIF_EASY) ? 140 : 100;
        last_death_score = score;
    }
}

/* score multiplier grows with the current combo (x1..x5) */
static i16 combo_mult(void)
{
    i16 m = 1 + player.combo / 6;
    return (m > 5) ? 5 : m;
}

static void kill_enemy(Enemy *e)
{
    i16 cx = e->x + SH_EN_W / 2, cy = e->y + SH_EN_H / 2;
    i16 old_mult = combo_mult();
    i16 tier_up;
    u32 base = enemy_score(e);
    typeburst(cx, cy, e->type, e->elite);
    spawn_blast(cx, cy, 0);
    wave_kills++;
    player.combo++; player.combo_t = 130;
    if (player.combo > player.max_combo) player.max_combo = player.combo;
    tier_up = (combo_mult() > old_mult);
    score_add(base * combo_mult());
    if (!risk_spawned && player.combo >= 10) {
        drop_powerup(rrange(96, 212), rrange(38, 72), PU_SCORE);
        risk_spawned = 1;
    }
    {
        i16 drop_chance = (g_diff == DIF_EASY) ? 18 : (g_diff == DIF_HARD) ? 12 : 15;
        if (rnd() % 100 < drop_chance) {   /* weighted drop table */
        u16 r = rnd() % 100;
        u8 t = (r < 16) ? PU_GUN : (r < 30) ? PU_RAPID : (r < 44) ? PU_SHIELD
             : (r < 64) ? PU_MISSILE : (r < 76) ? PU_LASER : (r < 88) ? PU_WAVE
             : (r < 95) ? PU_BOMB : PU_LIFE;
        drop_powerup(cx - SH_PU_W / 2, cy, t);
        }
    }
    e->active = FALSE;
    snd_sfx(tier_up ? SFX_COMBO : SFX_EXPLODE);
}

static void boss_die(void)
{
    fireburst(boss.x + boss.w / 2, boss.y + boss.h / 2, 60);
    spawn_blast(boss.x + boss.w / 2, boss.y + boss.h / 2, 3);
    score_add(5000 * combo_mult());
    flash = 8; shk = 24;
    bosses_defeated++;
    force_powerup(boss.x + 10, boss.y + 10, PU_LIFE);
    force_powerup(boss.x + boss.w - 22, boss.y + 10, PU_BOMB);
    force_powerup(boss.x + boss.w / 2 - SH_PU_W / 2, boss.y + 20, PU_MISSILE);
    boss.active = FALSE;
    if (boss.kind == (u8)(NBOSS - 1) && wave == 60 && !campaign_won) {
        campaign_won = 1;
        win_pending = 1;
    }
    snd_sfx(SFX_EXPLODE);
}

/* missile AoE: damage everything near (mx,my) */
static void missile_boom(i16 mx, i16 my)
{
    int i;
    fireburst(mx, my, 26);
    spawn_blast(mx, my, 1);
    shk = 6;
    for (i = 0; i < MAX_ENEMY; i++) if (enemy[i].active &&
        overlap(mx - 26, my - 26, 52, 52, enemy[i].x, enemy[i].y, SH_EN_W, SH_EN_H)) {
        enemy[i].hp -= 4;
        if (enemy[i].hp <= 0) kill_enemy(&enemy[i]);
    }
    if (boss.active &&
        overlap(mx - 26, my - 26, 52, 52, boss.x, boss.y, boss.w, boss.h)) {
        apply_boss_damage(boss_pct_damage(10));
    }
    snd_sfx(SFX_EXPLODE);
}

/* ---------------- per-frame update ---------------- */
static void update_stars(void)
{
    int i;
    for (i = 0; i < MAX_STARS; i++) {
        star[i].y += star[i].layer + 1;
        if (star[i].y >= SCRH) { star[i].y = 0; star[i].x = rrange(0, SCRW - 1); }
    }
}

static void update_dust(void)
{
    int i;
    for (i = 0; i < 36; i++) {
        dust[i].y += dust[i].layer;
        if (dust[i].y >= SCRH) {
            dust[i].y = 0;
            dust[i].x = rrange(0, SCRW - 1);
            dust[i].col = (u8)(PAL_NEB + 6 + rnd() % 7);
        }
    }
}

static void update_play(void)
{
    int i;

    /* ---- input / player ---- */
    if (player.alive) {
        i16 sp = 3;
        bool wants_boost = (key_pressed(SC_LSHIFT) || key_pressed(SC_RSHIFT)) ? TRUE : FALSE;
        bool was_boosting = player.boosting;
        player.boosting = FALSE;
        if (wants_boost && player.boost >= BOOST_MIN_START) {
            player.boosting = TRUE;
            sp = 5;
            player.boost -= BOOST_DRAIN;
            if (player.boost < 0) player.boost = 0;
            player.boost_cd = BOOST_RECHARGE_CD;
            if (!was_boosting) snd_sfx(SFX_BOOST);
        } else {
            if (player.boost_cd > 0) player.boost_cd--;
            else if (player.boost < BOOST_MAX) {
                player.boost += BOOST_RECHARGE;
                if (player.boost > BOOST_MAX) player.boost = BOOST_MAX;
            }
        }
        ship_bank = 1;
        if (key_pressed(SC_LEFT) || key_pressed(SC_A))  player.x -= sp;
        if (key_pressed(SC_RIGHT) || key_pressed(SC_D)) player.x += sp;
        if (key_pressed(SC_LEFT) || key_pressed(SC_A)) ship_bank = 0;
        if (key_pressed(SC_RIGHT) || key_pressed(SC_D)) ship_bank = 2;
        if (key_pressed(SC_UP) || key_pressed(SC_W))    player.y -= sp;
        if (key_pressed(SC_DOWN) || key_pressed(SC_S))  player.y += sp;
        if (player.boosting && (frame & 1)) {
            spawn_part(player.x + 4, player.y + SH_SHIP_H + 1, 0);
            spawn_part(player.x + 12, player.y + SH_SHIP_H + 1, 0);
        }
        if (player.x < 0) player.x = 0;
        if (player.x > SCRW - SH_SHIP_W) player.x = SCRW - SH_SHIP_W;
        if (player.y < 8) player.y = 8;
        if (player.y > SCRH - SH_SHIP_H) player.y = SCRH - SH_SHIP_H;
        if (player.firecd > 0) player.firecd--;
        if (key_pressed(SC_SPACE) && player.firecd <= 0) player_fire();
        if (key_hit(SC_CTRL)) fire_missile();
        if (key_hit(SC_B)) smart_bomb();
        if (player.invuln > 0) player.invuln--;
        if (player.shield > 0) player.shield--;
        if (player.rapid  > 0) player.rapid--;
        if (player.wave_boost > 0) player.wave_boost--;
        if (player.combo_t > 0) { if (--player.combo_t == 0) player.combo = 0; }
        if (wave_banner > 0) wave_banner--;
    }

    /* ---- explosion rings ---- */
    for (i = 0; i < 10; i++) if (blast[i].active) {
        blast[i].t++;
        if (blast[i].t > (blast[i].big ? 14 : 8)) blast[i].active = FALSE;
    }

    /* ---- homing missiles ---- */
    for (i = 0; i < MAX_MISSILE; i++) if (msl[i].active) {
        Missile *m = &msl[i];
        i16 tx = -1;
        long best = 0x7FFFFFFFL;
        int j;
        /* steer toward the nearest target ahead of the missile */
        for (j = 0; j < MAX_ENEMY; j++) if (enemy[j].active && enemy[j].y < m->y) {
            long dx = (long)(enemy[j].x + SH_EN_W / 2) - m->x;
            long dy = (long)(enemy[j].y + SH_EN_H / 2) - m->y;
            long d = dx * dx + dy * dy;
            if (d < best) { best = d; tx = enemy[j].x + SH_EN_W / 2; }
        }
        if (boss.active && boss.y < m->y) tx = boss.x + boss.w / 2;
        if (tx >= 0) {
            if (tx > m->x + 2 && m->dx < 2)  m->dx++;
            if (tx < m->x - 2 && m->dx > -2) m->dx--;
        }
        m->x += m->dx; m->y -= 4;
        spawn_part(m->x + SH_MSL_W / 2, m->y + SH_MSL_H, 0);  /* fire trail */
        if (m->y < -SH_MSL_H || m->x < -8 || m->x > SCRW) { m->active = FALSE; continue; }
        /* impact? */
        for (j = 0; j < MAX_ENEMY; j++) if (enemy[j].active &&
            overlap(m->x, m->y, SH_MSL_W, SH_MSL_H,
                    enemy[j].x, enemy[j].y, SH_EN_W, SH_EN_H)) {
            m->active = FALSE;
            missile_boom(m->x + SH_MSL_W / 2, m->y);
            break;
        }
        if (m->active && boss.active &&
            overlap(m->x, m->y, SH_MSL_W, SH_MSL_H,
                    boss.x, boss.y, boss.w, boss.h)) {
            m->active = FALSE;
            missile_boom(m->x + SH_MSL_W / 2, m->y);
        }
    }

    /* ---- player bullets ---- */
    for (i = 0; i < MAX_PBULLET; i++) if (pbul[i].active) {
        pbul[i].x += pbul[i].dx; pbul[i].y += pbul[i].dy;
        if (pbul[i].y < -8 || pbul[i].x < -4 || pbul[i].x > SCRW) pbul[i].active = FALSE;
    }
    /* ---- enemy bullets ---- */
    for (i = 0; i < MAX_EBULLET; i++) if (ebul[i].active) {
        ebul[i].x += ebul[i].dx; ebul[i].y += ebul[i].dy;
        if (ebul[i].y > SCRH || ebul[i].x < -6 || ebul[i].x > SCRW) ebul[i].active = FALSE;
        else if (player.alive && player.invuln == 0 &&
                 overlap(ebul[i].x, ebul[i].y, SH_EB_W, SH_EB_H,
                         player.x + 3, player.y + 3, SH_SHIP_W - 6, SH_SHIP_H - 6)) {
            ebul[i].active = FALSE; hurt_player();
        } else if (player.alive && !ebul[i].grazed &&
                 overlap(ebul[i].x, ebul[i].y, SH_EB_W, SH_EB_H,
                         player.x - 5, player.y - 5, SH_SHIP_W + 10, SH_SHIP_H + 10)) {
            ebul[i].grazed = 1;
            score_add(10);
            if (player.combo > 0) player.combo_t = 130;
        }
    }

    /* ---- enemies ---- */
    for (i = 0; i < MAX_ENEMY; i++) if (enemy[i].active) {
        Enemy *e = &enemy[i];
        e->t++;
        e->y += e->vy;
        if (e->type == E_WEAVER) {
            e->x = e->base + sintab[(e->t) & 63];
            if (e->x < 4) e->x = 4;
            if (e->x > SCRW - SH_EN_W - 4) e->x = SCRW - SH_EN_W - 4;
        }
        if (e->elite && e->type == E_WEAVER && (e->t & 31) == 0)
            add_ebullet(e->x + SH_EN_W / 2, e->y + SH_EN_H, 0, 2);
        if (e->type == E_SHOOTER) {
            if (e->y > 40 && e->y < 120) e->y -= (e->vy - 0); /* linger a touch */
            else if (e->mode == 1 && e->y >= 120) {
                e->x += e->aux * 2;
                if (e->x < -SH_EN_W || e->x > SCRW) { e->active = FALSE; wave_missed++; continue; }
            }
            if (--e->firecd <= 0) {
                if (e->elite) {
                    add_ebullet(e->x + SH_EN_W / 2, e->y + SH_EN_H, -1, 3);
                    add_ebullet(e->x + SH_EN_W / 2, e->y + SH_EN_H,  0, 4);
                    add_ebullet(e->x + SH_EN_W / 2, e->y + SH_EN_H,  1, 3);
                } else enemy_fire(e->x + SH_EN_W / 2, e->y + SH_EN_H);
                e->firecd = rrange(50, 100) + diff_enemy_fire_adjust();
                if (e->firecd < 18) e->firecd = 18;
            }
        }
        if (e->y > SCRH) { e->active = FALSE; wave_missed++; continue; }
        /* collide with player */
        if (player.alive && player.invuln == 0 &&
            overlap(e->x + 2, e->y + 2, SH_EN_W - 4, SH_EN_H - 4,
                    player.x + 3, player.y + 3, SH_SHIP_W - 6, SH_SHIP_H - 6)) {
            kill_enemy(e); hurt_player(); continue;
        }
        /* collide with player bullets (laser pierces: doesn't despawn) */
        {
            int j;
            i16 ey0 = e->y, ey1 = e->y + SH_EN_H;
            for (j = 0; j < MAX_PBULLET; j++) {
                if (!pbul[j].active) continue;
                /* cheap y-band reject before the full box test (most bullets
                   are nowhere near this enemy's row) */
                if (pbul[j].y >= ey1 || pbul[j].y + SH_PB_H <= ey0) continue;
                if (!overlap(pbul[j].x, pbul[j].y, SH_PB_W, SH_PB_H,
                             e->x + 2, e->y, SH_EN_W - 4, SH_EN_H)) continue;
                if (pbul[j].kind != WT_LASER) pbul[j].active = FALSE;
                burst(pbul[j].x, pbul[j].y, 3, C_WHITE, C_YELLOW);
                e->hp -= pbul[j].dmg;
                if (e->hp <= 0) { kill_enemy(e); break; }
            }
        }
    }

    /* ---- boss ---- */
    if (boss.active) {
        if (boss.entering) {
            i16 ry = boss_rest_y();
            boss.y += 3;
            if (boss.y >= ry) { boss.y = ry; boss.entering = FALSE; }
        } else {
            boss_move();
            if (boss.firecd == 18) { boss.charge = 18; snd_sfx(SFX_PHASE); }
            if (--boss.firecd <= 0) { boss_fire(); boss.firecd = diff_boss_fire_cd(); }
        }
        boss.t++;
        if (boss.charge > 0) boss.charge--;
        /* cycle attack pattern; switch faster when enraged */
        if (--boss.atk_t <= 0) {
            boss.atk = (u8)((boss.atk + 1) % boss_attack_count());
            boss.atk_t = boss_atk_time();
        }
        /* phase from remaining HP */
        boss.phase = (boss.hp * 3 <= boss.maxhp) ? 2 : (boss.hp * 3 <= boss.maxhp * 2) ? 1 : 0;
        if (boss.phase != boss.last_phase) {
            boss.last_phase = boss.phase;
            snd_sfx(SFX_PHASE);
            set_msg("BOSS PHASE");
            /* carrier already fills the screen with its own fighters; other
               non-striker bosses call in a one-shot escort per phase */
            if (boss.mvt != MV_CARRIER && boss.mvt != MV_DIVER
                && boss.phase > 0 && !(boss.summons & (1 << boss.phase))) {
                boss.summons |= (u8)(1 << boss.phase);
                summon_escort();
            }
        }
        /* player bullets hit boss (laser pierces) */
        {
            int j;
            for (j = 0; j < MAX_PBULLET; j++) if (pbul[j].active &&
                overlap(pbul[j].x, pbul[j].y, SH_PB_W, SH_PB_H,
                        boss.x + 2, boss.y, (i16)(boss.w - 4), boss.h)) {
                if (pbul[j].kind != WT_LASER) pbul[j].active = FALSE;
                burst(pbul[j].x, pbul[j].y, 2, C_WHITE, C_YELLOW);
                boss.hp -= pbul[j].dmg;
                if (boss.hp <= 0) { boss_die(); break; }
            }
        }
        /* boss body vs player */
        if (boss.active && player.alive && player.invuln == 0 &&
            overlap(boss.x + 2, boss.y, (i16)(boss.w - 4), boss.h,
                    player.x + 3, player.y + 3, SH_SHIP_W - 6, SH_SHIP_H - 6))
            hurt_player();
    }

    /* ---- powerups ---- */
    for (i = 0; i < MAX_POWERUP; i++) if (powr[i].active) {
        powr[i].y += 1; powr[i].t++;
        if (powr[i].y > SCRH) { powr[i].active = FALSE; continue; }
        if (player.alive &&
            overlap(powr[i].x, powr[i].y, SH_PU_W, SH_PU_H,
                    player.x, player.y, SH_SHIP_W, SH_SHIP_H)) {
            apply_powerup(powr[i].type); powr[i].active = FALSE;
        }
    }

    /* ---- particles ---- */
    for (i = 0; i < MAX_PART; i++) if (part[i].active) {
        part[i].x += part[i].dx; part[i].y += part[i].dy;
        if (--part[i].life <= 0) part[i].active = FALSE;
    }

    /* ---- wave director ---- */
    if (win_pending) {
        /* main loop switches to ST_WIN before freeplay wave 61 starts */
    } else if (to_spawn > 0) {
        if (--spawn_cd <= 0) { spawn_enemy(); spawn_cd = rrange(diff_spawn_cd_min(), diff_spawn_cd_max()); }
    } else if (!boss.active) {
        bool clear = TRUE;
        for (i = 0; i < MAX_ENEMY; i++) if (enemy[i].active) { clear = FALSE; break; }
        if (clear) { finish_wave(); start_wave(); }
    }

    if (flash > 0) flash--;
    if (msg_timer > 0) msg_timer--;
    if (shk > 0) { shk--; shx = rrange(-3, 3); shy = rrange(-3, 3); }
    else { shx = shy = 0; }
}

/* ---------------- drawing ---------------- */
static void draw_stars(void)
{
    int i;
    for (i = 0; i < MAX_STARS; i++) vga_pixel(star[i].x, star[i].y, star[i].col);
}

static void draw_dust(void)
{
    int i;
    for (i = 0; i < 36; i++) {
        vga_pixel(dust[i].x, dust[i].y, dust[i].col);
        if (dust[i].layer > 1) vga_pixel(dust[i].x + 1, dust[i].y, dust[i].col);
    }
}

static const char *WNAME[WT_COUNT] = { "CANNON", "LASER", "WAVE" };
static const char *BOSSNAME[NBOSS] = { "GORGON", "REAPER", "SEEKER", "LEVIATHAN", "OVERLORD" };

static void draw_hud(void)
{
    char buf[32];
    sprintf(buf, "SCORE %06lu", score);
    text_draw(4, 2, buf, C_WHITE);
    if (player.combo >= 2) {
        sprintf(buf, "x%d", combo_mult());
        text_draw(116, 2, buf, (combo_mult() >= 3) ? C_YELLOW : C_LGRAY);
    }
    sprintf(buf, "WAVE %d", wave);
    text_draw(SCRW - 8 * (int)strlen(buf) - 4, 2, buf, C_LCYAN);
    /* bottom row: ships / weapon+gun / missiles / bombs */
    sprintf(buf, "SH%d", player.lives);
    text_draw(4, 190, buf, C_LGREEN);
    /* wave gun shows a "*" while its laser boost is active */
    if (player.wtype == WT_WAVE && player.wave_boost > 0)
        sprintf(buf, "WAVE*%d", player.gun);
    else
        sprintf(buf, "%s%d", WNAME[player.wtype], player.gun);
    text_draw(36, 190, buf, (player.wtype == WT_LASER) ? C_LCYAN
                          : (player.wtype == WT_WAVE) ? (player.wave_boost > 0 ? C_WHITE : C_LMAG)
                          : (u8)(PAL_FIRE + 11));
    sprintf(buf, "M%02d", player.msl);
    text_draw(124, 190, buf, C_LGRAY);
    sprintf(buf, "B%02d", player.bombs);
    text_draw(156, 190, buf, C_LRED);
    text_draw(184, 190, "BST", C_LGRAY);
    {
        i16 bw = (i16)((long)player.boost * 36 / BOOST_MAX);
        u8 bc = (player.boost > 90) ? C_LGREEN : (player.boost > 35) ? C_YELLOW : C_LRED;
        vga_frame(210, 190, 38, 6, C_DGRAY);
        if (bw > 0) vga_rect(211, 191, bw, 4, bc);
    }
    if (player.rapid  > 0) text_draw(252, 190, "R", C_YELLOW);
    if (player.shield > 0) text_draw(268, 190, "S", C_LBLUE);
    /* boss hp bar */
    if (boss.active && !boss.entering) {
        i16 w = (i16)((long)boss.hp * 200 / boss.maxhp);
        u8 c = (boss.phase >= 2) ? C_LRED : (boss.phase >= 1) ? C_YELLOW : C_LGREEN;
        vga_frame(59, 12, 202, 6, C_DGRAY);
        vga_rect(60, 13, w, 4, c);
    }
}

/* concentric fire rings for an explosion, radius grows with t */
static void draw_blasts(void)
{
    int i, a;
    for (i = 0; i < 10; i++) if (blast[i].active) {
        i16 r = (i16)(blast[i].t * (blast[i].big ? 3 : 2));
        u8 c = (u8)(PAL_FIRE + 14 - blast[i].t);
        i16 cx = blast[i].x + shx, cy = blast[i].y + shy;
        for (a = 0; a < 16; a++) {                 /* 16-point ring via sintab */
            i16 px = cx + (i16)((long)sintab[(a * 4 + 16) & 63] * r / 46);
            i16 py = cy + (i16)((long)sintab[(a * 4) & 63] * r / 46);
            vga_pixel(px, py, c);
            vga_pixel(px + 1, py, c);
        }
    }
}

/* small alternating thruster glow behind each enemy (cheap animation) */
static void draw_enemy_anim(const Enemy *e)
{
    u8 c = ((e->t >> 2) & 1) ? (u8)(PAL_FIRE + 10) : (u8)(PAL_FIRE + 6);
    i16 fx = e->x + SH_EN_W / 2 + shx, fy;
    if (e->type == E_WEAVER) return;               /* weaver has no tail */
    fy = e->y - 1 + shy;                           /* enemies face down: tail on top */
    vga_pixel(fx - 3, fy, c); vga_pixel(fx + 2, fy, c);
}

static void draw_elite_overlay(const Enemy *e)
{
    u8 c;
    if (!e->elite) return;
    c = (frame & 4) ? C_WHITE : C_LCYAN;
    vga_frame(e->x - 1 + shx, e->y - 1 + shy, SH_EN_W + 2, SH_EN_H + 2, c);
}

static void draw_play(void)
{
    int i;
    for (i = 0; i < MAX_PART; i++) if (part[i].active) {
        u8 c = part_col(&part[i]);
        vga_pixel(part[i].x + shx, part[i].y + shy, c);
        vga_pixel(part[i].x + 1 + shx, part[i].y + shy, c);   /* 2px = chunkier fire */
    }
    draw_blasts();
    for (i = 0; i < MAX_POWERUP; i++) if (powr[i].active) {
        i16 bob = (i16)(sintab[(powr[i].t * 4) & 63] / 18);
        DS(powr[i].x, powr[i].y + bob, SH_PU_W, SH_PU_H, spr_powerup[powr[i].type]);
        if ((frame + i) & 4) vga_pixel(powr[i].x + shx, powr[i].y + bob + shy, C_WHITE);
        {
            static const char *L = "GRHLMZWB$"; /* Gun Rapid sHield Life Missile laserZ Wave Bomb Score */
            char s[2]; s[0] = L[powr[i].type]; s[1] = 0;
            text_draw(powr[i].x + 2 + shx, powr[i].y + 2 + bob + shy, s, C_BLACK);
        }
    }
    {
        u8 estage = (wave > 0) ? (u8)(((wave - 1) / 4) & 3) : 0;   /* skin per 4-wave block */
        for (i = 0; i < MAX_ENEMY; i++) if (enemy[i].active) {
            draw_enemy_anim(&enemy[i]);
            DS(enemy[i].x, enemy[i].y, SH_EN_W, SH_EN_H, spr_enemy[estage][enemy[i].type]);
            draw_elite_overlay(&enemy[i]);
        }
    }
    if (boss.active) {
        i16 cx = (i16)(boss.x + boss.w / 2 + shx), cy = (i16)(boss.y + boss.h / 2 + shy);
        DS(boss.x, boss.y, boss.w, boss.h, spr_boss[boss.spr]);
        if (boss.phase >= 1) {                /* battle damage, scaled to footprint */
            vga_hline((i16)(cx - boss.w / 3), (i16)(cy - boss.h / 6), (i16)(2 * boss.w / 3), C_DGRAY);
            vga_hline((i16)(cx - boss.w / 4), (i16)(cy + boss.h / 4), (i16)(boss.w / 2), C_DGRAY);
        }
        if (boss.phase >= 2) {
            vga_frame((i16)(cx - 9), (i16)(cy - 6), 18, 12, C_LRED);
            vga_pixel((i16)(cx - boss.w / 3), cy, C_YELLOW);
            vga_pixel((i16)(cx + boss.w / 3), cy, C_YELLOW);
        }
        if (boss.charge > 0) vga_frame((i16)(boss.x - 2 + shx), (i16)(boss.y - 2 + shy),
                                       (i16)(boss.w + 4), (i16)(boss.h + 4),
                                       (boss.charge & 2) ? C_WHITE : C_LRED);
        /* pulsing core overlay (faster when enraged) */
        vga_rect((i16)(cx - 3), (i16)(cy - 3), 6, 6,
                 (u8)(PAL_FIRE + 8 + ((frame >> (boss.phase >= 2 ? 0 : 1)) & 7)));
    }
    for (i = 0; i < MAX_MISSILE; i++) if (msl[i].active)
        DS(msl[i].x, msl[i].y, SH_MSL_W, SH_MSL_H, spr_missile);
    for (i = 0; i < MAX_EBULLET; i++) if (ebul[i].active)
        DS(ebul[i].x, ebul[i].y, SH_EB_W, SH_EB_H, spr_ebullet);
    for (i = 0; i < MAX_PBULLET; i++) if (pbul[i].active)
        DS(pbul[i].x, pbul[i].y, SH_PB_W, SH_PB_H, spr_pbullet[pbul[i].kind]);
    /* player (blink while invulnerable) */
    if (player.alive && !(player.invuln > 0 && (frame & 2))) {
        if (player.shield > 0)
            vga_frame(player.x - 2 + shx, player.y - 2 + shy, SH_SHIP_W + 4, SH_SHIP_H + 4,
                      (frame & 2) ? (u8)(PAL_GLOW + 14) : (u8)(PAL_GLOW + 8));
        DS(player.x, player.y, SH_SHIP_W, SH_SHIP_H, spr_ship[ship_bank]);
        /* animated engine flame */
        {
            u8 fc = (frame & 2) ? (u8)(PAL_FIRE + 12) : (u8)(PAL_FIRE + 8);
            i16 fx = player.x + shx, fy = player.y + SH_SHIP_H + shy;
            vga_pixel(fx + 3, fy, fc);  vga_pixel(fx + 12, fy, fc);
            vga_pixel(fx + 3, fy + 1, (u8)(PAL_FIRE + 5));
            vga_pixel(fx + 12, fy + 1, (u8)(PAL_FIRE + 5));
            if (frame & 1) { vga_pixel(fx + 3, fy + 2, (u8)(PAL_FIRE + 3));
                             vga_pixel(fx + 12, fy + 2, (u8)(PAL_FIRE + 3)); }
            if (player.boosting) {
                vga_hline(fx + 1, fy + 3, 5, (u8)(PAL_FIRE + 11));
                vga_hline(fx + 10, fy + 3, 5, (u8)(PAL_FIRE + 11));
                vga_pixel(fx + 3, fy + 4, (u8)(PAL_FIRE + 6));
                vga_pixel(fx + 12, fy + 4, (u8)(PAL_FIRE + 6));
            }
        }
    }
    draw_hud();
    if (wave_banner > 0) {
        char b[32];
        if (boss.active) {
            sprintf(b, "%s APPROACHING", BOSSNAME[boss.kind]);
            text_center(76, b, (boss.phase >= 2) ? C_LRED : C_YELLOW);
        } else {
            sprintf(b, "WAVE %d", wave);
            text_center(78, b, C_YELLOW);
            text_center(90, "GET READY", C_LCYAN);
        }
    }
    if (msg_timer > 0) text_center(104, msg_text, C_YELLOW);
}

/* ---------------- title / scores ---------------- */
static const char *DIFNAME[3] = { "EASY", "NORMAL", "HARD" };

static void draw_title(void)
{
    char b[40];
    i16 page = (i16)((frame / 210) & 3);
    i16 fx = (i16)(frame % 360) - 32;
    text_center(34, "STELLAR ASSAULT", C_YELLOW);
    text_center(50, "a vga space shooter", C_LGRAY);
    if (frame & 16) text_center(84, "PRESS SPACE TO PLAY", C_WHITE);
    sprintf(b, "< DIFFICULTY: %s >", DIFNAME[g_diff]);
    text_center(104, b, (g_diff == DIF_HARD) ? C_LRED : (g_diff == DIF_EASY) ? C_LGREEN : C_YELLOW);
    if (page == 0) {
        text_center(120, "WASD/ARROWS MOVE  SHIFT BOOST", C_LCYAN);
        text_center(132, "SPACE FIRE  CTRL MISSILE", C_LCYAN);
        text_center(144, "B BOMB  H HELP  UP/DN DIFF", C_DGRAY);
    } else if (page == 1) {
        sprintf(b, "HI %06lu  %.8s", g_hi[0].score, g_hi[0].name);
        text_center(122, b, C_LGREEN);
        sprintf(b, "2  %06lu  %.8s", g_hi[1].score, g_hi[1].name);
        text_center(136, b, C_LCYAN);
    } else if (page == 2) {
        sprintf(b, "LAST WAVE %d", last_wave);
        text_center(120, b, C_LCYAN);
        sprintf(b, "MAX COMBO %d", last_combo);
        text_center(134, b, C_YELLOW);
        sprintf(b, "BOSSES %d", last_bosses);
        text_center(148, b, C_LGREEN);
    } else {
        text_center(122, "ELITES AFTER WAVE 6", C_LMAG);
        text_center(136, "GRAZE FOR BONUS", C_LCYAN);
    }
    if (fx < SCRW) vga_sprite(fx, 166, SH_SHIP_W, SH_SHIP_H, spr_ship[1]);
    if (fx > 40 && fx < SCRW + 40) vga_sprite(SCRW - fx, 20, SH_EN_W, SH_EN_H, spr_enemy[0][(frame / 70) % 3]);
}

/* ---------------- game over ---------------- */
static i16 over_timer = 0;

static void draw_over(void)
{
    char b[40];
    text_center(60, "GAME OVER", C_LRED);
    sprintf(b, "FINAL SCORE %06lu", score);
    text_center(84, b, C_WHITE);
    sprintf(b, "REACHED WAVE %d", wave);
    text_center(100, b, C_LCYAN);
    sprintf(b, "BEST COMBO x%d", player.max_combo);
    text_center(116, b, C_YELLOW);
    if (frame & 16) text_center(150, "PRESS SPACE", C_LGRAY);
}

static void draw_win(void)
{
    char b[40];
    text_center(44, "VICTORY", C_YELLOW);
    text_center(64, "FINAL BOSS DESTROYED", C_WHITE);
    sprintf(b, "SCORE %06lu", score);
    text_center(88, b, C_LGREEN);
    sprintf(b, "MAX COMBO x%d", player.max_combo);
    text_center(104, b, C_LCYAN);
    sprintf(b, "BOSSES %d", bosses_defeated);
    text_center(120, b, C_LMAG);
    if (frame & 16) text_center(152, "SPACE FREEPLAY", C_WHITE);
    text_center(168, "ESC TITLE", C_DGRAY);
}

/* ---------------- instruction menu ---------------- */
static i16 help_page = 0;

/* letter identifying each pickup gem (index = PU_* type) */
static const char PU_LETTER[PU_COUNT + 1] = "GRHLMZWB$";

static void help_row(i16 y, u8 pu, const char *txt)
{
    char s[2];
    vga_sprite(52, y, SH_PU_W, SH_PU_H, spr_powerup[pu]);
    s[0] = PU_LETTER[pu]; s[1] = 0;
    text_draw(54, y + 2, s, C_BLACK);        /* gem's identifying letter */
    text_draw(70, y + 2, txt, C_LGRAY);
}

static void draw_help(void)
{
    if (help_page == 0) {
        text_center(8, "PICKUPS", C_YELLOW);
        help_row( 24, PU_GUN,     "GUN: +1 LEVEL (LOSE 1 ON DEATH)");
        help_row( 40, PU_RAPID,   "RAPID: FASTER FIRE, TIMED");
        help_row( 56, PU_SHIELD,  "SHIELD: 10 SEC INVULNERABLE");
        help_row( 72, PU_LIFE,    "LIFE: EXTRA SHIP");
        help_row( 88, PU_MISSILE, "MISSILES: +4 AMMO (MAX 30)");
        help_row(104, PU_LASER,   "LASER: FAST PIERCING GUN");
        help_row(120, PU_WAVE,    "WAVE: WIDE ARC GUN");
        help_row(136, PU_BOMB,    "BOMB: +1 SMART BOMB (MAX 10)");
        help_row(152, PU_SCORE,   "SCORE GEM: RISKY BONUS");
        text_center(176, "SPACE NEXT PAGE   ESC BACK", C_LCYAN);
    } else {
        text_center(8, "COMBAT MANUAL", C_YELLOW);
        text_draw(24,  24, "SPACE  FIRE MAIN WEAPON", C_LGRAY);
        text_draw(24,  36, "       GUN 1-4 WIDENS FIRE", C_DGRAY);
        text_draw(24,  52, "WAVE + LASER PICKUP GIVES", C_LMAG);
        text_draw(24,  64, "       A 10 SEC PIERCE BOOST", C_DGRAY);
        vga_sprite(28, 78, SH_MSL_W, SH_MSL_H, spr_missile);
        text_draw(40,  80, "CTRL   MISSILE: BIG BLAST,", C_LGRAY);
        text_draw(40,  92, "       BEST AGAINST BOSSES", C_DGRAY);
        text_draw(24, 108, "B      BOMB: CLEAR SHOTS,", C_LGRAY);
        text_draw(24, 120, "       DAMAGE EVERYTHING", C_DGRAY);
        text_draw(24, 136, "SHIFT  BOOST: SPEED BURST", C_LGRAY);
        text_draw(24, 152, "FAST KILLS COMBO UP TO X5", C_LGRAY);
        text_draw(24, 164, "GRAZE SHOTS FOR BONUS POINTS", C_DGRAY);
        text_center(182, "SPACE PICKUPS   ESC BACK", C_LCYAN);
    }
}

static void draw_scores(void)
{
    int i; char b[40];
    text_center(20, "HIGH SCORES", C_YELLOW);
    for (i = 0; i < HISCORE_N; i++) {
        sprintf(b, "%d. %-8.8s %06lu", i + 1, g_hi[i].name, g_hi[i].score);
        text_center(44 + i * 14, b, (i == 0) ? C_WHITE : C_LCYAN);
    }
    if (frame & 16) text_center(180, "PRESS SPACE", C_LGREEN);
    {
        char s[32];
        sprintf(s, "LAST W%d  COMBO %d", last_wave, last_combo);
        text_center(166, s, C_DGRAY);
    }
}

/* ---------------- main loop ---------------- */
void game_run(void)
{
    int state = ST_TITLE;
    int entry_rank = -1;
    char name[NAME_LEN + 1]; int nlen = 0;
    bool paused = FALSE;
    int prev_state = -1, prev_help = -1; u8 prev_blink = 2;

    hi_load();
    init_stars();
    init_dust();
    gen_nebula();
    snd_music_set(MUS_TITLE);

    while (state != ST_QUIT) {
        if (!(state == ST_PLAY && paused)) frame++;

        /* global keys */
        if (key_hit(SC_M)) snd_mute_toggle();

        if (state == ST_PLAY && key_hit(SC_P)) {
            paused = !paused;
            if (paused) snd_silence();
            kbd_clear();               /* drop held keys so the ship can't drift across a pause */
        }

        if (state != ST_PLAY || !paused) { update_stars(); update_dust(); }

        switch (state) {
        case ST_TITLE:
            if (key_hit(SC_UP)   && g_diff > 0) g_diff--;
            if (key_hit(SC_DOWN) && g_diff < 2) g_diff++;
            if (key_hit(SC_H))   { help_page = 0; state = ST_HELP; }
            if (key_hit(SC_SPACE)) { reset_game(); start_wave(); state = ST_PLAY; paused = FALSE;
                                     snd_music_set(MUS_GAME); }
            if (key_hit(SC_ESC))   state = ST_QUIT;
            break;
        case ST_HELP:
            if (key_hit(SC_SPACE)) help_page ^= 1;
            if (key_hit(SC_ESC) || key_hit(SC_H)) state = ST_TITLE;
            break;
        case ST_PLAY:
            if (!paused) update_play();
            if (win_pending) {
                remember_run();
                snd_music_set(MUS_WIN);
                state = ST_WIN;
                kbd_clear();
                break;
            }
            if (key_hit(SC_ESC)) { state = ST_TITLE; snd_music_set(MUS_TITLE); kbd_clear(); }
            if (!player.alive) {
                remember_run();
                snd_music_set(MUS_TITLE);
                over_timer = 160;              /* let the moment land */
                state = ST_OVER;
            }
            break;
        case ST_OVER:
            if (over_timer > 0) over_timer--;
            if ((over_timer <= 130 && (key_hit(SC_SPACE) || key_hit(SC_ENTER))) || over_timer == 0) {
                entry_rank = hi_qualifies(score);
                if (entry_rank >= 0) { name[0] = 0; nlen = 0; state = ST_ENTRY; }
                else state = ST_SCORES;
            }
            break;
        case ST_ENTRY: {
            char c = kbd_getchar();
            if (c && nlen < NAME_LEN) { name[nlen++] = c; name[nlen] = 0; }
            if (key_hit(SC_BKSP) && nlen > 0) { name[--nlen] = 0; }
            if (key_hit(SC_ENTER)) {
                if (nlen == 0) strcpy(name, "PLAYER");
                hi_insert(entry_rank, name, score);
                hi_save();
                state = ST_SCORES;
            }
            if (key_hit(SC_ESC)) {
                if (nlen == 0) strcpy(name, "PLAYER");
                hi_insert(entry_rank, name, score);
                hi_save();
                reset_game(); start_wave(); state = ST_PLAY; paused = FALSE;
                snd_music_set(MUS_GAME);
            }
            break; }
        case ST_SCORES:
            if (key_hit(SC_SPACE)) state = ST_TITLE;
            if (key_hit(SC_CTRL)) { reset_game(); start_wave(); state = ST_PLAY; paused = FALSE;
                                    snd_music_set(MUS_GAME); }
            break;
        case ST_WIN:
            if (key_hit(SC_ESC)) { state = ST_TITLE; snd_music_set(MUS_TITLE); kbd_clear(); }
            if (key_hit(SC_SPACE) || key_hit(SC_ENTER) || key_hit(SC_CTRL)) {
                win_pending = 0;
                finish_wave();
                start_wave();
                state = ST_PLAY; paused = FALSE;
                snd_music_set(MUS_GAME);
                kbd_clear();
            }
            break;
        }

        /* keep music/sfx running every frame regardless of what we draw */
        if (!paused || state != ST_PLAY) snd_update();

        /* ---- render (static menus only redraw when they change) ---- */
        {
            u8 blink = (u8)((frame >> 3) & 1);
            bool render;
            switch (state) {
            case ST_HELP:   render = (state != prev_state) || (help_page != prev_help); break;
            case ST_SCORES: render = (state != prev_state) || (blink != prev_blink);    break;
            default:        render = TRUE; break;   /* animated / interactive screens */
            }
            prev_state = state; prev_help = help_page; prev_blink = blink;

            if (!render) {
                /* nothing changed - hold the last frame, just pace the loop */
                vga_wait_vsync(); vga_wait_vsync();
                continue;
            }

            if (flash > 0) vga_clear(C_RED);
            else if (state == ST_HELP || state == ST_SCORES) vga_bg_blit(0); /* static bg */
            else vga_bg_blit((u16)((frame >> 1) % SCRH));                    /* scrolling  */
            draw_stars();
            draw_dust();
            switch (state) {
            case ST_TITLE:  draw_title(); break;
            case ST_HELP:   draw_help(); break;
            case ST_PLAY:   draw_play();
                            if (paused) text_center(96, "PAUSED", C_WHITE);
                            break;
            case ST_OVER:   draw_over(); break;
            case ST_WIN:    draw_win(); break;
            case ST_SCORES: draw_scores(); break;
            case ST_ENTRY: {
                char b[32];
                text_center(60, "NEW HIGH SCORE!", C_YELLOW);
                sprintf(b, "SCORE %06lu", score);
                text_center(80, b, C_WHITE);
                text_center(108, "ENTER NAME:", C_LCYAN);
                {
                    char nb[16]; sprintf(nb, "%s%s", name, (frame & 8) ? "_" : " ");
                    text_center(124, nb, C_WHITE);
                }
                text_center(150, "TYPE THEN PRESS ENTER", C_LGRAY);
                text_center(164, "ESC SAVES + REPLAYS", C_DGRAY);
                break; }
            }
            if (snd_muted()) text_draw(SCRW - 40, 190, "MUTE", C_LRED);

            /* animate the palette only on the live screens, less often */
            if ((state == ST_PLAY || state == ST_TITLE) && !(paused && state == ST_PLAY)
                && (frame & 7) == 0)
                vga_cycle_palette();
            vga_present_paced();
        }
    }
    snd_music_stop();
    snd_silence();
}

/* ---------------- self-test: populate + render one frame ---------------- */
void game_selftest(const char *bmp)
{
    int i;
    hi_load();
    init_stars();
    init_dust();
    gen_nebula();
    reset_game();
    wave = 8;
    vga_set_theme(2);
    /* hand-place a representative scene */
    player.x = 150; player.y = 150; player.shield = 300; player.gun = 3;
    player.wtype = WT_WAVE; player.wave_boost = 200; player.msl = 12; player.bombs = 3;
    player.boost = 82; player.boost_cd = 12; player.boosting = TRUE;
    player.combo = 12; player.combo_t = 100; player.max_combo = 18; ship_bank = 2;
    wave_banner = 80;
    set_msg("NO HIT +1000");
    msl[0].active = TRUE; msl[0].x = 120; msl[0].y = 96; msl[0].dx = 1;
    msl[1].active = TRUE; msl[1].x = 226; msl[1].y = 120; msl[1].dx = -1;
    powr[2].active = TRUE; powr[2].type = PU_LASER; powr[2].x = 250; powr[2].y = 150;
    powr[3].active = TRUE; powr[3].type = PU_BOMB;  powr[3].x = 44;  powr[3].y = 142;
    powr[4].active = TRUE; powr[4].type = PU_WAVE;  powr[4].x = 150; powr[4].y = 175;
    powr[5].active = TRUE; powr[5].type = PU_SCORE; powr[5].x = 170; powr[5].y = 58;
    for (i = 0; i < 6; i++) {
        enemy[i].active = TRUE; enemy[i].type = (u8)(i % 3);
        enemy[i].x = 20 + i * 45; enemy[i].y = 30 + (i % 2) * 24;
        enemy[i].base = enemy[i].x; enemy[i].hp = 2; enemy[i].vy = 1; enemy[i].t = i * 8;
        enemy[i].elite = (i == 1 || i == 4);
    }
    boss.active = TRUE; boss.entering = FALSE; boss.kind = 3;   /* LEVIATHAN carrier */
    { const BossDef *bd = &BOSSDEF[boss.kind];
      boss.spr = bd->spr; boss.mvt = bd->mvt; boss.atkset = bd->atkset;
      boss.w = spr_boss_w[boss.spr]; boss.h = spr_boss_h[boss.spr]; }
    boss.x = (i16)(SCRW/2 - boss.w/2); boss.y = 40;
    boss.hp = 40; boss.maxhp = 120; boss.phase = 1; boss.charge = 12;
    for (i = 0; i < 5; i++) { pbul[i].active = TRUE; pbul[i].kind = WT_LASER; pbul[i].x = 60 + i * 40; pbul[i].y = 100 - i * 10; pbul[i].dy = -12; pbul[i].dmg = 2; }
    for (i = 0; i < 4; i++) { ebul[i].active = TRUE; ebul[i].x = 80 + i * 50; ebul[i].y = 90 + i * 8; ebul[i].dy = 3; }
    powr[0].active = TRUE; powr[0].type = PU_SHIELD; powr[0].x = 100; powr[0].y = 120;
    powr[1].active = TRUE; powr[1].type = PU_LIFE;   powr[1].x = 200; powr[1].y = 130;
    fireburst(160, 90, 30);
    spawn_blast(90, 70, 1); spawn_blast(230, 60, 3);
    blast[0].t = 5; blast[1].t = 7;
    score = 12345; frame = 1;

    vga_bg_blit(0);
    draw_stars();
    draw_dust();
    draw_play();
    bmp_dump(bmp);
}

void game_selftest_title(const char *bmp)
{
    hi_load();
    init_stars();
    init_dust();
    gen_nebula();
    vga_set_theme(1);
    last_wave = 12; last_combo = 31; last_bosses = 3;
    frame = 656;               /* attract page + blinking prompt visible */
    vga_bg_blit(0);
    draw_stars();
    draw_dust();
    draw_title();
    bmp_dump(bmp);
}

/* Benchmark: run the real gameplay loop (update + draw + vsync present) for
   ~10 s of wall clock (measured via the 18.2 Hz BIOS tick at 0040:006C, which
   DOSBox keeps synced to real time) and record the achieved frame count. This
   is the actual on-screen speed the game runs at. */
static void bench_write(const char *tag, u32 frames, u32 ticks)
{
    FILE *f = fopen("BENCH.TXT", "a");
    if (f) { fprintf(f, "%s frames %lu ticks %lu\n", tag, frames, ticks); fclose(f); }
}

/* Measure raw per-frame throughput (unpaced present, capped only by 70 Hz
   vsync) so before/after optimisation shows compute headroom, not the 35 Hz
   gameplay cap. scene 0 = a sustained boss fight, scene 1 = the help screen. */
void game_bench(int scene)
{
    u32 __far *bios = (u32 __far *)MKFP(0x0040, 0x006C);
    u32 t0, tnow, frames = 0;
    hi_load(); init_stars(); init_dust(); gen_nebula();
    reset_game();

    if (scene == 1) {                              /* --- help screen --- */
        help_page = 0; vga_set_theme(0);
        t0 = *bios;
        while (((tnow = *bios) - t0) < 182UL) {
            update_stars(); update_dust();
            vga_bg_blit((u16)((frame >> 1) % SCRH));
            draw_stars(); draw_dust(); draw_help();
            if ((frame & 3) == 0) vga_cycle_palette();
            frame++; frames++;
            vga_blit_novsync();
        }
        bench_write("help", frames, (u32)(tnow - t0));
        return;
    }

    wave = 7; start_wave();                        /* --- wave 8 boss --- */
    t0 = *bios;
    while (((tnow = *bios) - t0) < 182UL) {
        player.invuln = 10; player.alive = TRUE; player.lives = 9;
        if (boss.active) boss.hp = boss.maxhp;     /* keep the boss alive & firing */
        if ((frame % 5) == 0) player_fire();
        update_play();
        if (flash > 0) vga_clear(C_RED);
        else vga_bg_blit((u16)((frame >> 1) % SCRH));
        draw_stars(); draw_dust(); draw_play();
        if ((frame & 3) == 0) vga_cycle_palette();
        frame++; frames++;
        vga_blit_novsync();
    }
    bench_write("boss", frames, (u32)(tnow - t0));
}

void game_selftest_help(const char *bmp, int page)
{
    init_stars();
    init_dust();
    gen_nebula();
    vga_set_theme(0);
    help_page = (i16)page;
    frame = 16;
    vga_bg_blit(0);
    draw_stars();
    draw_dust();
    draw_help();
    bmp_dump(bmp);
}

/* per-stage enemy lineup: 4 stage skins (rows) x 3 types (cols) */
void game_selftest_stages(const char *bmp)
{
    int s, t;
    gen_nebula();
    vga_set_theme(0);
    vga_bg_blit(0);
    text_center(4, "ENEMY STAGES", C_YELLOW);
    for (s = 0; s < NSTAGE; s++) {
        char lab[16];
        sprintf(lab, "S%d", s + 1);
        text_draw(6, 30 + s * 40, lab, C_LGRAY);
        for (t = 0; t < 3; t++)
            vga_sprite(60 + t * 70, 24 + s * 40, SH_EN_W, SH_EN_H, spr_enemy[s][t]);
    }
    bmp_dump(bmp);
}
