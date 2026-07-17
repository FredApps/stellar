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

/* roster slots (footprint/hitbox is owned by the sprite: spr_boss_w/h[spr]). */
typedef struct { u8 spr; i16 hpbonus; } BossDef;
static const BossDef BOSSDEF[NBOSS] = {
    /* spr hpbonus */
    {  0,  10 },   /* W04 GORGON    - fair opening wall tank    */
    {  1,   0 },   /* W08 REAPER    - small diving dagger       */
    {  2,  20 },   /* W12 LEVIATHAN - high carrier              */
    {  3,   0 },   /* W16 SEEKER    - orbiting green core       */
    {  4,  15 },   /* W20 MANTIS    - twin pincer craft         */
    {  5,  35 },   /* W24 ANVIL     - squat press block         */
    {  6,  10 },   /* W28 SERAPH    - tall wing sweeper         */
    {  7,  25 },   /* W32 NEXUS     - split-core pods           */
    {  8,  30 },   /* W36 KRAKEN    - tentacle carrier          */
    {  9,   5 },   /* W40 PHANTOM   - blink striker             */
    { 10,  45 },   /* W44 CITADEL   - turret fortress           */
    { 11,  15 },   /* W48 VORTEX    - rotating ring             */
    { 12,  20 },   /* W52 BASILISK  - eye-beam corridors        */
    { 13,  60 },   /* W56 TITAN     - crushing dreadnought      */
    { 14,  90 },   /* W60 OVERLORD  - campaign finale           */
};

static struct {
    bool active, entering;
    i16  x, y, hp, maxhp, dir, t, firecd, charge;
    i16  tx, ty, mv_t;       /* tx target-x; ty reused as diver sub-state    */
    i16  w, h;               /* per-boss footprint = hitbox                  */
    u8   kind;               /* roster slot 0..NBOSS-1 (name + finale test)  */
    u8   spr;                /* sprite index into spr_boss[]                 */
    u8   mvt;                /* spare/diagnostic: authored kind now drives AI */
    u8   atkset;             /* spare/diagnostic: authored kind now drives AI */
    u8   phase;              /* 0 full, 1 <66%, 2 <33% (enraged) */
    u8   last_phase, summons;
    u8   atk;                /* current attack pattern within the script     */
    i16  atk_t;              /* frames until the next pattern switch    */
    i16  spin;              /* rotating-spiral angle (sintab index)    */
    i16  launch_t;           /* carrier squad-launch countdown               */
    u8   squads;             /* squads launched this phase (bay throttle)    */
    i16  support_t;          /* non-carrier support-wave countdown            */
    u8   drop_budget;        /* random support drops remaining this encounter */
    i16  recent_dmg;         /* bright health-bar damage segment              */
    i16  hurt_t;             /* white hit-flash frames                       */
    i16  dive_t;             /* dive/lunge/telegraph sub-timer               */
    i16  px[2], py[2];       /* pod anchors (NEXUS) / scratch per-boss state */
    i16  die_t;              /* staged-death countdown (0 = alive)           */
    i16  warn;               /* WARNING intro frames before the entrance     */
} boss;

/* expanding-ring explosion animation */
typedef struct { bool active; i16 x, y, t, big; } Blast;
static Blast    blast[10];

/* floating score/combo popup text */
typedef struct { bool active; i16 x, y, t; char txt[8]; } Popup;
static Popup    popup[4];

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
static i16  win_t;
static char pilot_name[NAME_LEN + 1];
static u8   entry_name_error;

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
#define WIN_INPUT_DELAY  210

/* forward declarations */
static void kill_enemy(Enemy *e);
static void boss_die(void);
static void set_msg(const char *s);
static void summon_escort(void);
static void apply_boss_damage(i16 dmg);
static void spawn_popup(i16 x, i16 y, const char *s);
static u8 choose_powerup(void);

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

static const char *weapon_name(u8 type)
{
    if (type == WT_LASER) return "LASER";
    if (type == WT_WAVE) return "WAVE";
    return "CANNON";
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
    switch (boss.kind) {
    case 0:  return (i16)(60 + d - boss.phase * 8);  /* GORGON */
    case 1:  return (i16)(34 + d - boss.phase * 6);  /* REAPER (dives carry the threat) */
    case 2:  return (i16)(66 + d - boss.phase * 8);  /* LEVIATHAN */
    case 3:  return (i16)(44 + d - boss.phase * 9);  /* SEEKER */
    case 4:  return (i16)(46 + d - boss.phase * 8);  /* MANTIS (lunges carry the threat) */
    case 5:  return (i16)(62 + d - boss.phase * 10); /* ANVIL */
    case 6:  return (i16)(40 + d - boss.phase * 8);  /* SERAPH */
    case 7:  return (i16)(38 + d - boss.phase * 7);  /* NEXUS */
    case 8:  return (i16)(58 + d - boss.phase * 8);  /* KRAKEN */
    case 9:  return (i16)(34 + d - boss.phase * 7);  /* PHANTOM */
    case 10: return (i16)(48 + d - boss.phase * 8);  /* CITADEL */
    case 11: return (i16)(36 + d - boss.phase * 7);  /* VORTEX */
    case 12: return (i16)(56 + d - boss.phase * 8);  /* BASILISK (guillotine carries the threat) */
    case 13: return (i16)(50 + d - boss.phase * 8);  /* TITAN */
    default: return (i16)(34 + d - boss.phase * 6);  /* OVERLORD */
    }
}

static i16 boss_attack_count(void)
{
    switch (boss.kind) {
    case 2: case 8: return 2;                 /* carrier cadence */
    case 14: return 4;
    default: return 3;
    }
}

static i16 boss_atk_time(void)
{
    i16 t;
    switch (boss.kind) {
    case 0:  t = 140; break;
    case 1:  t =  84; break;
    case 2:  t = 150; break;
    case 5:  t = 132; break;
    case 8:  t = 142; break;
    case 10: t = 128; break;
    case 13: t = 120; break;
    case 14: t = 112; break;
    default: t = 118; break;
    }
    t -= boss.phase * ((boss.kind == 0 || boss.kind == 5 || boss.kind == 13) ? 22 : 24);
    return (t < 48) ? 48 : t;
}

static i16 boss_pct_damage(i16 div)
{
    i16 d = (i16)((boss.maxhp + div - 1) / div);
    return (d < 1) ? 1 : d;
}

static i16 boss_health_for(i16 w, i16 kind)
{
    i16 hp_wave = (w > 60) ? 60 : w;
    i16 hp_index = (w > 60) ? 14 : (i16)(w / 4 - 1);
    if (hp_index < 0) hp_index = 0;
    return (i16)(36 + hp_wave * diff_boss_hp_mul() + hp_index * 8
                 + BOSSDEF[kind].hpbonus);
}

static bool shooter_lingering(const Enemy *e)
{
    return (e->y > 40 && e->y < 120 && e->t < 140) ? TRUE : FALSE;
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

/* floating score/combo popup: rises for ~30 frames then fades out */
static void spawn_popup(i16 x, i16 y, const char *s)
{
    int i;
    for (i = 0; i < 4; i++) if (!popup[i].active) {
        popup[i].active = TRUE;
        popup[i].t = 30;
        if (x < 2) x = 2;
        if (x > SCRW - 8 * (i16)strlen(s) - 2) x = (i16)(SCRW - 8 * (i16)strlen(s) - 2);
        if (y < 12) y = 12;
        popup[i].x = x; popup[i].y = y;
        strncpy(popup[i].txt, s, sizeof(popup[i].txt) - 1);
        popup[i].txt[sizeof(popup[i].txt) - 1] = 0;
        return;
    }
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
    memset(popup, 0, sizeof(popup));
    boss.active = FALSE;
    player.x = 152; player.y = 168;
    player.lives = (g_diff == DIF_EASY) ? 5 : (g_diff == DIF_HARD) ? 2 : 3;
    player.gun = GUN_MIN; player.wtype = WT_CANNON;
    player.msl = 5; player.bombs = (g_diff == DIF_HARD) ? 1 : 2;
    player.shield = (g_diff == DIF_EASY) ? 200 : 0;   /* easy: brief invuln head-start */
    player.invuln = 0; player.firecd = 0; player.rapid = 0; player.wave_boost = 0;
    player.boost = BOOST_MAX; player.boost_cd = 0; player.boosting = FALSE;
    player.facing_down = FALSE;
    player.combo = 0; player.combo_t = 0; player.max_combo = 0; player.ram_cd = 0; player.alive = TRUE;
    score = 0; wave = 0; flash = 0; shk = 0;
    wave_banner = 0; msg_timer = 0; msg_text[0] = 0; ship_bank = 1;
    wave_kills = wave_missed = wave_hit = combo_broken = 0;
    risk_spawned = 0; bosses_defeated = 0; campaign_won = 0; win_pending = 0; win_t = 0;
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
        i16 boss_index = (i16)((wave / 4) - 1);
        const BossDef *bd;
        boss.active = TRUE; boss.entering = TRUE;
        if (boss_index < 0) boss_index = 0;
        boss.kind = (boss_index < NBOSS) ? (u8)boss_index : (u8)(boss_index % NBOSS);
        bd = &BOSSDEF[boss.kind];
        boss.spr = bd->spr; boss.mvt = boss.kind; boss.atkset = boss.kind;
        boss.w = spr_boss_w[boss.spr]; boss.h = spr_boss_h[boss.spr];
        boss.phase = 0; boss.last_phase = 0; boss.summons = 0;
        boss.atk = 0; boss.atk_t = boss_atk_time(); boss.spin = 0;
        boss.x = SCRW / 2 - boss.w / 2; boss.y = -boss.h;
        boss.maxhp = boss.hp = boss_health_for(wave, boss.kind);
        if (boss.maxhp < 20) boss.maxhp = boss.hp = 20;
        boss.dir = 1; boss.t = 0; boss.firecd = 60; boss.charge = 0;
        boss.tx = boss.x; boss.ty = 0; boss.mv_t = 50;
        boss.launch_t = 90; boss.squads = 0;
        boss.support_t = 260; boss.drop_budget = 4; boss.recent_dmg = 0;
        boss.hurt_t = 0; boss.dive_t = 0; boss.die_t = 0;
        boss.px[0] = boss.px[1] = boss.py[0] = boss.py[1] = 0;
        boss.warn = 70;                     /* WARNING banner before the entrance */
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
    e->vy = 1 + (wave > 6 ? 1 : 0) + (wave > 24 ? 1 : 0) + (g_diff == DIF_HARD ? 1 : 0);
    e->firecd = rrange(40, 90) + diff_enemy_fire_adjust();
    {
        i16 elite_chance = (wave >= 20 ? 22 : wave >= 13 ? 18 : 12);
        if (g_diff == DIF_EASY) elite_chance -= 4;
        if (g_diff == DIF_HARD) elite_chance += 5;
        e->elite = (wave >= 7 && (rnd() % 100) < elite_chance) ? 1 : 0;
    }
    e->mode = 0; e->drop_class = DROP_NORMAL; e->aux = 0;
    if (type == E_SCOUT)   { e->hp = 1; e->vy += 1; }
    if (type == E_WEAVER)  { e->hp = 2; }
    if (type == E_SHOOTER) { e->hp = 3; if (rnd() % 100 < 35) { e->mode = 1; e->aux = (rnd() & 1) ? 1 : -1; } }
    if (e->elite) {
        if (type == E_SCOUT) { e->hp = 2; e->vy++; }
        else if (type == E_WEAVER) e->hp = 3;
        else e->hp = 4;
    }
}

static Enemy *spawn_one(u8 type, i16 x, i16 y, u8 mode)
{
    int i;
    if (x < 4) x = 4;
    if (x > SCRW - SH_EN_W - 4) x = SCRW - SH_EN_W - 4;
    for (i = 0; i < MAX_ENEMY; i++) if (!enemy[i].active) {
        init_enemy(&enemy[i], type, x);
        enemy[i].y = y;
        enemy[i].mode = mode;
        return &enemy[i];
    }
    return 0;
}

static i16 free_enemy_slots(void)
{
    int i; i16 n = 0;
    for (i = 0; i < MAX_ENEMY; i++) if (!enemy[i].active) n++;
    return n;
}

static i16 active_enemy_count(void)
{
    int i; i16 n = 0;
    for (i = 0; i < MAX_ENEMY; i++) if (enemy[i].active) n++;
    return n;
}

static void summon_escort(void)
{
    int n;
    i16 size = (i16)(2 + (boss.phase >= 1 ? 1 : 0));
    u8 type = (boss.kind == 3 || boss.kind == 11) ? E_WEAVER : E_SCOUT;
    i16 x0 = (boss.x < 80) ? boss.x + 36 : boss.x - 28;
    if (active_enemy_count() >= 6 || free_enemy_slots() < size) return;
    for (n = 0; n < size; n++) {
        Enemy *e = spawn_one(type, x0 + n * 20, -SH_EN_H - n * 12, 0);
        if (e) e->drop_class = (n == 0) ? DROP_SUPPLY : DROP_SUPPORT;
    }
    set_msg("SUPPLY ESCORTS");
}

/* carrier: launch a fighter squad from the two hangar bays, but only if the
   shared enemy[] pool has room for the whole squad (never partial/overflow). */
static void launch_squad(void)
{
    i16 size = (i16)(3 + (boss.phase >= 1 ? 1 : 0));
    i16 lx = boss.x + 6;
    i16 rx = boss.x + boss.w - 6 - SH_EN_W;
    int n;
    if (active_enemy_count() >= 6 || free_enemy_slots() < size) return;
    for (n = 0; n < size; n++) {
        i16 bx = (n & 1) ? rx : lx;
        Enemy *e = spawn_one(E_SCOUT, bx, -SH_EN_H - n * 10, 0);
        if (e) e->drop_class = (n == 0) ? DROP_SUPPLY : DROP_SUPPORT;
    }
    boss.squads++;
    set_msg("SUPPLY FIGHTERS");
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
        /* pick a drip x that doesn't drop straight onto a fresh spawn */
        i16 x = rrange(8, SCRW - SH_EN_W - 8);
        int tries, j;
        for (tries = 0; tries < 4; tries++) {
            bool clear = TRUE;
            for (j = 0; j < MAX_ENEMY; j++) if (enemy[j].active && enemy[j].y < 30) {
                i16 d = (i16)(x - enemy[j].x);
                if (d > -(SH_EN_W + 2) && d < SH_EN_W + 2) { clear = FALSE; break; }
            }
            if (clear) break;
            x = rrange(8, SCRW - SH_EN_W - 8);
        }
        init_enemy(&enemy[i], pick_type(), x);
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

static void update_player_facing(void)
{
    i16 py, by;
    if (!boss.active || boss.entering || boss.die_t > 0) {
        player.facing_down = FALSE;
        return;
    }
    py = (i16)(player.y + SH_SHIP_H / 2);
    by = (i16)(boss.y + boss.h / 2);
    if (!player.facing_down && py <= by - 6) player.facing_down = TRUE;
    else if (player.facing_down && py >= by + 6) player.facing_down = FALSE;
}

/* fire pattern scales with gun level; weapon type sets the projectile feel */
static void player_fire(void)
{
    i16 cx = player.x + SH_SHIP_W / 2 - SH_PB_W / 2;
    i16 dir = player.facing_down ? 1 : -1;
    i16 cy = player.facing_down ? player.y + SH_SHIP_H : player.y - 4;
    i16 g  = player.gun;
    i16 cd;
    switch (player.wtype) {
    case WT_LASER:                                /* fast, straight, pierces */
        switch (g) {
        case 1:  add_pbullet(cx, cy + dir * 2, 0, dir * 12, WT_LASER); break;
        case 2:  add_pbullet(cx - 4, cy, 0, dir * 12, WT_LASER);
                 add_pbullet(cx + 4, cy, 0, dir * 12, WT_LASER); break;
        case 3:  add_pbullet(cx - 7, cy, 0, dir * 12, WT_LASER);
                 add_pbullet(cx, cy + dir * 2, 0, dir * 12, WT_LASER);
                 add_pbullet(cx + 7, cy, 0, dir * 12, WT_LASER); break;
        default: add_pbullet(cx - 10, cy, 0, dir * 12, WT_LASER);
                 add_pbullet(cx - 4, cy + dir * 2, 0, dir * 12, WT_LASER);
                 add_pbullet(cx + 4, cy + dir * 2, 0, dir * 12, WT_LASER);
                 add_pbullet(cx + 10, cy, 0, dir * 12, WT_LASER); break;
        }
        cd = 7;
        break;
    case WT_WAVE: {
        i16 spread = g + 1, k;
        if (player.wave_boost > 0) {              /* boosted: piercing beams + centre lane */
            for (k = -spread; k <= spread; k++)
                add_pbullet(cx + k * 2, cy + dir * 2, k, dir * 8, WT_LASER);
            add_pbullet(cx, cy + dir * 4, 0, dir * 11, WT_LASER); /* fast centre lane */
            cd = 11;
        } else {                                  /* base: narrower slow arc */
            for (k = -spread; k <= spread; k++)
                add_pbullet(cx + k * 2, cy, k, dir * 5, WT_WAVE);
            cd = 15;
        }
        break; }
    default:                                      /* WT_CANNON */
        switch (g) {
        case 1:  add_pbullet(cx - 3, cy, 0, dir * 7, 0); add_pbullet(cx + 3, cy, 0, dir * 7, 0); break;
        case 2:  add_pbullet(cx, cy + dir * 2, 0, dir * 7, 0);
                 add_pbullet(cx - 5, cy, 0, dir * 7, 0); add_pbullet(cx + 5, cy, 0, dir * 7, 0); break;
        case 3:  add_pbullet(cx, cy + dir * 2, 0, dir * 7, 0);
                 add_pbullet(cx - 5, cy, -1, dir * 7, 0); add_pbullet(cx + 5, cy, 1, dir * 7, 0); break;
        default: add_pbullet(cx, cy + dir * 2, 0, dir * 7, 0);
                 add_pbullet(cx - 5, cy, 0, dir * 7, 0); add_pbullet(cx + 5, cy, 0, dir * 7, 0);
                 add_pbullet(cx - 7, cy - dir * 2, -2, dir * 7, 0); add_pbullet(cx + 7, cy - dir * 2, 2, dir * 7, 0); break;
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
        msl[i].y = player.facing_down ? player.y + SH_SHIP_H : player.y - SH_MSL_H;
        msl[i].dx = 0; msl[i].dy = player.facing_down ? 4 : -4;
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

/* Boss attacks are keyed by campaign boss id. Shared bullet helpers stay tiny,
   but each stage owns its own pattern shape so the authored campaign does not
   collapse back into a rotating archetype roster. */
static void boss_fire(void)
{
    i16 bx = boss.x + boss.w / 2, by = boss.y + boss.h - 6;
    i16 dir = (player.x + 8 > bx) ? 1 : -1;
    u8  hard = (g_diff == DIF_HARD);
    i16 k;
    switch (boss.kind) {
    case 0:                                   /* GORGON: horizontal lane walls */
        switch (boss.atk) {
        case 1:
            { i16 lo = (g_diff == DIF_EASY) ? -2 : (g_diff == DIF_HARD && boss.phase >= 1) ? -4 : -3;
              i16 hi = (i16)-lo;
              i16 gap = (i16)((player.x + 8 - bx) / 10);
              i16 gap_lo = (i16)(lo + 1), gap_hi = (i16)(hi - ((g_diff == DIF_EASY) ? 1 : 0));
              if (gap < gap_lo) gap = gap_lo;
              if (gap > gap_hi) gap = gap_hi;
              for (k = lo; k <= hi; k++) {
                  bool open = (k == gap || k == gap - 1);
                  if (g_diff == DIF_EASY && k == gap + 1) open = TRUE;
                  if (!open) add_ebullet((i16)(bx + k * 10), by, 0, 3);
              }
              if (g_diff == DIF_HARD && boss.phase >= 2)
                  add_ebullet(bx, by, dir, 5); }
            break;
        case 2:
            for (k = -4; k <= 4; k += 2) add_ebullet(bx + k * 7, by, k / 2, 3);
            if (boss.phase >= 2) add_ebullet(bx, by, 0, 5);
            break;
        default:
            { i16 lo = (g_diff == DIF_EASY) ? -2 : (g_diff == DIF_HARD && boss.phase >= 1) ? -4 : -3;
              for (k = lo; k <= -lo; k++)
                  add_ebullet((i16)(bx + k * 10), by, 0, (k & 1) ? 4 : 3); }
            break;
        }
        break;
    case 1:                                   /* REAPER: diving dagger lances */
        switch (boss.atk) {
        case 1:
            add_ebullet(bx - 6, by, -1, 5); add_ebullet(bx + 6, by, 1, 5);
            if (boss.phase >= 1) add_ebullet(bx, by, dir, 6);
            break;
        case 2:
            add_ebullet(bx - 2, by, dir, 6); add_ebullet(bx + 2, by, dir, 6);
            break;
        default:
            add_ebullet(bx, by, dir * 2, 5);
            add_ebullet(bx, by, dir, 6);
            add_ebullet(bx, by, 0, 7);
            break;
        }
        break;
    case 2:                                   /* LEVIATHAN: carrier suppression */
        if (boss.atk == 1) {
            add_ebullet(boss.x + 14, by, dir, 3);
            add_ebullet(boss.x + boss.w - 14, by, dir, 3);
        } else {
            add_ebullet(bx, by, 0, 3);
            if (boss.phase >= 1) { add_ebullet(bx - 16, by, -1, 3); add_ebullet(bx + 16, by, 1, 3); }
        }
        break;
    case 3:                                   /* SEEKER: rings and orbit spokes */
        switch (boss.atk) {
        case 1:
            for (k = 0; k < (boss.phase >= 1 ? 4 : 3); k++) {
                i16 a = (i16)((boss.spin + k * 16) & 63);
                add_ebullet(bx, by, sintab[a] / 12, (i16)(3 + (sintab[(a + 16) & 63] > 0)));
            }
            boss.spin = (i16)((boss.spin + 5) & 63);
            break;
        case 2:
            for (k = hard ? -3 : -2; k <= (hard ? 3 : 2); k++) add_ebullet(bx, by, k, 3);
            break;
        default:
            add_ebullet(bx - 12, by, boss.dir, 4); add_ebullet(bx + 12, by, boss.dir, 4);
            break;
        }
        break;
    case 4:                                   /* MANTIS: side pincers, center gap */
        switch (boss.atk) {
        case 1:
            for (k = 0; k < 3; k++) {
                add_ebullet(boss.x + 6, by - k * 3, 2, 3);
                add_ebullet(boss.x + boss.w - 6, by - k * 3, -2, 3);
            }
            break;
        case 2:
            add_ebullet(boss.x + 4, by, 1, 5); add_ebullet(boss.x + boss.w - 4, by, -1, 5);
            if (boss.phase >= 2) { add_ebullet(bx - 12, by, -1, 4); add_ebullet(bx + 12, by, 1, 4); }
            break;
        default:
            add_ebullet(boss.x + 3, by, 2, 4); add_ebullet(boss.x + boss.w - 3, by, -2, 4);
            add_ebullet(bx - 18, by, 1, 3); add_ebullet(bx + 18, by, -1, 3);
            break;
        }
        break;
    case 5:                                   /* ANVIL: bullet shutters */
        switch (boss.atk) {
        case 1:
            for (k = -3; k <= 3; k++) if (((boss.t / 20 + k) & 1) == 0) add_ebullet(bx + k * 9, by, 0, 4);
            break;
        case 2:
            add_ebullet(bx - 18, by, -1, 3); add_ebullet(bx + 18, by, 1, 3);
            add_ebullet(bx - 6, by, 0, 5); add_ebullet(bx + 6, by, 0, 5);
            break;
        default:
            for (k = -2; k <= 2; k++) add_ebullet(bx + k * 12, by, 0, 3);
            break;
        }
        break;
    case 6:                                   /* SERAPH: sweeping wing arcs */
        switch (boss.atk) {
        case 1:
            for (k = 0; k < 4; k++) { add_ebullet(boss.x + 4 + k * 5, by - 8, -1, 3 + k / 2); add_ebullet(boss.x + boss.w - 4 - k * 5, by - 8, 1, 3 + k / 2); }
            break;
        case 2:
            for (k = -3; k <= 3; k += 2) add_ebullet(bx, by, k, 4);
            break;
        default:
            add_ebullet(bx - 16, by, -2, 3); add_ebullet(bx + 16, by, 2, 3); add_ebullet(bx, by, 0, 5);
            break;
        }
        break;
    case 7: {                                 /* NEXUS: the orbiting pods own the crossfire */
        i16 p0x = boss.px[0], p0y = boss.py[0];
        i16 p1x = boss.px[1], p1y = boss.py[1];
        switch (boss.atk) {
        case 1:
            add_ebullet(p0x, p0y, 2, 3); add_ebullet(p1x, p1y, -2, 3);
            add_ebullet(p0x, p0y, 1, 4); add_ebullet(p1x, p1y, -1, 4);
            break;
        case 2:
            add_ebullet(p0x, p0y, dir, 4); add_ebullet(p1x, p1y, dir, 4);
            if (boss.phase >= 1) add_ebullet(bx, by, 0, 5);
            break;
        default:
            add_ebullet(p0x, p0y, 0, 4); add_ebullet(p1x, p1y, 0, 4);
            add_ebullet(bx, by - 5, dir, 3);
            break;
        }
        break; }
    case 8:                                   /* KRAKEN: appendage drips */
        if (boss.atk == 1) {
            for (k = 0; k < 4; k++) add_ebullet(boss.x + 8 + k * 14, by - (k & 1) * 6, (k & 1) ? 1 : -1, 3);
        } else {
            add_ebullet(bx - 12, by, -1, 3); add_ebullet(bx + 12, by, 1, 3);
            if (boss.phase >= 1) add_ebullet(bx, by, dir, 4);
        }
        break;
    case 9:                                   /* PHANTOM: delayed ghost splits */
        switch (boss.atk) {
        case 1:
            add_ebullet(bx - 10, by - 4, -1, 4); add_ebullet(bx + 10, by - 4, 1, 4);
            add_ebullet(bx, by, dir, 5);
            break;
        case 2:
            for (k = -2; k <= 2; k += 2) add_ebullet(bx + k * 7, by, k, 4);
            break;
        default:
            add_ebullet(bx, by, dir * 2, 5); add_ebullet(bx, by, -dir * 2, 5);
            break;
        }
        break;
    case 10:                                  /* CITADEL: segmented turrets */
        switch (boss.atk) {
        case 1:
            for (k = 0; k < 5; k++) if (((boss.t / 18 + k) % 3) != 1) add_ebullet(boss.x + 9 + k * 11, by, 0, 3);
            break;
        case 2:
            add_ebullet(boss.x + 12, by, -1, 4); add_ebullet(boss.x + boss.w - 12, by, 1, 4);
            add_ebullet(bx, by, dir, 4);
            break;
        default:                              /* turrets rake along the travel direction */
            for (k = -2; k <= 2; k++) add_ebullet(bx + k * 12, by, (i16)(boss.dir + k / 2), 3);
            break;
        }
        break;
    case 11:                                  /* VORTEX: orbit bullets and spirals */
        switch (boss.atk) {
        case 1:
            for (k = 0; k < 4; k++) {
                i16 a = (i16)((boss.spin + k * 16) & 63);
                add_ebullet(bx, by, sintab[a] / 11, (i16)(2 + (sintab[(a + 16) & 63] > 0 ? 2 : 1)));
            }
            boss.spin = (i16)((boss.spin + 9) & 63);
            break;
        case 2:
            add_ebullet(bx - 14, by, -2, 3); add_ebullet(bx + 14, by, 2, 3); add_ebullet(bx, by, 0, 5);
            break;
        default:
            for (k = -3; k <= 3; k += 2) add_ebullet(bx, by, k, 3);
            break;
        }
        break;
    case 12:                                  /* BASILISK: eye-beam corridors */
        switch (boss.atk) {
        case 1:
            for (k = -3; k <= 3; k++) if (k != 0) add_ebullet(bx + k * 8, by, 0, 4);
            break;
        case 2:
            add_ebullet(bx - 4, by, 0, 6); add_ebullet(bx, by, 0, 6); add_ebullet(bx + 4, by, 0, 6);
            if (boss.phase >= 2) { add_ebullet(bx - 18, by, -1, 4); add_ebullet(bx + 18, by, 1, 4); }
            break;
        default:
            add_ebullet(bx, by, dir, 5); add_ebullet(bx, by, dir * 2, 4);
            break;
        }
        break;
    case 13:                                  /* TITAN: heavy dreadnought volleys */
        switch (boss.atk) {
        case 1:
            for (k = -4; k <= 4; k += 2) add_ebullet(bx + k * 7, by, k / 2, 4);
            break;
        case 2:
            add_ebullet(boss.x + 8, by, 1, 4); add_ebullet(boss.x + boss.w - 8, by, -1, 4);
            add_ebullet(bx - 8, by, 0, 5); add_ebullet(bx + 8, by, 0, 5);
            break;
        default:
            for (k = -3; k <= 3; k++) add_ebullet(bx + k * 9, by, 0, (k & 1) ? 3 : 5);
            break;
        }
        break;
    default:                                  /* OVERLORD: campaign finale */
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
    }
    snd_sfx(SFX_HIT);
}

static void move_toward(i16 *v, i16 target, i16 step)
{
    if (*v < target) { *v += step; if (*v > target) *v = target; }
    else if (*v > target) { *v -= step; if (*v < target) *v = target; }
}

static i16 danger_speed(void) { return (g_diff == DIF_EASY) ? 3 : 4; }

/* Per-boss bottom of the movement envelope. Divers and slammers are allowed
   deep into the dodge zone (with telegraphs); hoverers stay high. Never above
   156 so the bottom rows always stay survivable. */
static i16 boss_max_y(void)
{
    static const i16 maxy[NBOSS] = {
        126, 150,  40,  96, 150, 140, 110, 100, 116, 110,
        100, 120, 150, 126, 150
    };
    return maxy[boss.kind];
}

/* Campaign bosses each own a movement band and motion language. Every move
   that invades the player's zone is telegraphed via boss.charge first.
   The clamps at the end only keep the authored movement inside the field. */
static void boss_move(void)
{
    i16 cx = (i16)(SCRW / 2 - boss.w / 2);
    i16 k;
    switch (boss.kind) {
    case 0:                                   /* GORGON: wall crawl + rampart slam */
        switch (boss.ty) {
        case 1:                               /* telegraph: dig in */
            if (--boss.dive_t <= 0) boss.ty = 2;
            break;
        case 2:                               /* slam down */
            boss.y += 4;
            if (boss.y >= ((boss.phase >= 1) ? 126 : 118)) { boss.ty = 3; boss.dive_t = 40; }
            break;
        case 3:                               /* grind sideways at depth */
            boss.x += boss.dir * 2;
            if (--boss.dive_t <= 0) boss.ty = 4;
            break;
        case 4:                               /* winch back up */
            boss.y -= 1;
            if (boss.y <= 84) { boss.y = 84; boss.ty = 0;
                boss.mv_t = (i16)((boss.phase >= 2) ? 110 : 160); }
            break;
        default:                              /* rampart crawl */
            boss.x += boss.dir * (1 + (boss.phase >= 2 ? 1 : 0));
            boss.y = (i16)(84 + ((boss.t >> 5) & 1) * 3);
            if (--boss.mv_t <= 0) {
                boss.ty = 1;
                boss.dive_t = (g_diff == DIF_EASY) ? 34 : (g_diff == DIF_HARD) ? 22 : 28;
                boss.charge = boss.dive_t;
            }
            break;
        }
        break;
    case 1:                                   /* REAPER: dives through the player band */
        if (boss.ty == 0) {                   /* strafe high, pick a mark */
            boss.y = (i16)(18 + (sintab[boss.t & 63] + 46) / 30);
            move_toward(&boss.x, boss.tx, 2);
            if (--boss.mv_t <= 0) {
                boss.ty = 1; boss.charge = 24; boss.dive_t = 24;
                boss.tx = (i16)(player.x + 8 - boss.w / 2);
                boss.py[0] = (i16)((boss.phase >= 2) ? 2 : 1);   /* chained dives */
            }
        } else if (boss.ty == 1) {            /* hold on the marked dive lane */
            if (--boss.dive_t <= 0) boss.ty = 2;
        } else if (boss.ty == 2) {            /* dive deep, afterimage trail */
            move_toward(&boss.x, boss.tx, danger_speed());
            boss.y += danger_speed();
            if (frame & 1) spawn_part((i16)(boss.x + boss.w / 2), boss.y, C_LRED);
            if (boss.y >= 150) boss.ty = 3;
        } else {                              /* strafing arc back up */
            boss.x += sintab[(boss.t * 2) & 63] / 12;
            boss.y -= 4;
            if (boss.y <= 18) {
                boss.y = 18;
                if (--boss.py[0] > 0) {       /* enraged: chain one warned dive */
                    boss.ty = 1; boss.charge = 24; boss.dive_t = 24;
                    boss.tx = (i16)(player.x + 8 - boss.w / 2);
                } else {
                    boss.ty = 0; boss.mv_t = (i16)(44 - boss.phase * 10);
                    boss.tx = rrange(8, (i16)(SCRW - boss.w - 8));
                }
            }
        }
        break;
    case 2:                                   /* LEVIATHAN: hover + bombing trawl */
        if (boss.ty == 0) {                   /* high hover between runs */
            boss.y = (i16)(8 + (sintab[boss.t & 63] + 46) / 40);
            move_toward(&boss.x, boss.tx, 1);
            if ((boss.t & 63) == 0) boss.tx = rrange(8, (i16)(SCRW - boss.w - 8));
            if (--boss.mv_t <= 0) {
                boss.ty = 1; boss.dive_t = 24; boss.charge = 24;
                boss.dir = (boss.x < cx) ? 1 : -1;   /* trawl toward the far wall */
            }
        } else if (boss.ty == 1) {            /* engines spool up */
            if (--boss.dive_t <= 0) boss.ty = 2;
        } else {                              /* full-width trawl, bombs from both bays */
            boss.x += boss.dir * (3 + (boss.phase >= 1 ? 1 : 0));
            boss.y = 10;
            if ((boss.t & ((boss.phase >= 2) ? 7 : 15)) == 0) {
                add_ebullet((i16)(boss.x + 10), (i16)(boss.y + boss.h - 4), 0, 3);
                add_ebullet((i16)(boss.x + boss.w - 14), (i16)(boss.y + boss.h - 4), 0, 3);
            }
            if (boss.x <= 8 || boss.x >= SCRW - boss.w - 8) {
                boss.ty = 0; boss.mv_t = 200; boss.tx = boss.x;
            }
        }
        if (--boss.launch_t <= 0) {
            launch_squad();
            boss.launch_t = (i16)(180 - boss.phase * 40);
        }
        break;
    case 3: {                                 /* SEEKER: circles the player, closing in */
        i16 mul = (i16)(3 - boss.phase);      /* orbit tightens per phase */
        move_toward(&boss.tx, (i16)(player.x + 8 - boss.w / 2), 1);
        boss.x = (i16)(boss.tx + sintab[(boss.t * 2) & 63] * mul / 2);
        boss.y = (i16)(46 + (sintab[(boss.t * 2 + 16) & 63] * mul) / 3);
        boss.dir = (sintab[(boss.t * 2 + 16) & 63] >= 0) ? 1 : -1;
        break; }
    case 4:                                   /* MANTIS: wall-cling + cross-screen lunge */
        if (boss.ty == 0) {                   /* slide along the wall */
            move_toward(&boss.x, (boss.dir > 0) ? 6 : (i16)(SCRW - boss.w - 6), 3);
            boss.y = (i16)(60 + sintab[(boss.t * 2) & 63] / 2);
            if (--boss.mv_t <= 0) {
                boss.ty = 1; boss.dive_t = 24; boss.charge = 24;
                boss.py[0] = player.y;        /* lock the lunge lane */
                if (boss.py[0] > 150) boss.py[0] = 150;
                if (boss.py[0] < 20)  boss.py[0] = 20;
            }
        } else if (boss.ty == 1) {            /* line up with the player */
            move_toward(&boss.y, boss.py[0], 3);
            if (--boss.dive_t <= 0) { boss.ty = 2; boss.dive_t = 10; }
        } else {                              /* lunge across the screen */
            boss.x += boss.dir * 4;
            if (boss.phase >= 2 && boss.dive_t > 0 &&
                boss.x + boss.w / 2 >= player.x && boss.x + boss.w / 2 <= player.x + 16) {
                boss.x -= boss.dir * 4;       /* menacing mid-lunge pause */
                boss.dive_t--;
            }
            if (boss.x <= 6 || boss.x >= SCRW - boss.w - 6) {
                boss.dir = (i16)-boss.dir;    /* attach to the far wall */
                boss.ty = 0; boss.mv_t = (i16)(64 - boss.phase * 6);
            }
        }
        break;
    case 5:                                   /* ANVIL: hover, then floor crush */
        if (boss.ty == 0) {                   /* drift above the lane */
            boss.x = (i16)(cx + sintab[(boss.t >> 1) & 63] / 2);
            move_toward(&boss.y, 64, 1);
            if (--boss.mv_t <= 0) { boss.ty = 1; boss.dive_t = 22; boss.charge = 22; }
        } else if (boss.ty == 1) {            /* telegraph, edge over the player */
            move_toward(&boss.x, (i16)(player.x + 8 - boss.w / 2), 2);
            if (--boss.dive_t <= 0) { boss.ty = 2; boss.dive_t = 0; }
        } else if (boss.ty == 2) {            /* crush */
            boss.y += 4;
            if (boss.y >= 140) {
                boss.y = 140; shk = 10; snd_sfx(SFX_EXPLODE);
                for (k = 1; k <= 3; k++) {    /* horizontal shockwave along the floor */
                    add_ebullet((i16)(boss.x + boss.w / 2), (i16)(boss.y + boss.h - 6), k, 0);
                    add_ebullet((i16)(boss.x + boss.w / 2), (i16)(boss.y + boss.h - 6), (i16)-k, 0);
                }
                boss.ty = 3;
                boss.dive_t = (i16)((boss.phase >= 2) ? 1 : 0);   /* enraged: double crush */
            }
        } else {                              /* rise; maybe side-step + crush again */
            boss.y -= (boss.dive_t ? 3 : 1);
            if (boss.dive_t && boss.y <= 100) {
                boss.x += (player.x + 8 > boss.x + boss.w / 2) ? 40 : -40;
                boss.ty = 1; boss.dive_t = 20; boss.py[0] = player.y; boss.charge = 20;
            } else if (boss.y <= 64) {
                boss.y = 64; boss.ty = 0;
                boss.mv_t = (i16)(96 - boss.phase * 30);
            }
        }
        break;
    case 6: {                                 /* SERAPH: pendulum scythe sweep */
        i16 ph = (i16)((boss.phase >= 2) ? (boss.t + boss.t / 2) : boss.t);
        i16 s = sintab[ph & 63];
        i16 as = (s < 0) ? (i16)-s : s;
        boss.x = (i16)(cx + s * 7 / 4);
        boss.y = (i16)(20 + (46 - as) * 2 + ((boss.phase >= 2) ? 12 : 0));
        boss.dir = (sintab[(ph + 16) & 63] >= 0) ? 1 : -1;
        break; }
    case 7: {                                 /* NEXUS: anchored core, orbiting fire-pods */
        i16 pcx, pcy;
        if (--boss.mv_t <= 0) {
            boss.mv_t = 62;
            boss.ty = (i16)((boss.ty + 1) % 3);
            boss.tx = (boss.ty == 0) ? 22 : (boss.ty == 1) ? cx : (i16)(SCRW - boss.w - 22);
        }
        move_toward(&boss.x, boss.tx, 2);
        boss.y = (i16)(40 + sintab[(boss.t * 3) & 63] / 12);
        boss.spin = (i16)((boss.spin + 1) & 63);
        pcx = (i16)(boss.x + boss.w / 2);
        pcy = (i16)(boss.y + boss.h / 2);
        if (boss.phase >= 2) {                /* pods detach low and converge on the player */
            pcx = (i16)((pcx + player.x + 8) / 2);
            pcy += 30;
        }
        boss.px[0] = (i16)(pcx + sintab[boss.spin & 63] * 26 / 46);
        boss.py[0] = (i16)(pcy + sintab[(boss.spin + 16) & 63] * 12 / 46);
        boss.px[1] = (i16)(pcx - sintab[boss.spin & 63] * 26 / 46);
        boss.py[1] = (i16)(pcy - sintab[(boss.spin + 16) & 63] * 12 / 46);
        break; }
    case 8: {                                 /* KRAKEN: advancing wall, recoils on phase change */
        i16 depth = (i16)(88 + boss.phase * 8);
        boss.x = (i16)(cx + sintab[boss.t & 63]);
        if (boss.ty == 1) {                   /* recoil back to the top */
            boss.y -= 3;
            if (boss.y <= 14) { boss.y = 14; boss.ty = 0; }
        } else if ((boss.t & 3) == 0 && boss.y < depth) boss.y++;
        if (--boss.launch_t <= 0) {
            launch_squad();
            boss.launch_t = (i16)(205 - boss.phase * 36);
        }
        break; }
    case 9:                                   /* PHANTOM: true telegraphed teleports */
        if (boss.ty == 0) {                   /* materialised: drift + countdown */
            boss.x += sintab[(boss.t * 2) & 63] / 24;
            if (--boss.mv_t <= 0) { boss.ty = 1; boss.dive_t = 16; }
        } else if (boss.ty == 1) {            /* dematerialise (checkerboard mask) */
            if (--boss.dive_t <= 0) {
                if (boss.phase >= 2 && (++boss.px[1] % 3) == 0) {
                    boss.tx = (i16)(player.x + 8 - boss.w / 2);   /* land on the player */
                    boss.px[0] = (i16)(player.y - boss.h - 12);
                } else if (boss.phase >= 1) {                     /* biased near the player */
                    boss.tx = (i16)(player.x + 8 - boss.w / 2 + rrange(-40, 40));
                    boss.px[0] = rrange(22, 96);
                } else {
                    boss.tx = rrange(18, (i16)(SCRW - boss.w - 18));
                    boss.px[0] = rrange(22, 80);
                }
                if (boss.px[0] > 110) boss.px[0] = 110;
                if (boss.px[0] < 16)  boss.px[0] = 16;
                boss.x = boss.tx; boss.y = boss.px[0];            /* blink */
                boss.ty = 2; boss.dive_t = 8;
                snd_sfx(SFX_PHASE);
                for (k = 0; k < 8; k++)                           /* arrival burst */
                    add_ebullet((i16)(boss.x + boss.w / 2), (i16)(boss.y + boss.h / 2),
                                (i16)(sintab[(k * 8) & 63] / 14),
                                (i16)(2 + (sintab[(k * 8 + 16) & 63] > 0 ? 2 : 0)));
            }
        } else {                              /* re-materialise */
            if (--boss.dive_t <= 0) { boss.ty = 0; boss.mv_t = (i16)(64 - boss.phase * 8); }
        }
        break;
    case 10: {                                /* CITADEL: perimeter patrol, corner to corner */
        i16 x0 = (i16)(18 + boss.phase * 10), x1 = (i16)(SCRW - boss.w - 18 - boss.phase * 10);
        i16 y0 = (i16)(40 + boss.phase * 8),  y1 = 96;
        i16 spd = (i16)((boss.phase >= 2) ? 2 : 1);
        i16 gx = (boss.ty == 1 || boss.ty == 2) ? x1 : x0;   /* ty = corner index */
        i16 gy = (boss.ty >= 2) ? y1 : y0;
        move_toward(&boss.x, gx, spd);
        move_toward(&boss.y, gy, spd);
        if (boss.x == gx && boss.y == gy) boss.ty = (i16)((boss.ty + 1) & 3);
        boss.dir = (gx > boss.x) ? 1 : -1;    /* turrets rake the travel direction */
        break; }
    case 11: {                                /* VORTEX: breathing spiral over the player */
        i16 r = (i16)(14 + (sintab[(boss.t >> 1) & 63] + 46) / ((boss.phase >= 2) ? 3 : 4));
        i16 oi = (i16)(boss.t * ((boss.phase >= 2) ? 3 : 2));
        if (boss.phase >= 2 && r < 22) r = 22;              /* radius floor when enraged */
        move_toward(&boss.tx, (i16)(player.x + 8 - boss.w / 2), 1);
        boss.x = (i16)(boss.tx + (i16)((long)sintab[oi & 63] * r / 46));
        boss.y = (i16)(52 + (i16)((long)sintab[(oi + 16) & 63] * r / 60));
        boss.spin = (i16)((boss.spin + 2) & 63);
        break; }
    case 12:                                  /* BASILISK: stalks the column, guillotines it */
        if (boss.ty == 0) {                   /* track the player's x */
            boss.tx = (i16)(player.x + 8 - boss.w / 2);
            move_toward(&boss.x, boss.tx, (i16)(2 + (boss.phase >= 1 ? 1 : 0)));
            boss.y = (i16)(24 + (sintab[(boss.t + 8) & 63] + 46) / 18);
            if (--boss.mv_t <= 0) { boss.ty = 1; boss.dive_t = 24; boss.charge = 24; }
        } else if (boss.ty == 1) {            /* frozen eye telegraph */
            if (--boss.dive_t <= 0) boss.ty = 2;
        } else if (boss.ty == 2) {            /* guillotine drop */
            boss.y += 4;
            if (boss.y >= 150) boss.ty = 3;
        } else {                              /* drag back up, raking the corridor */
            boss.y -= 2;
            if ((boss.t & 7) == 0) {
                add_ebullet((i16)(boss.x + 2), (i16)(boss.y + boss.h / 2), -1, 3);
                add_ebullet((i16)(boss.x + boss.w - 6), (i16)(boss.y + boss.h / 2), 1, 3);
            }
            if (boss.y <= 26) {
                boss.ty = 0;
                boss.mv_t = (i16)((boss.phase >= 2) ? 90 : 150);
            }
        }
        break;
    case 13:                                  /* TITAN: quake slams; steamrolls when enraged */
        if (boss.ty == 0) {                   /* heavy crawl */
            boss.x += boss.dir;
            move_toward(&boss.y, 88, 1);
            if (--boss.mv_t <= 0) {
                boss.dive_t = 24; boss.charge = 24;
                if (boss.phase >= 2 && (boss.px[1] ^= 1) != 0) {
                    boss.ty = 3;              /* every other attack: steamroll */
                    boss.dir = (boss.x < cx) ? 1 : -1;
                } else boss.ty = 1;
            }
        } else if (boss.ty == 1) {            /* telegraph */
            if (--boss.dive_t <= 0) boss.ty = 2;
        } else if (boss.ty == 2) {            /* quake slam */
            boss.y += 4;
            if (boss.y >= 120) {
                boss.y = 120; shk = 16; snd_sfx(SFX_EXPLODE);
                for (k = -2; k <= 2; k++)     /* vertical bullet columns */
                    add_ebullet((i16)(boss.x + boss.w / 2 + k * 22),
                                (i16)(boss.y + boss.h - 4), 0, 4);
                boss.ty = 4;
            }
        } else if (boss.ty == 3) {            /* steamroll charge */
            if (boss.dive_t > 0) { boss.dive_t--; move_toward(&boss.y, 104, 2); }
            else {
                boss.x += boss.dir * 3;
                if (boss.x <= 4 || boss.x >= SCRW - boss.w - 4) boss.ty = 4;
            }
        } else {                              /* recover */
            boss.y -= 1;
            if (boss.y <= 88) { boss.y = 88; boss.ty = 0;
                boss.mv_t = (i16)(120 - boss.phase * 20); }
        }
        break;
    default:                                  /* OVERLORD: finale speaks every boss's language */
        if (boss.phase == 0) {                /* figure-eight */
            boss.x = (i16)(cx + sintab[(boss.t * 2) & 63] * 3 / 2);
            boss.y = (i16)(16 + (sintab[(boss.t * 3 + 16) & 63] + 46) / 4);
            if ((boss.t & 127) == 0) boss.charge = 18;
        } else if (boss.phase == 1) {         /* PHANTOM: telegraphed blinks */
            if (boss.ty == 0) {
                if (--boss.mv_t <= 0) { boss.ty = 1; boss.dive_t = 14; }
            } else if (boss.ty == 1) {
                if (--boss.dive_t <= 0) {
                    boss.x = rrange(18, (i16)(SCRW - boss.w - 18));
                    boss.y = rrange(16, 84);
                    boss.ty = 2; boss.dive_t = 8;
                    snd_sfx(SFX_PHASE);
                }
            } else if (--boss.dive_t <= 0) { boss.ty = 0; boss.mv_t = 52; }
        } else if (boss.ty == 2) {            /* visible hold on the dive lane */
            if (--boss.dive_t <= 0) boss.ty = 3;
        } else if (boss.ty == 3) {            /* REAPER: one dive per orbit cycle */
            move_toward(&boss.x, boss.tx, danger_speed());
            boss.y += danger_speed();
            if (frame & 1) spawn_part((i16)(boss.x + boss.w / 2), boss.y, C_LMAG);
            if (boss.y >= 150) boss.ty = 4;
        } else if (boss.ty == 4) {            /* retreat from the dive */
            boss.y -= 4;
            if (boss.y <= 20) { boss.ty = 0; boss.mv_t = 140; }
        } else {                              /* SEEKER: tightening player orbit */
            move_toward(&boss.tx, (i16)(player.x + 8 - boss.w / 2), 1);
            boss.x = (i16)(boss.tx + sintab[(boss.t * 2) & 63]);
            boss.y = (i16)(20 + (sintab[(boss.t * 2 + 16) & 63] + 46) / 2);
            if (--boss.mv_t <= 0) {
                boss.ty = 2; boss.dive_t = 24; boss.charge = 24;
                boss.tx = (i16)(player.x + 8 - boss.w / 2);
            }
        }
        boss.spin = (i16)((boss.spin + 1) & 63);   /* crown spokes always turn */
        break;
    }
    if (boss.x < 4) { boss.x = 4; boss.dir = 1; }
    if (boss.x > SCRW - boss.w - 4) { boss.x = (i16)(SCRW - boss.w - 4); boss.dir = -1; }
    if (boss.y < 6) boss.y = 6;
    if (boss.y > boss_max_y()) boss.y = boss_max_y();
}

/* nominal resting height per archetype, so the slide-in stops where the boss
   will actually hover instead of snapping after the entrance. */
static i16 boss_rest_y(void)
{
    switch (boss.kind) {
    case 0:  return 84;
    case 1:  return 18;
    case 2:  return 9;
    case 3:  return 46;
    case 4:  return 60;      /* wall-cling band */
    case 5:  return 64;
    case 6:  return 20;
    case 7:  return 40;
    case 8:  return 14;
    case 9:  return 28;
    case 10: return 40;      /* patrol-rectangle top */
    case 11: return 52;
    case 12: return 24;
    case 13: return 88;
    default: return 16;
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
    bool capped = FALSE;
    bool weapon_fx = FALSE;
    u8 weapon_col = C_WHITE;
    char pb[24];
    switch (type) {
        case PU_GUN:     if (player.gun < GUN_MAX) {
                             player.gun++;
                             sprintf(pb, "%s LEVEL %d", weapon_name(player.wtype), player.gun);
                             set_msg(pb); weapon_fx = TRUE;
                             weapon_col = (player.wtype == WT_LASER) ? C_LCYAN
                                        : (player.wtype == WT_WAVE) ? C_LMAG : C_YELLOW;
                         } else capped = TRUE; break;
        case PU_RAPID:   player.rapid  = 700; break;
        case PU_SHIELD:  player.shield = 350; break;   /* ~10 s of invulnerability at 35 FPS */
        case PU_LIFE:    if (player.lives < 9) player.lives++; else capped = TRUE; break;
        case PU_MISSILE: if (player.msl < 30) { player.msl += 4; if (player.msl > 30) player.msl = 30; }
                         else capped = TRUE; break;
        /* Laser while flying the Wave gun doesn't swap weapons - it super-
           charges the Wave for a while (piercing beams + a centre lane). */
        case PU_LASER:   if (player.wtype == WT_WAVE) {
                             player.wave_boost = 350; set_msg("WAVE PIERCE 10 SEC");
                         } else { player.wtype = WT_LASER; set_msg("LASER EQUIPPED"); }
                         weapon_fx = TRUE; weapon_col = C_LCYAN; break;
        case PU_WAVE:    player.wtype = WT_WAVE; set_msg("WAVE EQUIPPED");
                         weapon_fx = TRUE; weapon_col = C_LMAG; break;
        case PU_BOMB:    if (player.bombs < 10) player.bombs++; else capped = TRUE; break;
        case PU_SCORE:   score_add(500 + (u32)wave * 50UL); break;
    }
    if (capped) {
        score_add(300);
        strcpy(pb, "+300");
        spawn_popup(player.x, (i16)(player.y - 8), pb);
        if (type == PU_GUN) set_msg("WEAPON LEVEL MAX +300");
    }
    if (weapon_fx) burst((i16)(player.x + SH_SHIP_W / 2),
                         (i16)(player.y + SH_SHIP_H / 2), 12, C_WHITE, weapon_col);
    if (type == PU_LIFE || type == PU_SHIELD) snd_sfx(SFX_PICK1);
    else if (type == PU_SCORE) snd_sfx(SFX_COMBO);
    else snd_sfx(SFX_PICK2);
    score_add(50);
}

static u8 choose_powerup(void)
{
    i16 w[PU_COUNT], total = 0, pick;
    int i;
    w[PU_GUN] = (player.gun < GUN_MAX) ? 16 : 0;
    w[PU_RAPID] = (player.rapid > 350) ? 5 : 13;
    w[PU_SHIELD] = (player.shield > 0) ? 5 : 18;
    w[PU_LIFE] = (player.lives >= 9) ? 0 : (player.lives <= 2 ? 10 : 3);
    w[PU_MISSILE] = (player.msl <= 5) ? 24 : (player.msl > 20 ? 8 : 18);
    w[PU_LASER] = 10; w[PU_WAVE] = 10;
    w[PU_BOMB] = (player.bombs == 0) ? 12 : (player.bombs >= 5 ? 3 : 7);
    w[PU_SCORE] = 0;
    for (i = 0; i < PU_COUNT; i++) total += w[i];
    if (total <= 0) return PU_SCORE;
    pick = (i16)(rnd() % (u16)total);
    for (i = 0; i < PU_COUNT; i++) {
        if (pick < w[i]) return (u8)i;
        pick -= w[i];
    }
    return PU_SCORE;
}

static i16 enemy_drop_chance(const Enemy *e)
{
    if (boss.active && e->drop_class != DROP_NORMAL)
        return (g_diff == DIF_EASY) ? 55 : (g_diff == DIF_HARD) ? 45 : 50;
    return (g_diff == DIF_EASY) ? 18 : (g_diff == DIF_HARD) ? 12 : 15;
}

/* smart bomb: clear enemy fire and damage everything on screen */
static void apply_boss_damage(i16 dmg)
{
    if (!boss.active || boss.die_t > 0) return;
    boss.hp -= dmg;
    boss.recent_dmg += dmg;
    if (boss.recent_dmg > boss.maxhp) boss.recent_dmg = boss.maxhp;
    boss.hurt_t = 3;
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

static void shield_bounce(i16 ox, i16 oy)
{
    i16 px = (i16)(player.x + SH_SHIP_W / 2);
    i16 py = (i16)(player.y + SH_SHIP_H / 2);
    player.x += (px < ox) ? -12 : 12;
    player.y += (py < oy) ? -16 : 16;
    if (player.x < 0) player.x = 0;
    if (player.x > SCRW - SH_SHIP_W) player.x = SCRW - SH_SHIP_W;
    if (player.y < 8) player.y = 8;
    if (player.y > SCRH - SH_SHIP_H) player.y = SCRH - SH_SHIP_H;
    player.invuln = 12; player.ram_cd = 24;
    burst(px, py, 18, C_WHITE, C_LCYAN);
    flash = 3; shk = 10;
    snd_sfx(SFX_PHASE);
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
    u32 base = enemy_score(e), award;
    typeburst(cx, cy, e->type, e->elite);
    spawn_blast(cx, cy, 0);
    wave_kills++;
    player.combo++; player.combo_t = 130;
    if (player.combo > player.max_combo) player.max_combo = player.combo;
    tier_up = (combo_mult() > old_mult);
    award = score_scaled(base * combo_mult());
    score += award;
    if (tier_up) {                       /* combo milestone popup */
        char pb[8];
        sprintf(pb, "X%d", combo_mult());
        spawn_popup(cx - 8, (i16)(cy - 10), pb);
    } else if (e->elite) {               /* elite bounty popup */
        char pb[8];
        sprintf(pb, "%lu", award);
        spawn_popup(cx - 12, (i16)(cy - 10), pb);
    }
    if (!risk_spawned && player.combo >= 10) {
        drop_powerup(rrange(96, 212), rrange(38, 72), PU_SCORE);
        risk_spawned = 1;
    }
    {
        bool can_drop = !boss.active || boss.drop_budget > 0;
        bool guaranteed = boss.active && e->drop_class == DROP_SUPPLY;
        if (can_drop && (guaranteed || rnd() % 100 < enemy_drop_chance(e))) {
            if (drop_powerup(cx - SH_PU_W / 2, cy, choose_powerup()) && boss.active)
                boss.drop_budget--;
        }
    }
    e->active = FALSE;
    snd_sfx(tier_up ? SFX_COMBO : SFX_EXPLODE);
}

/* stage 1: start the chained death sequence (boss goes inert + invulnerable) */
static void boss_die(void)
{
    if (boss.die_t > 0) return;
    boss.hp = 0;
    boss.die_t = 45;
    boss.charge = 0;
    snd_sfx(SFX_EXPLODE);
}

/* stage 2: the final blast once the chained explosions finish */
static void boss_finish_death(void)
{
    u32 award = score_scaled(5000UL * (u32)combo_mult());
    char pb[12];
    fireburst(boss.x + boss.w / 2, boss.y + boss.h / 2, 60);
    spawn_blast(boss.x + boss.w / 2, boss.y + boss.h / 2, 3);
    score += award;
    sprintf(pb, "+%lu", award);
    spawn_popup((i16)(boss.x + boss.w / 2 - 20), boss.y, pb);
    flash = 8; shk = 24;
    bosses_defeated++;
    snd_music_game((u8)bosses_defeated);
    force_powerup(boss.x + 10, boss.y + 10,
                  (player.lives <= 3) ? PU_LIFE : (player.shield <= 0) ? PU_SHIELD
                  : (player.gun < GUN_MAX) ? PU_GUN : PU_SCORE);
    force_powerup(boss.x + boss.w - 22, boss.y + 10,
                  (player.bombs < 5) ? PU_BOMB : PU_SCORE);
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

/* One separation pass: push overlapping enemy pairs 1 px apart so formations
   and drips never sit stacked on the same cell. Weavers recompute x from
   base + sintab each frame, so their push must go through base. O(n^2) over
   at most 28 actives with a cheap y-band reject - trivial per frame. */
static void separate_enemies(void)
{
    int a, b;
    for (a = 0; a < MAX_ENEMY - 1; a++) {
        if (!enemy[a].active) continue;
        for (b = a + 1; b < MAX_ENEMY; b++) {
            i16 dy, dx;
            i16 *xl, *xh;
            bool moved = FALSE;
            if (!enemy[b].active) continue;
            dy = (i16)(enemy[a].y - enemy[b].y);
            if (dy >= 12 || dy <= -12) continue;
            dx = (i16)(enemy[a].x - enemy[b].x);
            if (dx >= 14 || dx <= -14) continue;
            if (dx < 0 || (dx == 0 && (a & 1))) {
                xl = (enemy[a].type == E_WEAVER) ? &enemy[a].base : &enemy[a].x;
                xh = (enemy[b].type == E_WEAVER) ? &enemy[b].base : &enemy[b].x;
            } else {
                xl = (enemy[b].type == E_WEAVER) ? &enemy[b].base : &enemy[b].x;
                xh = (enemy[a].type == E_WEAVER) ? &enemy[a].base : &enemy[a].x;
            }
            if (*xl > 4) { (*xl)--; moved = TRUE; }
            if (*xh < SCRW - SH_EN_W - 4) { (*xh)++; moved = TRUE; }
            if (!moved) enemy[b].y++;          /* both pinned: slip one downward */
        }
    }
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
        if (player.x < 0) player.x = 0;
        if (player.x > SCRW - SH_SHIP_W) player.x = SCRW - SH_SHIP_W;
        if (player.y < 8) player.y = 8;
        if (player.y > SCRH - SH_SHIP_H) player.y = SCRH - SH_SHIP_H;
        update_player_facing();
        if (player.boosting && (frame & 1)) {
            i16 fy = player.facing_down ? player.y - 1 : player.y + SH_SHIP_H + 1;
            spawn_part(player.x + 4, fy, 0);
            spawn_part(player.x + 12, fy, 0);
        }
        if (player.firecd > 0) player.firecd--;
        if (key_pressed(SC_SPACE) && player.firecd <= 0) player_fire();
        if (key_hit(SC_CTRL)) fire_missile();
        if (key_hit(SC_B)) smart_bomb();
        if (player.invuln > 0) player.invuln--;
        if (player.ram_cd > 0) player.ram_cd--;
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
        /* steer toward the nearest target ahead in the launch direction */
        for (j = 0; j < MAX_ENEMY; j++) if (enemy[j].active &&
            ((m->dy < 0 && enemy[j].y < m->y) || (m->dy > 0 && enemy[j].y > m->y))) {
            long dx = (long)(enemy[j].x + SH_EN_W / 2) - m->x;
            long dy = (long)(enemy[j].y + SH_EN_H / 2) - m->y;
            long d = dx * dx + dy * dy;
            if (d < best) { best = d; tx = enemy[j].x + SH_EN_W / 2; }
        }
        if (boss.active && ((m->dy < 0 && boss.y < m->y) ||
                           (m->dy > 0 && boss.y + boss.h > m->y)))
            tx = boss.x + boss.w / 2;
        if (tx >= 0) {
            if (tx > m->x + 2 && m->dx < 2)  m->dx++;
            if (tx < m->x - 2 && m->dx > -2) m->dx--;
        }
        m->x += m->dx; m->y += m->dy;
        spawn_part(m->x + SH_MSL_W / 2,
                   (m->dy < 0) ? m->y + SH_MSL_H : m->y - 1, 0);
        if (m->y < -SH_MSL_H || m->y > SCRH || m->x < -8 || m->x > SCRW) {
            m->active = FALSE; continue;
        }
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
        if (pbul[i].y < -8 || pbul[i].y > SCRH || pbul[i].x < -4 || pbul[i].x > SCRW)
            pbul[i].active = FALSE;
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
            if (shooter_lingering(e)) e->y -= e->vy;
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
        if (player.alive && overlap(e->x + 2, e->y + 2, SH_EN_W - 4, SH_EN_H - 4,
                                   player.x + 3, player.y + 3, SH_SHIP_W - 6, SH_SHIP_H - 6)) {
            if (player.shield > 0) { kill_enemy(e); continue; }
            if (player.invuln == 0) { kill_enemy(e); hurt_player(); continue; }
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

    /* ---- enemy separation: dissolve stacked ships (every other frame) ---- */
    if (frame & 1) separate_enemies();

    /* ---- popups ---- */
    for (i = 0; i < 4; i++) if (popup[i].active) {
        if (popup[i].t & 1) popup[i].y--;
        if (--popup[i].t <= 0) popup[i].active = FALSE;
    }

    /* ---- boss ---- */
    if (boss.active && boss.warn > 0) {
        boss.warn--;                        /* hold the entrance for the WARNING */
    } else if (boss.active && boss.die_t > 0) {
        /* staged death: inert + invulnerable, chained explosions ripple through */
        boss.die_t--;
        if ((boss.die_t % 6) == 0)
            fireburst((i16)(boss.x + 4 + (i16)(rnd() % (u16)(boss.w - 8))),
                      (i16)(boss.y + 2 + (i16)(rnd() % (u16)(boss.h - 4))), 10);
        if ((boss.die_t % 9) == 0) snd_sfx(SFX_EXPLODE);
        if (shk < 3) shk = 3;
        if (boss.die_t == 0) boss_finish_death();
    } else if (boss.active) {
        if (boss.entering) {
            i16 ry = boss_rest_y();
            boss.y += 3;
            if (boss.y >= ry) { boss.y = ry; boss.entering = FALSE; }
        } else {
            boss_move();
            if (!((boss.kind == 9 && (boss.ty == 1 || boss.ty == 2)) ||
                  (boss.kind == 14 && boss.phase == 1 && (boss.ty == 1 || boss.ty == 2)))) {
                if (boss.firecd == 18) { boss.charge = 18; snd_sfx(SFX_PHASE); }
                if (--boss.firecd <= 0) { boss_fire(); boss.firecd = diff_boss_fire_cd(); }
            }
        }
        boss.t++;
        if (boss.charge > 0) boss.charge--;
        if (boss.hurt_t > 0) boss.hurt_t--;
        if (boss.recent_dmg > 0 && (frame & 1)) boss.recent_dmg--;
        if (!boss.entering && boss.kind != 2 && boss.kind != 8 && --boss.support_t <= 0) {
            summon_escort();
            boss.support_t = (boss.phase == 0) ? 260 : (boss.phase == 1) ? 220 : 190;
        }
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
            if ((boss.kind == 2 || boss.kind == 8)
                && boss.phase > 0 && !(boss.summons & (1 << boss.phase))) {
                boss.summons |= (u8)(1 << boss.phase);
                summon_escort();
            }
            if (boss.kind == 8) boss.ty = 1;               /* KRAKEN: recoil to the top */
            if (boss.kind == 14) { boss.ty = 0; boss.mv_t = 90; }  /* OVERLORD: new language */
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
                boss.recent_dmg += pbul[j].dmg;
                if (boss.recent_dmg > boss.maxhp) boss.recent_dmg = boss.maxhp;
                boss.hurt_t = 2;
                if (boss.hp <= 0) { boss_die(); break; }
            }
        }
        /* boss body vs player */
        if (boss.active && boss.die_t == 0 && player.alive &&
            overlap(boss.x + 2, boss.y, (i16)(boss.w - 4), boss.h,
                    player.x + 3, player.y + 3, SH_SHIP_W - 6, SH_SHIP_H - 6)) {
            if (player.shield > 0 && player.ram_cd == 0) {
                apply_boss_damage(boss_pct_damage(10));
                shield_bounce((i16)(boss.x + boss.w / 2), (i16)(boss.y + boss.h / 2));
            } else if (player.shield <= 0 && player.invuln == 0) hurt_player();
        }
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
static const char *BOSSNAME[NBOSS] = {
    "GORGON", "REAPER", "LEVIATHAN", "SEEKER", "MANTIS",
    "ANVIL", "SERAPH", "NEXUS", "KRAKEN", "PHANTOM",
    "CITADEL", "VORTEX", "BASILISK", "TITAN", "OVERLORD"
};

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
    if (player.shield > 0)                    /* red once it's about to expire */
        text_draw(268, 190, "S", (player.shield < 70) ? C_LRED : C_LBLUE);
    /* boss hp bar */
    if (boss.active && !boss.entering) {
        i16 w = (i16)((long)boss.hp * 200 / boss.maxhp);
        u8 c = (boss.phase >= 2) ? C_LRED : (boss.phase >= 1) ? C_YELLOW : C_LGREEN;
        vga_frame(59, 12, 202, 6, C_DGRAY);
        vga_rect(60, 13, w, 4, c);
        if (boss.recent_dmg > 0) {
            i16 rw = (i16)((long)boss.recent_dmg * 200 / boss.maxhp);
            if (rw < 1) rw = 1;
            if (w + rw > 200) rw = (i16)(200 - w);
            if (rw > 0) vga_rect((i16)(60 + w), 13, rw, 4, C_WHITE);
        }
        vga_rect(126, 12, 1, 6, C_DGRAY);
        vga_rect(193, 12, 1, 6, C_DGRAY);
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

/* boss drawn through a shifting checkerboard: reads as dematerialising */
static void draw_boss_masked(void)
{
    i16 x, y;
    const u8 *s = spr_boss[boss.spr];
    for (y = 0; y < boss.h; y++)
        for (x = 0; x < boss.w; x++) {
            u8 c = s[y * boss.w + x];
            if (!c) continue;
            if (((x ^ y) ^ (i16)(frame >> 1)) & 1) continue;
            vga_pixel((i16)(boss.x + x + shx), (i16)(boss.y + y + shy), c);
        }
}

/* per-boss animated overlays: the "alive" layer on top of the static bitmap */
static void draw_boss_extras(i16 cx, i16 cy)
{
    i16 k, s;
    switch (boss.kind) {
    case 3: case 12: {                        /* SEEKER / BASILISK: pupil tracks you */
        i16 ex = cx, ey = (boss.kind == 3) ? cy : (i16)(boss.y + 13 + shy);
        i16 dx = (i16)((player.x + 8 - (boss.x + boss.w / 2)) / 24);
        i16 dy = (i16)((player.y + 8 - (boss.y + boss.h / 2)) / 40);
        if (dx > 2) dx = 2; if (dx < -2) dx = -2;
        if (dy > 2) dy = 2; if (dy < -2) dy = -2;
        if (boss.kind == 12 && boss.ty == 1) {         /* eyelid shuts: guillotine tell */
            vga_hline((i16)(ex - 6), ey, 12, C_GREEN);
            vga_hline((i16)(ex - 6), (i16)(ey - 1), 12, C_GREEN);
        } else {
            vga_rect((i16)(ex + dx - 1), (i16)(ey + dy - 1), 2, 2, C_WHITE);
        }
        break; }
    case 4:                                   /* MANTIS: claw tips spark when lunging */
        if (boss.ty == 2) {
            u8 c = (frame & 2) ? C_WHITE : C_YELLOW;
            vga_pixel((i16)(boss.x + 21 + shx), (i16)(boss.y + 14 + shy), c);
            vga_pixel((i16)(boss.x + boss.w - 22 + shx), (i16)(boss.y + 14 + shy), c);
        }
        break;
    case 5:                                   /* ANVIL: pistons extend during the crush */
        if (boss.ty == 2 || boss.ty == 3) {
            for (k = 10; k <= boss.w - 12; k += 12) {
                vga_hline((i16)(boss.x + k + shx), (i16)(boss.y - 3 + shy), 2, C_LGRAY);
                vga_hline((i16)(boss.x + k + shx), (i16)(boss.y - 6 + shy), 2, C_DGRAY);
            }
        }
        break;
    case 7:                                   /* NEXUS: tethered orbiting fire-pods */
        for (k = 0; k < 2; k++) {
            i16 tx = boss.px[k], ty = boss.py[k];
            for (s = 1; s < 5; s++)           /* glow tether, core -> pod */
                vga_pixel((i16)(cx + (tx + shx - cx) * s / 5),
                          (i16)(cy + (ty + shy - cy) * s / 5),
                          (u8)(PAL_GLOW + 5 + s * 2));
            DS((i16)(tx - SH_POD_W / 2), (i16)(ty - SH_POD_H / 2),
               SH_POD_W, SH_POD_H, spr_bosspod);
        }
        break;
    case 8:                                   /* KRAKEN: five writhing tentacles */
        for (k = 0; k < 5; k++) {
            i16 ax = (i16)(boss.x + 8 + k * 12 + shx);
            i16 ay = (i16)(boss.y + boss.h - 2 + shy);
            for (s = 0; s < 8; s++) {
                i16 px = (i16)(ax + sintab[((i16)frame * 3 + k * 13 + s * 8) & 63] * s / 40);
                i16 py = (i16)(ay + s * 3);
                u8 c = (s < 5) ? C_GREEN : C_LGREEN;
                vga_pixel(px, py, c);
                vga_pixel((i16)(px + 1), py, c);
            }
        }
        break;
    case 10:                                  /* CITADEL: muzzle flash on the live turret */
        if (boss.firecd < 5) {
            i16 tx = (player.x + 8 > boss.x + boss.w / 2) ? (i16)(boss.w - 20) : 8;
            vga_rect((i16)(boss.x + tx + 4 + shx), (i16)(boss.y + 17 + shy), 3, 2,
                     (frame & 1) ? C_WHITE : C_YELLOW);
        }
        break;
    case 11:                                  /* VORTEX: eight orbiting singularity orbs */
        for (k = 0; k < 8; k++) {
            i16 a = (i16)((boss.spin + k * 8) & 63);
            i16 ox = (i16)(cx + sintab[a] * 24 / 46);
            i16 oy = (i16)(cy + sintab[(a + 16) & 63] * 18 / 46);
            vga_rect((i16)(ox - 1), (i16)(oy - 1), 2, 2, (k & 1) ? C_LMAG : C_LCYAN);
        }
        break;
    case 13:                                  /* TITAN: armour cracks open per phase */
        if (boss.phase >= 1) {
            vga_rect((i16)(boss.x + 12 + shx), (i16)(boss.y + 16 + shy), 8, 6, C_BLACK);
            vga_rect((i16)(boss.x + 14 + shx), (i16)(boss.y + 18 + shy), 4, 2,
                     (u8)(PAL_FIRE + 6 + ((frame >> 1) & 3)));
        }
        if (boss.phase >= 2) {
            vga_rect((i16)(boss.x + boss.w - 24 + shx), (i16)(boss.y + 28 + shy), 10, 6, C_BLACK);
            vga_rect((i16)(boss.x + boss.w - 21 + shx), (i16)(boss.y + 30 + shy), 4, 2,
                     (u8)(PAL_FIRE + 8 + ((frame >> 1) & 3)));
            vga_rect((i16)(boss.x + 30 + shx), (i16)(boss.y + 6 + shy), 6, 4, C_BLACK);
        }
        break;
    case 14:                                  /* OVERLORD: rotating crown spokes + aura */
        for (k = 0; k < 4; k++) {
            i16 a = (i16)((boss.spin + k * 16) & 63);
            for (s = 3; s < 6; s++)
                vga_pixel((i16)(cx + sintab[a] * s * 6 / 46),
                          (i16)(cy + sintab[(a + 16) & 63] * s * 5 / 46),
                          (s == 5) ? C_WHITE : C_LMAG);
        }
        if (boss.phase >= 2 && (frame & 2))
            vga_frame((i16)(boss.x - 3 + shx), (i16)(boss.y - 3 + shy),
                      (i16)(boss.w + 6), (i16)(boss.h + 6), C_LRED);
        break;
    default:
        break;
    }
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
            i16 ex, ey;
            draw_enemy_anim(&enemy[i]);
            DS(enemy[i].x, enemy[i].y, SH_EN_W, SH_EN_H, spr_enemy[estage][enemy[i].type]);
            draw_elite_overlay(&enemy[i]);
            if (enemy[i].drop_class == DROP_SUPPLY) {
                u8 c = (frame & 4) ? C_WHITE : C_LGREEN;
                ex = (i16)(enemy[i].x + SH_EN_W / 2 + shx);
                ey = (i16)(enemy[i].y - 4 + shy);
                vga_hline((i16)(ex - 2), ey, 5, c);
                vga_pixel(ex, (i16)(ey - 2), c);
                vga_pixel(ex, (i16)(ey + 2), c);
            }
        }
    }
    if (boss.active && boss.warn == 0) {
        i16 cx = (i16)(boss.x + boss.w / 2 + shx), cy = (i16)(boss.y + boss.h / 2 + shy);
        bool teleporting = (boss.kind == 9 && (boss.ty == 1 || boss.ty == 2) && !boss.entering)
                        || (boss.kind == 14 && boss.phase == 1
                            && (boss.ty == 1 || boss.ty == 2) && !boss.entering);
        if (teleporting) draw_boss_masked();   /* dematerialising checkerboard */
        else if (boss.die_t > 0) {             /* dying: flickering, breaking apart */
            if (boss.die_t & 2) DS(boss.x, boss.y, boss.w, boss.h, spr_boss[boss.spr]);
            else draw_boss_masked();
        } else DS(boss.x, boss.y, boss.w, boss.h, spr_boss[boss.spr]);
        draw_boss_extras(cx, cy);              /* per-boss animated overlays */
        if (boss.die_t == 0) {
            if (boss.phase >= 1 && boss.kind != 13) {   /* scorch (TITAN has armour-break) */
                vga_hline((i16)(cx - boss.w / 3), (i16)(cy - boss.h / 6), (i16)(2 * boss.w / 3), C_DGRAY);
                vga_hline((i16)(cx - boss.w / 4), (i16)(cy + boss.h / 4), (i16)(boss.w / 2), C_DGRAY);
            }
            if (boss.phase >= 2 && boss.kind != 13) {
                vga_frame((i16)(cx - 9), (i16)(cy - 6), 18, 12, C_LRED);
                vga_pixel((i16)(cx - boss.w / 3), cy, C_YELLOW);
                vga_pixel((i16)(cx + boss.w / 3), cy, C_YELLOW);
            }
            if (boss.hurt_t > 0)               /* white hit flash */
                vga_frame((i16)(boss.x - 1 + shx), (i16)(boss.y - 1 + shy),
                          (i16)(boss.w + 2), (i16)(boss.h + 2), C_WHITE);
            if (boss.charge > 0) vga_frame((i16)(boss.x - 2 + shx), (i16)(boss.y - 2 + shy),
                                           (i16)(boss.w + 4), (i16)(boss.h + 4),
                                           (boss.charge & 2) ? C_WHITE : C_LRED);
            if ((boss.kind == 1 && boss.ty == 1) || (boss.kind == 14 && boss.ty == 2)) {
                i16 mx = (i16)(boss.tx + boss.w / 2 + shx);
                vga_hline((i16)(mx - 7), (i16)(player.y + 7), 15,
                          (frame & 2) ? C_WHITE : C_LRED);
            }
            if (boss.kind == 9 && boss.ty == 2)
                vga_frame((i16)(boss.x - 3 + shx), (i16)(boss.y - 3 + shy),
                          (i16)(boss.w + 6), (i16)(boss.h + 6), C_LMAG);
            /* pulsing core overlay (faster when enraged) */
            vga_rect((i16)(cx - 3), (i16)(cy - 3), 6, 6,
                     (u8)(PAL_FIRE + 8 + ((frame >> (boss.phase >= 2 ? 0 : 1)) & 7)));
        }
    }
    for (i = 0; i < MAX_MISSILE; i++) if (msl[i].active)
        DS(msl[i].x, msl[i].y, SH_MSL_W, SH_MSL_H,
           (msl[i].dy > 0) ? spr_missile_down : spr_missile);
    for (i = 0; i < MAX_EBULLET; i++) if (ebul[i].active)
        DS(ebul[i].x, ebul[i].y, SH_EB_W, SH_EB_H, spr_ebullet);
    for (i = 0; i < MAX_PBULLET; i++) if (pbul[i].active)
        DS(pbul[i].x, pbul[i].y, SH_PB_W, SH_PB_H, spr_pbullet[pbul[i].kind]);
    /* player (blink while invulnerable) */
    if (player.alive && !(player.invuln > 0 && (frame & 2))) {
        /* shield ring; blinks off intermittently during the last ~2 s */
        if (player.shield > 0 && !(player.shield < 70 && (frame & 2)))
            vga_frame(player.x - 2 + shx, player.y - 2 + shy, SH_SHIP_W + 4, SH_SHIP_H + 4,
                      (frame & 2) ? (u8)(PAL_GLOW + 14) : (u8)(PAL_GLOW + 8));
        DS(player.x, player.y, SH_SHIP_W, SH_SHIP_H,
           player.facing_down ? spr_ship_down[ship_bank] : spr_ship[ship_bank]);
        /* animated engine flame */
        {
            u8 fc = (frame & 2) ? (u8)(PAL_FIRE + 12) : (u8)(PAL_FIRE + 8);
            i16 fx = player.x + shx;
            i16 fy = player.facing_down ? player.y - 1 + shy : player.y + SH_SHIP_H + shy;
            i16 tail = player.facing_down ? -1 : 1;
            vga_pixel(fx + 3, fy, fc);  vga_pixel(fx + 12, fy, fc);
            vga_pixel(fx + 3, fy + tail, (u8)(PAL_FIRE + 5));
            vga_pixel(fx + 12, fy + tail, (u8)(PAL_FIRE + 5));
            if (frame & 1) { vga_pixel(fx + 3, fy + tail * 2, (u8)(PAL_FIRE + 3));
                             vga_pixel(fx + 12, fy + tail * 2, (u8)(PAL_FIRE + 3)); }
            if (player.boosting) {
                vga_hline(fx + 1, fy + tail * 3, 5, (u8)(PAL_FIRE + 11));
                vga_hline(fx + 10, fy + tail * 3, 5, (u8)(PAL_FIRE + 11));
                vga_pixel(fx + 3, fy + tail * 4, (u8)(PAL_FIRE + 6));
                vga_pixel(fx + 12, fy + tail * 4, (u8)(PAL_FIRE + 6));
            }
        }
    }
    {
        int p;
        for (p = 0; p < 4; p++) if (popup[p].active)
            text_draw((i16)(popup[p].x + shx), (i16)(popup[p].y + shy), popup[p].txt,
                      (popup[p].t > 10) ? C_WHITE : C_LGRAY);
    }
    draw_hud();
    if (boss.active && boss.warn > 0) {          /* klaxon intro before the entrance */
        u8 bc = (boss.warn & 8) ? C_LRED : C_RED;
        vga_rect(0, 64, SCRW, 2, bc);
        vga_rect(0, 100, SCRW, 2, bc);
        if (boss.warn & 8) text_center(74, "! WARNING !", C_WHITE);
        text_center(88, BOSSNAME[boss.kind], C_YELLOW);
    } else if (wave_banner > 0) {
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
    text_center(34, "AYRIEN ASSAULT", C_YELLOW);
    text_center(50, "a vga space shooter", C_LGRAY);
    sprintf(b, "AUDIO: %s", snd_device_name());
    text_center(64, b, C_DGRAY);
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

static bool game_over_continue_requested(void)
{
    return (over_timer <= 130 && key_hit(SC_ENTER)) || over_timer == 0;
}

static void clear_combat_fx(void)
{
    flash = shk = shx = shy = 0;
}

static bool win_freeplay_requested(void)
{
    bool enter = key_hit(SC_ENTER);
    key_hit(SC_SPACE);                   /* held fire never skips victory */
    return (win_t >= WIN_INPUT_DELAY && enter) ? TRUE : FALSE;
}

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
    if (frame & 16) text_center(150, "FINALIZING RUN", C_LGRAY);
}

static void draw_win(void)
{
    char b[40];
    int i;
    i16 fly = (i16)((win_t * 2) % 390 - 30);
    /* Planet horizon and fleet flyby: all procedural, period-friendly pixels. */
    for (i = 0; i < 26; i++) {
        i16 half = (i16)(88 - i * i / 9);
        if (half > 0) vga_hline((i16)(160 - half), (i16)(174 + i), (i16)(half * 2),
                                (u8)(PAL_NEB + 5 + (i >> 2)));
    }
    for (i = 0; i < 8; i++) {
        i16 fx = (i16)(34 + ((win_t * (i + 3) + i * 57) % 252));
        i16 fy = (i16)(30 + ((i * 37) % 82));
        i16 r = (i16)((win_t * 2 + i * 9) & 15);
        if (r < 8) {
            u8 c = (i % 3 == 0) ? C_YELLOW : (i & 1) ? C_LCYAN : C_LMAG;
            vga_hline((i16)(fx - r), fy, (i16)(r * 2 + 1), c);
            vga_pixel(fx, (i16)(fy - r), c); vga_pixel(fx, (i16)(fy + r), c);
        }
    }
    for (i = 0; i < 18; i++) {
        i16 cx = (i16)((i * 53 + win_t * (1 + i % 3)) % SCRW);
        i16 cy = (i16)((i * 29 + win_t * 2) % 154);
        vga_rect(cx, cy, (i & 1) ? 2 : 1, 2,
                 (i % 3 == 0) ? C_YELLOW : (i & 1) ? C_LCYAN : C_LMAG);
    }
    vga_sprite(fly, 126, SH_SHIP_W, SH_SHIP_H, spr_ship[2]);
    vga_sprite((i16)(fly - 28), 138, SH_SHIP_W, SH_SHIP_H, spr_ship[1]);
    vga_sprite((i16)(fly - 56), 126, SH_SHIP_W, SH_SHIP_H, spr_ship[0]);
    for (i = 0; i < 8; i++) {
        i16 dx = (i16)(136 + ((i * 37 + win_t) % 50));
        i16 dy = (i16)(54 + ((i * 19 + win_t / 2) % 38));
        vga_rect(dx, dy, (i16)(2 + (i & 2)), 2, (i & 1) ? C_LRED : C_DGRAY);
    }
    vga_frame(50, 4, 220, 52, (frame & 4) ? C_YELLOW : C_WHITE);
    text_center(10, "*** VICTORY ***", (frame & 8) ? C_YELLOW : C_WHITE);
    text_center(26, "CAMPAIGN COMPLETE", C_YELLOW);
    text_center(42, (win_t < 70) ? "OVERLORD DESTROYED" : "THE SECTOR IS FREE", C_WHITE);
    sprintf(b, "SCORE %06lu  COMBO x%d", score, player.max_combo);
    text_center(90, b, C_LGREEN);
    sprintf(b, "%d / %d BOSSES DEFEATED", bosses_defeated, NBOSS);
    text_center(106, b, C_LCYAN);
    if (win_t >= WIN_INPUT_DELAY) {
        if (frame & 16) text_center(152, "ENTER FREEPLAY", C_WHITE);
        text_center(166, "ESC SAVE + TITLE", C_LGRAY);
    } else {
        text_center(156, (win_t < 70) ? "THREAT ELIMINATED"
                    : (win_t < 140) ? "FLEET SALUTE" : "VICTORY CONFIRMED", C_LMAG);
    }
}

/* ---------------- instruction menu ---------------- */
static i16 help_page = 0;
#define HELP_PAGES 6

typedef struct {
    const char *text;
    u8 col;
} HelpLine;

static const char *HELP_TITLE[HELP_PAGES] = {
    "CONTROLS", "PICKUPS", "WEAPONS", "SURVIVAL",
    "ENEMIES AND BOSSES", "SCORING AND DIFFICULTY"
};

static const HelpLine HELP_CONTROLS[] = {
    { "ARROWS / WASD   MOVE SHIP", C_WHITE },
    { "SPACE           FIRE", C_LGRAY },
    { "SHIFT           BOOST", C_LGREEN },
    { "CTRL            HOMING MISSILE", C_LCYAN },
    { "B               SMART BOMB", C_LMAG },
    { "P               PAUSE / RESUME", C_LGRAY },
    { "M               MUTE / UNMUTE", C_LGRAY },
    { "H               OPEN / CLOSE HELP", C_LGRAY },
    { "ESC             BACK / TITLE", C_LGRAY },
    { "HELP PAGES: USE UP / DOWN", C_YELLOW }
};
static const HelpLine HELP_WEAPONS[] = {
    { "CANNON: BALANCED SPREAD", C_YELLOW },
    { "LASER: PIERCING, 2 DAMAGE", C_LCYAN },
    { "LEVELS 1-4 FIRE 1/2/3/4 LANES", C_LCYAN },
    { "WAVE: WIDE CROWD CONTROL", C_LMAG },
    { "LEVELS 1-4 FIRE 5/7/9/11 SHOTS", C_LMAG },
    { "G UPGRADES EQUIPPED WEAPON", C_WHITE },
    { "Z ON WAVE: 10 SEC PIERCE BOOST", C_WHITE },
    { "R RAPID SHORTENS FIRE COOLDOWN", C_LGRAY },
    { "MISSILES HOME AHEAD, BEST VS BOSS", C_LGRAY },
    { "DEATH LOWERS GUN LEVEL BY ONE", C_DGRAY }
};
static const HelpLine HELP_SURVIVAL[] = {
    { "BST DRAINS DURING BOOST", C_LGREEN },
    { "RELEASE SHIFT; BST RECHARGES", C_LGREEN },
    { "SHIELD: 10 SEC INVULNERABLE", C_LCYAN },
    { "SHIELD RAM DESTROYS SMALL ENEMIES", C_LCYAN },
    { "BOSS RAM: 10 PCT DAMAGE + BOUNCE", C_LCYAN },
    { "SMART BOMB CLEARS ENEMY SHOTS", C_LMAG },
    { "FLY ABOVE BOSS TO FIRE DOWN", C_WHITE },
    { "BOSS FLASH = HEAVY ATTACK TELL", C_YELLOW },
    { "EASY/NORMAL GIVE RECOVERY SHIELDS", C_DGRAY },
    { "PICKUPS FAVOR RESOURCES YOU NEED", C_DGRAY }
};
static const HelpLine HELP_ENEMIES[] = {
    { "SCOUT: FAST STRAIGHT ATTACKER", C_LGRAY },
    { "WEAVER: SWERVES; ELITE TRAILS", C_LGRAY },
    { "SHOOTER: FIRES; ELITE SPLITS", C_LGRAY },
    { "BOX WARNS: TOUGH ELITE ENEMY", C_LCYAN },
    { "STRONGER ATTACKS + BONUS SCORE", C_WHITE },
    { "EVERY 4TH WAVE IS A BOSS", C_YELLOW },
    { "GREEN + MARKS SUPPLY ESCORT", C_LGREEN },
    { "SUPPLY ESCORT GUARANTEES DROP", C_LGREEN },
    { "SUPPORT DROPS CAPPED AT 4 / BOSS", C_DGRAY },
    { "BEAT W60 TO UNLOCK FREEPLAY", C_LMAG }
};
static const HelpLine HELP_SCORING[] = {
    { "FAST KILLS BUILD COMBO TO X5", C_WHITE },
    { "GRAZE SHOTS: +10 + COMBO TIME", C_LCYAN },
    { "NO HIT / PERFECT WAVE MEDALS", C_YELLOW },
    { "BOMBS SCORE FOR CLEARED SHOTS", C_LMAG },
    { "$ GEM VALUE RISES WITH WAVE", C_WHITE },
    { "EASY SCORE X1.00", C_LGREEN },
    { "NORMAL SCORE X1.25", C_LCYAN },
    { "HARD SCORE X1.60", C_LRED },
    { "HARD: MORE + DENSER ATTACKS", C_LRED },
    { "NORMAL DROPS E/N/H: 18/15/12 PCT", C_DGRAY },
    { "ESCORT DROPS E/N/H: 55/50/45 PCT", C_LGREEN },
    { "ALL MODES SHARE ONE HIGH SCORE", C_WHITE }
};

static void help_lines(const HelpLine *line, u8 count, i16 y, i16 step)
{
    u8 i;
    for (i = 0; i < count; i++, y += step)
        text_draw(20, y, line[i].text, line[i].col);
}

/* letter identifying each pickup gem (index = PU_* type) */
static const char PU_LETTER[PU_COUNT + 1] = "GRHLMZWB$";

static void help_row(i16 y, u8 pu, const char *txt)
{
    char s[2];
    vga_sprite(20, y, SH_PU_W, SH_PU_H, spr_powerup[pu]);
    s[0] = PU_LETTER[pu]; s[1] = 0;
    text_draw(35, y + 3, s, C_BLACK);
    text_draw(34, y + 2, s, C_WHITE);        /* separate high-contrast label */
    text_draw(50, y + 2, txt, C_LGRAY);
}

static void help_arrow(i16 x, i16 y, bool up, u8 col)
{
    if (up) {
        vga_pixel((i16)(x + 3), y, col);
        vga_hline((i16)(x + 2), (i16)(y + 1), 3, col);
        vga_hline((i16)(x + 1), (i16)(y + 2), 5, col);
        vga_hline(x, (i16)(y + 3), 7, col);
    } else {
        vga_hline(x, y, 7, col);
        vga_hline((i16)(x + 1), (i16)(y + 1), 5, col);
        vga_hline((i16)(x + 2), (i16)(y + 2), 3, col);
        vga_pixel((i16)(x + 3), (i16)(y + 3), col);
    }
}

static void help_nav(void)
{
    char b[16];
    u8 upc = (help_page > 0) ? C_LCYAN : C_DGRAY;
    u8 dnc = (help_page < HELP_PAGES - 1) ? C_LCYAN : C_DGRAY;
    help_arrow(14, 182, TRUE, upc);  text_draw(24, 180, "UP", upc);
    help_arrow(62, 182, FALSE, dnc); text_draw(72, 180, "DOWN", dnc);
    sprintf(b, "PAGE %d/%d", help_page + 1, HELP_PAGES);
    text_center(180, b, C_LGRAY);
    text_draw(240, 180, "ESC BACK", C_LCYAN);
}

static void help_scroll(i16 dir)
{
    help_page += dir;
    if (help_page < 0) help_page = 0;
    if (help_page >= HELP_PAGES) help_page = HELP_PAGES - 1;
}

static void draw_help(void)
{
    text_center(6, HELP_TITLE[help_page], C_YELLOW);
    if (help_page == 0) {
        help_lines(HELP_CONTROLS, 10, 24, 14);
    } else if (help_page == 1) {
        help_row( 22, PU_GUN,     "GUN: +1 EQUIPPED WEAPON LEVEL");
        help_row( 38, PU_RAPID,   "RAPID: FASTER FIRE, TIMED");
        help_row( 54, PU_SHIELD,  "SHIELD: 10 SEC INVULNERABLE");
        help_row( 70, PU_LIFE,    "LIFE: EXTRA SHIP (MAX 9)");
        help_row( 86, PU_MISSILE, "MISSILES: +4 AMMO (MAX 30)");
        help_row(102, PU_LASER,   "LASER: FAST PIERCING GUN");
        help_row(118, PU_WAVE,    "WAVE: WIDE ARC GUN");
        help_row(134, PU_BOMB,    "BOMB: +1 SMART BOMB (MAX 10)");
        help_row(150, PU_SCORE,   "SCORE GEM: RISKY WAVE BONUS");
    } else if (help_page == 2) {
        help_lines(HELP_WEAPONS, 10, 24, 14);
    } else if (help_page == 3) {
        help_lines(HELP_SURVIVAL, 10, 24, 14);
    } else if (help_page == 4) {
        help_lines(HELP_ENEMIES, 10, 24, 14);
    } else {
        help_lines(HELP_SCORING, 12, 22, 13);
    }
    help_nav();
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
    bool entry_replay = FALSE;
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
            snd_pause(paused);
            kbd_clear();               /* drop held keys so the ship can't drift across a pause */
        }

        if (state != ST_PLAY || !paused) { update_stars(); update_dust(); }

        switch (state) {
        case ST_TITLE:
            if (key_hit(SC_UP)   && g_diff > 0) g_diff--;
            if (key_hit(SC_DOWN) && g_diff < 2) g_diff++;
            if (key_hit(SC_H))   { help_page = 0; state = ST_HELP; }
            if (key_hit(SC_SPACE)) { reset_game(); start_wave(); state = ST_PLAY; paused = FALSE;
                                     snd_music_game(0); }
            if (key_hit(SC_ESC))   state = ST_QUIT;
            break;
        case ST_HELP:
            if (key_hit(SC_UP) || key_hit(SC_W)) help_scroll(-1);
            if (key_hit(SC_DOWN) || key_hit(SC_S)) help_scroll(1);
            if (key_hit(SC_SPACE)) {
                if (help_page < HELP_PAGES - 1) help_scroll(1);
                else help_page = 0;
            }
            if (key_hit(SC_ESC) || key_hit(SC_H)) state = ST_TITLE;
            break;
        case ST_PLAY:
            if (!paused) update_play();
            if (win_pending) {
                remember_run();
                snd_music_set(MUS_WIN);
                state = ST_WIN; win_t = 0;
                clear_combat_fx();
                kbd_clear();
                break;
            }
            if (key_hit(SC_ESC)) { state = ST_TITLE; snd_music_set(MUS_TITLE);
                                   clear_combat_fx(); kbd_clear(); }
            if (!player.alive) {
                remember_run();
                snd_music_set(MUS_TITLE);
                over_timer = 160;              /* let the moment land */
                key_hit(SC_SPACE);             /* held fire never advances Game Over */
                key_hit(SC_ENTER);
                state = ST_OVER;
            }
            break;
        case ST_OVER:
            if (over_timer > 0) over_timer--;
            if (game_over_continue_requested()) {
                clear_combat_fx();
                entry_rank = hi_qualifies(score);
                if (entry_rank >= 0) {
                    strcpy(name, pilot_name); nlen = (int)strlen(name);
                    entry_name_error = 0; entry_replay = TRUE; state = ST_ENTRY;
                }
                else state = ST_SCORES;
            }
            break;
        case ST_ENTRY: {
            char c = kbd_getchar();
            if (c && nlen < NAME_LEN) { name[nlen++] = c; name[nlen] = 0; entry_name_error = 0; }
            if (key_hit(SC_BKSP) && nlen > 0) { name[--nlen] = 0; }
            if (key_hit(SC_ENTER)) {
                if (nlen == 0) entry_name_error = 1;
                else {
                    strcpy(pilot_name, name);
                    hi_insert(entry_rank, name, score); hi_save(); state = ST_SCORES;
                }
            }
            if (key_hit(SC_ESC)) {
                if (nlen == 0) entry_name_error = 1;
                else {
                    strcpy(pilot_name, name);
                    hi_insert(entry_rank, name, score); hi_save();
                    if (entry_replay) {
                        reset_game(); start_wave(); state = ST_PLAY; paused = FALSE;
                        snd_music_game(0);
                    } else state = ST_SCORES;
                }
            }
            break; }
        case ST_SCORES:
            if (key_hit(SC_SPACE)) state = ST_TITLE;
            if (key_hit(SC_CTRL)) { reset_game(); start_wave(); state = ST_PLAY; paused = FALSE;
                                    snd_music_game(0); }
            break;
        case ST_WIN: {
            bool save = key_hit(SC_ESC);
            bool freeplay;
            win_t++;
            freeplay = win_freeplay_requested();
            if (win_t < WIN_INPUT_DELAY && win_t % 28 == 0) snd_sfx(SFX_PHASE);
            if (win_t >= WIN_INPUT_DELAY && save) {
                finish_wave(); entry_rank = hi_qualifies(score); snd_music_set(MUS_TITLE);
                if (entry_rank >= 0) {
                    strcpy(name, pilot_name); nlen = (int)strlen(name);
                    entry_name_error = 0; entry_replay = FALSE; state = ST_ENTRY;
                } else state = ST_SCORES;
                kbd_clear();
            }
            else if (freeplay) {
                win_pending = 0;
                finish_wave();
                start_wave();
                state = ST_PLAY; paused = FALSE;
                snd_music_game((u8)bosses_defeated);
                kbd_clear();
            }
            break; }
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
                text_center(164, entry_name_error ? "NAME REQUIRED" :
                            (entry_replay ? "ESC SAVES + REPLAYS" : "ESC SAVES SCORE"),
                            entry_name_error ? C_LRED : C_DGRAY);
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
    player.x = 150; player.y = 52; player.shield = 300; player.gun = 3;
    player.wtype = WT_WAVE; player.wave_boost = 200; player.msl = 12; player.bombs = 3;
    player.boost = 82; player.boost_cd = 12; player.boosting = TRUE;
    player.facing_down = TRUE;
    player.combo = 12; player.combo_t = 100; player.max_combo = 18; ship_bank = 2;
    wave_banner = 80;
    set_msg("NO HIT +1000");
    msl[0].active = TRUE; msl[0].x = 120; msl[0].y = 78; msl[0].dx = 1; msl[0].dy = 4;
    msl[1].active = TRUE; msl[1].x = 226; msl[1].y = 94; msl[1].dx = -1; msl[1].dy = 4;
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
    boss.active = TRUE; boss.entering = FALSE; boss.kind = 0;   /* GORGON below player */
    { const BossDef *bd = &BOSSDEF[boss.kind];
      boss.spr = bd->spr; boss.mvt = boss.kind; boss.atkset = boss.kind;
      boss.w = spr_boss_w[boss.spr]; boss.h = spr_boss_h[boss.spr]; }
    boss.x = (i16)(SCRW/2 - boss.w/2); boss.y = 108;
    boss.hp = 40; boss.maxhp = 120; boss.phase = 1; boss.charge = 12;
    for (i = 0; i < 5; i++) { pbul[i].active = TRUE; pbul[i].kind = WT_LASER; pbul[i].x = 60 + i * 40; pbul[i].y = 74 + i * 5; pbul[i].dy = 12; pbul[i].dmg = 2; }
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

/* Measure the actual paced presentation path used by gameplay. scene 0 is a
   sustained boss fight; scene 1 is the help screen. */
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
            vga_present_paced();
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
        vga_present_paced();
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

/* boss atlas: 15 authored campaign bosses, wave order left-to-right.
   72px-wide sprites need 78px cells, so the roster spans two pages. */
void game_selftest_bosses(const char *bmp, int page)
{
    int b, first = (page == 0) ? 0 : 8, last = (page == 0) ? 7 : 14;
    gen_nebula();
    vga_set_theme(2);
    vga_bg_blit(0);
    text_center(3, (page == 0) ? "CAMPAIGN BOSSES 1" : "CAMPAIGN BOSSES 2", C_YELLOW);
    for (b = first; b <= last; b++) {
        i16 idx = (i16)(b - first);
        i16 col = (i16)(idx % 4), row = (i16)(idx / 4);
        i16 cellx = (i16)(4 + col * 79), celly = (i16)(16 + row * 90);
        i16 x = (i16)(cellx + (74 - spr_boss_w[b]) / 2);
        i16 y = (i16)(celly + 10 + (54 - spr_boss_h[b]) / 2);
        char lab[24];
        sprintf(lab, "W%02d", (b + 1) * 4);
        text_draw(cellx, celly, lab, C_LGRAY);
        vga_sprite(x, y, spr_boss_w[b], spr_boss_h[b], spr_boss[b]);
        if (strlen(BOSSNAME[b]) <= 9) text_draw(cellx, (i16)(celly + 68), BOSSNAME[b], C_WHITE);
        else {
            char shortn[10];
            strncpy(shortn, BOSSNAME[b], 9);
            shortn[9] = 0;
            text_draw(cellx, (i16)(celly + 68), shortn, C_WHITE);
        }
    }
    bmp_dump(bmp);
}

void game_selftest_win(const char *bmp)
{
    reset_game();
    init_stars(); init_dust(); gen_nebula();
    score = 987650; player.max_combo = 42; bosses_defeated = 15;
    win_t = WIN_INPUT_DELAY + 30;
    vga_bg_blit(0); draw_stars(); draw_dust(); draw_win();
    bmp_dump(bmp);
}

/* ---------------- headless logic self-tests -> SELFTEST.TXT ---------------- */
static void selftest_log(FILE *f, const char *tag, int pass)
{
    if (f) fprintf(f, "%s %s\n", tag, pass ? "PASS" : "FAIL");
}

/* separation: a 20-ship pile must dissolve until no pair is stacked.
   Neighbours settle 5-13 px apart (pushes cancel in a dense crowd), which
   reads as distinct ships; the failure we guard against is dx ~ 0. */
static int selftest_sep_run(void)
{
    int i, j, n;
    memset(enemy, 0, sizeof(enemy));
    for (i = 0; i < 20; i++) {
        init_enemy(&enemy[i], (u8)(i % 2 ? E_WEAVER : E_SCOUT), 150);
        enemy[i].y = 80;
        enemy[i].vy = 0;
    }
    for (n = 0; n < 250; n++) {
        separate_enemies();
        /* the play loop refreshes weaver x from base every frame; mirror that */
        for (i = 0; i < MAX_ENEMY; i++) if (enemy[i].active && enemy[i].type == E_WEAVER)
            enemy[i].x = enemy[i].base;
    }
    for (i = 0; i < MAX_ENEMY - 1; i++) if (enemy[i].active)
        for (j = i + 1; j < MAX_ENEMY; j++) if (enemy[j].active) {
            i16 dx = (i16)(enemy[i].x - enemy[j].x);
            i16 dy = (i16)(enemy[i].y - enemy[j].y);
            if (dx < 0) dx = (i16)-dx;
            if (dy < 0) dy = (i16)-dy;
            if (dx < 4 && dy < 8) return 0;
        }
    return 1;
}

/* movement: every boss kind x phase must stay inside its authored envelope */
static int selftest_bossmove_run(u8 kind, u8 phase)
{
    int f;
    const BossDef *bd = &BOSSDEF[kind];
    memset(enemy, 0, sizeof(enemy));
    memset(ebul, 0, sizeof(ebul));
    boss.active = TRUE; boss.entering = FALSE;
    boss.kind = kind; boss.spr = bd->spr;
    boss.w = spr_boss_w[boss.spr]; boss.h = spr_boss_h[boss.spr];
    boss.phase = phase; boss.last_phase = phase;
    boss.x = (i16)(SCRW / 2 - boss.w / 2); boss.y = boss_rest_y();
    boss.dir = 1; boss.t = 0; boss.tx = boss.x; boss.ty = 0;
    boss.mv_t = 50; boss.dive_t = 0; boss.charge = 0; boss.spin = 0;
    boss.launch_t = 90; boss.die_t = 0; boss.warn = 0;
    boss.px[0] = boss.px[1] = boss.py[0] = boss.py[1] = 0;
    boss.hp = boss.maxhp = 200;
    player.x = 152; player.y = 168; player.alive = TRUE;
    for (f = 0; f < 800; f++) {
        player.x = (i16)(8 + ((f * 3) % 288));    /* scripted player sweep */
        boss_move();
        boss.t++;
        if (boss.x < 4 || boss.x > SCRW - boss.w - 4) return 0;
        if (boss.y < 6 || boss.y > boss_max_y()) return 0;
    }
    return 1;
}

static int selftest_facing_run(void)
{
    int i;
    reset_game();
    memset(pbul, 0, sizeof(pbul));
    memset(msl, 0, sizeof(msl));
    boss.active = TRUE; boss.entering = FALSE; boss.die_t = 0;
    boss.x = 136; boss.y = 108; boss.w = 48; boss.h = 34;
    player.x = 152; player.y = 48; player.alive = TRUE;
    update_player_facing();
    if (!player.facing_down) return 0;
    player.wtype = WT_CANNON; player.gun = 1; player.firecd = 0;
    player_fire();
    for (i = 0; i < MAX_PBULLET; i++) if (pbul[i].active && pbul[i].dy <= 0) return 0;
    player.msl = 1;
    fire_missile();
    if (!msl[0].active || msl[0].dy <= 0) return 0;
    player.y = 126;                       /* cross below the boss centre */
    update_player_facing();
    if (player.facing_down) return 0;
    boss.active = FALSE;
    player.facing_down = TRUE;
    update_player_facing();
    return player.facing_down ? 0 : 1;
}

static int selftest_damage_rules_run(void)
{
    boss.maxhp = 301;
    return boss_pct_damage(3) == 101 && boss_pct_damage(10) == 31;
}

static int selftest_shooter_linger_run(void)
{
    Enemy e;
    memset(&e, 0, sizeof(e));
    e.type = E_SHOOTER; e.y = 60; e.t = 139;
    if (!shooter_lingering(&e)) return 0;
    e.t = 140;
    if (shooter_lingering(&e)) return 0;
    e.t = 20; e.y = 120;
    return shooter_lingering(&e) ? 0 : 1;
}

static int selftest_freeplay_hp_run(void)
{
    g_diff = DIF_NORMAL;
    return boss_health_for(64, 0) == boss_health_for(120, 0)
        && boss_health_for(64, 14) == boss_health_for(400, 14);
}

static int selftest_shield_ram_run(void)
{
    reset_game();
    boss.active = TRUE; boss.die_t = 0; boss.maxhp = boss.hp = 100;
    boss.x = 130; boss.y = 80; boss.w = 50; boss.h = 40;
    player.x = 150; player.y = 110; player.shield = 100; player.ram_cd = 0;
    apply_boss_damage(boss_pct_damage(10));
    shield_bounce((i16)(boss.x + boss.w / 2), (i16)(boss.y + boss.h / 2));
    return boss.hp == 90 && player.ram_cd == 24 && player.invuln == 12 && player.y > 110;
}

static int selftest_drop_policy_run(void)
{
    int i;
    reset_game(); player.gun = GUN_MAX; player.lives = 9;
    for (i = 0; i < 200; i++) {
        u8 p = choose_powerup();
        if (p == PU_GUN || p == PU_LIFE) return 0;
    }
    return 1;
}

static int selftest_game_over_input_run(void)
{
    over_timer = 120;
    g_edge[SC_SPACE] = 1; g_edge[SC_ENTER] = 0;
    if (game_over_continue_requested()) return 0;
    g_edge[SC_SPACE] = 0; g_edge[SC_ENTER] = 1;
    if (!game_over_continue_requested()) return 0;
    over_timer = 0;
    return game_over_continue_requested() ? 1 : 0;
}

static int selftest_victory_input_run(void)
{
    win_t = WIN_INPUT_DELAY + 1;
    g_edge[SC_SPACE] = 1; g_edge[SC_ENTER] = 0;
    if (win_freeplay_requested()) return 0;
    g_edge[SC_ENTER] = 1;
    return win_freeplay_requested() ? 1 : 0;
}

static i16 active_player_bullets(u8 kind)
{
    int i; i16 n = 0;
    for (i = 0; i < MAX_PBULLET; i++)
        if (pbul[i].active && pbul[i].kind == kind) n++;
    return n;
}

static int selftest_weapon_tiers_run(void)
{
    i16 g;
    reset_game(); player.x = 150; player.y = 160; player.facing_down = FALSE;
    for (g = 1; g <= GUN_MAX; g++) {
        memset(pbul, 0, sizeof(pbul));
        player.wtype = WT_LASER; player.gun = (u8)g; player.firecd = 0;
        player_fire();
        if (active_player_bullets(WT_LASER) != g) return 0;
        memset(pbul, 0, sizeof(pbul));
        player.wtype = WT_WAVE; player.wave_boost = 0; player.firecd = 0;
        player_fire();
        if (active_player_bullets(WT_WAVE) != (i16)(g * 2 + 3)) return 0;
    }
    return 1;
}

static int selftest_support_run(void)
{
    int i; i16 supply = 0, support = 0, drops = 0;
    Enemy *carrier = 0;
    reset_game(); wave = 20; boss.active = TRUE; boss.kind = 0; boss.phase = 0;
    boss.drop_budget = 4;
    boss.x = 120; boss.w = 56;
    summon_escort();
    if (active_enemy_count() != 2) return 0;
    for (i = 0; i < MAX_ENEMY; i++) if (enemy[i].active) {
        if (enemy[i].drop_class == DROP_SUPPLY) { supply++; carrier = &enemy[i]; }
        if (enemy[i].drop_class == DROP_SUPPORT) support++;
    }
    if (supply != 1 || support != 1 || !carrier) return 0;
    carrier->x = 120; carrier->y = 60;
    kill_enemy(carrier);
    for (i = 0; i < MAX_POWERUP; i++) if (powr[i].active) drops++;
    return drops == 1 && boss.drop_budget == 3;
}

static int selftest_escort_drop_rates_run(void)
{
    Enemy ordinary, support;
    memset(&ordinary, 0, sizeof(ordinary));
    memset(&support, 0, sizeof(support));
    ordinary.drop_class = DROP_NORMAL;
    support.drop_class = DROP_SUPPORT;

    boss.active = FALSE;
    g_diff = DIF_EASY;
    if (enemy_drop_chance(&ordinary) != 18 || enemy_drop_chance(&support) != 18) return 0;
    g_diff = DIF_NORMAL;
    if (enemy_drop_chance(&ordinary) != 15 || enemy_drop_chance(&support) != 15) return 0;
    g_diff = DIF_HARD;
    if (enemy_drop_chance(&ordinary) != 12 || enemy_drop_chance(&support) != 12) return 0;

    boss.active = TRUE;
    g_diff = DIF_EASY;
    if (enemy_drop_chance(&ordinary) != 18 || enemy_drop_chance(&support) != 55) return 0;
    g_diff = DIF_NORMAL;
    if (enemy_drop_chance(&ordinary) != 15 || enemy_drop_chance(&support) != 50) return 0;
    g_diff = DIF_HARD;
    return enemy_drop_chance(&ordinary) == 12 && enemy_drop_chance(&support) == 45;
}

static int selftest_danger_tells_run(void)
{
    int i; i16 y;
    reset_game(); g_diff = DIF_NORMAL; player.x = 150; player.y = 150;
    boss.active = TRUE; boss.entering = FALSE; boss.kind = 1;
    boss.w = spr_boss_w[1]; boss.h = spr_boss_h[1]; boss.x = 140; boss.y = 18;
    boss.ty = 0; boss.mv_t = 1; boss.tx = boss.x; boss.t = 0; boss.phase = 2;
    boss_move(); y = boss.y;
    if (boss.ty != 1 || boss.charge < 20) return 0;
    for (i = 0; i < 20; i++) { boss_move(); boss.t++; if (boss.y != y) return 0; }
    boss.kind = 14; boss.w = spr_boss_w[14]; boss.h = spr_boss_h[14];
    boss.x = 140; boss.y = 20; boss.ty = 0; boss.mv_t = 1; boss.phase = 2; boss.t = 10;
    boss_move(); y = boss.y;
    if (boss.ty != 2 || boss.charge < 20) return 0;
    for (i = 0; i < 20; i++) { boss_move(); boss.t++; if (boss.y != y) return 0; }
    return danger_speed() <= 4;
}

void game_selftest_logic(void)
{
    FILE *f = fopen("SELFTEST.TXT", "w");
    u8 kind, phase;
    reset_game();
    wave = 8;
    selftest_log(f, "SEPARATION", selftest_sep_run());
    selftest_log(f, "DIRECTIONAL FIRE", selftest_facing_run());
    selftest_log(f, "BOSS PERCENT DAMAGE", selftest_damage_rules_run());
    selftest_log(f, "SHOOTER LINGER EXIT", selftest_shooter_linger_run());
    selftest_log(f, "FREEPLAY HP CAP", selftest_freeplay_hp_run());
    selftest_log(f, "SHIELD RAM", selftest_shield_ram_run());
    selftest_log(f, "ADAPTIVE DROPS", selftest_drop_policy_run());
    selftest_log(f, "GAME OVER INPUT LOCK", selftest_game_over_input_run());
    selftest_log(f, "VICTORY INPUT LOCK", selftest_victory_input_run());
    selftest_log(f, "WEAPON TIER PATTERNS", selftest_weapon_tiers_run());
    selftest_log(f, "BOSS SUPPORT DROP", selftest_support_run());
    selftest_log(f, "ESCORT DROP RATES", selftest_escort_drop_rates_run());
    selftest_log(f, "DANGEROUS MOVE TELLS", selftest_danger_tells_run());
    for (kind = 0; kind < NBOSS; kind++)
        for (phase = 0; phase < 3; phase++) {
            char tag[24];
            sprintf(tag, "BOSSMOVE K%02d P%d", kind, phase);
            selftest_log(f, tag, selftest_bossmove_run(kind, phase));
        }
    if (f) fclose(f);
    boss.active = FALSE;
    memset(enemy, 0, sizeof(enemy));
}
