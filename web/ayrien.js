/* AYRIEN ASSAULT - browser port of the MS-DOS/VGA original.
 * Faithful 1:1 translation of the C game logic (src/game.c et al.):
 * 320x200 indexed framebuffer, same sprites, same waves/bosses/scoring,
 * fixed 35 Hz logic matching the paced DOS build, Web Audio music.
 */
'use strict';

/* ================= constants (defs.h) ================= */
const SCRW = 320, SCRH = 200;
/* on-screen frame rate to emulate; the DOS build is frame-rate-locked and
   runs at ~35 fps on the target 486. Gameplay speed, animation, SFX cadence
   and music tempo are all tied to this. */
const LOGIC_HZ = 35;

const C_BLACK=0, C_BLUE=1, C_GREEN=2, C_CYAN=3, C_RED=4, C_MAGENTA=5, C_BROWN=6,
      C_LGRAY=7, C_DGRAY=8, C_LBLUE=9, C_LGREEN=10, C_LCYAN=11, C_LRED=12,
      C_LMAG=13, C_YELLOW=14, C_WHITE=15;
const PAL_FIRE=16, PAL_NEB=32, PAL_GLOW=48;

const GUN_MIN=1, GUN_MAX=4;
const WT_CANNON=0, WT_LASER=1, WT_WAVE=2, WT_COUNT=3;
const DIF_EASY=0, DIF_NORMAL=1, DIF_HARD=2;

const PU_GUN=0, PU_RAPID=1, PU_SHIELD=2, PU_LIFE=3, PU_MISSILE=4,
      PU_LASER=5, PU_WAVE=6, PU_BOMB=7, PU_SCORE=8, PU_COUNT=9;

const E_SCOUT=0, E_WEAVER=1, E_SHOOTER=2;
const DROP_NORMAL=0, DROP_SUPPORT=1, DROP_SUPPLY=2;

const MAX_PBULLET=48, MAX_EBULLET=64, MAX_ENEMY=28, MAX_POWERUP=8,
      MAX_PART=160, MAX_STARS=96, MAX_MISSILE=6;

const HISCORE_N=10, NAME_LEN=8;

const SH_SHIP_W=16, SH_SHIP_H=16, SH_EN_W=16, SH_EN_H=14,
      SH_PB_W=3, SH_PB_H=8, SH_EB_W=5, SH_EB_H=7,
      SH_PU_W=12, SH_PU_H=12, SH_BOSS_W=48, SH_BOSS_H=34,
      SH_MSL_W=5, SH_MSL_H=10;

const BOOST_MAX=140, BOOST_MIN_START=12, BOOST_DRAIN=2,
      BOOST_RECHARGE=1, BOOST_RECHARGE_CD=25;
const WIN_INPUT_DELAY=210;

const SFX_FIRE=0, SFX_EXPLODE=1, SFX_POWER=2, SFX_HIT=3, SFX_BOSS=4,
      SFX_MISSILE=5, SFX_PICK1=6, SFX_PICK2=7, SFX_COMBO=8, SFX_PHASE=9,
      SFX_BOOST=10;
const MUS_TITLE=0, MUS_GAME=1, MUS_WIN=2;

const ST_TITLE=0, ST_PLAY=1, ST_OVER=2, ST_ENTRY=3, ST_SCORES=4, ST_HELP=5, ST_WIN=6;

/* integer sine table, amplitude +-46 (identical to DOS build) */
const sintab = [
   0,  5,  9, 13, 18, 22, 26, 29, 33, 36, 38, 41, 42, 44, 45, 46,
  46, 46, 45, 44, 42, 41, 38, 36, 33, 29, 26, 22, 18, 13,  9,  5,
   0, -5, -9,-13,-18,-22,-26,-29,-33,-36,-38,-41,-42,-44,-45,-46,
 -46,-46,-45,-44,-42,-41,-38,-36,-33,-29,-26,-22,-18,-13, -9, -5];

/* xorshift rng, 16-bit like the DOS build */
let rng = 0x1234;
function rnd() {
  rng ^= (rng << 7) & 0xFFFF;
  rng ^= rng >> 9;
  rng ^= (rng << 8) & 0xFFFF;
  return rng & 0xFFFF;
}
function rrange(lo, hi) { return lo + (rnd() % (hi - lo + 1)); }

/* ================= palette (vga.c) ================= */
let pal_theme = 0, pal_phase = 0;
const paletteRGB = new Uint8Array(256 * 3);

function clamp63(v) { return v < 0 ? 0 : v > 63 ? 63 : v; }
function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }
function pal_set(i, r, g, b) {
  paletteRGB[i*3]   = clamp63(r) << 2;
  paletteRGB[i*3+1] = clamp63(g) << 2;
  paletteRGB[i*3+2] = clamp63(b) << 2;
}
function set_palette() {
  const ega = [
    [0,0,0],[0,0,42],[0,42,0],[0,42,42],[42,0,0],[42,0,42],[42,21,0],[42,42,42],
    [21,21,21],[21,21,63],[21,63,21],[21,63,63],[63,21,21],[63,21,63],[63,63,21],[63,63,63]];
  for (let i = 0; i < 16; i++) pal_set(i, ega[i][0], ega[i][1], ega[i][2]);
  for (let i = 0; i < 16; i++) {
    const flick = ((i + pal_phase) & 3) === 0 ? 3 : 0;
    pal_set(PAL_FIRE + i, 20 + i*3 + flick,
            (i < 5 ? 0 : (i-5)*6) + (flick >> 1),
            i < 11 ? 0 : (i-11)*12);
  }
  for (let i = 0; i < 16; i++) {
    let r, g, b;
    const pulse = ((i + pal_phase) & 7) === 0 ? 2 : 0;
    switch (pal_theme & 3) {
      default:
      case 0: r = i>>1;      g = (i/3)|0;  b = 3 + i*2; break;      /* blue   */
      case 1: r = 2 + i;     g = i>>2;     b = 5 + i*2; break;      /* violet */
      case 2: r = i>>2;      g = 2 + i;    b = 5 + i;   break;      /* teal   */
      case 3: r = 4 + i*2;   g = 1 + (i>>1); b = i>>2;  break;      /* ember  */
    }
    pal_set(PAL_NEB + i, r + pulse, g + pulse, b + pulse);
  }
  for (let i = 0; i < 16; i++) {
    let r, g, b;
    const pulse = (pal_phase & 1) ? 2 : 0;
    if ((pal_theme & 3) === 1)      { r = 8 + i*2; g = i;        b = 12 + i*3; }
    else if ((pal_theme & 3) === 2) { r = i>>2;    g = 10 + i*3; b = 10 + i*2; }
    else if ((pal_theme & 3) === 3) { r = 14+i*3;  g = 6 + i*2;  b = i/3|0;    }
    else                            { r = i>>2;    g = 8 + i*3;  b = 12 + i*3; }
    pal_set(PAL_GLOW + i, r + pulse, g + pulse, b + pulse);
  }
}
function vga_set_theme(t) { pal_theme = t & 0xFF; set_palette(); }
function vga_cycle_palette() { pal_phase = (pal_phase + 1) & 7; set_palette(); }

/* ================= framebuffer (vga.c) ================= */
const fb = new Uint8Array(SCRW * SCRH);      /* back buffer   */
const bg = new Uint8Array(SCRW * SCRH);      /* nebula buffer */

function vga_clear(col) { fb.fill(col); }
function vga_pixel(x, y, col) {
  if (x >= 0 && x < SCRW && y >= 0 && y < SCRH) fb[y * SCRW + x] = col;
}
function vga_hline(x, y, w, col) {
  if (y < 0 || y >= SCRH) return;
  let x2 = x + w;
  if (x < 0) x = 0;
  if (x2 > SCRW) x2 = SCRW;
  if (x2 <= x) return;
  fb.fill(col, y * SCRW + x, y * SCRW + x2);
}
function vga_rect(x, y, w, h, col) { for (let j = 0; j < h; j++) vga_hline(x, y+j, w, col); }
function vga_frame(x, y, w, h, col) {
  vga_hline(x, y, w, col); vga_hline(x, y+h-1, w, col);
  for (let j = 0; j < h; j++) { vga_pixel(x, y+j, col); vga_pixel(x+w-1, y+j, col); }
}
function vga_sprite(x, y, w, h, data) {
  for (let sy = 0; sy < h; sy++) {
    const py = y + sy;
    if (py < 0 || py >= SCRH) continue;
    const row = sy * w, prow = py * SCRW;
    for (let sx = 0; sx < w; sx++) {
      const c = data[row + sx];
      const px = x + sx;
      if (c && px >= 0 && px < SCRW) fb[prow + px] = c;
    }
  }
}
function vga_bg_blit(yoff) {
  const top = (SCRH - yoff) * SCRW;
  if (yoff === 0) { fb.set(bg); return; }
  fb.set(bg.subarray(0, top), yoff * SCRW);
  fb.set(bg.subarray(top), 0);
}

/* ================= 8x8 font + text (sprites.c) ================= */
const FONT = {
  'A':[0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0],'B':[0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0],
  'C':[0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0],'D':[0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0],
  'E':[0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0],'F':[0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0],
  'G':[0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0],'H':[0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0],
  'I':[0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0],'J':[0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0],
  'K':[0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0],'L':[0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0],
  'M':[0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0],'N':[0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0],
  'O':[0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0],'P':[0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0],
  'Q':[0x3C,0x66,0x66,0x66,0x66,0x3C,0x0E,0],'R':[0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0],
  'S':[0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0],'T':[0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0],
  'U':[0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0],'V':[0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0],
  'W':[0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0],'X':[0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0],
  'Y':[0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0],'Z':[0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0],
  '0':[0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0],'1':[0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0],
  '2':[0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0],'3':[0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0],
  '4':[0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0],'5':[0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0],
  '6':[0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0],'7':[0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0],
  '8':[0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0],'9':[0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0],
  '.':[0,0,0,0,0,0x18,0x18,0],':':[0,0x18,0x18,0,0x18,0x18,0,0],
  '!':[0x18,0x18,0x18,0x18,0x18,0,0x18,0],'?':[0x3C,0x66,0x06,0x0C,0x18,0,0x18,0],
  '+':[0,0x18,0x18,0x7E,0x18,0x18,0,0],'-':[0,0,0,0x7E,0,0,0,0],
  '<':[0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0],'>':[0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0],
  '$':[0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0],'/':[0x02,0x06,0x0C,0x18,0x30,0x60,0x40,0],
  ',':[0,0,0,0,0,0x18,0x18,0x30],'=':[0,0,0x7E,0,0x7E,0,0,0],
  "'":[0x18,0x18,0x30,0,0,0,0,0],'%':[0x62,0x66,0x0C,0x18,0x30,0x66,0x46,0],
  '_':[0,0,0,0,0,0,0,0x7E]
};
function text_draw(x, y, s, col) {
  s = String(s).toUpperCase();
  for (let i = 0; i < s.length; i++, x += 8) {
    const g = FONT[s[i]];
    if (!g) continue;
    for (let r = 0; r < 8; r++) {
      const bits = g[r];
      if (!bits) continue;
      for (let b = 0; b < 8; b++)
        if (bits & (0x80 >> b)) vga_pixel(x + b, y + r, col);
    }
  }
}
function text_center(y, s, col) { text_draw((SCRW - s.length * 8) >> 1, y, s, col); }
function pad6(n) { return String(n).padStart(6, '0'); }
function pad2(n) { return String(n).padStart(2, '0'); }

/* ================= sprites (sprites.c) ================= */
function legend(ch) {
  switch (ch) {
    case 'o': return C_DGRAY;  case 'w': return C_WHITE;  case 'l': return C_LGRAY;
    case 'r': return C_RED;    case 'R': return C_LRED;   case 'y': return C_YELLOW;
    case 'g': return C_GREEN;  case 'G': return C_LGREEN; case 'b': return C_BLUE;
    case 'B': return C_LBLUE;  case 'c': return C_CYAN;   case 'C': return C_LCYAN;
    case 'm': return C_MAGENTA;case 'M': return C_LMAG;   case 'n': return C_BROWN;
    case 'D': return PAL_FIRE+3; case 'O': return PAL_FIRE+8; case 'F': return PAL_FIRE+13;
    case 'x': return PAL_GLOW+7; case 'X': return PAL_GLOW+14;
    default:  return 0;
  }
}
function build(w, h, rows) {
  const d = new Uint8Array(w * h);
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++) d[y*w+x] = legend(rows[y][x]);
  return d;
}

const SHIP_ART = [
".......ww.......",".......lw.......","......olwl......","......olwl......",
".....oolwll.....",".....olxXwl.....","....lolxXwll....","...llolxXwlll...",
"..lloolwwllwll..",".ll.oolwwloo.wl.","ll..ooolloo...wl","l.owooo..ooowo.w",
"..oro......oro..","..oro......oro..","..OFO......OFO..","..F.F......F.F.."];
const SCOUT_ART = [
"................","..rr........rr..","..rRl......lRr..","..rRRl....lRRr..",
"...rRRllllRRr...","...rRRRRRRRRr...","....rRXxxXRr....","....rRXxxXRr....",
".....rRRRRr.....","......lRRl......","......oRRo......",".......FF.......",
".......DD.......","................"];
const WEAVER_ART = [
"................","......gGGg......","....ggGGGGgg....","...gGGGGGGGGg...",
"..gGGXGGGGXGGg..","..gGGXGGGGXGGg..",".gGGGGllllGGGGg.","gGGGGGllllGGGGGg",
".gGlGlGGGGlGlGg.","..g.l.gGGg.l.g..",".....gl..lg.....","......F..F......",
"................","................"];
const SHOOTER_ART = [
"...m........m...","...mm......mm...","...mMl....lMm...","..mMMmllllmMMm..",
"..mMMMMMMMMMMm..","..mMXxMMMMxXMm..","..mMXxMwwMxXMm..","...mMMMwwMMMm...",
"....mMMMMMMm....",".....mMMMMm.....","......mMMm......",".......mm.......",
".......FF.......","................"];
const MISSILE_ART = ["..R..",".RRR.",".lwl.",".lwl.",".lwl.",".lwl.",".olo.",".olo.",".OFO.","F.F.F"];
const PBULLET_ART = [".w.","ywy","ywy","ywy","ywy","ywy","ywy",".y."];
const EBULLET_ART = ["..r..",".rRr.","rRRRr","rRRRr","rRRRr",".rRr.","..r.."];
const PU_SHAPE = [
".....oo.....","....oFFo....","...oFFFFo...","..oFFFFFFo..",".oFFFFFFFFo.",
"oFFFFFFFFFFo","oFFFFFFFFFFo",".oFFFFFFFFo.","..oFFFFFFo..","...oFFFFo...",
"....oFFo....",".....oo....."];

const spr_ship = [null, build(SH_SHIP_W, SH_SHIP_H, SHIP_ART), null];
const spr_ship_down = [null, null, null];
const spr_enemy = [build(SH_EN_W,SH_EN_H,SCOUT_ART), build(SH_EN_W,SH_EN_H,WEAVER_ART), build(SH_EN_W,SH_EN_H,SHOOTER_ART)];
const spr_missile = build(SH_MSL_W, SH_MSL_H, MISSILE_ART);
let spr_missile_down = null;
const spr_ebullet = build(SH_EB_W, SH_EB_H, EBULLET_ART);
const spr_pbullet = [build(SH_PB_W, SH_PB_H, PBULLET_ART), null, null];
const spr_powerup = [];
const NBOSS = 15;
const spr_boss = Array(NBOSS).fill(null);
const spr_boss_w = Array(NBOSS).fill(0), spr_boss_h = Array(NBOSS).fill(0);

/* fixed campaign roster: one authored boss for each boss wave, W04..W60. */
const BOSSDEF = [
  { spr:0,  hpbonus: 10 }, { spr:1,  hpbonus:  0 }, { spr:2,  hpbonus: 20 },
  { spr:3,  hpbonus:  0 }, { spr:4,  hpbonus: 15 }, { spr:5,  hpbonus: 35 },
  { spr:6,  hpbonus: 10 }, { spr:7,  hpbonus: 25 }, { spr:8,  hpbonus: 30 },
  { spr:9,  hpbonus:  5 }, { spr:10, hpbonus: 45 }, { spr:11, hpbonus: 15 },
  { spr:12, hpbonus: 20 }, { spr:13, hpbonus: 60 }, { spr:14, hpbonus: 90 },
];
const SH_POD_W = 14, SH_POD_H = 12;      /* NEXUS orbiting fire-pod */
let spr_bosspod = null;

function build_pbul(body, core) {
  const d = new Uint8Array(SH_PB_W * SH_PB_H);
  for (let y = 0; y < SH_PB_H; y++)
    for (let x = 0; x < SH_PB_W; x++) {
      const ch = PBULLET_ART[y][x];
      d[y*SH_PB_W+x] = (ch === 'w') ? core : (ch === 'y') ? body : 0;
    }
  return d;
}
function build_pu(fill) {
  const d = new Uint8Array(SH_PU_W * SH_PU_H);
  for (let y = 0; y < SH_PU_H; y++)
    for (let x = 0; x < SH_PU_W; x++) {
      const ch = PU_SHAPE[y][x];
      d[y*SH_PU_W+x] = (ch === 'F') ? fill : legend(ch);
    }
  return d;
}
/* Each roster slot has its own silhouette, footprint and palette (mirror of
   the native build_boss). Returns the tightly-packed (stride = w) sprite and
   records its dimensions in spr_boss_w/h[kind]. Several bosses lean on
   run-time overlays in draw_play (KRAKEN tentacles, VORTEX orbs, tracking
   pupils, NEXUS pods) on top of the static bitmap. */
function build_boss(kind) {
  const dims = [[68,30],[34,33],[72,32],[40,40],[60,30],[48,30],[56,48],[22,28],
                [64,30],[34,40],[72,34],[44,44],[48,32],[72,44],[64,52]];
  const [w, h] = dims[kind] || dims[14];
  spr_boss_w[kind] = w; spr_boss_h[kind] = h;
  const d = new Uint8Array(w * h);
  const bset = (x, y, c) => { if (x >= 0 && x < w && y >= 0 && y < h) d[y*w+x] = c; };
  const bhline = (x0, x1, y, c) => { if (x0 < 0) x0 = 0; if (x1 > w-1) x1 = w-1; for (let x = x0; x <= x1; x++) bset(x, y, c); };
  const brect = (x0, y0, x1, y1, c) => { for (let y = y0; y <= y1; y++) bhline(x0, x1, y, c); };
  const bvline = (x, y0, y1, c) => { if (y0 < 0) y0 = 0; if (y1 > h-1) y1 = h-1; for (let y = y0; y <= y1; y++) bset(x, y, c); };
  const bdisc = (dx, dy, r, c) => { for (let y = -r; y <= r; y++) for (let x = -r; x <= r; x++) if (x*x + y*y <= r*r) bset(dx+x, dy+y, c); };
  const bring = (dx, dy, r0, r1, c) => { const a = r0*r0, b = r1*r1; for (let y = -r1; y <= r1; y++) for (let x = -r1; x <= r1; x++) { const d2 = x*x + y*y; if (d2 >= a && d2 <= b) bset(dx+x, dy+y, c); } };
  const bdither = (x0, y0, x1, y1, c) => { for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) if (((x ^ y) & 1) === 0) bset(x, y, c); };
  const bmirror = () => { for (let y = 0; y < h; y++) for (let x = 0; x < (w >> 1); x++) d[y*w + (w-1-x)] = d[y*w + x]; };
  const cx = w >> 1;
  let x, y;

  if (kind === 0) {          /* GORGON - serrated siege wall, exposed fire trench */
    for (x = 0; x < w; x += 8) {
      bhline(x + 2, x + 5, 2, C_LGRAY);
      bhline(x + 1, x + 6, 3, C_LGRAY);
      bhline(x, x + 7, 4, C_LGRAY);
      bset(x + 3, 1, C_YELLOW);
    }
    brect(0, 5, w - 1, 12, C_LGRAY);
    brect(1, 13, w - 2, 19, C_DGRAY);
    bdither(2, 20, w - 3, 26, C_LGRAY);
    bhline(0, w - 1, 5, C_WHITE);
    for (x = 10; x < w - 10; x += 12) bvline(x, 6, 18, C_DGRAY);
    brect(cx - 11, 8, cx + 10, 17, C_BLACK);
    brect(cx - 10, 9, cx + 9, 16, PAL_FIRE + 6);
    brect(cx - 6, 11, cx + 5, 14, PAL_FIRE + 11);
    bhline(cx - 11, cx + 10, 8, C_RED);
    bhline(cx - 11, cx + 10, 17, C_RED);
    for (x = 6; x < w - 6; x += 8) { bset(x, 27, PAL_FIRE + 12); bset(x, 28, PAL_FIRE + 8); }
  } else if (kind === 1) {   /* REAPER - asymmetric crescent scythe, skull cockpit */
    for (y = 0; y < 30; y++) {
      const x0 = 2 + Math.trunc(y * y / 45);
      let x1 = x0 + 11 - Math.trunc(y / 3);
      if (x1 <= x0) x1 = x0 + 1;
      bhline(x0, x1, y, C_LRED);
      bset(x0, y, C_WHITE);
      bset(x1, y, C_RED);
    }
    bhline(21, 22, 30, C_LRED);
    bset(22, 31, C_WHITE);
    bvline(26, 4, 26, C_DGRAY);
    bvline(27, 3, 27, C_LGRAY);
    bvline(28, 4, 26, C_DGRAY);
    brect(12, 2, 26, 4, C_RED);
    brect(23, 6, 31, 13, C_LGRAY);
    bset(25, 9, PAL_FIRE + 13); bset(29, 9, PAL_FIRE + 13);
    bhline(24, 30, 12, C_DGRAY);
    bset(25, 13, C_DGRAY); bset(27, 13, C_DGRAY); bset(29, 13, C_DGRAY);
    brect(26, 27, 28, 31, PAL_FIRE + 9);
  } else if (kind === 2) {   /* LEVIATHAN - full-width strike carrier, runway deck */
    brect(2, 8, w - 3, 26, C_LGRAY);
    bhline(2, w - 3, 8, C_WHITE);
    brect(0, 12, 3, 24, C_DGRAY);
    brect(w - 4, 12, w - 1, 24, C_DGRAY);
    for (x = 6; x < w - 6; x += 6) bset(x, 17, (x & 4) ? C_WHITE : C_YELLOW);
    brect(8, 11, 24, 23, C_BLACK);
    brect(w - 25, 11, w - 9, 23, C_BLACK);
    for (y = 11; y <= 23; y++) {
      bset(8, y, C_LGREEN);  bset(24, y, C_LGREEN);
      bset(w - 25, y, C_LGREEN); bset(w - 9, y, C_LGREEN);
    }
    for (x = 10; x < 24; x += 4) bset(x, 23, C_GREEN);
    for (x = w - 23; x < w - 9; x += 4) bset(x, 23, C_GREEN);
    brect(cx - 7, 2, cx + 6, 10, C_DGRAY);
    brect(cx - 4, 4, cx + 3, 8, PAL_GLOW + 9);
    bhline(cx - 7, cx + 6, 2, C_LGRAY);
    bhline(4, w - 5, 27, C_DGRAY);
    for (x = 8; x < w - 8; x += 8) { bset(x, 28, PAL_FIRE + 11); bset(x, 29, PAL_FIRE + 7); }
  } else if (kind === 3) {   /* SEEKER - concentric glow rings around a watching eye */
    bring(cx, 20, 18, 19, PAL_GLOW + 5);
    bring(cx, 20, 13, 14, PAL_GLOW + 9);
    bring(cx, 20, 8, 9, PAL_GLOW + 13);
    bdisc(cx, 20, 6, C_GREEN);
    bdisc(cx, 20, 4, C_LGREEN);
    bvline(cx, 1, 6, C_LGREEN);  bvline(cx, 33, 38, C_LGREEN);
    bhline(1, 6, 20, C_LGREEN);  bhline(33, 38, 20, C_LGREEN);
    bset(cx, 1, C_WHITE); bset(cx, 38, C_WHITE);
    bset(1, 20, C_WHITE); bset(38, 20, C_WHITE);
    brect(cx - 2, 18, cx + 1, 21, C_BLACK);   /* pupil tracks at draw time */
  } else if (kind === 4) {   /* MANTIS - oversized serrated claw crescents */
    brect(cx - 4, 6, cx + 3, 25, C_GREEN);
    brect(cx - 2, 10, cx + 1, 20, PAL_GLOW + 10);
    bset(cx - 3, 4, C_LGREEN); bset(cx - 4, 3, C_LGREEN);
    for (y = 1; y < 27; y++) {
      const dy2 = y - 14;
      let span = 17 - Math.trunc(dy2 * dy2 / 12);
      if (span < 3) span = 3;
      bhline(2, 2 + span, y, (y & 1) ? C_GREEN : C_LGREEN);
    }
    for (y = 3; y < 25; y += 3) {
      const dy2 = y - 14;
      let span = 17 - Math.trunc(dy2 * dy2 / 12);
      if (span < 3) span = 3;
      bset(3 + span, y, C_LGREEN);
      bset(4 + span, y, C_GREEN);
    }
    bset(20, 13, C_YELLOW); bset(20, 15, C_YELLOW);
    bset(21, 14, C_WHITE);
    bmirror();
  } else if (kind === 5) {   /* ANVIL - industrial crush press with hazard chevrons */
    brect(2, 1, w - 3, 8, C_DGRAY);
    bhline(2, w - 3, 1, C_LGRAY);
    for (x = 5; x < w - 5; x += 6) bset(x, 4, C_WHITE);
    for (x = 10; x <= w - 12; x += 12) {
      bvline(x, 9, 20, C_LGRAY);
      bvline(x + 1, 9, 20, C_DGRAY);
    }
    brect(4, 21, w - 5, 27, C_RED);
    bhline(4, w - 5, 21, C_LRED);
    for (x = 4; x <= w - 5; x++) if (((x >> 2) & 1) === 0) bset(x, 24, C_YELLOW);
    bhline(6, w - 7, 28, C_DGRAY);
  } else if (kind === 6) {   /* SERAPH - haloed sentinel, layered feather ribs */
    bhline(cx - 6, cx + 5, 1, PAL_GLOW + 14);
    bset(cx - 7, 2, PAL_GLOW + 11); bset(cx + 6, 2, PAL_GLOW + 11);
    brect(cx - 3, 4, cx + 2, 9, C_WHITE);
    for (y = 10; y < 44; y++) {
      const half = y < 30 ? 5 : 5 + Math.trunc((y - 30) / 3);
      bhline(cx - half, cx + half, y, C_LCYAN);
    }
    bvline(cx - 1, 12, 40, C_WHITE);
    bvline(cx, 12, 40, C_WHITE);
    brect(cx - 2, 22, cx + 1, 28, PAL_GLOW + 12);
    for (x = 0; x < 5; x++) {
      const rx = cx - 9 - x * 4, top = 12 + x * 2, bot = 42 - x * 4;
      bvline(rx, top, bot, PAL_GLOW + 13 - x * 2);
      bvline(rx - 1, top, bot, PAL_GLOW + 11 - x * 2);
      bset(rx, bot + 1, C_WHITE);
    }
    bmirror();
  } else if (kind === 7) {   /* NEXUS - narrow core spire; pods orbit at run time */
    for (y = 0; y < h; y++) {
      const ay = Math.abs(y - (h >> 1));
      let half = 9 - Math.trunc(ay * 2 / 3);
      if (half < 1) half = 1;
      bhline(cx - half, cx + half, y, C_DGRAY);
      bset(cx - half, y, C_LMAG);
      bset(cx + half, y, C_LMAG);
    }
    brect(cx - 3, 10, cx + 2, 17, PAL_GLOW + 8);
    brect(cx - 1, 12, cx, 15, C_WHITE);
    bset(cx - 1, 0, C_WHITE); bset(cx, 0, C_WHITE);
    bset(cx - 1, h - 1, C_WHITE); bset(cx, h - 1, C_WHITE);
  } else if (kind === 8) {   /* KRAKEN - bulbous mantle + eyes; tentacles at run time */
    for (y = 0; y < h; y++) {
      let half = y < 4 ? 14 + y * 4 : y < 22 ? 30 : 30 - (y - 22) * 3;
      if (half > 30) half = 30;
      if (half < 6) half = 6;
      bhline(cx - half, cx + half, y, C_GREEN);
      bset(cx - half, y, C_LGREEN);
      bset(cx + half, y, C_LGREEN);
    }
    bdither(cx - 24, 3, cx + 24, 10, PAL_NEB + 10);
    bdisc(cx - 12, 15, 4, C_WHITE);
    bdisc(cx + 12, 15, 4, C_WHITE);
    bdisc(cx - 12, 15, 2, C_BLACK);
    bdisc(cx + 12, 15, 2, C_BLACK);
    bset(cx - 13, 14, C_LCYAN); bset(cx + 11, 14, C_LCYAN);
    brect(cx - 3, 22, cx + 2, 27, C_BLACK);
    bset(cx - 1, 27, C_YELLOW); bset(cx, 27, C_YELLOW);
  } else if (kind === 9) {   /* PHANTOM - hollow spectre: dashed outline */
    for (y = 0; y < h; y++) {
      let half = y < 10 ? 3 + y : y < 28 ? 13 : 13 - (y - 28);
      if (half < 2) half = 2;
      if ((y & 3) !== 3) {
        bset(cx - half, y, (y & 1) ? C_LBLUE : C_LCYAN);
        bset(cx + half, y, (y & 1) ? C_LBLUE : C_LCYAN);
      }
    }
    bvline(cx - 1, 6, 26, C_WHITE);
    bvline(cx, 6, 26, C_WHITE);
    bset(cx - 5, 14, PAL_GLOW + 13); bset(cx + 5, 14, PAL_GLOW + 13);
    bset(cx - 5, 15, PAL_GLOW + 13); bset(cx + 5, 15, PAL_GLOW + 13);
    bdither(cx - 8, 30, cx + 8, 38, C_LBLUE);
  } else if (kind === 10) {  /* CITADEL - crenellated battlement, three turret towers */
    brect(1, 14, w - 2, 30, C_DGRAY);
    bhline(1, w - 2, 14, C_LGRAY);
    bdither(3, 24, w - 4, 29, C_LGRAY);
    for (x = 2; x < w - 2; x += 8) brect(x, 10, x + 4, 14, C_LGRAY);
    for (x = 0; x < 3; x++) {
      const tx = x === 0 ? 8 : x === 1 ? cx - 6 : w - 20;
      brect(tx, 2, tx + 11, 16, C_LGRAY);
      bhline(tx, tx + 11, 2, C_WHITE);
      brect(tx + 3, 6, tx + 8, 9, C_BLACK);
      brect(tx + 4, 7, tx + 7, 8, C_RED);
      bset(tx + 5, 17, PAL_FIRE + 12);
    }
    brect(cx - 5, 20, cx + 4, 29, C_BLACK);
    brect(cx - 4, 21, cx + 3, 28, PAL_FIRE + 7);
    for (x = 6; x < w - 6; x += 10) bset(x, 31, PAL_FIRE + 10);
  } else if (kind === 11) {  /* VORTEX - split mag/cyan ring; orbs at run time */
    bring(cx, 22, 15, 20, C_LMAG);
    for (y = 0; y < h; y++)
      for (x = cx; x < w; x++)
        if (d[y*w+x] === C_LMAG) d[y*w+x] = C_LCYAN;
    bring(cx, 22, 6, 8, C_LCYAN);
    for (y = 0; y < h; y++)
      for (x = 0; x < cx; x++)
        if (d[y*w+x] === C_LCYAN) d[y*w+x] = C_LMAG;
    brect(cx - 1, 21, cx, 22, C_WHITE);
    bset(cx, 3, C_WHITE); bset(cx, 40, C_WHITE);
    bset(3, 22, C_WHITE); bset(40, 22, C_WHITE);
  } else if (kind === 12) {  /* BASILISK - serpent skull, one huge tracking eye */
    for (y = 2; y < 30; y++) {
      let half = y < 12 ? 10 + y : 22 - Math.trunc((y - 12) / 2);
      if (half > 22) half = 22;
      bhline(cx - half, cx + half, y, C_GREEN);
      bset(cx - half, y, C_LGREEN);
      bset(cx + half, y, C_LGREEN);
    }
    bdither(cx - 18, 16, cx + 18, 27, C_LGREEN);
    for (x = 0; x < 4; x++) {
      const sx = cx - 9 + x * 6;
      bset(sx, 1, C_LGREEN);
      bset(sx, 0, C_WHITE);
    }
    bring(cx, 13, 7, 8, C_YELLOW);
    bdisc(cx, 13, 6, C_YELLOW);
    bdisc(cx, 13, 4, C_RED);
    brect(cx - 1, 12, cx, 14, C_BLACK);
    bset(cx - 6, 28, C_WHITE); bset(cx + 5, 28, C_WHITE);
    bset(cx - 6, 29, C_WHITE); bset(cx + 5, 29, C_WHITE);
  } else if (kind === 13) {  /* TITAN - tri-layer dreadnought; armour breaks per phase */
    brect(0, 26, w - 1, 38, C_DGRAY);
    bdither(2, 27, w - 3, 33, C_LGRAY);
    brect(6, 14, w - 7, 26, C_LGRAY);
    brect(14, 4, w - 15, 14, C_DGRAY);
    bhline(14, w - 15, 4, C_WHITE);
    bhline(6, w - 7, 14, C_WHITE);
    bhline(0, w - 1, 26, C_LGRAY);
    brect(cx - 9, 0, cx + 8, 24, C_LGRAY);
    bvline(cx - 9, 0, 24, C_DGRAY); bvline(cx + 8, 0, 24, C_DGRAY);
    brect(cx - 4, 6, cx + 3, 20, PAL_FIRE + 8);
    brect(cx - 2, 9, cx + 1, 17, PAL_FIRE + 13);
    for (x = 10; x < w - 10; x += 12) {
      brect(x, 30, x + 5, 36, C_RED);
      bset(x + 2, 37, PAL_FIRE + 12);
    }
    for (x = 4; x < w - 4; x += 10) brect(x, 40, x + 4, 42, PAL_FIRE + 9);
    brect(0, 16, 6, 30, C_LGRAY);
    brect(w - 7, 16, w - 1, 30, C_LGRAY);
  } else {                   /* OVERLORD - crowned obelisk finale, cycling core */
    for (x = 0; x < 5; x++) {
      const sx = cx - 12 + x * 6;
      bvline(sx, x === 2 ? 0 : 2, 7, C_LMAG);
      bset(sx, x === 2 ? 0 : 2, C_WHITE);
    }
    for (y = 7; y < 46; y++) {
      const half = 8 + Math.trunc(y * 14 / 46);
      bhline(cx - half, cx + half, y, C_MAGENTA);
      bset(cx - half, y, C_LMAG);
      bset(cx + half, y, C_LMAG);
    }
    bdither(cx - 18, 30, cx + 18, 44, C_LMAG);
    bvline(cx - 1, 7, 45, C_WHITE);
    bvline(cx, 7, 45, C_WHITE);
    brect(cx - 6, 20, cx + 5, 32, PAL_GLOW + 4);
    brect(cx - 3, 23, cx + 2, 29, PAL_GLOW + 10);
    brect(2, 24, 8, 40, C_MAGENTA);
    brect(w - 9, 24, w - 3, 40, C_MAGENTA);
    bvline(5, 20, 23, C_LMAG); bvline(w - 6, 20, 23, C_LMAG);
    bset(5, 19, C_WHITE); bset(w - 6, 19, C_WHITE);
    bhline(8, cx - 8, 27, C_LMAG);
    bhline(cx + 7, w - 9, 27, C_LMAG);
    for (x = cx - 16; x <= cx + 16; x += 4) bset(x, 47, PAL_FIRE + 10);
    bhline(cx - 20, cx + 20, 46, C_DGRAY);
  }
  return d;
}
/* NEXUS orbiting fire-pod (drawn twice at boss.px[]/py[]) */
function build_bosspod() {
  const w = SH_POD_W, h = SH_POD_H;
  const d = new Uint8Array(w * h);
  const bset = (x, y, c) => { if (x >= 0 && x < w && y >= 0 && y < h) d[y*w+x] = c; };
  for (let y = -5; y <= 5; y++)
    for (let x = -5; x <= 5; x++) {
      const d2 = x*x + y*y;
      if (d2 >= 16 && d2 <= 25) bset(7+x, 6+y, C_LMAG);
      else if (d2 <= 9) bset(7+x, 6+y, PAL_GLOW + 8);
    }
  bset(7, 6, C_WHITE);
  return d;
}
function make_banked_ships() {
  const W = SH_SHIP_W, H = SH_SHIP_H;
  spr_ship[0] = new Uint8Array(W*H); spr_ship[2] = new Uint8Array(W*H);
  for (let y = 0; y < H; y++)
    for (let x = 0; x < W; x++) {
      const c = spr_ship[1][y*W+x];
      if (!c) continue;
      if (x > 0) spr_ship[0][y*W + x - (y > 6 ? 1 : 0)] = c;
      if (x < W-1) spr_ship[2][y*W + x + (y > 6 ? 1 : 0)] = c;
    }
  for (let y = 8; y < 13; y++) { spr_ship[0][y*W+2] = C_LGRAY; spr_ship[2][y*W+13] = C_LGRAY; }
}
function flip_vertical(src, w, h) {
  const dst = new Uint8Array(w*h);
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++) dst[y*w+x] = src[(h-1-y)*w+x];
  return dst;
}
function sprites_init() {
  make_banked_ships();
  for (let i = 0; i < 3; i++) spr_ship_down[i] = flip_vertical(spr_ship[i], SH_SHIP_W, SH_SHIP_H);
  spr_missile_down = flip_vertical(spr_missile, SH_MSL_W, SH_MSL_H);
  spr_pbullet[WT_LASER] = build_pbul(C_LCYAN, C_WHITE);
  spr_pbullet[WT_WAVE]  = build_pbul(C_LMAG,  C_WHITE);
  spr_powerup[PU_GUN]     = build_pu(PAL_FIRE+9);
  spr_powerup[PU_RAPID]   = build_pu(C_YELLOW);
  spr_powerup[PU_SHIELD]  = build_pu(C_LBLUE);
  spr_powerup[PU_LIFE]    = build_pu(C_LGREEN);
  spr_powerup[PU_MISSILE] = build_pu(C_LGRAY);
  spr_powerup[PU_LASER]   = build_pu(C_LCYAN);
  spr_powerup[PU_WAVE]    = build_pu(C_LMAG);
  spr_powerup[PU_BOMB]    = build_pu(C_LRED);
  spr_powerup[PU_SCORE]   = build_pu(C_WHITE);
  for (let b = 0; b < NBOSS; b++) spr_boss[b] = build_boss(b);
  spr_bosspod = build_bosspod();
}

/* ================= input ================= */
const keyState = {}, keyEdge = {};
const typedQueue = [];
const pointerAim = {
  active:false, x:SCRW >> 1, y:SCRH - 30, id:null,
  dragX:0, dragY:0, shipX:0, shipY:0
};
const KEYMAP = {
  ArrowLeft:'LEFT', ArrowRight:'RIGHT', ArrowUp:'UP', ArrowDown:'DOWN',
  KeyA:'LEFT', KeyD:'RIGHT', KeyW:'UP', KeyS:'DOWN',
  Space:'SPACE', ControlLeft:'CTRL', ControlRight:'CTRL',
  ShiftLeft:'SHIFT', ShiftRight:'SHIFT',
  KeyB:'B', KeyP:'P', KeyM:'M', KeyH:'H', Escape:'ESC',
  Enter:'ENTER', Backspace:'BKSP', KeyF:'F'
};
function key_pressed(k) { return !!keyState[k]; }
function key_hit(k) { if (keyEdge[k]) { keyEdge[k] = 0; return true; } return false; }
function kbd_getchar() { return typedQueue.length ? typedQueue.shift() : 0; }
function pressKey(k) {
  if (!keyState[k]) keyEdge[k] = 1;
  keyState[k] = 1;
}
function releaseKey(k) { keyState[k] = 0; }
function tapKey(k) { pressKey(k); keyState[k] = 0; }
function clearInput() {
  for (const k in keyState) keyState[k] = 0;
  pointerAim.active = false;
  pointerAim.id = null;
  resetMobileMove();
}
function eventGamePos(e) {
  const r = canvas.getBoundingClientRect();
  return {
    x: Math.max(0, Math.min(SCRW - 1, Math.floor((e.clientX - r.left) * SCRW / r.width))),
    y: Math.max(0, Math.min(SCRH - 1, Math.floor((e.clientY - r.top) * SCRH / r.height)))
  };
}
function setPointerAim(e) {
  const p = eventGamePos(e);
  pointerAim.x = p.x;
  pointerAim.y = p.y;
}
function beginTouchAim(e) {
  const p = eventGamePos(e);
  pointerAim.active = true;
  pointerAim.id = e.pointerId;
  pointerAim.dragX = p.x;
  pointerAim.dragY = p.y;
  pointerAim.shipX = player.x + (SH_SHIP_W >> 1);
  pointerAim.shipY = player.y + (SH_SHIP_H >> 1);
  pointerAim.x = pointerAim.shipX;
  pointerAim.y = pointerAim.shipY;
}
function updateTouchAim(e) {
  const p = eventGamePos(e);
  pointerAim.x = Math.max(0, Math.min(SCRW - 1, pointerAim.shipX + p.x - pointerAim.dragX));
  pointerAim.y = Math.max(0, Math.min(SCRH - 1, pointerAim.shipY + p.y - pointerAim.dragY));
}

window.addEventListener('keydown', (e) => {
  bootAudio();
  if (e.target && e.target.id === 'nameInput') {
    if (e.code === 'Enter') { pressKey('ENTER'); e.preventDefault(); }
    else if (e.code === 'Escape') { pressKey('ESC'); e.preventDefault(); }
    return;
  }
  const k = KEYMAP[e.code];
  if (k === 'F') { toggleFullscreen(); e.preventDefault(); return; }
  if (k) {
    pressKey(k);
    e.preventDefault();
  }
  /* name-entry characters */
  if (/^Key[A-Z]$/.test(e.code)) typedQueue.push(e.code[3]);
  else if (/^Digit[0-9]$/.test(e.code)) typedQueue.push(e.code[5]);
  else if (e.code === 'Space') typedQueue.push(' ');
  if (typedQueue.length > 8) typedQueue.length = 8;
  if (k === 'F' && keyEdge['F']) { /* handled in step() so it also works from title */ }
});
window.addEventListener('keyup', (e) => {
  const k = KEYMAP[e.code];
  if (k) { releaseKey(k); e.preventDefault(); }
});
window.addEventListener('blur', clearInput);

/* ================= audio (Web Audio: multi-voice music + ducked SFX) ================= */
let ac = null, masterGain = null, musicGain = null, sfxGain = null, noiseBuf = null;
let sfxMuted = false, musicMuted = false;
let musicTrack = -1, musicChapter = 0, musIdx = 0, nextNoteTime = 0, musicPaused = false;
const legacyMusicSources = new Set();
const titleMusicPlayer = createTitleMusicPlayer({
  context: () => ac,
  musicGain: () => musicGain,
  noiseBuffer: () => noiseBuf,
  score: TITLE_MUSIC_SCORE,
  maxEvents: 32,
  maxSources: 64
});

/* Legacy gameplay/victory melodies. The menu uses generated TITLE_MUSIC_SCORE spans. */
const game_mel = [
 [[110,7],[131,7],[147,8],[165,12],[0,5],[147,6],[131,6],[110,10],[0,8],[98,6],[110,6],[123,8],[131,10],[110,8],[0,8],[110,14]],
 [[123,6],[147,6],[185,8],[165,10],[0,4],[147,6],[123,8],[98,10],[0,7],[123,6],[131,6],[147,8],[185,8],[165,10],[0,6],[123,14]],
 [[131,6],[165,6],[196,8],[262,10],[0,5],[220,6],[196,6],[165,10],[0,7],[147,6],[165,6],[196,8],[175,8],[147,10],[0,7],[131,14]],
 [[147,5],[185,5],[220,7],[294,10],[0,4],[262,6],[220,7],[185,9],[0,6],[147,5],[175,5],[220,7],[196,7],[175,9],[0,6],[147,13]],
 [[98,7],[123,7],[147,7],[196,11],[0,4],[185,6],[147,7],[123,9],[0,8],[110,6],[147,6],[165,8],[147,8],[123,10],[0,6],[98,14]],
 [[165,5],[196,5],[247,7],[330,10],[0,4],[294,6],[247,6],[196,9],[0,6],[165,5],[185,5],[220,7],[247,7],[220,9],[0,7],[165,13]],
 [[175,6],[220,6],[262,8],[349,10],[0,5],[330,5],[262,7],[220,9],[0,6],[196,6],[220,6],[277,8],[262,8],[220,10],[0,6],[175,14]],
 [[110,5],[165,5],[220,7],[330,9],[0,4],[294,5],[220,6],[165,9],[0,6],[123,5],[185,5],[247,7],[220,7],[165,9],[0,7],[110,13]],
 [[196,5],[247,5],[294,7],[392,10],[0,4],[349,5],[294,6],[247,9],[0,6],[220,5],[262,5],[330,7],[294,8],[247,9],[0,7],[196,13]],
 [[123,5],[185,5],[247,7],[370,9],[0,4],[330,5],[247,6],[185,9],[0,6],[147,5],[220,5],[294,7],[247,8],[185,9],[0,7],[123,13]],
 [[220,5],[262,5],[330,7],[440,10],[0,4],[392,5],[330,6],[262,9],[0,6],[247,5],[294,5],[349,7],[330,8],[262,9],[0,7],[220,13]],
 [[147,5],[220,5],[294,7],[440,9],[0,4],[392,5],[294,6],[220,9],[0,6],[175,5],[262,5],[349,7],[294,8],[220,9],[0,7],[147,13]],
 [[247,5],[294,5],[370,7],[494,10],[0,4],[440,5],[370,6],[294,9],[0,6],[262,5],[330,5],[392,7],[370,8],[294,9],[0,7],[247,13]],
 [[165,4],[247,4],[330,6],[494,9],[0,4],[440,5],[330,6],[247,8],[0,5],[196,4],[294,4],[392,6],[330,7],[247,8],[0,6],[165,12]],
 [[262,4],[330,4],[392,6],[523,9],[0,4],[494,5],[392,5],[330,8],[0,5],[294,4],[349,4],[440,6],[392,7],[330,8],[0,6],[262,12]],
 [[294,4],[370,4],[440,6],[587,9],[0,3],[523,5],[440,5],[370,8],[0,5],[330,4],[392,4],[494,6],[440,7],[370,8],[0,6],[294,12]]
];
const win_mel = [
  [523,8],[659,8],[784,8],[1047,16],[0,4],
  [988,8],[880,8],[784,12],[659,8],[784,18],[0,6],
  [698,8],[880,8],[1047,8],[1175,18],[0,6],
  [1047,10],[784,10],[880,10],[1047,24],[0,14]];

function titleMusicStop() {
  titleMusicPlayer.stop();
  titleMusicPublish();
}
function titleMusicReset(delay = .06) {
  titleMusicPlayer.stop();
  if (!ac || ac.state !== 'running' || musicMuted || musicPaused || musicTrack !== MUS_TITLE) return;
  titleMusicPlayer.reset(delay);
  titleMusicPublish();
}
function titleMusicStart() {
  if (ac && ac.state === 'running' && !musicMuted && !musicPaused && musicTrack === MUS_TITLE) titleMusicPlayer.start();
  titleMusicPublish();
}
function titleMusicSchedule() {
  if (!ac || ac.state !== 'running' || musicMuted || musicPaused || musicTrack !== MUS_TITLE) return;
  titleMusicPlayer.schedule();
  titleMusicPublish();
}
function titleMusicAudioReady() {
  if (musicTrack === MUS_TITLE && !musicMuted && !musicPaused) titleMusicStart();
}
function titleMusicPublish() {
  const debug = titleMusicPlayer.debug(), root = document.documentElement;
  root.dataset.titleMusicContext = debug.context;
  root.dataset.titleMusicFaults = String(debug.faults);
  root.dataset.titleMusicLive = String(debug.liveSources);
  root.dataset.titleMusicPeak = String(debug.peakSources);
  root.dataset.titleMusicRunning = debug.running ? '1' : '0';
  root.dataset.titleMusicTotal = String(debug.totalScheduled);
}
function trackLegacyMusicSource(source) {
  legacyMusicSources.add(source);
  source.addEventListener('ended', () => legacyMusicSources.delete(source), { once: true });
}
function stopLegacyMusicSources() {
  const stopAt = ac ? ac.currentTime + .005 : 0;
  for (const source of legacyMusicSources) {
    try { source.stop(stopAt); } catch (_) {}
  }
  legacyMusicSources.clear();
}
function primeAudioUnlock() {
  if (!ac || ac.state === 'running' || !masterGain) return;
  try {
    const source = ac.createBufferSource();
    source.buffer = ac.createBuffer(1, 1, ac.sampleRate || 22050);
    source.connect(masterGain);
    source.start(0);
  } catch (_) {}
}

function bootAudio() {
  if (ac) {
    if (ac.state === 'suspended') {
      primeAudioUnlock();
      const resumed = ac.resume();
      if (resumed && resumed.then) resumed.then(titleMusicAudioReady).catch(() => {});
    }
    titleMusicPublish();
    return;
  }
  ac = new (window.AudioContext || window.webkitAudioContext)();
  /* soft limiter so overlapping SFX + music voices never hard-clip */
  const comp = ac.createDynamicsCompressor();
  comp.threshold.value = -10; comp.knee.value = 20; comp.ratio.value = 12;
  comp.attack.value = 0.003; comp.release.value = 0.2;
  comp.connect(ac.destination);
  masterGain = ac.createGain(); masterGain.gain.value = 0.5; masterGain.connect(comp);
  musicGain = ac.createGain(); musicGain.gain.value = 0.42; musicGain.connect(masterGain);
  sfxGain = ac.createGain(); sfxGain.gain.value = 0.55; sfxGain.connect(masterGain);
  noiseBuf = ac.createBuffer(1, ac.sampleRate, ac.sampleRate);
  const d = noiseBuf.getChannelData(0);
  for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
  if (ac.state === 'suspended') {
    primeAudioUnlock();
    const resumed = ac.resume();
    if (resumed && resumed.then) resumed.then(titleMusicAudioReady).catch(() => {});
  } else titleMusicAudioReady();
  ac.onstatechange = () => {
    if (ac.state === 'running') titleMusicAudioReady();
    else titleMusicStop();
  };
  nextNoteTime = ac.currentTime + 0.05;
  titleMusicPublish();
}
window.addEventListener('pointerdown', bootAudio, { capture: true, passive: true });
window.addEventListener('touchend', bootAudio, { capture: true, passive: true });
window.addEventListener('click', bootAudio, { capture: true, passive: true });
function snd_music_muted() { return musicMuted; }
function snd_sfx_muted() { return sfxMuted; }
function snd_music_toggle() {
  musicMuted = !musicMuted;
  if (musicMuted) { titleMusicStop(); stopLegacyMusicSources(); }
  if (musicGain && ac) {
    musicGain.gain.cancelScheduledValues(ac.currentTime);
    musicGain.gain.setTargetAtTime(musicMuted ? 0 : 0.42, ac.currentTime, 0.02);
  }
  if (!musicMuted && ac) {
    nextNoteTime = ac.currentTime + 0.05;
    if (musicTrack === MUS_TITLE) titleMusicReset();
  }
  updateAudioButtons();
}
function snd_sfx_toggle() {
  sfxMuted = !sfxMuted;
  if (sfxGain && ac) sfxGain.gain.setTargetAtTime(sfxMuted ? 0 : 0.55, ac.currentTime, 0.02);
  updateAudioButtons();
}
function duck(amount, secs) {
  if (!ac || musicMuted) return;
  musicGain.gain.cancelScheduledValues(ac.currentTime);
  musicGain.gain.setTargetAtTime(amount, ac.currentTime, 0.015);
  musicGain.gain.setTargetAtTime(0.42, ac.currentTime + secs, 0.12);
}
function voice(type, f0, f1, t0, dur, vol, dest) {
  const o = ac.createOscillator(), g = ac.createGain();
  o.type = type;
  o.frequency.setValueAtTime(f0, t0);
  if (f1 !== f0) o.frequency.exponentialRampToValueAtTime(Math.max(30, f1), t0 + dur);
  g.gain.setValueAtTime(0, t0);
  g.gain.linearRampToValueAtTime(vol, t0 + 0.008);
  g.gain.exponentialRampToValueAtTime(0.001, t0 + dur);
  o.connect(g); g.connect(dest || sfxGain);
  o.start(t0); o.stop(t0 + dur + 0.05);
  if (dest === musicGain) trackLegacyMusicSource(o);
}
function noise(t0, dur, vol, fc0, fc1, type, dest) {
  const s = ac.createBufferSource(); s.buffer = noiseBuf; s.loop = true;
  const f = ac.createBiquadFilter(); f.type = type || 'lowpass';
  f.frequency.setValueAtTime(fc0, t0);
  if (fc1 && fc1 !== fc0) f.frequency.exponentialRampToValueAtTime(fc1, t0 + dur);
  const g = ac.createGain();
  g.gain.setValueAtTime(0, t0);
  g.gain.linearRampToValueAtTime(vol, t0 + 0.006);
  g.gain.exponentialRampToValueAtTime(0.001, t0 + dur);
  s.connect(f); f.connect(g); g.connect(dest || sfxGain);
  s.start(t0); s.stop(t0 + dur + 0.05);
  if (dest === musicGain) trackLegacyMusicSource(s);
}
function snd_sfx(id) {
  if (!ac || sfxMuted) return;
  const t = ac.currentTime;
  switch (id) {
    case SFX_FIRE:    voice('square', 880, 500, t, 0.055, 0.16); break;
    case SFX_EXPLODE: noise(t, 0.34, 0.5, 900, 130); voice('sine', 150, 55, t, 0.3, 0.4); duck(0.15, 0.3); break;
    case SFX_POWER:   voice('square', 520, 520, t, 0.06, 0.2); voice('square', 780, 780, t+0.07, 0.09, 0.2); break;
    case SFX_HIT:     noise(t, 0.09, 0.3, 2000, 600); voice('sawtooth', 170, 90, t, 0.1, 0.22); break;
    case SFX_BOSS:    voice('sawtooth', 75, 160, t, 0.5, 0.3); voice('sawtooth', 76, 162, t, 0.5, 0.3); duck(0.2, 0.5); break;
    case SFX_MISSILE: noise(t, 0.24, 0.35, 2400, 300, 'bandpass'); break;
    case SFX_PICK1:   voice('triangle', 620, 620, t, 0.07, 0.28); voice('triangle', 930, 930, t+0.08, 0.12, 0.28); break;
    case SFX_PICK2:   voice('square', 430, 430, t, 0.05, 0.18); voice('square', 645, 645, t+0.06, 0.05, 0.18); voice('square', 860, 860, t+0.12, 0.08, 0.18); break;
    case SFX_COMBO:   voice('square', 760, 1500, t, 0.09, 0.2); break;
    case SFX_PHASE:   voice('sawtooth', 220, 440, t, 0.3, 0.26); duck(0.25, 0.3); break;
    case SFX_BOOST:   noise(t, 0.16, 0.2, 800, 3200, 'highpass'); break;
  }
}
function snd_music_set(track) {
  titleMusicStop();
  stopLegacyMusicSources();
  musicTrack = track; musIdx = 0; musicPaused = false;
  if (ac) {
    /* clear any pending SFX "duck" ramps and force full music level, so
       switching tracks (or returning to the menu) always restores volume */
    musicGain.gain.cancelScheduledValues(ac.currentTime);
    musicGain.gain.setValueAtTime(0.42, ac.currentTime);
    nextNoteTime = ac.currentTime + 0.08;
    if (track === MUS_TITLE) titleMusicReset(.08);
  }
}
function snd_music_game(chapter) {
  musicChapter = clamp(Math.floor(chapter || 0), 0, 15);
  snd_music_set(MUS_GAME);
}
function snd_music_stop() { titleMusicStop(); stopLegacyMusicSources(); musicTrack = -1; }

/* Pause/resume audio to mirror the DOS build's snd_silence() on pause.
   Ramps music to silence (killing already-scheduled notes routed through
   musicGain) and stops new notes from being scheduled. */
function snd_pause(p) {
  musicPaused = p;
  if (!ac) return;
  if (p) {
    titleMusicStop();
    stopLegacyMusicSources();
    musicGain.gain.cancelScheduledValues(ac.currentTime);
    musicGain.gain.setValueAtTime(0, ac.currentTime);
  } else {
    musicGain.gain.cancelScheduledValues(ac.currentTime);
    musicGain.gain.setValueAtTime(0.42, ac.currentTime);
  }
  if (!p) {
    nextNoteTime = ac.currentTime + 0.05;
    if (musicTrack === MUS_TITLE) titleMusicReset();
  }
}

/* three-voice arrangement: lead + sub-octave bass + noise hat */
function scheduleMusic() {
  if (!ac || ac.state !== 'running' || musicMuted || musicPaused || musicTrack < 0) return;
  if (musicTrack === MUS_TITLE) { titleMusicSchedule(); return; }
  if (nextNoteTime < ac.currentTime - 0.1) nextNoteTime = ac.currentTime + 0.05; /* tab-switch snap */
  const mel = musicTrack === MUS_WIN ? win_mel : game_mel[musicChapter];
  while (nextNoteTime < ac.currentTime + 0.30) {
    const [f, frames] = mel[musIdx];
    const dur = frames / LOGIC_HZ;   /* frame-locked tempo, like snd_update() */
    const t = nextNoteTime;
    if (f > 0) {
      /* Gameplay groove remains on the proven bounded legacy scheduler. */
      const leadType = (musicChapter % 3 === 0) ? 'triangle' : (musicChapter % 3 === 1) ? 'square' : 'sawtooth';
      voice(leadType, f * 2, f * 2, t, dur, 0.20, musicGain);
      voice('square', f * (musicChapter & 1 ? 2.5 : 3), f * (musicChapter & 1 ? 2.5 : 3), t, dur * 0.5, 0.05, musicGain);
      voice('square', f, f, t, dur, 0.16, musicGain);
      if ((musIdx & 3) === 0) noise(t, 0.03, 0.05, 4000, 4000, 'highpass', musicGain);
    }
    nextNoteTime += dur;
    musIdx = (musIdx + 1) % mel.length;
  }
}

document.addEventListener('visibilitychange', () => {
  if (document.hidden) titleMusicStop();
  else if (ac && ac.state === 'running' && musicTrack === MUS_TITLE) titleMusicReset();
});

window.AYRIEN_TITLE_MUSIC_DEBUG = () => Object.assign(titleMusicPlayer.debug(), {
  cue: musicTrack === MUS_TITLE ? 'TITLE' : musicTrack === MUS_GAME ? 'GAME' : musicTrack === MUS_WIN ? 'WIN' : 'NONE',
  chapter: musicChapter,
  legacyLiveSources: legacyMusicSources.size,
  paused: musicPaused
});

/* ================= high scores (server + local fallback) ================= */
const HS_KEY = 'ayrien_assault_hiscores';
const LEGACY_HS_KEY = 'stellar_assault_hiscores';
const SCORE_API = 'api/ayrien-scores.ashx';
const DEF_NAMES = ['ACE','NOVA','COMET','ORION','VEGA','PULSAR','ROOKIE','CADET','DRIFT','PIXEL'];
let g_hi = [];
let scoreStatus = 'SYNCING SCORES';
let scoresOnline = false;
function cleanName(s) { return String(s || '').toUpperCase().replace(/[^A-Z0-9 $]/g, '').slice(0, NAME_LEN); }
function hi_defaults() {
  g_hi = DEF_NAMES.map((n, i) => ({ name: n, score: (HISCORE_N - i) * 1000 }));
}
function cleanScoreList(v) {
  if (!Array.isArray(v)) return null;
  const list = v.filter(s => s && Number.isFinite(Number(s.score)))
    .map(s => ({
      name: cleanName(s.name || 'PLAYER') || 'PLAYER',
      score: Math.max(0, Math.floor(Number(s.score) || 0)),
      wave: Math.max(0, Math.floor(Number(s.wave) || 0)),
      maxCombo: Math.max(0, Math.floor(Number(s.maxCombo) || 0)),
      bosses: Math.max(0, Math.floor(Number(s.bosses) || 0))
    }))
    .sort((a, b) => (b.score - a.score) || (b.wave - a.wave) || (b.maxCombo - a.maxCombo))
    .slice(0, HISCORE_N);
  if (!list.length) return null;
  for (let i = list.length; i < HISCORE_N; i++) {
    list.push({ name: DEF_NAMES[i] || 'PLAYER', score: (HISCORE_N - i) * 1000, wave: 1, maxCombo: 0, bosses: 0 });
  }
  return list
    .sort((a, b) => (b.score - a.score) || (b.wave - a.wave) || (b.maxCombo - a.maxCombo))
    .slice(0, HISCORE_N);
}
function setScoreStatus(s) {
  scoreStatus = s;
  const el = document.getElementById('scoreStatus');
  if (el) el.textContent = s;
}
function hi_load() {
  try {
    const saved = localStorage.getItem(HS_KEY) || localStorage.getItem(LEGACY_HS_KEY);
    const v = cleanScoreList(JSON.parse(saved));
    if (v) { g_hi = v; hi_save(); return; }
  } catch (e) {}
  hi_defaults();
}
function hi_save() { try { localStorage.setItem(HS_KEY, JSON.stringify(g_hi)); } catch (e) {} }
function hi_qualifies(s) { for (let i = 0; i < HISCORE_N; i++) if (s > g_hi[i].score) return i; return -1; }
function hi_insert(rank, name, s) {
  g_hi.splice(rank, 0, { name: name.slice(0, NAME_LEN), score: s, wave, maxCombo: player.max_combo || 0, bosses: bosses_defeated || 0 });
  g_hi.length = HISCORE_N;
}
async function syncScores() {
  try {
    const r = await fetch(SCORE_API, { cache: 'no-store' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const data = await r.json();
    const list = cleanScoreList(data.scores);
    if (!data.ok || !list) throw new Error(data.error || 'bad scores');
    g_hi = list; hi_save(); scoresOnline = true; setScoreStatus('ONLINE SCORES');
  } catch (e) {
    scoresOnline = false; setScoreStatus('LOCAL FALLBACK');
  }
}
async function submitScore(name, finalScore) {
  setScoreStatus('SAVING SCORE');
  try {
    const r = await fetch(SCORE_API, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name, score: finalScore, wave: last_wave || wave, maxCombo: last_combo || player.max_combo || 0, bosses: last_bosses || bosses_defeated || 0 })
    });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const data = await r.json();
    const list = cleanScoreList(data.scores);
    if (!data.ok || !list) throw new Error(data.error || 'bad scores');
    g_hi = list; hi_save(); scoresOnline = true; setScoreStatus('SCORE SAVED');
  } catch (e) {
    hi_save(); scoresOnline = false; setScoreStatus('LOCAL FALLBACK');
  }
}

/* ================= game state (game.c) ================= */
const player = {};
const pbul = [], ebul = [], enemies = [], powr = [], part = [], stars = [], mslA = [], blast = [], dust = [], popup = [];
for (let i = 0; i < MAX_PBULLET; i++) pbul.push({active:false});
for (let i = 0; i < MAX_EBULLET; i++) ebul.push({active:false});
for (let i = 0; i < MAX_ENEMY; i++) enemies.push({active:false});
for (let i = 0; i < MAX_POWERUP; i++) powr.push({active:false, t:0, type:0});
for (let i = 0; i < MAX_PART; i++) part.push({active:false});
for (let i = 0; i < MAX_STARS; i++) stars.push({});
for (let i = 0; i < MAX_MISSILE; i++) mslA.push({active:false});
for (let i = 0; i < 10; i++) blast.push({active:false});
for (let i = 0; i < 36; i++) dust.push({});
for (let i = 0; i < 4; i++) popup.push({active:false});

/* floating score/combo popup: rises for ~30 frames then fades out */
function spawn_popup(x, y, s) {
  for (const p of popup) if (!p.active) {
    p.active = true; p.t = 30;
    p.x = clamp(x, 2, SCRW - 8 * s.length - 2);
    p.y = Math.max(12, y);
    p.txt = s;
    return;
  }
}

const boss = { active:false };
let score = 0, wave = 0, to_spawn = 0, spawn_cd = 0, wave_mix = 0;
let flash = 0, shk = 0, shx = 0, shy = 0;
let wave_banner = 0, msg_timer = 0, msg_text = '';
let frame = 0;
let g_diff = DIF_NORMAL;
let last_death_score = 0;
let wave_kills = 0, wave_missed = 0, wave_hit = 0, combo_broken = 0;
let risk_spawned = 0, bosses_defeated = 0, last_wave = 0, last_combo = 0, last_bosses = 0;
let ship_bank = 1;
let campaign_won = 0, win_pending = 0, win_t = 0;
let state = ST_TITLE, paused = false, mobileOrientationSuspended = false;
const HELP_PAGES = 6;
let entry_rank = -1, entry_name = '', over_timer = 0, help_page = 0;
let entrySubmitted = false, entryNameError = false, entryReplay = false, uiState = null;
let pilotName = '';

function set_msg(s) { msg_text = s; msg_timer = 120; }
function score_scaled(raw) {
  if (g_diff === DIF_HARD) return Math.floor((raw * 160 + 50) / 100);
  if (g_diff === DIF_NORMAL) return Math.floor((raw * 125 + 50) / 100);
  return raw;
}
function score_add(raw) { score += score_scaled(raw); }
function diff_spawn_cd_min() { return g_diff === DIF_EASY ? 22 : g_diff === DIF_HARD ? 13 : 16; }
function diff_spawn_cd_max() { return g_diff === DIF_EASY ? 36 : g_diff === DIF_HARD ? 24 : 30; }
function diff_enemy_fire_adjust() { return g_diff === DIF_EASY ? 15 : g_diff === DIF_HARD ? -18 : 0; }
function diff_boss_hp_mul() { return g_diff === DIF_EASY ? 7 : g_diff === DIF_HARD ? 14 : 10; }
function diff_boss_fire_cd() {
  const d = g_diff === DIF_EASY ? 8 : g_diff === DIF_HARD ? -8 : 0;
  switch (boss.kind) {
    case 0: return 60 + d - boss.phase * 8;
    case 1: return 34 + d - boss.phase * 6;    /* dives carry the threat */
    case 2: return 66 + d - boss.phase * 8;
    case 3: return 44 + d - boss.phase * 9;
    case 4: return 46 + d - boss.phase * 8;    /* lunges carry the threat */
    case 5: return 62 + d - boss.phase * 10;
    case 6: return 40 + d - boss.phase * 8;
    case 7: return 38 + d - boss.phase * 7;
    case 8: return 58 + d - boss.phase * 8;
    case 9: return 34 + d - boss.phase * 7;
    case 10: return 48 + d - boss.phase * 8;
    case 11: return 36 + d - boss.phase * 7;
    case 12: return 56 + d - boss.phase * 8;   /* guillotine carries the threat */
    case 13: return 50 + d - boss.phase * 8;
    default: return 34 + d - boss.phase * 6;
  }
}
function boss_attack_count() { return (boss.kind === 2 || boss.kind === 8) ? 2 : boss.kind === 14 ? 4 : 3; }
function boss_atk_time() {
  let t;
  switch (boss.kind) {
    case 0: t = 140; break; case 1: t = 84; break; case 2: t = 150; break;
    case 5: t = 132; break; case 8: t = 142; break; case 10: t = 128; break;
    case 13: t = 120; break; case 14: t = 112; break; default: t = 118; break;
  }
  t -= boss.phase * ((boss.kind === 0 || boss.kind === 5 || boss.kind === 13) ? 22 : 24);
  return Math.max(48, t);
}
function boss_pct_damage(div) { return Math.max(1, Math.ceil(boss.maxhp / div)); }
function shooter_lingering(e) { return e.y > 40 && e.y < 120 && e.t < 140; }
function boss_health_for(w, kind) {
  const hpWave = Math.min(w, 60), hpIndex = w > 60 ? 14 : Math.max(0, Math.floor(w / 4) - 1);
  return 36 + hpWave * diff_boss_hp_mul() + hpIndex * 8 + BOSSDEF[kind].hpbonus;
}
function enemy_score(e) {
  if (e.elite) return e.type === E_SCOUT ? 180 : e.type === E_WEAVER ? 240 : 375;
  return e.type === E_SCOUT ? 100 : e.type === E_WEAVER ? 150 : 250;
}
function overlap(ax, ay, aw, ah, bx, by, bw, bh) {
  return ax < bx+bw && ax+aw > bx && ay < by+bh && ay+ah > by;
}
function free_bullet(arr) { for (const b of arr) if (!b.active) return b; return null; }

function spawn_part(x, y, col) {
  for (const p of part) if (!p.active) {
    p.active = true; p.x = x; p.y = y;
    p.dx = rrange(-4,4); p.dy = rrange(-4,4);
    p.life = rrange(10,22); p.col = col;
    return;
  }
}
function burst(x, y, n, c1, c2) { for (let i = 0; i < n; i++) spawn_part(x, y, (i&1) ? c1 : c2); }
function fireburst(x, y, n) { for (let i = 0; i < n; i++) spawn_part(x, y, 0); }
function typeburst(x, y, type, elite) {
  if (type === E_SCOUT) burst(x, y, 10, PAL_FIRE+12, C_YELLOW);
  else if (type === E_WEAVER) burst(x, y, 14, C_LGREEN, PAL_GLOW+10);
  else burst(x, y, 16, C_LMAG, C_WHITE);
  if (elite) burst(x, y, 8, C_WHITE, C_LCYAN);
}
function part_col(p) {
  if (p.col) return p.col;
  const l = Math.min(15, p.life);
  return PAL_FIRE + l;
}
function remember_run() { last_wave = wave; last_combo = player.max_combo; last_bosses = bosses_defeated; }

/* nebula background */
function gen_nebula() {
  bg.fill(C_BLACK);
  for (let b = 0; b < 9; b++) {
    const cx = rrange(20, SCRW-20), cy = rrange(10, SCRH-10), r = rrange(24, 55);
    const r2 = r * r;
    for (let y = cy-r; y <= cy+r; y++) {
      const wy = ((y + SCRH) % SCRH + SCRH) % SCRH;
      for (let x = cx-r; x <= cx+r; x++) {
        if (x < 0 || x >= SCRW) continue;
        const d2 = (x-cx)*(x-cx) + (y-cy)*(y-cy);
        if (d2 >= r2) continue;
        let v = Math.floor((r2 - d2) * 10 / r2);
        if (v > 2 && ((x ^ wy) & 1)) v -= 2;
        let cur = bg[wy*SCRW + x];
        cur = cur >= PAL_NEB ? cur - PAL_NEB : 0;
        v = Math.min(13, v + cur);
        if (v) bg[wy*SCRW + x] = PAL_NEB + v;
      }
    }
  }
  for (let i = 0; i < 260; i++) {
    const o = (rnd() % 200) * SCRW + rnd() % SCRW;
    if (bg[o] === C_BLACK) bg[o] = PAL_NEB + 2 + rnd() % 3;
  }
}

function reset_game() {
  for (const a of [pbul, ebul, enemies, powr, part, mslA, blast, popup]) for (const o of a) o.active = false;
  boss.active = false;
  player.x = 152; player.y = 168;
  player.lives = g_diff === DIF_EASY ? 5 : g_diff === DIF_HARD ? 2 : 3;
  player.gun = GUN_MIN; player.wtype = WT_CANNON;
  player.msl = 5; player.bombs = g_diff === DIF_HARD ? 1 : 2;
  player.shield = g_diff === DIF_EASY ? 200 : 0;   /* easy: brief invuln head-start */
  player.invuln = 0; player.firecd = 0; player.rapid = 0; player.wave_boost = 0;
  player.boost = BOOST_MAX; player.boost_cd = 0; player.boosting = false;
  player.facing_down = false;
  player.combo = 0; player.combo_t = 0; player.max_combo = 0; player.ram_cd = 0; player.alive = true;
  score = 0; wave = 0; flash = 0; shk = 0;
  wave_banner = 0; msg_timer = 0; msg_text = ''; ship_bank = 1;
  wave_kills = wave_missed = wave_hit = combo_broken = 0;
  risk_spawned = 0; bosses_defeated = 0; campaign_won = 0; win_pending = 0; win_t = 0;
  to_spawn = 0; spawn_cd = 30; last_death_score = 0;
}
function init_stars() {
  for (let i = 0; i < MAX_STARS; i++) {
    stars[i].x = rrange(0, SCRW-1); stars[i].y = rrange(0, SCRH-1);
    stars[i].layer = i % 3;
    stars[i].col = stars[i].layer === 0 ? C_DGRAY : stars[i].layer === 1 ? C_LGRAY : C_WHITE;
  }
}
function init_dust() {
  for (let i = 0; i < 36; i++) {
    dust[i].x = rrange(0, SCRW-1); dust[i].y = rrange(0, SCRH-1);
    dust[i].layer = 1 + (i % 2);
    dust[i].col = PAL_NEB + 6 + rnd() % 7;
  }
}

/* ---------------- wave / spawn ---------------- */
function finish_wave() {
  if (wave <= 0) return;
  let bonus = 0;
  if (!wave_hit) bonus += 1000;
  if (!wave_missed) bonus += 750;
  if (!combo_broken && player.combo >= 3) bonus += 500;
  if (wave % 4 === 0 && player.bombs > 0) bonus += player.bombs * 400;
  if (bonus > 0) {
    const award = score_scaled(bonus);
    score += award;
    set_msg('WAVE BONUS ' + award);
  }
}
function start_wave() {
  wave++;
  wave_banner = 110;
  wave_kills = wave_missed = wave_hit = combo_broken = 0;
  risk_spawned = 0;
  vga_set_theme(Math.floor((wave - 1) / 4));
  if (wave % 10 === 0) {
    const award = score_scaled(2000);
    score += award;
    set_msg('ENDURANCE +' + award);
  }
  if (wave === 60 || wave % 4 === 0) {
    const boss_index = Math.floor(wave / 4) - 1;
    boss.active = true; boss.entering = true;
    boss.kind = boss_index < BOSSDEF.length ? boss_index : boss_index % BOSSDEF.length;
    const bd = BOSSDEF[boss.kind];
    boss.spr = bd.spr; boss.mvt = boss.kind; boss.atkset = boss.kind;
    boss.w = spr_boss_w[boss.spr]; boss.h = spr_boss_h[boss.spr];
    boss.phase = 0; boss.last_phase = 0; boss.summons = 0;
    boss.atk = 0; boss.atk_t = boss_atk_time(); boss.spin = 0;
    boss.x = (SCRW - boss.w) >> 1; boss.y = -boss.h;
    boss.maxhp = boss.hp = boss_health_for(wave, boss.kind);
    if (boss.maxhp < 20) boss.maxhp = boss.hp = 20;
    boss.dir = 1; boss.t = 0; boss.firecd = 60; boss.charge = 0;
    boss.tx = boss.x; boss.ty = 0; boss.mv_t = 50;
    boss.launch_t = 90; boss.squads = 0;
    boss.support_t = 260; boss.drop_budget = 4; boss.recent_dmg = 0;
    boss.hurt_t = 0; boss.dive_t = 0; boss.die_t = 0;
    boss.px = [0, 0]; boss.py = [0, 0];
    boss.warn = 70;                     /* WARNING banner before the entrance */
    to_spawn = 0;
    snd_sfx(SFX_BOSS);
  } else {
    to_spawn = 5 + wave;
    if (to_spawn > 18) to_spawn = 18;
    if (g_diff === DIF_HARD) to_spawn += 4;
    spawn_cd = g_diff === DIF_EASY ? 100 : g_diff === DIF_HARD ? 56 : 82;
    wave_mix = Math.floor((wave - 1) / 4) & 3;
  }
}
function pick_type() {
  const r = rnd() % 100;
  if (wave < 3) return r < 70 ? E_SCOUT : r < 90 ? E_WEAVER : E_SHOOTER;
  if (wave_mix === 0) return r < 62 ? E_SCOUT : r < 82 ? E_WEAVER : E_SHOOTER;
  if (wave_mix === 1) return r < 25 ? E_SCOUT : r < 74 ? E_WEAVER : E_SHOOTER;
  if (wave_mix === 2) return r < 24 ? E_SCOUT : r < 50 ? E_WEAVER : E_SHOOTER;
  return r < 34 ? E_SCOUT : r < 67 ? E_WEAVER : E_SHOOTER;
}
function init_enemy(e, type, x) {
  e.active = true; e.type = type;
  e.base = x; e.x = x; e.y = -SH_EN_H;
  e.t = rrange(0, 63);
  e.vy = 1 + (wave > 6 ? 1 : 0) + (wave > 24 ? 1 : 0) + (g_diff === DIF_HARD ? 1 : 0);
  e.firecd = rrange(40, 90) + diff_enemy_fire_adjust();
  let elite_chance = wave >= 20 ? 22 : wave >= 13 ? 18 : 12;
  if (g_diff === DIF_EASY) elite_chance -= 4;
  if (g_diff === DIF_HARD) elite_chance += 5;
  e.elite = (wave >= 7 && (rnd() % 100) < elite_chance) ? 1 : 0;
  e.mode = 0; e.drop_class = DROP_NORMAL; e.aux = 0;
  if (type === E_SCOUT)   { e.hp = 1; e.vy += 1; }
  if (type === E_WEAVER)  { e.hp = 2; }
  if (type === E_SHOOTER) { e.hp = 3; if (rnd() % 100 < 35) { e.mode = 1; e.aux = (rnd() & 1) ? 1 : -1; } }
  if (e.elite) {
    if (type === E_SCOUT) { e.hp = 2; e.vy++; }
    else if (type === E_WEAVER) e.hp = 3;
    else e.hp = 4;
  }
}
function spawn_one(type, x, y, mode) {
  if (x < 4) x = 4;
  if (x > SCRW - SH_EN_W - 4) x = SCRW - SH_EN_W - 4;
  for (const e of enemies) if (!e.active) {
    init_enemy(e, type, x);
    e.y = y; e.mode = mode;
    return e;
  }
  return null;
}
function free_enemy_slots() {
  let n = 0;
  for (let i = 0; i < MAX_ENEMY; i++) if (!enemies[i].active) n++;
  return n;
}
function active_enemy_count() {
  let n = 0; for (const e of enemies) if (e.active) n++; return n;
}
function summon_escort() {
  const size = 2 + (boss.phase >= 1 ? 1 : 0);
  if (active_enemy_count() >= 6 || free_enemy_slots() < size) return;
  const type = (boss.kind === 3 || boss.kind === 11) ? E_WEAVER : E_SCOUT;
  const x0 = boss.x < 80 ? boss.x + 36 : boss.x - 28;
  for (let n = 0; n < size; n++) {
    const e = spawn_one(type, x0 + n*20, -SH_EN_H - n*12, 0);
    if (e) e.drop_class = n === 0 ? DROP_SUPPLY : DROP_SUPPORT;
  }
  set_msg('SUPPLY ESCORTS');
}
/* carrier: launch a fighter squad from the two bays, only if the shared pool
   has room for the whole squad (never partial / never overflow MAX_ENEMY). */
function launch_squad() {
  const size = 3 + (boss.phase >= 1 ? 1 : 0);
  if (active_enemy_count() >= 6 || free_enemy_slots() < size) return;
  const lx = boss.x + 6, rx = boss.x + boss.w - 6 - SH_EN_W;
  for (let n = 0; n < size; n++) {
    const e = spawn_one(E_SCOUT, (n & 1) ? rx : lx, -SH_EN_H - n*10, 0);
    if (e) e.drop_class = n === 0 ? DROP_SUPPLY : DROP_SUPPORT;
  }
  boss.squads++;
  set_msg('SUPPLY FIGHTERS');
}
function spawn_enemy() {
  if (to_spawn >= 3 && (rnd() % 100) < 26) {
    const type = pick_type();
    const count = 3 + rnd() % 3;
    const form = rnd() % 4;
    let x0 = rrange(28, SCRW - 92);
    if (x0 < 8) x0 = 8;
    for (let n = 0; n < count && to_spawn > 0; n++) {
      let x = x0, y = -SH_EN_H - n*16, t = type;
      if (form === 1) { x = x0 + n*22; y = -SH_EN_H; }
      else if (form === 2) { x = x0 + (n - (count>>1))*18; y = -SH_EN_H - ((count>>1) - (n > (count>>1) ? count-n-1 : n))*14; }
      else if (form === 3) { t = (n & 1) ? E_WEAVER : type; x = x0 + n*18; }
      else x = x0 + n*20;
      for (const e of enemies) if (!e.active) {
        init_enemy(e, t, x);
        e.y = y;
        e.t = form === 3 ? n*12 : 16;
        to_spawn--;
        break;
      }
    }
    return;
  }
  for (const e of enemies) if (!e.active) {
    /* pick a drip x that doesn't drop straight onto a fresh spawn */
    let x = rrange(8, SCRW - SH_EN_W - 8);
    for (let tries = 0; tries < 4; tries++) {
      let clear = true;
      for (const o of enemies) if (o.active && o.y < 30 && Math.abs(x - o.x) < SH_EN_W + 2) { clear = false; break; }
      if (clear) break;
      x = rrange(8, SCRW - SH_EN_W - 8);
    }
    init_enemy(e, pick_type(), x);
    to_spawn--;
    return;
  }
}

/* One separation pass: push overlapping enemy pairs 1 px apart so formations
   and drips never sit stacked. Weavers recompute x from base each frame, so
   their push goes through base. Mirrors the native separate_enemies(). */
function separate_enemies() {
  for (let a = 0; a < MAX_ENEMY - 1; a++) {
    if (!enemies[a].active) continue;
    for (let b = a + 1; b < MAX_ENEMY; b++) {
      if (!enemies[b].active) continue;
      const dy = enemies[a].y - enemies[b].y;
      if (dy >= 12 || dy <= -12) continue;
      const dx = enemies[a].x - enemies[b].x;
      if (dx >= 14 || dx <= -14) continue;
      let lo, hi;
      if (dx < 0 || (dx === 0 && (a & 1))) { lo = enemies[a]; hi = enemies[b]; }
      else { lo = enemies[b]; hi = enemies[a]; }
      const lk = lo.type === E_WEAVER ? 'base' : 'x';
      const hk = hi.type === E_WEAVER ? 'base' : 'x';
      let moved = false;
      if (lo[lk] > 4) { lo[lk]--; moved = true; }
      if (hi[hk] < SCRW - SH_EN_W - 4) { hi[hk]++; moved = true; }
      if (!moved) enemies[b].y++;      /* both pinned: slip one downward */
    }
  }
}

/* ---------------- firing ---------------- */
function add_pbullet(x, y, dx, dy, kind) {
  const b = free_bullet(pbul);
  if (b) { b.active = true; b.x = x; b.y = y; b.dx = dx; b.dy = dy; b.kind = kind;
           b.grazed = 0; b.dmg = kind === WT_LASER ? 2 : 1; }
}
function update_player_facing() {
  if (!boss.active || boss.entering || boss.die_t > 0) {
    player.facing_down = false;
    return;
  }
  const py = player.y + (SH_SHIP_H>>1), by = boss.y + (boss.h>>1);
  if (!player.facing_down && py <= by - 6) player.facing_down = true;
  else if (player.facing_down && py >= by + 6) player.facing_down = false;
}
function player_fire() {
  const cx = player.x + (SH_SHIP_W>>1) - (SH_PB_W>>1);
  const dir = player.facing_down ? 1 : -1;
  const cy = player.facing_down ? player.y + SH_SHIP_H : player.y - 4;
  const g = player.gun;
  let cd;
  switch (player.wtype) {
  case WT_LASER:
    if (g === 1) add_pbullet(cx, cy+dir*2, 0, dir*12, WT_LASER);
    else if (g === 2) {
      add_pbullet(cx-4, cy, 0, dir*12, WT_LASER); add_pbullet(cx+4, cy, 0, dir*12, WT_LASER);
    } else if (g === 3) {
      add_pbullet(cx-7, cy, 0, dir*12, WT_LASER); add_pbullet(cx, cy+dir*2, 0, dir*12, WT_LASER);
      add_pbullet(cx+7, cy, 0, dir*12, WT_LASER);
    } else {
      add_pbullet(cx-10, cy, 0, dir*12, WT_LASER); add_pbullet(cx-4, cy+dir*2, 0, dir*12, WT_LASER);
      add_pbullet(cx+4, cy+dir*2, 0, dir*12, WT_LASER); add_pbullet(cx+10, cy, 0, dir*12, WT_LASER);
    }
    cd = 7;
    break;
  case WT_WAVE: {
    const spread = g + 1;
    if (player.wave_boost > 0) {
      for (let k = -spread; k <= spread; k++) add_pbullet(cx + k*2, cy+dir*2, k, dir*8, WT_LASER);
      add_pbullet(cx, cy+dir*4, 0, dir*11, WT_LASER);
      cd = 11;
    } else {
      for (let k = -spread; k <= spread; k++) add_pbullet(cx + k*2, cy, k, dir*5, WT_WAVE);
      cd = 15;
    }
    break; }
  default:
    switch (g) {
    case 1: add_pbullet(cx-3, cy, 0, dir*7, 0); add_pbullet(cx+3, cy, 0, dir*7, 0); break;
    case 2: add_pbullet(cx, cy+dir*2, 0, dir*7, 0);
            add_pbullet(cx-5, cy, 0, dir*7, 0); add_pbullet(cx+5, cy, 0, dir*7, 0); break;
    case 3: add_pbullet(cx, cy+dir*2, 0, dir*7, 0);
            add_pbullet(cx-5, cy, -1, dir*7, 0); add_pbullet(cx+5, cy, 1, dir*7, 0); break;
    default: add_pbullet(cx, cy+dir*2, 0, dir*7, 0);
             add_pbullet(cx-5, cy, 0, dir*7, 0); add_pbullet(cx+5, cy, 0, dir*7, 0);
             add_pbullet(cx-7, cy-dir*2, -2, dir*7, 0); add_pbullet(cx+7, cy-dir*2, 2, dir*7, 0); break;
    }
    cd = 9;
    break;
  }
  snd_sfx(SFX_FIRE);
  player.firecd = player.rapid > 0 ? (cd > 4 ? cd - 4 : 2) : cd;
}
function fire_missile() {
  if (player.msl <= 0) return;
  for (const m of mslA) if (!m.active) {
    m.active = true;
    m.x = player.x + (SH_SHIP_W>>1) - (SH_MSL_W>>1);
    m.y = player.facing_down ? player.y + SH_SHIP_H : player.y - SH_MSL_H;
    m.dx = 0; m.dy = player.facing_down ? 4 : -4;
    player.msl--;
    snd_sfx(SFX_MISSILE);
    return;
  }
}
function enemy_fire(ex, ey) {
  const b = free_bullet(ebul);
  if (!b) return;
  b.active = true; b.x = ex; b.y = ey;
  b.dx = player.x + 8 > ex ? 1 : -1;
  b.dy = 3; b.kind = 0; b.grazed = 0; b.dmg = 1;
}
function add_ebullet(x, y, dx, dy) {
  const b = free_bullet(ebul);
  if (!b) return;
  b.active = true; b.x = x; b.y = y; b.dx = dx; b.dy = dy;
  b.kind = 0; b.grazed = 0; b.dmg = 1;
}
function boss_fire() {
  const bx = boss.x + (boss.w>>1), by = boss.y + boss.h - 6;
  const dir = player.x + 8 > bx ? 1 : -1;
  const hard = g_diff === DIF_HARD;
  switch (boss.kind) {
  case 0:                                    /* GORGON */
    if (boss.atk === 1) {
      const lo = g_diff === DIF_EASY ? -2 : (g_diff === DIF_HARD && boss.phase >= 1) ? -4 : -3;
      const gapLo = lo + 1, gapHi = -lo - (g_diff === DIF_EASY ? 1 : 0);
      const gap = clamp(Math.trunc((player.x + 8 - bx) / 10), gapLo, gapHi);
      for (let k = lo; k <= -lo; k++) {
        let open = k === gap || k === gap - 1;
        if (g_diff === DIF_EASY && k === gap + 1) open = true;
        if (!open) add_ebullet(bx + k * 10, by, 0, 3);
      }
      if (g_diff === DIF_HARD && boss.phase >= 2) add_ebullet(bx, by, dir, 5);
    } else if (boss.atk === 2) {
      for (let k = -4; k <= 4; k += 2) add_ebullet(bx + k * 7, by, Math.trunc(k / 2), 3);
      if (boss.phase >= 2) add_ebullet(bx, by, 0, 5);
    } else {
      const lo = g_diff === DIF_EASY ? -2 : (g_diff === DIF_HARD && boss.phase >= 1) ? -4 : -3;
      for (let k = lo; k <= -lo; k++) add_ebullet(bx + k * 10, by, 0, (k & 1) ? 4 : 3);
    }
    break;
  case 1:                                    /* REAPER */
    if (boss.atk === 1) {
      add_ebullet(bx-6, by, -1, 5); add_ebullet(bx+6, by, 1, 5);
      if (boss.phase >= 1) add_ebullet(bx, by, dir, 6);
    } else if (boss.atk === 2) {
      add_ebullet(bx-2, by, dir, 6); add_ebullet(bx+2, by, dir, 6);
    } else {
      add_ebullet(bx, by, dir*2, 5); add_ebullet(bx, by, dir, 6); add_ebullet(bx, by, 0, 7);
    }
    break;
  case 2:                                    /* LEVIATHAN */
    if (boss.atk === 1) {
      add_ebullet(boss.x + 14, by, dir, 3); add_ebullet(boss.x + boss.w - 14, by, dir, 3);
    } else {
      add_ebullet(bx, by, 0, 3);
      if (boss.phase >= 1) { add_ebullet(bx - 16, by, -1, 3); add_ebullet(bx + 16, by, 1, 3); }
    }
    break;
  case 3:                                    /* SEEKER */
    if (boss.atk === 1) {
      for (let k = 0; k < (boss.phase >= 1 ? 4 : 3); k++) {
        const ang = (boss.spin + k * 16) & 63;
        add_ebullet(bx, by, Math.trunc(sintab[ang] / 12), 3 + (sintab[(ang + 16) & 63] > 0 ? 1 : 0));
      }
      boss.spin = (boss.spin + 5) & 63;
    } else if (boss.atk === 2) {
      for (let k = hard ? -3 : -2; k <= (hard ? 3 : 2); k++) add_ebullet(bx, by, k, 3);
    } else {
      add_ebullet(bx-12, by, boss.dir, 4); add_ebullet(bx+12, by, boss.dir, 4);
    }
    break;
  case 4:                                    /* MANTIS */
    if (boss.atk === 1) {
      for (let k = 0; k < 3; k++) {
        add_ebullet(boss.x + 6, by - k * 3, 2, 3);
        add_ebullet(boss.x + boss.w - 6, by - k * 3, -2, 3);
      }
    } else if (boss.atk === 2) {
      add_ebullet(boss.x + 4, by, 1, 5); add_ebullet(boss.x + boss.w - 4, by, -1, 5);
      if (boss.phase >= 2) { add_ebullet(bx - 12, by, -1, 4); add_ebullet(bx + 12, by, 1, 4); }
    } else {
      add_ebullet(boss.x + 3, by, 2, 4); add_ebullet(boss.x + boss.w - 3, by, -2, 4);
      add_ebullet(bx - 18, by, 1, 3); add_ebullet(bx + 18, by, -1, 3);
    }
    break;
  case 5:                                    /* ANVIL */
    if (boss.atk === 1) {
      for (let k = -3; k <= 3; k++) if (((Math.trunc(boss.t / 20) + k) & 1) === 0) add_ebullet(bx + k * 9, by, 0, 4);
    } else if (boss.atk === 2) {
      add_ebullet(bx - 18, by, -1, 3); add_ebullet(bx + 18, by, 1, 3);
      add_ebullet(bx - 6, by, 0, 5); add_ebullet(bx + 6, by, 0, 5);
    } else {
      for (let k = -2; k <= 2; k++) add_ebullet(bx + k * 12, by, 0, 3);
    }
    break;
  case 6:                                    /* SERAPH */
    if (boss.atk === 1) {
      for (let k = 0; k < 4; k++) { add_ebullet(boss.x + 4 + k * 5, by - 8, -1, 3 + Math.trunc(k / 2)); add_ebullet(boss.x + boss.w - 4 - k * 5, by - 8, 1, 3 + Math.trunc(k / 2)); }
    } else if (boss.atk === 2) {
      for (let k = -3; k <= 3; k += 2) add_ebullet(bx, by, k, 4);
    } else {
      add_ebullet(bx - 16, by, -2, 3); add_ebullet(bx + 16, by, 2, 3); add_ebullet(bx, by, 0, 5);
    }
    break;
  case 7: {                                  /* NEXUS: the orbiting pods own the crossfire */
    const p0x = boss.px[0], p0y = boss.py[0], p1x = boss.px[1], p1y = boss.py[1];
    if (boss.atk === 1) {
      add_ebullet(p0x, p0y, 2, 3); add_ebullet(p1x, p1y, -2, 3);
      add_ebullet(p0x, p0y, 1, 4); add_ebullet(p1x, p1y, -1, 4);
    } else if (boss.atk === 2) {
      add_ebullet(p0x, p0y, dir, 4); add_ebullet(p1x, p1y, dir, 4);
      if (boss.phase >= 1) add_ebullet(bx, by, 0, 5);
    } else {
      add_ebullet(p0x, p0y, 0, 4); add_ebullet(p1x, p1y, 0, 4); add_ebullet(bx, by - 5, dir, 3);
    }
    break; }
  case 8:                                    /* KRAKEN */
    if (boss.atk === 1) {
      for (let k = 0; k < 4; k++) add_ebullet(boss.x + 8 + k * 14, by - (k & 1) * 6, (k & 1) ? 1 : -1, 3);
    } else {
      add_ebullet(bx - 12, by, -1, 3); add_ebullet(bx + 12, by, 1, 3);
      if (boss.phase >= 1) add_ebullet(bx, by, dir, 4);
    }
    break;
  case 9:                                    /* PHANTOM */
    if (boss.atk === 1) {
      add_ebullet(bx - 10, by - 4, -1, 4); add_ebullet(bx + 10, by - 4, 1, 4); add_ebullet(bx, by, dir, 5);
    } else if (boss.atk === 2) {
      for (let k = -2; k <= 2; k += 2) add_ebullet(bx + k * 7, by, k, 4);
    } else {
      add_ebullet(bx, by, dir * 2, 5); add_ebullet(bx, by, -dir * 2, 5);
    }
    break;
  case 10:                                   /* CITADEL */
    if (boss.atk === 1) {
      for (let k = 0; k < 5; k++) if ((Math.trunc(boss.t / 18) + k) % 3 !== 1) add_ebullet(boss.x + 9 + k * 11, by, 0, 3);
    } else if (boss.atk === 2) {
      add_ebullet(boss.x + 12, by, -1, 4); add_ebullet(boss.x + boss.w - 12, by, 1, 4); add_ebullet(bx, by, dir, 4);
    } else {                                 /* turrets rake along the travel direction */
      for (let k = -2; k <= 2; k++) add_ebullet(bx + k * 12, by, boss.dir + Math.trunc(k / 2), 3);
    }
    break;
  case 11:                                   /* VORTEX */
    if (boss.atk === 1) {
      for (let k = 0; k < 4; k++) {
        const ang = (boss.spin + k * 16) & 63;
        add_ebullet(bx, by, Math.trunc(sintab[ang] / 11), 2 + (sintab[(ang + 16) & 63] > 0 ? 2 : 1));
      }
      boss.spin = (boss.spin + 9) & 63;
    } else if (boss.atk === 2) {
      add_ebullet(bx - 14, by, -2, 3); add_ebullet(bx + 14, by, 2, 3); add_ebullet(bx, by, 0, 5);
    } else {
      for (let k = -3; k <= 3; k += 2) add_ebullet(bx, by, k, 3);
    }
    break;
  case 12:                                   /* BASILISK */
    if (boss.atk === 1) {
      for (let k = -3; k <= 3; k++) if (k !== 0) add_ebullet(bx + k * 8, by, 0, 4);
    } else if (boss.atk === 2) {
      add_ebullet(bx - 4, by, 0, 6); add_ebullet(bx, by, 0, 6); add_ebullet(bx + 4, by, 0, 6);
      if (boss.phase >= 2) { add_ebullet(bx - 18, by, -1, 4); add_ebullet(bx + 18, by, 1, 4); }
    } else {
      add_ebullet(bx, by, dir, 5); add_ebullet(bx, by, dir * 2, 4);
    }
    break;
  case 13:                                   /* TITAN */
    if (boss.atk === 1) {
      for (let k = -4; k <= 4; k += 2) add_ebullet(bx + k * 7, by, Math.trunc(k / 2), 4);
    } else if (boss.atk === 2) {
      add_ebullet(boss.x + 8, by, 1, 4); add_ebullet(boss.x + boss.w - 8, by, -1, 4);
      add_ebullet(bx - 8, by, 0, 5); add_ebullet(bx + 8, by, 0, 5);
    } else {
      for (let k = -3; k <= 3; k++) add_ebullet(bx + k * 9, by, 0, (k & 1) ? 3 : 5);
    }
    break;
  default:                                   /* OVERLORD */
    if (boss.atk === 1) {
      add_ebullet(bx-18, by-2, 2, 4); add_ebullet(bx+18, by-2, -2, 4);
      add_ebullet(bx-8, by, 1, 5); add_ebullet(bx+8, by, -1, 5);
      if (boss.phase >= 1) add_ebullet(bx, by, 0, 6);
    } else if (boss.atk === 2) {
      const arms = boss.phase >= 2 ? 5 : 4;
      for (let a = 0; a < arms; a++) {
        const ang = (boss.spin + a * 16) & 63;
        add_ebullet(bx, by, Math.trunc(sintab[ang] / 14),
                    3 + (sintab[(ang + 16) & 63] > 0 ? 1 : 0));
      }
      boss.spin = (boss.spin + 7) & 63;
    } else if (boss.atk === 3) {
      add_ebullet(bx, by, dir*2, 3); add_ebullet(bx, by, dir, 4);
      add_ebullet(bx-12, by, -1, 4); add_ebullet(bx+12, by, 1, 4);
      if (hard || boss.phase >= 2) add_ebullet(bx, by, 0, 5);
    } else {
      for (let k = -4; k <= 4; k++)
        if ((Math.floor(boss.t / 18) + k + 8) % 5 !== 0)
          add_ebullet(bx + k * 8, by, Math.trunc(k / 3), 3);
    }
    break;
  }
  snd_sfx(SFX_HIT);
}
function move_toward(v, target, step) {
  if (v < target) return Math.min(target, v + step);
  if (v > target) return Math.max(target, v - step);
  return v;
}
/* Per-boss bottom of the movement envelope (mirror of native boss_max_y). */
const BOSS_MAX_Y = [126, 150, 40, 96, 150, 140, 110, 100, 116, 110,
                    100, 120, 150, 126, 150];
function boss_max_y() { return BOSS_MAX_Y[boss.kind]; }

/* Campaign bosses each own a movement band and motion language. Every move
   that invades the player's zone is telegraphed via boss.charge first.
   Mirror of the native boss_move(). */
function boss_move() {
  const cx = (SCRW>>1) - (boss.w>>1);
  switch (boss.kind) {
  case 0:                                   /* GORGON: wall crawl + rampart slam */
    if (boss.ty === 1) {                    /* telegraph: dig in */
      if (--boss.dive_t <= 0) boss.ty = 2;
    } else if (boss.ty === 2) {             /* slam down */
      boss.y += 4;
      if (boss.y >= (boss.phase >= 1 ? 126 : 118)) { boss.ty = 3; boss.dive_t = 40; }
    } else if (boss.ty === 3) {             /* grind sideways at depth */
      boss.x += boss.dir * 2;
      if (--boss.dive_t <= 0) boss.ty = 4;
    } else if (boss.ty === 4) {             /* winch back up */
      boss.y -= 1;
      if (boss.y <= 84) { boss.y = 84; boss.ty = 0;
        boss.mv_t = boss.phase >= 2 ? 110 : 160; }
    } else {                                /* rampart crawl */
      boss.x += boss.dir * (1 + (boss.phase >= 2 ? 1 : 0));
      boss.y = 84 + (((boss.t >> 5) & 1) * 3);
      if (--boss.mv_t <= 0) {
        boss.ty = 1;
        boss.dive_t = g_diff === DIF_EASY ? 34 : g_diff === DIF_HARD ? 22 : 28;
        boss.charge = boss.dive_t;
      }
    }
    break;
  case 1:                                   /* REAPER: dives through the player band */
    if (boss.ty === 0) {                    /* strafe high, pick a mark */
      boss.y = 18 + Math.trunc((sintab[boss.t & 63] + 46) / 30);
      boss.x = move_toward(boss.x, boss.tx, 2);
      if (--boss.mv_t <= 0) {
        boss.ty = 1; boss.charge = 24; boss.dive_t = 24;
        boss.tx = player.x + 8 - (boss.w>>1);
        boss.py[0] = boss.phase >= 2 ? 2 : 1;     /* chained dives */
      }
    } else if (boss.ty === 1) {             /* hold on the marked dive lane */
      if (--boss.dive_t <= 0) boss.ty = 2;
    } else if (boss.ty === 2) {             /* dive deep, afterimage trail */
      boss.x = move_toward(boss.x, boss.tx, g_diff === DIF_EASY ? 3 : 4);
      boss.y += g_diff === DIF_EASY ? 3 : 4;
      if (frame & 1) spawn_part(boss.x + (boss.w>>1), boss.y, C_LRED);
      if (boss.y >= 150) boss.ty = 3;
    } else {                                /* strafing arc back up */
      boss.x += Math.trunc(sintab[(boss.t * 2) & 63] / 12);
      boss.y -= 4;
      if (boss.y <= 18) {
        boss.y = 18;
        if (--boss.py[0] > 0) {             /* enraged: chain one warned dive */
          boss.ty = 1; boss.charge = 24; boss.dive_t = 24;
          boss.tx = player.x + 8 - (boss.w>>1);
        } else {
          boss.ty = 0; boss.mv_t = 44 - boss.phase * 10;
          boss.tx = rrange(8, SCRW - boss.w - 8);
        }
      }
    }
    break;
  case 2:                                   /* LEVIATHAN: hover + bombing trawl */
    if (boss.ty === 0) {                    /* high hover between runs */
      boss.y = 8 + Math.trunc((sintab[boss.t & 63] + 46) / 40);
      boss.x = move_toward(boss.x, boss.tx, 1);
      if ((boss.t & 63) === 0) boss.tx = rrange(8, SCRW - boss.w - 8);
      if (--boss.mv_t <= 0) {
        boss.ty = 1; boss.dive_t = 24; boss.charge = 24;
        boss.dir = boss.x < cx ? 1 : -1;    /* trawl toward the far wall */
      }
    } else if (boss.ty === 1) {             /* engines spool up */
      if (--boss.dive_t <= 0) boss.ty = 2;
    } else {                                /* full-width trawl, bombs from both bays */
      boss.x += boss.dir * (3 + (boss.phase >= 1 ? 1 : 0));
      boss.y = 10;
      if ((boss.t & (boss.phase >= 2 ? 7 : 15)) === 0) {
        add_ebullet(boss.x + 10, boss.y + boss.h - 4, 0, 3);
        add_ebullet(boss.x + boss.w - 14, boss.y + boss.h - 4, 0, 3);
      }
      if (boss.x <= 8 || boss.x >= SCRW - boss.w - 8) {
        boss.ty = 0; boss.mv_t = 200; boss.tx = boss.x;
      }
    }
    if (--boss.launch_t <= 0) { launch_squad(); boss.launch_t = 180 - boss.phase * 40; }
    break;
  case 3: {                                 /* SEEKER: circles the player, closing in */
    const mul = 3 - boss.phase;             /* orbit tightens per phase */
    boss.tx = move_toward(boss.tx, player.x + 8 - (boss.w>>1), 1);
    boss.x = boss.tx + Math.trunc(sintab[(boss.t * 2) & 63] * mul / 2);
    boss.y = 46 + Math.trunc(sintab[(boss.t * 2 + 16) & 63] * mul / 3);
    boss.dir = sintab[(boss.t * 2 + 16) & 63] >= 0 ? 1 : -1;
    break; }
  case 4:                                   /* MANTIS: wall-cling + cross-screen lunge */
    if (boss.ty === 0) {                    /* slide along the wall */
      boss.x = move_toward(boss.x, boss.dir > 0 ? 6 : SCRW - boss.w - 6, 3);
      boss.y = 60 + Math.trunc(sintab[(boss.t * 2) & 63] / 2);
      if (--boss.mv_t <= 0) {
        boss.ty = 1; boss.dive_t = 24; boss.charge = 24;
        boss.py[0] = clamp(player.y, 20, 150);    /* lock the lunge lane */
      }
    } else if (boss.ty === 1) {             /* line up with the player */
      boss.y = move_toward(boss.y, boss.py[0], 3);
      if (--boss.dive_t <= 0) { boss.ty = 2; boss.dive_t = 10; }
    } else {                                /* lunge across the screen */
      boss.x += boss.dir * 4;
      if (boss.phase >= 2 && boss.dive_t > 0 &&
          boss.x + (boss.w>>1) >= player.x && boss.x + (boss.w>>1) <= player.x + 16) {
        boss.x -= boss.dir * 4;             /* menacing mid-lunge pause */
        boss.dive_t--;
      }
      if (boss.x <= 6 || boss.x >= SCRW - boss.w - 6) {
        boss.dir = -boss.dir;               /* attach to the far wall */
        boss.ty = 0; boss.mv_t = 64 - boss.phase * 6;
      }
    }
    break;
  case 5:                                   /* ANVIL: hover, then floor crush */
    if (boss.ty === 0) {                    /* drift above the lane */
      boss.x = cx + Math.trunc(sintab[(boss.t >> 1) & 63] / 2);
      boss.y = move_toward(boss.y, 64, 1);
      if (--boss.mv_t <= 0) { boss.ty = 1; boss.dive_t = 22; boss.charge = 22; }
    } else if (boss.ty === 1) {             /* telegraph, edge over the player */
      boss.x = move_toward(boss.x, player.x + 8 - (boss.w>>1), 2);
      if (--boss.dive_t <= 0) { boss.ty = 2; boss.dive_t = 0; }
    } else if (boss.ty === 2) {             /* crush */
      boss.y += 4;
      if (boss.y >= 140) {
        boss.y = 140; shk = 10; snd_sfx(SFX_EXPLODE);
        for (let k = 1; k <= 3; k++) {      /* horizontal shockwave along the floor */
          add_ebullet(boss.x + (boss.w>>1), boss.y + boss.h - 6, k, 0);
          add_ebullet(boss.x + (boss.w>>1), boss.y + boss.h - 6, -k, 0);
        }
        boss.ty = 3;
        boss.dive_t = boss.phase >= 2 ? 1 : 0;    /* enraged: double crush */
      }
    } else {                                /* rise; maybe side-step + crush again */
      boss.y -= boss.dive_t ? 3 : 1;
      if (boss.dive_t && boss.y <= 100) {
        boss.x += player.x + 8 > boss.x + (boss.w>>1) ? 40 : -40;
        boss.ty = 1; boss.dive_t = 20; boss.py[0] = player.y; boss.charge = 20;
      } else if (boss.y <= 64) {
        boss.y = 64; boss.ty = 0;
        boss.mv_t = 96 - boss.phase * 30;
      }
    }
    break;
  case 6: {                                 /* SERAPH: pendulum scythe sweep */
    const ph = boss.phase >= 2 ? boss.t + Math.trunc(boss.t / 2) : boss.t;
    const s = sintab[ph & 63];
    const as = s < 0 ? -s : s;
    boss.x = cx + Math.trunc(s * 7 / 4);
    boss.y = 20 + (46 - as) * 2 + (boss.phase >= 2 ? 12 : 0);
    boss.dir = sintab[(ph + 16) & 63] >= 0 ? 1 : -1;
    break; }
  case 7: {                                 /* NEXUS: anchored core, orbiting fire-pods */
    if (--boss.mv_t <= 0) {
      boss.mv_t = 62;
      boss.ty = (boss.ty + 1) % 3;
      boss.tx = boss.ty === 0 ? 22 : boss.ty === 1 ? cx : SCRW - boss.w - 22;
    }
    boss.x = move_toward(boss.x, boss.tx, 2);
    boss.y = 40 + Math.trunc(sintab[(boss.t * 3) & 63] / 12);
    boss.spin = (boss.spin + 1) & 63;
    let pcx = boss.x + (boss.w>>1);
    let pcy = boss.y + (boss.h>>1);
    if (boss.phase >= 2) {                  /* pods detach low, converge on the player */
      pcx = (pcx + player.x + 8) >> 1;
      pcy += 30;
    }
    boss.px[0] = pcx + Math.trunc(sintab[boss.spin & 63] * 26 / 46);
    boss.py[0] = pcy + Math.trunc(sintab[(boss.spin + 16) & 63] * 12 / 46);
    boss.px[1] = pcx - Math.trunc(sintab[boss.spin & 63] * 26 / 46);
    boss.py[1] = pcy - Math.trunc(sintab[(boss.spin + 16) & 63] * 12 / 46);
    break; }
  case 8: {                                 /* KRAKEN: advancing wall, recoils on phase change */
    const depth = 88 + boss.phase * 8;
    boss.x = cx + sintab[boss.t & 63];
    if (boss.ty === 1) {                    /* recoil back to the top */
      boss.y -= 3;
      if (boss.y <= 14) { boss.y = 14; boss.ty = 0; }
    } else if ((boss.t & 3) === 0 && boss.y < depth) boss.y++;
    if (--boss.launch_t <= 0) { launch_squad(); boss.launch_t = 205 - boss.phase * 36; }
    break; }
  case 9:                                   /* PHANTOM: true telegraphed teleports */
    if (boss.ty === 0) {                    /* materialised: drift + countdown */
      boss.x += Math.trunc(sintab[(boss.t * 2) & 63] / 24);
      if (--boss.mv_t <= 0) { boss.ty = 1; boss.dive_t = 16; }
    } else if (boss.ty === 1) {             /* dematerialise (checkerboard mask) */
      if (--boss.dive_t <= 0) {
        if (boss.phase >= 2 && (++boss.px[1] % 3) === 0) {
          boss.tx = player.x + 8 - (boss.w>>1);   /* land on the player */
          boss.px[0] = player.y - boss.h - 12;
        } else if (boss.phase >= 1) {             /* biased near the player */
          boss.tx = player.x + 8 - (boss.w>>1) + rrange(-40, 40);
          boss.px[0] = rrange(22, 96);
        } else {
          boss.tx = rrange(18, SCRW - boss.w - 18);
          boss.px[0] = rrange(22, 80);
        }
        boss.px[0] = clamp(boss.px[0], 16, 110);
        boss.x = boss.tx; boss.y = boss.px[0];    /* blink */
        boss.ty = 2; boss.dive_t = 8;
        snd_sfx(SFX_PHASE);
        for (let k = 0; k < 8; k++)               /* arrival burst */
          add_ebullet(boss.x + (boss.w>>1), boss.y + (boss.h>>1),
                      Math.trunc(sintab[(k * 8) & 63] / 14),
                      2 + (sintab[(k * 8 + 16) & 63] > 0 ? 2 : 0));
      }
    } else {                                /* re-materialise */
      if (--boss.dive_t <= 0) { boss.ty = 0; boss.mv_t = 64 - boss.phase * 8; }
    }
    break;
  case 10: {                                /* CITADEL: perimeter patrol, corner to corner */
    const x0 = 18 + boss.phase * 10, x1 = SCRW - boss.w - 18 - boss.phase * 10;
    const y0 = 40 + boss.phase * 8, y1 = 96;
    const spd = boss.phase >= 2 ? 2 : 1;
    const gx = (boss.ty === 1 || boss.ty === 2) ? x1 : x0;   /* ty = corner index */
    const gy = boss.ty >= 2 ? y1 : y0;
    boss.x = move_toward(boss.x, gx, spd);
    boss.y = move_toward(boss.y, gy, spd);
    if (boss.x === gx && boss.y === gy) boss.ty = (boss.ty + 1) & 3;
    boss.dir = gx > boss.x ? 1 : -1;        /* turrets rake the travel direction */
    break; }
  case 11: {                                /* VORTEX: breathing spiral over the player */
    let r = 14 + Math.trunc((sintab[(boss.t >> 1) & 63] + 46) / (boss.phase >= 2 ? 3 : 4));
    const oi = boss.t * (boss.phase >= 2 ? 3 : 2);
    if (boss.phase >= 2 && r < 22) r = 22;  /* radius floor when enraged */
    boss.tx = move_toward(boss.tx, player.x + 8 - (boss.w>>1), 1);
    boss.x = boss.tx + Math.trunc(sintab[oi & 63] * r / 46);
    boss.y = 52 + Math.trunc(sintab[(oi + 16) & 63] * r / 60);
    boss.spin = (boss.spin + 2) & 63;
    break; }
  case 12:                                  /* BASILISK: stalks the column, guillotines it */
    if (boss.ty === 0) {                    /* track the player's x */
      boss.tx = player.x + 8 - (boss.w>>1);
      boss.x = move_toward(boss.x, boss.tx, 2 + (boss.phase >= 1 ? 1 : 0));
      boss.y = 24 + Math.trunc((sintab[(boss.t + 8) & 63] + 46) / 18);
      if (--boss.mv_t <= 0) { boss.ty = 1; boss.dive_t = 24; boss.charge = 24; }
    } else if (boss.ty === 1) {             /* frozen eye telegraph */
      if (--boss.dive_t <= 0) boss.ty = 2;
    } else if (boss.ty === 2) {             /* guillotine drop */
      boss.y += 4;
      if (boss.y >= 150) boss.ty = 3;
    } else {                                /* drag back up, raking the corridor */
      boss.y -= 2;
      if ((boss.t & 7) === 0) {
        add_ebullet(boss.x + 2, boss.y + (boss.h>>1), -1, 3);
        add_ebullet(boss.x + boss.w - 6, boss.y + (boss.h>>1), 1, 3);
      }
      if (boss.y <= 26) {
        boss.ty = 0;
        boss.mv_t = boss.phase >= 2 ? 90 : 150;
      }
    }
    break;
  case 13:                                  /* TITAN: quake slams; steamrolls when enraged */
    if (boss.ty === 0) {                    /* heavy crawl */
      boss.x += boss.dir;
      boss.y = move_toward(boss.y, 88, 1);
      if (--boss.mv_t <= 0) {
        boss.dive_t = 24; boss.charge = 24;
        if (boss.phase >= 2 && (boss.px[1] ^= 1) !== 0) {
          boss.ty = 3;                      /* every other attack: steamroll */
          boss.dir = boss.x < cx ? 1 : -1;
        } else boss.ty = 1;
      }
    } else if (boss.ty === 1) {             /* telegraph */
      if (--boss.dive_t <= 0) boss.ty = 2;
    } else if (boss.ty === 2) {             /* quake slam */
      boss.y += 4;
      if (boss.y >= 120) {
        boss.y = 120; shk = 16; snd_sfx(SFX_EXPLODE);
        for (let k = -2; k <= 2; k++)       /* vertical bullet columns */
          add_ebullet(boss.x + (boss.w>>1) + k * 22, boss.y + boss.h - 4, 0, 4);
        boss.ty = 4;
      }
    } else if (boss.ty === 3) {             /* steamroll charge */
      if (boss.dive_t > 0) { boss.dive_t--; boss.y = move_toward(boss.y, 104, 2); }
      else {
        boss.x += boss.dir * 3;
        if (boss.x <= 4 || boss.x >= SCRW - boss.w - 4) boss.ty = 4;
      }
    } else {                                /* recover */
      boss.y -= 1;
      if (boss.y <= 88) { boss.y = 88; boss.ty = 0; boss.mv_t = 120 - boss.phase * 20; }
    }
    break;
  default:                                  /* OVERLORD: finale speaks every boss's language */
    if (boss.phase === 0) {                 /* figure-eight */
      boss.x = cx + Math.trunc(sintab[(boss.t * 2) & 63] * 3 / 2);
      boss.y = 16 + Math.trunc((sintab[(boss.t * 3 + 16) & 63] + 46) / 4);
      if ((boss.t & 127) === 0) boss.charge = 18;
    } else if (boss.phase === 1) {          /* PHANTOM: telegraphed blinks */
      if (boss.ty === 0) {
        if (--boss.mv_t <= 0) { boss.ty = 1; boss.dive_t = 14; }
      } else if (boss.ty === 1) {
        if (--boss.dive_t <= 0) {
          boss.x = rrange(18, SCRW - boss.w - 18);
          boss.y = rrange(16, 84);
          boss.ty = 2; boss.dive_t = 8;
          snd_sfx(SFX_PHASE);
        }
      } else if (--boss.dive_t <= 0) { boss.ty = 0; boss.mv_t = 52; }
    } else if (boss.ty === 2) {             /* visible hold on the dive lane */
      if (--boss.dive_t <= 0) boss.ty = 3;
    } else if (boss.ty === 3) {             /* REAPER: one dive per orbit cycle */
      boss.x = move_toward(boss.x, boss.tx, g_diff === DIF_EASY ? 3 : 4);
      boss.y += g_diff === DIF_EASY ? 3 : 4;
      if (frame & 1) spawn_part(boss.x + (boss.w>>1), boss.y, C_LMAG);
      if (boss.y >= 150) boss.ty = 4;
    } else if (boss.ty === 4) {             /* retreat from the dive */
      boss.y -= 4;
      if (boss.y <= 20) { boss.ty = 0; boss.mv_t = 140; }
    } else {                                /* SEEKER: tightening player orbit */
      boss.tx = move_toward(boss.tx, player.x + 8 - (boss.w>>1), 1);
      boss.x = boss.tx + sintab[(boss.t * 2) & 63];
      boss.y = 20 + Math.trunc((sintab[(boss.t * 2 + 16) & 63] + 46) / 2);
      if (--boss.mv_t <= 0) {
        boss.ty = 2; boss.dive_t = 24; boss.charge = 24;
        boss.tx = player.x + 8 - (boss.w>>1);
      }
    }
    boss.spin = (boss.spin + 1) & 63;       /* crown spokes always turn */
    break;
  }
  if (boss.x < 4) { boss.x = 4; boss.dir = 1; }
  if (boss.x > SCRW - boss.w - 4) { boss.x = SCRW - boss.w - 4; boss.dir = -1; }
  if (boss.y < 6) boss.y = 6;
  if (boss.y > boss_max_y()) boss.y = boss_max_y();
}
function boss_rest_y() {
  switch (boss.kind) {
    case 0: return 84; case 1: return 18; case 2: return 9; case 3: return 46;
    case 4: return 60; case 5: return 64; case 6: return 20; case 7: return 40;
    case 8: return 14; case 9: return 28; case 10: return 40; case 11: return 52;
    case 12: return 24; case 13: return 88; default: return 16;
  }
}
function spawn_blast(x, y, big) {
  for (const b of blast) if (!b.active) {
    b.active = true; b.x = x; b.y = y; b.t = 0; b.big = big;
    return;
  }
}

/* ---------------- powerups ---------------- */
function drop_powerup(x, y, type) {
  for (const p of powr) if (!p.active) {
    p.active = true; p.type = type; p.x = x; p.y = y; p.t = 0;
    return true;
  }
  return false;
}
function force_powerup(x, y, type) {
  if (drop_powerup(x, y, type)) return;
  let best = 0, oldest = -1;
  for (let i = 0; i < MAX_POWERUP; i++) {
    if (powr[i].type === PU_SCORE) { best = i; break; }
    if (powr[i].t > oldest) { oldest = powr[i].t; best = i; }
  }
  const p = powr[best];
  p.active = true; p.type = type; p.x = x; p.y = y; p.t = 0;
}
function apply_powerup(type) {
  let capped = false;
  let weaponFx = false, weaponCol = C_WHITE;
  switch (type) {
    case PU_GUN:
      if (player.gun < GUN_MAX) {
        player.gun++;
        set_msg((player.wtype === WT_LASER ? 'LASER' : player.wtype === WT_WAVE ? 'WAVE' : 'CANNON') + ' LEVEL ' + player.gun);
        weaponFx = true;
        weaponCol = player.wtype === WT_LASER ? C_LCYAN : player.wtype === WT_WAVE ? C_LMAG : C_YELLOW;
      } else capped = true;
      break;
    case PU_RAPID:   player.rapid = 700; break;
    case PU_SHIELD:  player.shield = 350; break;   /* ~10 s of invulnerability at 35 FPS */
    case PU_LIFE:    if (player.lives < 9) player.lives++; else capped = true; break;
    case PU_MISSILE: if (player.msl < 30) player.msl = Math.min(30, player.msl + 4); else capped = true; break;
    case PU_LASER:
      if (player.wtype === WT_WAVE) { player.wave_boost = 350; set_msg('WAVE PIERCE 10 SEC'); }
      else { player.wtype = WT_LASER; set_msg('LASER EQUIPPED'); }
      weaponFx = true; weaponCol = C_LCYAN; break;
    case PU_WAVE:
      player.wtype = WT_WAVE; set_msg('WAVE EQUIPPED');
      weaponFx = true; weaponCol = C_LMAG; break;
    case PU_BOMB:    if (player.bombs < 10) player.bombs++; else capped = true; break;
    case PU_SCORE:   score_add(500 + wave * 50); break;
  }
  if (capped) {
    score_add(300); spawn_popup(player.x, player.y - 8, '+300');
    if (type === PU_GUN) set_msg('WEAPON LEVEL MAX +300');
  }
  if (weaponFx) burst(player.x + (SH_SHIP_W>>1), player.y + (SH_SHIP_H>>1), 12, C_WHITE, weaponCol);
  if (type === PU_LIFE || type === PU_SHIELD) snd_sfx(SFX_PICK1);
  else if (type === PU_SCORE) snd_sfx(SFX_COMBO);
  else snd_sfx(SFX_PICK2);
  score_add(50);
}
function choose_powerup() {
  const weights = [];
  weights[PU_GUN] = player.gun < GUN_MAX ? 16 : 0;
  weights[PU_RAPID] = player.rapid > 350 ? 5 : 13;
  weights[PU_SHIELD] = player.shield > 0 ? 5 : 18;
  weights[PU_LIFE] = player.lives >= 9 ? 0 : player.lives <= 2 ? 10 : 3;
  weights[PU_MISSILE] = player.msl <= 5 ? 24 : player.msl > 20 ? 8 : 18;
  weights[PU_LASER] = 10; weights[PU_WAVE] = 10;
  weights[PU_BOMB] = player.bombs === 0 ? 12 : player.bombs >= 5 ? 3 : 7;
  weights[PU_SCORE] = 0;
  let total = weights.reduce((a, b) => a + b, 0), pick = total ? rnd() % total : 0;
  if (!total) return PU_SCORE;
  for (let i = 0; i < weights.length; i++) { if (pick < weights[i]) return i; pick -= weights[i]; }
  return PU_SCORE;
}
function enemy_drop_chance(e) {
  if (boss.active && e.drop_class !== DROP_NORMAL)
    return g_diff === DIF_EASY ? 55 : g_diff === DIF_HARD ? 45 : 50;
  return g_diff === DIF_EASY ? 18 : g_diff === DIF_HARD ? 12 : 15;
}
function apply_boss_damage(dmg) {
  if (!boss.active || boss.die_t > 0) return;
  boss.hp -= dmg;
  boss.recent_dmg = Math.min(boss.maxhp, (boss.recent_dmg || 0) + dmg);
  boss.hurt_t = 3;
  burst(boss.x + (boss.w>>1), boss.y + (boss.h>>1), 8, C_WHITE, C_YELLOW);
  if (boss.hp <= 0) boss_die();
}
function smart_bomb() {
  if (player.bombs <= 0 || !player.alive) return;
  player.bombs--;
  flash = 8; shk = 18;
  let cleared = 0;
  for (const b of ebul) if (b.active) { b.active = false; cleared++; }
  score_add(cleared * 25);
  spawn_blast(SCRW>>1, SCRH>>1, 3);
  for (const e of enemies) if (e.active) {
    e.hp -= 5;
    if (e.hp <= 0) { score_add(100); kill_enemy(e); }
  }
  if (boss.active) apply_boss_damage(boss_pct_damage(3));
  snd_sfx(SFX_EXPLODE);
}

/* ---------------- damage ---------------- */
function hurt_player() {
  if (player.invuln > 0) return;
  /* Active shield = full invulnerability for its whole duration: absorb the
     hit without popping the shield or breaking the combo. Brief i-frames
     keep it to one spark per incoming shot. */
  if (player.shield > 0) {
    player.invuln = 8;
    burst(player.x+8, player.y+8, 10, C_LBLUE, C_WHITE);
    snd_sfx(SFX_HIT);
    return;
  }
  wave_hit = 1; combo_broken = 1;
  player.combo = 0; player.combo_t = 0;
  player.lives--;
  player.invuln = 90;
  player.rapid = 0;
  if (player.gun > GUN_MIN) player.gun--;
  player.wtype = WT_CANNON;
  fireburst(player.x+8, player.y+8, 24);
  spawn_blast(player.x+8, player.y+8, 2);
  flash = 4; shk = 12;
  snd_sfx(SFX_EXPLODE);
  if (player.lives <= 0) player.alive = false;
  else {
    if (g_diff !== DIF_HARD && score - last_death_score < 800)
      player.shield = g_diff === DIF_EASY ? 140 : 100;
    last_death_score = score;
  }
}
function shield_bounce(ox, oy) {
  const px = player.x + (SH_SHIP_W>>1), py = player.y + (SH_SHIP_H>>1);
  player.x += px < ox ? -12 : 12;
  player.y += py < oy ? -16 : 16;
  player.x = Math.max(0, Math.min(SCRW-SH_SHIP_W, player.x));
  player.y = Math.max(8, Math.min(SCRH-SH_SHIP_H, player.y));
  player.invuln = 12; player.ram_cd = 24;
  burst(px, py, 18, C_WHITE, C_LCYAN); flash = 3; shk = 10; snd_sfx(SFX_PHASE);
}
function combo_mult() { return Math.min(5, 1 + Math.floor(player.combo / 6)); }
function kill_enemy(e) {
  const cx = e.x + (SH_EN_W>>1), cy = e.y + (SH_EN_H>>1);
  const old_mult = combo_mult();
  typeburst(cx, cy, e.type, e.elite);
  spawn_blast(cx, cy, 0);
  wave_kills++;
  player.combo++; player.combo_t = 130;
  if (player.combo > player.max_combo) player.max_combo = player.combo;
  const tier_up = combo_mult() > old_mult;
  const award = score_scaled(enemy_score(e) * combo_mult());
  score += award;
  if (tier_up) spawn_popup(cx - 8, cy - 10, 'X' + combo_mult());               /* combo milestone */
  else if (e.elite) spawn_popup(cx - 12, cy - 10, String(award));  /* bounty */
  if (!risk_spawned && player.combo >= 10) {
    drop_powerup(rrange(96, 212), rrange(38, 72), PU_SCORE);
    risk_spawned = 1;
  }
  const canDrop = !boss.active || boss.drop_budget > 0;
  const guaranteed = boss.active && e.drop_class === DROP_SUPPLY;
  if (canDrop && (guaranteed || rnd() % 100 < enemy_drop_chance(e))) {
    if (drop_powerup(cx - (SH_PU_W>>1), cy, choose_powerup()) && boss.active) boss.drop_budget--;
  }
  e.active = false;
  snd_sfx(tier_up ? SFX_COMBO : SFX_EXPLODE);
}
/* stage 1: start the chained death sequence (boss goes inert + invulnerable) */
function boss_die() {
  if (boss.die_t > 0) return;
  boss.hp = 0;
  boss.die_t = 45;
  boss.charge = 0;
  snd_sfx(SFX_EXPLODE);
}
/* stage 2: the final blast once the chained explosions finish */
function boss_finish_death() {
  const award = score_scaled(5000 * combo_mult());
  fireburst(boss.x + (boss.w>>1), boss.y + (boss.h>>1), 60);
  spawn_blast(boss.x + (boss.w>>1), boss.y + (boss.h>>1), 3);
  score += award;
  spawn_popup(boss.x + (boss.w>>1) - 20, boss.y, '+' + award);
  flash = 8; shk = 24;
  bosses_defeated++;
  snd_music_game(bosses_defeated);
  force_powerup(boss.x + 10, boss.y + 10,
    player.lives <= 3 ? PU_LIFE : player.shield <= 0 ? PU_SHIELD : player.gun < GUN_MAX ? PU_GUN : PU_SCORE);
  force_powerup(boss.x + boss.w - 22, boss.y + 10, player.bombs < 5 ? PU_BOMB : PU_SCORE);
  force_powerup(boss.x + (boss.w>>1) - (SH_PU_W>>1), boss.y + 20, PU_MISSILE);
  boss.active = false;
  if (boss.kind === (BOSSDEF.length - 1) && wave === 60 && !campaign_won) {
    campaign_won = 1;
    win_pending = 1;
  }
  snd_sfx(SFX_EXPLODE);
}
function missile_boom(mx, my) {
  fireburst(mx, my, 26);
  spawn_blast(mx, my, 1);
  shk = 6;
  for (const e of enemies) if (e.active &&
      overlap(mx-26, my-26, 52, 52, e.x, e.y, SH_EN_W, SH_EN_H)) {
    e.hp -= 4;
    if (e.hp <= 0) kill_enemy(e);
  }
  if (boss.active && overlap(mx-26, my-26, 52, 52, boss.x, boss.y, boss.w, boss.h)) {
    apply_boss_damage(boss_pct_damage(10));
  }
  snd_sfx(SFX_EXPLODE);
}

/* ---------------- per-frame update ---------------- */
function update_stars() {
  for (const s of stars) {
    s.y += s.layer + 1;
    if (s.y >= SCRH) { s.y = 0; s.x = rrange(0, SCRW-1); }
  }
}
function update_dust() {
  for (const d of dust) {
    d.y += d.layer;
    if (d.y >= SCRH) { d.y = 0; d.x = rrange(0, SCRW-1); d.col = PAL_NEB + 6 + rnd() % 7; }
  }
}
function update_play() {
  if (player.alive) {
    let sp = 3;
    const wants_boost = key_pressed('SHIFT');
    const was_boosting = player.boosting;
    player.boosting = false;
    if (wants_boost && player.boost >= BOOST_MIN_START) {
      player.boosting = true;
      sp = 5;
      player.boost = Math.max(0, player.boost - BOOST_DRAIN);
      player.boost_cd = BOOST_RECHARGE_CD;
      if (!was_boosting) snd_sfx(SFX_BOOST);
    } else {
      if (player.boost_cd > 0) player.boost_cd--;
      else if (player.boost < BOOST_MAX) player.boost = Math.min(BOOST_MAX, player.boost + BOOST_RECHARGE);
    }
    ship_bank = 1;
    const kbMove = key_pressed('LEFT') || key_pressed('RIGHT') || key_pressed('UP') || key_pressed('DOWN');
    const joyMove = mobileMode && mobileMove.active && !pointerAim.active && !kbMove;
    const moveLeft = key_pressed('LEFT') || (joyMove && mobileMove.x < 0);
    const moveRight = key_pressed('RIGHT') || (joyMove && mobileMove.x > 0);
    const moveUp = key_pressed('UP') || (joyMove && mobileMove.y < 0);
    const moveDown = key_pressed('DOWN') || (joyMove && mobileMove.y > 0);
    const moveSp = joyMove ? sp + 1 : sp;
    if (moveLeft)  { player.x -= moveSp; ship_bank = 0; }
    if (moveRight) { player.x += moveSp; ship_bank = 2; }
    if (moveUp)    player.y -= moveSp;
    if (moveDown)  player.y += moveSp;
    /* keyboard has priority: using a movement key drops mouse ownership, and
       the mouse only reclaims it by actually moving again (a stationary cursor
       fires no pointermove, so the ship won't snap back to it) */
    if (kbMove) pointerAim.active = false;
    if (pointerAim.active) {
      const aimSp = mobileMode && pointerAim.id !== null ? sp + 3 : sp;
      const tx = pointerAim.x - (SH_SHIP_W >> 1);
      const ty = pointerAim.y - (SH_SHIP_H >> 1);
      const dx = tx - player.x;
      const dy = ty - player.y;
      if (Math.abs(dx) > 1) {
        const mx = Math.max(-aimSp, Math.min(aimSp, dx));
        player.x += mx;
        ship_bank = mx < 0 ? 0 : 2;
      }
      if (Math.abs(dy) > 1) player.y += Math.max(-aimSp, Math.min(aimSp, dy));
    }
    player.x = Math.max(0, Math.min(SCRW - SH_SHIP_W, player.x));
    player.y = Math.max(8, Math.min(SCRH - SH_SHIP_H, player.y));
    update_player_facing();
    if (player.boosting && (frame & 1)) {
      const fy = player.facing_down ? player.y-1 : player.y+SH_SHIP_H+1;
      spawn_part(player.x+4, fy, 0);
      spawn_part(player.x+12, fy, 0);
    }
    if (player.firecd > 0) player.firecd--;
    if ((key_pressed('SPACE') || mobileMode) && player.firecd <= 0) player_fire();
    if (key_hit('CTRL')) fire_missile();
    if (key_hit('B')) smart_bomb();
    if (player.invuln > 0) player.invuln--;
    if (player.ram_cd > 0) player.ram_cd--;
    if (player.shield > 0) player.shield--;
    if (player.rapid > 0) player.rapid--;
    if (player.wave_boost > 0) player.wave_boost--;
    if (player.combo_t > 0 && --player.combo_t === 0) player.combo = 0;
    if (wave_banner > 0) wave_banner--;
  }

  for (const b of blast) if (b.active) {
    b.t++;
    if (b.t > (b.big ? 14 : 8)) b.active = false;
  }

  /* homing missiles */
  for (const m of mslA) if (m.active) {
    let tx = -1, best = Infinity;
    for (const e of enemies) if (e.active && ((m.dy < 0 && e.y < m.y) || (m.dy > 0 && e.y > m.y))) {
      const dx = e.x + (SH_EN_W>>1) - m.x, dy = e.y + (SH_EN_H>>1) - m.y;
      const d = dx*dx + dy*dy;
      if (d < best) { best = d; tx = e.x + (SH_EN_W>>1); }
    }
    if (boss.active && ((m.dy < 0 && boss.y < m.y) || (m.dy > 0 && boss.y + boss.h > m.y)))
      tx = boss.x + (boss.w>>1);
    if (tx >= 0) {
      if (tx > m.x + 2 && m.dx < 2) m.dx++;
      if (tx < m.x - 2 && m.dx > -2) m.dx--;
    }
    m.x += m.dx; m.y += m.dy;
    spawn_part(m.x + (SH_MSL_W>>1), m.dy < 0 ? m.y + SH_MSL_H : m.y - 1, 0);
    if (m.y < -SH_MSL_H || m.y > SCRH || m.x < -8 || m.x > SCRW) { m.active = false; continue; }
    let hit = false;
    for (const e of enemies) if (e.active &&
        overlap(m.x, m.y, SH_MSL_W, SH_MSL_H, e.x, e.y, SH_EN_W, SH_EN_H)) {
      m.active = false; missile_boom(m.x + (SH_MSL_W>>1), m.y); hit = true; break;
    }
    if (!hit && m.active && boss.active &&
        overlap(m.x, m.y, SH_MSL_W, SH_MSL_H, boss.x, boss.y, boss.w, boss.h)) {
      m.active = false; missile_boom(m.x + (SH_MSL_W>>1), m.y);
    }
  }

  /* player bullets */
  for (const b of pbul) if (b.active) {
    b.x += b.dx; b.y += b.dy;
    if (b.y < -8 || b.y > SCRH || b.x < -4 || b.x > SCRW) b.active = false;
  }
  /* enemy bullets + graze */
  for (const b of ebul) if (b.active) {
    b.x += b.dx; b.y += b.dy;
    if (b.y > SCRH || b.x < -6 || b.x > SCRW) b.active = false;
    else if (player.alive && player.invuln === 0 &&
             overlap(b.x, b.y, SH_EB_W, SH_EB_H, player.x+3, player.y+3, SH_SHIP_W-6, SH_SHIP_H-6)) {
      b.active = false; hurt_player();
    } else if (player.alive && !b.grazed &&
             overlap(b.x, b.y, SH_EB_W, SH_EB_H, player.x-5, player.y-5, SH_SHIP_W+10, SH_SHIP_H+10)) {
      b.grazed = 1;
      score_add(10);
      if (player.combo > 0) player.combo_t = 130;
    }
  }

  /* enemies */
  for (const e of enemies) if (e.active) {
    e.t++;
    e.y += e.vy;
    if (e.type === E_WEAVER) {
      e.x = e.base + sintab[e.t & 63];
      e.x = Math.max(4, Math.min(SCRW - SH_EN_W - 4, e.x));
    }
    if (e.elite && e.type === E_WEAVER && (e.t & 31) === 0)
      add_ebullet(e.x + (SH_EN_W>>1), e.y + SH_EN_H, 0, 2);
    if (e.type === E_SHOOTER) {
      if (shooter_lingering(e)) e.y -= e.vy;
      else if (e.mode === 1 && e.y >= 120) {
        e.x += e.aux * 2;
        if (e.x < -SH_EN_W || e.x > SCRW) { e.active = false; wave_missed++; continue; }
      }
      if (--e.firecd <= 0) {
        if (e.elite) {
          add_ebullet(e.x + (SH_EN_W>>1), e.y + SH_EN_H, -1, 3);
          add_ebullet(e.x + (SH_EN_W>>1), e.y + SH_EN_H, 0, 4);
          add_ebullet(e.x + (SH_EN_W>>1), e.y + SH_EN_H, 1, 3);
        } else enemy_fire(e.x + (SH_EN_W>>1), e.y + SH_EN_H);
        e.firecd = Math.max(18, rrange(50, 100) + diff_enemy_fire_adjust());
      }
    }
    if (e.y > SCRH) { e.active = false; wave_missed++; continue; }
    if (player.alive && overlap(e.x+2, e.y+2, SH_EN_W-4, SH_EN_H-4,
                                player.x+3, player.y+3, SH_SHIP_W-6, SH_SHIP_H-6)) {
      if (player.shield > 0) { kill_enemy(e); continue; }
      if (player.invuln === 0) { kill_enemy(e); hurt_player(); continue; }
    }
    for (const b of pbul) if (b.active &&
        overlap(b.x, b.y, SH_PB_W, SH_PB_H, e.x+2, e.y, SH_EN_W-4, SH_EN_H)) {
      if (b.kind !== WT_LASER) b.active = false;
      burst(b.x, b.y, 3, C_WHITE, C_YELLOW);
      e.hp -= b.dmg;
      if (e.hp <= 0) { kill_enemy(e); break; }
    }
  }

  /* enemy separation: dissolve stacked ships (every other frame) */
  if (frame & 1) separate_enemies();

  /* popups */
  for (const p of popup) if (p.active) {
    if (p.t & 1) p.y--;
    if (--p.t <= 0) p.active = false;
  }

  /* boss */
  if (boss.active && boss.warn > 0) {
    boss.warn--;                            /* hold the entrance for the WARNING */
  } else if (boss.active && boss.die_t > 0) {
    /* staged death: inert + invulnerable, chained explosions ripple through */
    boss.die_t--;
    if (boss.die_t % 6 === 0)
      fireburst(boss.x + 4 + rnd() % (boss.w - 8), boss.y + 2 + rnd() % (boss.h - 4), 10);
    if (boss.die_t % 9 === 0) snd_sfx(SFX_EXPLODE);
    if (shk < 3) shk = 3;
    if (boss.die_t === 0) boss_finish_death();
  } else if (boss.active) {
    if (boss.entering) {
      const ry = boss_rest_y();
      boss.y += 3;
      if (boss.y >= ry) { boss.y = ry; boss.entering = false; }
    } else {
      boss_move();
      if (!((boss.kind === 9 && (boss.ty === 1 || boss.ty === 2)) ||
            (boss.kind === 14 && boss.phase === 1 && (boss.ty === 1 || boss.ty === 2)))) {
        if (boss.firecd === 18) { boss.charge = 18; snd_sfx(SFX_PHASE); }
        if (--boss.firecd <= 0) { boss_fire(); boss.firecd = diff_boss_fire_cd(); }
      }
    }
    boss.t++;
    if (boss.charge > 0) boss.charge--;
    if (boss.hurt_t > 0) boss.hurt_t--;
    if (boss.recent_dmg > 0 && (frame & 1)) boss.recent_dmg--;
    if (!boss.entering && boss.kind !== 2 && boss.kind !== 8 && --boss.support_t <= 0) {
      summon_escort();
      boss.support_t = boss.phase === 0 ? 260 : boss.phase === 1 ? 220 : 190;
    }
    if (--boss.atk_t <= 0) {
      boss.atk = (boss.atk + 1) % boss_attack_count();
      boss.atk_t = boss_atk_time();
    }
    boss.phase = boss.hp * 3 <= boss.maxhp ? 2 : boss.hp * 3 <= boss.maxhp * 2 ? 1 : 0;
    if (boss.phase !== boss.last_phase) {
      boss.last_phase = boss.phase;
      snd_sfx(SFX_PHASE);
      set_msg('BOSS PHASE');
      if ((boss.kind === 2 || boss.kind === 8)
          && boss.phase > 0 && !(boss.summons & (1 << boss.phase))) {
        boss.summons |= 1 << boss.phase;
        summon_escort();
      }
      if (boss.kind === 8) boss.ty = 1;               /* KRAKEN: recoil to the top */
      if (boss.kind === 14) { boss.ty = 0; boss.mv_t = 90; }  /* OVERLORD: new language */
    }
    for (const b of pbul) if (b.active &&
        overlap(b.x, b.y, SH_PB_W, SH_PB_H, boss.x+2, boss.y, boss.w-4, boss.h)) {
      if (b.kind !== WT_LASER) b.active = false;
      burst(b.x, b.y, 2, C_WHITE, C_YELLOW);
      boss.hp -= b.dmg;
      boss.recent_dmg = Math.min(boss.maxhp, boss.recent_dmg + b.dmg);
      boss.hurt_t = 2;
      if (boss.hp <= 0) { boss_die(); break; }
    }
    if (boss.active && boss.die_t === 0 && player.alive &&
        overlap(boss.x+2, boss.y, boss.w-4, boss.h, player.x+3, player.y+3, SH_SHIP_W-6, SH_SHIP_H-6)) {
      if (player.shield > 0 && player.ram_cd === 0) {
        apply_boss_damage(boss_pct_damage(10));
        shield_bounce(boss.x+(boss.w>>1), boss.y+(boss.h>>1));
      } else if (player.shield <= 0 && player.invuln === 0) hurt_player();
    }
  }

  /* powerups */
  for (const p of powr) if (p.active) {
    p.y += 1; p.t++;
    if (p.y > SCRH) { p.active = false; continue; }
    if (player.alive && overlap(p.x, p.y, SH_PU_W, SH_PU_H, player.x, player.y, SH_SHIP_W, SH_SHIP_H)) {
      apply_powerup(p.type); p.active = false;
    }
  }

  /* particles */
  for (const p of part) if (p.active) {
    p.x += p.dx; p.y += p.dy;
    if (--p.life <= 0) p.active = false;
  }

  /* wave director */
  if (win_pending) {
    /* step() switches to ST_WIN before freeplay wave 61 starts */
  } else if (to_spawn > 0) {
    if (--spawn_cd <= 0) { spawn_enemy(); spawn_cd = rrange(diff_spawn_cd_min(), diff_spawn_cd_max()); }
  } else if (!boss.active) {
    let clear = true;
    for (const e of enemies) if (e.active) { clear = false; break; }
    if (clear) { finish_wave(); start_wave(); }
  }

  if (flash > 0) flash--;
  if (msg_timer > 0) msg_timer--;
  if (shk > 0) { shk--; shx = rrange(-3, 3); shy = rrange(-3, 3); }
  else { shx = shy = 0; }
}

/* ---------------- drawing ---------------- */
function DS(x, y, w, h, d) { vga_sprite(x + shx, y + shy, w, h, d); }
function draw_stars() { for (const s of stars) vga_pixel(s.x, s.y, s.col); }
function draw_dust() {
  for (const d of dust) {
    vga_pixel(d.x, d.y, d.col);
    if (d.layer > 1) vga_pixel(d.x+1, d.y, d.col);
  }
}
const WNAME = ['CANNON', 'LASER', 'WAVE'];
const BOSSNAME = [
  'GORGON', 'REAPER', 'LEVIATHAN', 'SEEKER', 'MANTIS',
  'ANVIL', 'SERAPH', 'NEXUS', 'KRAKEN', 'PHANTOM',
  'CITADEL', 'VORTEX', 'BASILISK', 'TITAN', 'OVERLORD'
];
const DIFNAME = ['EASY', 'NORMAL', 'HARD'];

function draw_hud() {
  text_draw(4, 2, 'SCORE ' + pad6(score), C_WHITE);
  if (player.combo >= 2)
    text_draw(116, 2, 'X' + combo_mult(), combo_mult() >= 3 ? C_YELLOW : C_LGRAY);
  const wv = 'WAVE ' + wave;
  text_draw(SCRW - 8*wv.length - 4, 2, wv, C_LCYAN);
  text_draw(4, 190, 'SH' + player.lives, C_LGREEN);
  const weapon = player.wtype === WT_WAVE && player.wave_boost > 0 ? 'WAVE*' + player.gun : WNAME[player.wtype] + player.gun;
  text_draw(36, 190, weapon,
            player.wtype === WT_LASER ? C_LCYAN : player.wtype === WT_WAVE ? (player.wave_boost > 0 ? C_WHITE : C_LMAG) : PAL_FIRE+11);
  text_draw(124, 190, 'M' + pad2(player.msl), C_LGRAY);
  text_draw(156, 190, 'B' + pad2(player.bombs), C_LRED);
  text_draw(184, 190, 'BST', C_LGRAY);
  {
    const bw = Math.floor(player.boost * 36 / BOOST_MAX);
    const bc = player.boost > 90 ? C_LGREEN : player.boost > 35 ? C_YELLOW : C_LRED;
    vga_frame(210, 190, 38, 6, C_DGRAY);
    if (bw > 0) vga_rect(211, 191, bw, 4, bc);
  }
  if (player.rapid > 0) text_draw(252, 190, 'R', C_YELLOW);
  if (player.shield > 0)                    /* red once it's about to expire */
    text_draw(268, 190, 'S', player.shield < 70 ? C_LRED : C_LBLUE);
  if (boss.active && !boss.entering) {
    const w = Math.floor(boss.hp * 200 / boss.maxhp);
    const c = boss.phase >= 2 ? C_LRED : boss.phase >= 1 ? C_YELLOW : C_LGREEN;
    vga_frame(59, 12, 202, 6, C_DGRAY);
    vga_rect(60, 13, Math.max(0, w), 4, c);
    if (boss.recent_dmg > 0) {
      const rw = Math.max(1, Math.min(200-w, Math.floor(boss.recent_dmg * 200 / boss.maxhp)));
      if (rw > 0) vga_rect(60+w, 13, rw, 4, C_WHITE);
    }
    vga_rect(126, 12, 1, 6, C_DGRAY); vga_rect(193, 12, 1, 6, C_DGRAY);
  }
}
function draw_blasts() {
  for (const b of blast) if (b.active) {
    const r = b.t * (b.big ? 3 : 2);
    const c = PAL_FIRE + 14 - b.t;
    const cx = b.x + shx, cy = b.y + shy;
    for (let a = 0; a < 16; a++) {
      const px = cx + Math.floor(sintab[(a*4 + 16) & 63] * r / 46);
      const py = cy + Math.floor(sintab[(a*4) & 63] * r / 46);
      vga_pixel(px, py, c); vga_pixel(px+1, py, c);
    }
  }
}
function draw_enemy_anim(e) {
  if (e.type === E_WEAVER) return;
  const c = (e.t >> 2) & 1 ? PAL_FIRE+10 : PAL_FIRE+6;
  const fx = e.x + (SH_EN_W>>1) + shx, fy = e.y - 1 + shy;
  vga_pixel(fx-3, fy, c); vga_pixel(fx+2, fy, c);
}
function draw_elite_overlay(e) {
  if (!e.elite) return;
  const c = frame & 4 ? C_WHITE : C_LCYAN;
  vga_frame(e.x-1+shx, e.y-1+shy, SH_EN_W+2, SH_EN_H+2, c);
}
/* boss drawn through a shifting checkerboard: reads as dematerialising */
function draw_boss_masked() {
  const s = spr_boss[boss.spr];
  for (let y = 0; y < boss.h; y++)
    for (let x = 0; x < boss.w; x++) {
      const c = s[y * boss.w + x];
      if (!c) continue;
      if (((x ^ y) ^ (frame >> 1)) & 1) continue;
      vga_pixel(boss.x + x + shx, boss.y + y + shy, c);
    }
}
/* per-boss animated overlays: the "alive" layer on top of the static bitmap */
function draw_boss_extras(cx, cy) {
  switch (boss.kind) {
  case 3: case 12: {                        /* SEEKER / BASILISK: pupil tracks you */
    const ex = cx, ey = boss.kind === 3 ? cy : boss.y + 13 + shy;
    const dx = clamp(Math.trunc((player.x + 8 - (boss.x + (boss.w>>1))) / 24), -2, 2);
    const dy = clamp(Math.trunc((player.y + 8 - (boss.y + (boss.h>>1))) / 40), -2, 2);
    if (boss.kind === 12 && boss.ty === 1) {          /* eyelid shuts: guillotine tell */
      vga_hline(ex - 6, ey, 12, C_GREEN);
      vga_hline(ex - 6, ey - 1, 12, C_GREEN);
    } else {
      vga_rect(ex + dx - 1, ey + dy - 1, 2, 2, C_WHITE);
    }
    break; }
  case 4:                                   /* MANTIS: claw tips spark when lunging */
    if (boss.ty === 2) {
      const c = frame & 2 ? C_WHITE : C_YELLOW;
      vga_pixel(boss.x + 21 + shx, boss.y + 14 + shy, c);
      vga_pixel(boss.x + boss.w - 22 + shx, boss.y + 14 + shy, c);
    }
    break;
  case 5:                                   /* ANVIL: pistons extend during the crush */
    if (boss.ty === 2 || boss.ty === 3) {
      for (let k = 10; k <= boss.w - 12; k += 12) {
        vga_hline(boss.x + k + shx, boss.y - 3 + shy, 2, C_LGRAY);
        vga_hline(boss.x + k + shx, boss.y - 6 + shy, 2, C_DGRAY);
      }
    }
    break;
  case 7:                                   /* NEXUS: tethered orbiting fire-pods */
    for (let k = 0; k < 2; k++) {
      const tx = boss.px[k], ty = boss.py[k];
      for (let s = 1; s < 5; s++)           /* glow tether, core -> pod */
        vga_pixel(cx + Math.trunc((tx + shx - cx) * s / 5),
                  cy + Math.trunc((ty + shy - cy) * s / 5),
                  PAL_GLOW + 5 + s * 2);
      DS(tx - (SH_POD_W>>1), ty - (SH_POD_H>>1), SH_POD_W, SH_POD_H, spr_bosspod);
    }
    break;
  case 8:                                   /* KRAKEN: five writhing tentacles */
    for (let k = 0; k < 5; k++) {
      const ax = boss.x + 8 + k * 12 + shx;
      const ay = boss.y + boss.h - 2 + shy;
      for (let s = 0; s < 8; s++) {
        const px = ax + Math.trunc(sintab[(frame * 3 + k * 13 + s * 8) & 63] * s / 40);
        const py = ay + s * 3;
        const c = s < 5 ? C_GREEN : C_LGREEN;
        vga_pixel(px, py, c);
        vga_pixel(px + 1, py, c);
      }
    }
    break;
  case 10:                                  /* CITADEL: muzzle flash on the live turret */
    if (boss.firecd < 5) {
      const tx = player.x + 8 > boss.x + (boss.w>>1) ? boss.w - 20 : 8;
      vga_rect(boss.x + tx + 4 + shx, boss.y + 17 + shy, 3, 2, frame & 1 ? C_WHITE : C_YELLOW);
    }
    break;
  case 11:                                  /* VORTEX: eight orbiting singularity orbs */
    for (let k = 0; k < 8; k++) {
      const a = (boss.spin + k * 8) & 63;
      const ox = cx + Math.trunc(sintab[a] * 24 / 46);
      const oy = cy + Math.trunc(sintab[(a + 16) & 63] * 18 / 46);
      vga_rect(ox - 1, oy - 1, 2, 2, k & 1 ? C_LMAG : C_LCYAN);
    }
    break;
  case 13:                                  /* TITAN: armour cracks open per phase */
    if (boss.phase >= 1) {
      vga_rect(boss.x + 12 + shx, boss.y + 16 + shy, 8, 6, C_BLACK);
      vga_rect(boss.x + 14 + shx, boss.y + 18 + shy, 4, 2, PAL_FIRE + 6 + ((frame >> 1) & 3));
    }
    if (boss.phase >= 2) {
      vga_rect(boss.x + boss.w - 24 + shx, boss.y + 28 + shy, 10, 6, C_BLACK);
      vga_rect(boss.x + boss.w - 21 + shx, boss.y + 30 + shy, 4, 2, PAL_FIRE + 8 + ((frame >> 1) & 3));
      vga_rect(boss.x + 30 + shx, boss.y + 6 + shy, 6, 4, C_BLACK);
    }
    break;
  case 14:                                  /* OVERLORD: rotating crown spokes + aura */
    for (let k = 0; k < 4; k++) {
      const a = (boss.spin + k * 16) & 63;
      for (let s = 3; s < 6; s++)
        vga_pixel(cx + Math.trunc(sintab[a] * s * 6 / 46),
                  cy + Math.trunc(sintab[(a + 16) & 63] * s * 5 / 46),
                  s === 5 ? C_WHITE : C_LMAG);
    }
    if (boss.phase >= 2 && (frame & 2))
      vga_frame(boss.x - 3 + shx, boss.y - 3 + shy, boss.w + 6, boss.h + 6, C_LRED);
    break;
  }
}
function draw_play() {
  for (const p of part) if (p.active) {
    const c = part_col(p);
    vga_pixel(p.x+shx, p.y+shy, c);
    vga_pixel(p.x+1+shx, p.y+shy, c);
  }
  draw_blasts();
  const PU_LETTER = 'GRHLMZWB$';
  for (const p of powr) if (p.active) {
    const bob = Math.floor(sintab[(p.t*4) & 63] / 18);
    DS(p.x, p.y + bob, SH_PU_W, SH_PU_H, spr_powerup[p.type]);
    if ((frame + p.t) & 4) vga_pixel(p.x+shx, p.y+bob+shy, C_WHITE);
    text_draw(p.x+2+shx, p.y+2+bob+shy, PU_LETTER[p.type], C_BLACK);
  }
  for (const e of enemies) if (e.active) {
    draw_enemy_anim(e);
    DS(e.x, e.y, SH_EN_W, SH_EN_H, spr_enemy[e.type]);
    draw_elite_overlay(e);
    if (e.drop_class === DROP_SUPPLY) {
      const c = frame & 4 ? C_WHITE : C_LGREEN;
      const ex = e.x + (SH_EN_W>>1) + shx, ey = e.y - 4 + shy;
      vga_hline(ex-2, ey, 5, c);
      vga_pixel(ex, ey-2, c); vga_pixel(ex, ey+2, c);
    }
  }
  if (boss.active && boss.warn === 0) {
    const cx = boss.x + (boss.w>>1) + shx, cy = boss.y + (boss.h>>1) + shy;
    const teleporting = (boss.kind === 9 && (boss.ty === 1 || boss.ty === 2) && !boss.entering)
                     || (boss.kind === 14 && boss.phase === 1
                         && (boss.ty === 1 || boss.ty === 2) && !boss.entering);
    if (teleporting) draw_boss_masked();    /* dematerialising checkerboard */
    else if (boss.die_t > 0) {              /* dying: flickering, breaking apart */
      if (boss.die_t & 2) DS(boss.x, boss.y, boss.w, boss.h, spr_boss[boss.spr]);
      else draw_boss_masked();
    } else DS(boss.x, boss.y, boss.w, boss.h, spr_boss[boss.spr]);
    draw_boss_extras(cx, cy);               /* per-boss animated overlays */
    if (boss.die_t === 0) {
      if (boss.phase >= 1 && boss.kind !== 13) {   /* scorch (TITAN has armour-break) */
        vga_hline(cx - ((boss.w/3)|0), cy - ((boss.h/6)|0), (2*boss.w/3)|0, C_DGRAY);
        vga_hline(cx - ((boss.w/4)|0), cy + ((boss.h/4)|0), (boss.w/2)|0, C_DGRAY);
      }
      if (boss.phase >= 2 && boss.kind !== 13) {
        vga_frame(cx - 9, cy - 6, 18, 12, C_LRED);
        vga_pixel(cx - ((boss.w/3)|0), cy, C_YELLOW);
        vga_pixel(cx + ((boss.w/3)|0), cy, C_YELLOW);
      }
      if (boss.hurt_t > 0)                  /* white hit flash */
        vga_frame(boss.x-1+shx, boss.y-1+shy, boss.w+2, boss.h+2, C_WHITE);
      if (boss.charge > 0)
        vga_frame(boss.x-2+shx, boss.y-2+shy, boss.w+4, boss.h+4, boss.charge & 2 ? C_WHITE : C_LRED);
      if ((boss.kind === 1 && boss.ty === 1) || (boss.kind === 14 && boss.ty === 2)) {
        const mx = boss.tx + (boss.w>>1) + shx;
        vga_hline(mx-7, player.y+7, 15, frame&2 ? C_WHITE : C_LRED);
      }
      if (boss.kind === 9 && boss.ty === 2)
        vga_frame(boss.x-3+shx,boss.y-3+shy,boss.w+6,boss.h+6,C_LMAG);
      vga_rect(cx - 3, cy - 3, 6, 6,
               PAL_FIRE + 8 + ((frame >> (boss.phase >= 2 ? 0 : 1)) & 7));
    }
  }
  for (const m of mslA) if (m.active) DS(m.x, m.y, SH_MSL_W, SH_MSL_H, m.dy > 0 ? spr_missile_down : spr_missile);
  for (const b of ebul) if (b.active) DS(b.x, b.y, SH_EB_W, SH_EB_H, spr_ebullet);
  for (const b of pbul) if (b.active) DS(b.x, b.y, SH_PB_W, SH_PB_H, spr_pbullet[b.kind]);
  if (player.alive && !(player.invuln > 0 && frame & 2)) {
    /* shield ring; blinks off intermittently during the last ~2 s */
    if (player.shield > 0 && !(player.shield < 70 && (frame & 2)))
      vga_frame(player.x-2+shx, player.y-2+shy, SH_SHIP_W+4, SH_SHIP_H+4,
                frame & 2 ? PAL_GLOW+14 : PAL_GLOW+8);
    DS(player.x, player.y, SH_SHIP_W, SH_SHIP_H, player.facing_down ? spr_ship_down[ship_bank] : spr_ship[ship_bank]);
    const fc = frame & 2 ? PAL_FIRE+12 : PAL_FIRE+8;
    const fx = player.x + shx, fy = player.facing_down ? player.y - 1 + shy : player.y + SH_SHIP_H + shy;
    const tail = player.facing_down ? -1 : 1;
    vga_pixel(fx+3, fy, fc); vga_pixel(fx+12, fy, fc);
    vga_pixel(fx+3, fy+tail, PAL_FIRE+5); vga_pixel(fx+12, fy+tail, PAL_FIRE+5);
    if (frame & 1) { vga_pixel(fx+3, fy+tail*2, PAL_FIRE+3); vga_pixel(fx+12, fy+tail*2, PAL_FIRE+3); }
    if (player.boosting) {
      vga_hline(fx+1, fy+tail*3, 5, PAL_FIRE+11); vga_hline(fx+10, fy+tail*3, 5, PAL_FIRE+11);
      vga_pixel(fx+3, fy+tail*4, PAL_FIRE+6); vga_pixel(fx+12, fy+tail*4, PAL_FIRE+6);
    }
  }
  for (const p of popup) if (p.active)
    text_draw(p.x + shx, p.y + shy, p.txt, p.t > 10 ? C_WHITE : C_LGRAY);
  draw_hud();
  if (boss.active && boss.warn > 0) {          /* klaxon intro before the entrance */
    const bc = boss.warn & 8 ? C_LRED : C_RED;
    vga_rect(0, 64, SCRW, 2, bc);
    vga_rect(0, 100, SCRW, 2, bc);
    if (boss.warn & 8) text_center(74, '! WARNING !', C_WHITE);
    text_center(88, BOSSNAME[boss.kind], C_YELLOW);
  } else if (wave_banner > 0) {
    if (boss.active) {
      text_center(76, BOSSNAME[boss.kind] + ' APPROACHING', boss.phase >= 2 ? C_LRED : C_YELLOW);
    } else {
      text_center(78, 'WAVE ' + wave, C_YELLOW);
      text_center(90, 'GET READY', C_LCYAN);
    }
  }
  if (msg_timer > 0) text_center(104, msg_text, C_YELLOW);
}
function draw_title() {
  const page = Math.floor(frame / 210) & 3;
  const fx = (frame % 360) - 32;
  text_center(34, 'AYRIEN ASSAULT', C_YELLOW);
  text_center(50, 'A VGA SPACE SHOOTER', C_LGRAY);
  if (frame & 16) text_center(84, 'CLICK / TAP TO PLAY', C_WHITE);
  text_center(104, '< DIFFICULTY: ' + DIFNAME[g_diff] + ' >',
              g_diff === DIF_HARD ? C_LRED : g_diff === DIF_EASY ? C_LGREEN : C_YELLOW);
  if (page === 0) {
    text_center(120, 'WASD/ARROWS OR DRAG', C_LCYAN);
    text_center(132, 'SHIFT BOOST  SPACE FIRE', C_LCYAN);
    text_center(144, 'CTRL MSL  B BOMB  H HELP', C_DGRAY);
  } else if (page === 1) {
    text_center(122, 'HI ' + pad6(g_hi[0].score) + '  ' + g_hi[0].name.slice(0,8), C_LGREEN);
    text_center(136, '2  ' + pad6(g_hi[1].score) + '  ' + g_hi[1].name.slice(0,8), C_LCYAN);
  } else if (page === 2) {
    text_center(120, 'LAST WAVE ' + last_wave, C_LCYAN);
    text_center(134, 'MAX COMBO ' + last_combo, C_YELLOW);
    text_center(148, 'BOSSES ' + last_bosses, C_LGREEN);
  } else {
    text_center(122, 'ELITES AFTER WAVE 6', C_LMAG);
    text_center(136, 'GRAZE FOR BONUS', C_LCYAN);
  }
  text_center(156, scoreStatus, scoresOnline ? C_LGREEN : C_YELLOW);
  if (fx < SCRW) vga_sprite(fx, 166, SH_SHIP_W, SH_SHIP_H, spr_ship[1]);
  if (fx > 40 && fx < SCRW + 40)
    vga_sprite(SCRW - fx, 20, SH_EN_W, SH_EN_H, spr_enemy[Math.floor(frame/70) % 3]);
}
function draw_over() {
  text_center(60, 'GAME OVER', C_LRED);
  text_center(84, 'FINAL SCORE ' + pad6(score), C_WHITE);
  text_center(100, 'REACHED WAVE ' + wave, C_LCYAN);
  text_center(116, 'BEST COMBO X' + player.max_combo, C_YELLOW);
  if (frame & 16) text_center(150, 'FINALIZING RUN', C_LGRAY);
}
function draw_win() {
  const fly = (win_t * 2) % 390 - 30;
  for (let i=0;i<26;i++) {
    const half=88-Math.floor(i*i/9);
    if(half>0) vga_hline(160-half,174+i,half*2,PAL_NEB+5+(i>>2));
  }
  for (let i=0;i<8;i++) {
    const fx=34+((win_t*(i+3)+i*57)%252), fy=30+((i*37)%82), r=(win_t*2+i*9)&15;
    const c=i%3===0?C_YELLOW:(i&1)?C_LCYAN:C_LMAG;
    if(r<8){vga_hline(fx-r,fy,r*2+1,c);vga_pixel(fx,fy-r,c);vga_pixel(fx,fy+r,c);}
  }
  for(let i=0;i<18;i++) vga_rect((i*53+win_t*(1+i%3))%SCRW,(i*29+win_t*2)%154,(i&1)?2:1,2,
                                  i%3===0?C_YELLOW:(i&1)?C_LCYAN:C_LMAG);
  vga_sprite(fly,126,SH_SHIP_W,SH_SHIP_H,spr_ship[2]);
  vga_sprite(fly-28,138,SH_SHIP_W,SH_SHIP_H,spr_ship[1]);
  vga_sprite(fly-56,126,SH_SHIP_W,SH_SHIP_H,spr_ship[0]);
  for(let i=0;i<8;i++) vga_rect(136+((i*37+win_t)%50),54+((i*19+(win_t>>1))%38),2+(i&2),2,(i&1)?C_LRED:C_DGRAY);
  vga_frame(50,4,220,52,frame&4?C_YELLOW:C_WHITE);
  text_center(10,'*** VICTORY ***',frame&8?C_YELLOW:C_WHITE);
  text_center(26,'CAMPAIGN COMPLETE',C_YELLOW);
  text_center(42,win_t<70?'OVERLORD DESTROYED':'THE SECTOR IS FREE',C_WHITE);
  text_center(90,'SCORE '+pad6(score)+'  COMBO X'+player.max_combo,C_LGREEN);
  text_center(106,bosses_defeated+' / '+NBOSS+' BOSSES DEFEATED',C_LCYAN);
  if(win_t>=WIN_INPUT_DELAY){if(frame&16)text_center(152,'ENTER FREEPLAY',C_WHITE);text_center(166,'ESC SAVE + TITLE',C_LGRAY);}
  else text_center(156,win_t<70?'THREAT ELIMINATED':win_t<140?'FLEET SALUTE':'VICTORY CONFIRMED',C_LMAG);
}
function help_row(y, pu, txt) {
  vga_sprite(20, y, SH_PU_W, SH_PU_H, spr_powerup[pu]);
  text_draw(35, y+3, 'GRHLMZWB$'[pu], C_BLACK);
  text_draw(34, y+2, 'GRHLMZWB$'[pu], C_WHITE);
  text_draw(50, y+2, txt, C_LGRAY);
}
function help_arrow(x, y, up, col) {
  if (up) {
    vga_pixel(x+3, y, col); vga_hline(x+2, y+1, 3, col);
    vga_hline(x+1, y+2, 5, col); vga_hline(x, y+3, 7, col);
  } else {
    vga_hline(x, y, 7, col); vga_hline(x+1, y+1, 5, col);
    vga_hline(x+2, y+2, 3, col); vga_pixel(x+3, y+3, col);
  }
}
function help_nav() {
  const upc = help_page > 0 ? C_LCYAN : C_DGRAY;
  const dnc = help_page < HELP_PAGES-1 ? C_LCYAN : C_DGRAY;
  help_arrow(14, 182, true, upc); text_draw(24, 180, 'UP', upc);
  help_arrow(62, 182, false, dnc); text_draw(72, 180, 'DOWN', dnc);
  text_center(180, 'PAGE ' + (help_page+1) + '/' + HELP_PAGES, C_LGRAY);
  text_draw(240, 180, 'ESC BACK', C_LCYAN);
}
function help_scroll(dir) { help_page = Math.max(0, Math.min(HELP_PAGES-1, help_page + dir)); }
function draw_help() {
  if (help_page === 0) {
    text_center(6, 'CONTROLS', C_YELLOW);
    text_draw(28, 24,  'ARROWS / WASD   MOVE SHIP', C_WHITE);
    text_draw(28, 38,  'SPACE           FIRE', C_LGRAY);
    text_draw(28, 52,  'SHIFT           BOOST', C_LGREEN);
    text_draw(28, 66,  'CTRL            HOMING MISSILE', C_LCYAN);
    text_draw(28, 80,  'B               SMART BOMB', C_LMAG);
    text_draw(28, 94,  'P               PAUSE / RESUME', C_LGRAY);
    text_draw(28, 108, 'M               MUSIC TOGGLE', C_LGRAY);
    text_draw(28, 122, 'H               OPEN / CLOSE HELP', C_LGRAY);
    text_draw(28, 136, 'ESC             BACK / TITLE', C_LGRAY);
    text_draw(28, 154, 'HELP PAGES: USE UP / DOWN', C_YELLOW);
  } else if (help_page === 1) {
    text_center(6, 'PICKUPS', C_YELLOW);
    help_row(22,  PU_GUN,     'GUN: +1 EQUIPPED WEAPON LEVEL');
    help_row(38,  PU_RAPID,   'RAPID: FASTER FIRE, TIMED');
    help_row(54,  PU_SHIELD,  'SHIELD: 10 SEC INVULNERABLE');
    help_row(70,  PU_LIFE,    'LIFE: EXTRA SHIP (MAX 9)');
    help_row(86,  PU_MISSILE, 'MISSILES: +4 AMMO (MAX 30)');
    help_row(102, PU_LASER,   'LASER: FAST PIERCING GUN');
    help_row(118, PU_WAVE,    'WAVE: WIDE ARC GUN');
    help_row(134, PU_BOMB,    'BOMB: +1 SMART BOMB (MAX 10)');
    help_row(150, PU_SCORE,   'SCORE GEM: RISKY WAVE BONUS');
  } else if (help_page === 2) {
    text_center(6, 'WEAPONS', C_YELLOW);
    text_draw(20, 24,  'CANNON: BALANCED SPREAD', C_YELLOW);
    text_draw(20, 38,  'LASER: PIERCING, 2 DAMAGE', C_LCYAN);
    text_draw(20, 52,  'LEVELS 1-4 FIRE 1/2/3/4 LANES', C_LCYAN);
    text_draw(20, 66,  'WAVE: WIDE CROWD CONTROL', C_LMAG);
    text_draw(20, 80,  'LEVELS 1-4 FIRE 5/7/9/11 SHOTS', C_LMAG);
    text_draw(20, 94,  'G UPGRADES EQUIPPED WEAPON', C_WHITE);
    text_draw(20, 108, 'Z ON WAVE: 10 SEC PIERCE BOOST', C_WHITE);
    text_draw(20, 122, 'R RAPID SHORTENS FIRE COOLDOWN', C_LGRAY);
    text_draw(20, 136, 'MISSILES HOME AHEAD, BEST VS BOSS', C_LGRAY);
    text_draw(20, 150, 'DEATH LOWERS GUN LEVEL BY ONE', C_DGRAY);
  } else if (help_page === 3) {
    text_center(6, 'SURVIVAL', C_YELLOW);
    text_draw(20, 24,  'BST DRAINS DURING BOOST', C_LGREEN);
    text_draw(20, 38,  'RELEASE SHIFT; BST RECHARGES', C_LGREEN);
    text_draw(20, 52,  'SHIELD: 10 SEC INVULNERABLE', C_LCYAN);
    text_draw(20, 66,  'SHIELD RAM DESTROYS SMALL ENEMIES', C_LCYAN);
    text_draw(20, 80,  'BOSS RAM: 10 PCT DAMAGE + BOUNCE', C_LCYAN);
    text_draw(20, 94,  'SMART BOMB CLEARS ENEMY SHOTS', C_LMAG);
    text_draw(20, 108, 'FLY ABOVE BOSS TO FIRE DOWN', C_WHITE);
    text_draw(20, 122, 'BOSS FLASH = HEAVY ATTACK TELL', C_YELLOW);
    text_draw(20, 136, 'EASY/NORMAL GIVE RECOVERY SHIELDS', C_DGRAY);
    text_draw(20, 150, 'PICKUPS FAVOR RESOURCES YOU NEED', C_DGRAY);
  } else if (help_page === 4) {
    text_center(6, 'ENEMIES AND BOSSES', C_YELLOW);
    text_draw(20, 24,  'SCOUT: FAST STRAIGHT ATTACKER', C_LGRAY);
    text_draw(20, 38,  'WEAVER: SWERVES; ELITE TRAILS', C_LGRAY);
    text_draw(20, 52,  'SHOOTER: FIRES; ELITE SPLITS', C_LGRAY);
    text_draw(20, 66,  'BOX WARNS: TOUGH ELITE ENEMY', C_LCYAN);
    text_draw(20, 80,  'STRONGER ATTACKS + BONUS SCORE', C_WHITE);
    text_draw(20, 94,  'EVERY 4TH WAVE IS A BOSS', C_YELLOW);
    text_draw(20, 108, 'GREEN + MARKS SUPPLY ESCORT', C_LGREEN);
    text_draw(20, 122, 'SUPPLY ESCORT GUARANTEES DROP', C_LGREEN);
    text_draw(20, 136, 'SUPPORT DROPS CAPPED AT 4 / BOSS', C_DGRAY);
    text_draw(20, 150, 'BEAT W60 TO UNLOCK FREEPLAY', C_LMAG);
  } else {
    text_center(6, 'SCORING AND DIFFICULTY', C_YELLOW);
    text_draw(20, 22,  'FAST KILLS BUILD COMBO TO X5', C_WHITE);
    text_draw(20, 35,  'GRAZE SHOTS: +10 + COMBO TIME', C_LCYAN);
    text_draw(20, 48,  'NO HIT / PERFECT WAVE MEDALS', C_YELLOW);
    text_draw(20, 61,  'BOMBS SCORE FOR CLEARED SHOTS', C_LMAG);
    text_draw(20, 74,  '$ GEM VALUE RISES WITH WAVE', C_WHITE);
    text_draw(20, 87,  'EASY SCORE X1.00', C_LGREEN);
    text_draw(20, 100, 'NORMAL SCORE X1.25', C_LCYAN);
    text_draw(20, 113, 'HARD SCORE X1.60', C_LRED);
    text_draw(20, 126, 'HARD: MORE + DENSER ATTACKS', C_LRED);
    text_draw(20, 139, 'NORMAL DROPS E/N/H: 18/15/12 PCT', C_DGRAY);
    text_draw(20, 152, 'ESCORT DROPS E/N/H: 55/50/45 PCT', C_LGREEN);
    text_draw(20, 165, 'ALL MODES SHARE ONE HIGH SCORE', C_WHITE);
  }
  help_nav();
}
function draw_scores() {
  text_center(16, 'HIGH SCORES', C_YELLOW);
  for (let i = 0; i < HISCORE_N; i++) {
    const b = (i+1) + '. ' + g_hi[i].name.padEnd(8) + ' ' + pad6(g_hi[i].score);
    text_center(34 + i*12, b, i === 0 ? C_WHITE : C_LCYAN);
  }
  text_center(164, scoreStatus, scoresOnline ? C_LGREEN : C_YELLOW);
  if (frame & 16) text_center(180, mobileMode ? 'TITLE OR REPLAY BELOW' : 'SPACE TITLE   CTRL REPLAY', C_LGREEN);
}
function draw_entry() {
  text_center(60, 'NEW HIGH SCORE!', C_YELLOW);
  text_center(80, 'SCORE ' + pad6(score), C_WHITE);
  text_center(108, 'ENTER NAME:', C_LCYAN);
  text_center(124, entry_name + (frame & 8 ? '_' : ' '), C_WHITE);
  text_center(150, 'TYPE THEN PRESS ENTER', C_LGRAY);
  text_center(164, entryNameError ? 'NAME REQUIRED' : (entryReplay ? 'ESC SAVES + REPLAYS' : 'ESC SAVES SCORE'),
              entryNameError ? C_LRED : C_DGRAY);
}

/* ================= state machine (game_run) ================= */
function begin_run() {
  reset_game(); start_wave();
  state = ST_PLAY; paused = false;
  pointerAim.active = false; pointerAim.id = null;   /* don't yank the ship to a stale cursor */
  resetMobileMove();
  snd_music_game(0);
}
function syncEntryInput(focus) {
  if (!nameInput) return;
  if (nameInput.value !== entry_name) nameInput.value = entry_name;
  if (focus) setTimeout(() => { nameInput.focus(); nameInput.select(); }, 0);
}
function syncHtmlUi(forceFocus) {
  const stateChanged = uiState !== state;
  const entering = state === ST_ENTRY;
  const playing = state === ST_PLAY;
  const winning = state === ST_WIN && win_t >= WIN_INPUT_DELAY;
  const portraitBlocked = isPortraitMobile();
  const suspendForOrientation = playing && portraitBlocked;
  const showLaunch = mobileMode && !mobileLaunchDismissed && !fullscreenElement();
  if (suspendForOrientation !== mobileOrientationSuspended) {
    mobileOrientationSuspended = suspendForOrientation;
    if (mobileOrientationSuspended) {
      releaseKey('SHIFT');
      pointerAim.active = false;
      pointerAim.id = null;
      resetMobileMove();
    }
    snd_pause(paused || mobileOrientationSuspended);
  }
  document.body.classList.toggle('entry-mode', entering);
  document.body.classList.toggle('score-mode', state === ST_SCORES);
  document.body.classList.toggle('victory-mode', winning);
  document.body.classList.toggle('help-mode', state === ST_HELP);
  document.body.classList.toggle('mobile-ui', mobileMode);
  document.body.classList.toggle('playing', playing);
  document.body.classList.toggle('paused', playing && paused);
  document.body.classList.toggle('portrait-blocked', portraitBlocked);
  document.body.classList.toggle('mobile-launch', showLaunch);
  if (pauseButton) pauseButton.textContent = paused ? 'RESUME' : 'PAUSE';
  if (entering) syncEntryInput(forceFocus || uiState !== state);
  else if (nameInput) nameInput.blur();
  updateSideMenu();
  uiState = state;
  if (stateChanged) rescale();
}
function enterHighScoreEntry(replay) {
  entry_name = pilotName;
  typedQueue.length = 0;
  entrySubmitted = false;
  entryNameError = false;
  entryReplay = !!replay;
  state = ST_ENTRY;
  syncHtmlUi(true);
}
function backspaceEntry() {
  if (state !== ST_ENTRY || entrySubmitted) return;
  entry_name = entry_name.slice(0, -1);
  entryNameError = false;
  syncEntryInput(false);
}
function submitEntry(replay) {
  if (state !== ST_ENTRY || entrySubmitted) return;
  const savedName = cleanName(entry_name);
  if (!savedName) {
    entryNameError = true;
    set_msg('NAME REQUIRED');
    syncEntryInput(true);
    return;
  }
  entrySubmitted = true;
  pilotName = savedName;
  entry_name = savedName;
  syncEntryInput(false);
  hi_insert(entry_rank, savedName, score);
  hi_save();
  submitScore(savedName, score);
  if (replay || entryReplay) begin_run();
  else state = ST_SCORES;
  syncHtmlUi(false);
}
function finishVictoryRun() {
  finish_wave(); remember_run();
  entry_rank = hi_qualifies(score);
  snd_music_set(MUS_TITLE);
  if (entry_rank >= 0) enterHighScoreEntry(false);
  else { state = ST_SCORES; syncHtmlUi(false); }
}
function clearCombatFx() { flash = shk = shx = shy = 0; }
function winFreeplayRequested() {
  const enter = key_hit('ENTER');
  key_hit('SPACE');                    // held fire never skips victory
  return win_t >= WIN_INPUT_DELAY && enter;
}
function continueFreeplay() {
  win_pending = 0; finish_wave(); start_wave();
  state = ST_PLAY; paused = false; snd_music_game(bosses_defeated); clearInput(); syncHtmlUi(false);
}
function playSuspended() {
  return state === ST_PLAY && (paused || mobileOrientationSuspended);
}
function step() {
  if (key_hit('M')) snd_music_toggle();
  if (!playSuspended()) frame++;
  if (state === ST_PLAY && key_hit('P')) {
    paused = !paused;
    snd_pause(paused || mobileOrientationSuspended);
  }
  if (!playSuspended()) { update_stars(); update_dust(); }

  switch (state) {
  case ST_TITLE:
    if (key_hit('UP') && g_diff > 0) g_diff--;
    if (key_hit('DOWN') && g_diff < 2) g_diff++;
    if (key_hit('H')) { help_page = 0; state = ST_HELP; }
    if (key_hit('SPACE')) begin_run();
    break;
  case ST_HELP:
    if (key_hit('UP')) help_scroll(-1);
    if (key_hit('DOWN')) help_scroll(1);
    if (key_hit('SPACE')) help_page = help_page < HELP_PAGES-1 ? help_page+1 : 0;
    if (key_hit('ESC') || key_hit('H')) state = ST_TITLE;
    break;
  case ST_PLAY:
    if (paused && key_hit('SPACE')) {
      paused = false;
      snd_pause(mobileOrientationSuspended);
    }
    if (!playSuspended()) update_play();
    if (win_pending) {
      remember_run();
      snd_music_set(MUS_WIN);
      state = ST_WIN; win_t = 0;
      clearCombatFx();
      clearInput();
      break;
    }
    if (key_hit('ESC')) {
      paused = !paused;
      snd_pause(paused || mobileOrientationSuspended);
    }
    if (!player.alive) {
      remember_run();
      snd_music_set(MUS_TITLE);
      over_timer = 160;
      key_hit('SPACE');                 /* held fire never advances Game Over */
      key_hit('ENTER');
      state = ST_OVER;
    }
    break;
  case ST_OVER:
    if (over_timer > 0) over_timer--;
    if ((over_timer <= 130 && key_hit('ENTER')) || over_timer === 0) {
      clearCombatFx();
      entry_rank = hi_qualifies(score);
      if (entry_rank >= 0) enterHighScoreEntry(true);
      else state = ST_SCORES;
    }
    break;
  case ST_ENTRY: {
    const c = kbd_getchar();
    if (c && entry_name.length < NAME_LEN) {
      entry_name = cleanName(entry_name + c);
      syncEntryInput(false);
    }
    if (key_hit('BKSP')) backspaceEntry();
    if (key_hit('ENTER')) submitEntry(false);
    if (key_hit('ESC')) submitEntry(true);
    break; }
  case ST_SCORES:
    if (key_hit('SPACE')) state = ST_TITLE;
    if (key_hit('CTRL')) begin_run();
    break;
  case ST_WIN:
    win_t++;
    { const save = key_hit('ESC'), freeplay = winFreeplayRequested();
      if (win_t < WIN_INPUT_DELAY && win_t % 28 === 0) snd_sfx(SFX_PHASE);
      if (win_t >= WIN_INPUT_DELAY && save) { finishVictoryRun(); clearInput(); }
      else if (freeplay) continueFreeplay(); }
    break;
  }

  /* render */
  if (flash > 0) vga_clear(C_RED);
  else vga_bg_blit((frame >> 1) % SCRH);
  draw_stars();
  draw_dust();
  switch (state) {
  case ST_TITLE:  draw_title(); break;
  case ST_HELP:   draw_help(); break;
  case ST_PLAY:   draw_play(); if (paused) text_center(96, 'PAUSED', C_WHITE); break;
  case ST_OVER:   draw_over(); break;
  case ST_WIN:    draw_win(); break;
  case ST_SCORES: draw_scores(); break;
  case ST_ENTRY:  draw_entry(); break;
  }
  if (musicMuted) text_draw(SCRW-32, 182, 'MUS', C_LRED);
  if (sfxMuted)   text_draw(SCRW-32, 190, 'SFX', C_LRED);
  if (!playSuspended()) {
    if ((frame & 3) === 0) vga_cycle_palette();
  }
  syncHtmlUi(false);
}

/* ================= presentation ================= */
const wrapEl = document.getElementById('wrap');
const canvas = document.getElementById('screen');
const nameInput = document.getElementById('nameInput');
const joystick = document.getElementById('joystick');
const joystickBase = document.getElementById('joystickBase');
const joystickKnob = document.getElementById('joystickKnob');
const pauseButton = document.getElementById('pauseButton');
const ctx = canvas.getContext('2d');
const imageData = ctx.createImageData(SCRW, SCRH);
const coarsePointer = window.matchMedia('(hover: none), (pointer: coarse)');
const urlParams = new URLSearchParams(window.location.search);
const mobileOverride = urlParams.get('mobile') === '1';
const mobileMove = { active:false, pointerId:null, x:0, y:0 };
let mobileMode = coarsePointer.matches || mobileOverride;
let mobileLaunchDismissed = !mobileMode;

function isPortraitMobile() {
  return mobileMode && window.innerHeight > window.innerWidth;
}
function resetMobileMove() {
  mobileMove.active = false;
  mobileMove.pointerId = null;
  mobileMove.x = 0;
  mobileMove.y = 0;
  if (joystickKnob) joystickKnob.style.transform = 'translate(-50%, -50%)';
}
function updateMobileMode() {
  const next = coarsePointer.matches || mobileOverride;
  if (next !== mobileMode) {
    mobileMode = next;
    mobileLaunchDismissed = !mobileMode;
    clearInput();
  }
  syncHtmlUi(false);
  rescale();
}

function present() {
  const px = imageData.data;
  for (let i = 0, j = 0; i < fb.length; i++, j += 4) {
    const c = fb[i] * 3;
    px[j] = paletteRGB[c]; px[j+1] = paletteRGB[c+1]; px[j+2] = paletteRGB[c+2]; px[j+3] = 255;
  }
  ctx.putImageData(imageData, 0, 0);
}
function rescale() {
  const w = wrapEl.clientWidth || window.innerWidth;
  const h = wrapEl.clientHeight || window.innerHeight;
  const rawScale = Math.min(w / SCRW, h / SCRH);
  let scale = rawScale < 2 || fullscreenElement() ? rawScale : Math.floor(rawScale);
  scale = Math.max(1, scale);
  canvas.style.width = (SCRW * scale) + 'px';
  canvas.style.height = (SCRH * scale) + 'px';
}
function fullscreenElement() { return document.fullscreenElement || document.webkitFullscreenElement || null; }
function fullscreenTarget() { return document.documentElement; }
function fullscreenSupported() {
  const target = fullscreenTarget();
  return !!(target.requestFullscreen || target.webkitRequestFullscreen);
}
function showUiStatus(s) { setScoreStatus(s); set_msg(s); }
function updateFullscreenButtons() {
  const supported = fullscreenSupported();
  const active = !!fullscreenElement();
  document.querySelectorAll('[data-action="fullscreen"]').forEach(btn => {
    btn.disabled = !supported;
    btn.textContent = active ? 'EXIT FULLSCREEN' : 'FULLSCREEN';
    btn.title = supported ? (active ? 'Exit fullscreen' : 'Fullscreen') : 'Fullscreen unavailable';
  });
  document.querySelectorAll('[data-action="play-fullscreen"]').forEach(btn => {
    btn.disabled = !supported;
    btn.title = supported ? 'Play fullscreen' : 'Fullscreen unavailable - rotate manually';
  });
}
function lockLandscape() {
  if (!screen.orientation || !screen.orientation.lock) return;
  screen.orientation.lock('landscape').catch(() => {});
}
function unlockOrientation() {
  if (!screen.orientation || !screen.orientation.unlock) return;
  try { screen.orientation.unlock(); } catch (e) {}
}
function toggleFullscreen() {
  if (!fullscreenSupported()) { showUiStatus('FULLSCREEN UNSUPPORTED'); updateFullscreenButtons(); return; }
  try {
    if (fullscreenElement()) {
      const exit = document.exitFullscreen || document.webkitExitFullscreen;
      const p = exit ? exit.call(document) : null;
      if (p && p.catch) p.catch(() => showUiStatus('FULLSCREEN FAILED'));
      unlockOrientation();
    } else {
      mobileLaunchDismissed = true;
      syncHtmlUi(false);
      const target = fullscreenTarget();
      const req = target.requestFullscreen || target.webkitRequestFullscreen;
      const p = req.call(target);
      if (p && p.then) p.then(lockLandscape).catch(() => showUiStatus('FULLSCREEN FAILED'));
      else lockLandscape();
    }
  } catch (e) {
    showUiStatus('FULLSCREEN FAILED');
  }
  setTimeout(() => { rescale(); updateFullscreenButtons(); }, 80);
}
function fullscreenChanged() {
  if (fullscreenElement()) mobileLaunchDismissed = true;
  syncHtmlUi(false);
  rescale();
  updateFullscreenButtons();
}
window.addEventListener('resize', () => { syncHtmlUi(false); rescale(); });
window.addEventListener('orientationchange', () => setTimeout(updateMobileMode, 60));
if (coarsePointer.addEventListener) coarsePointer.addEventListener('change', updateMobileMode);
else if (coarsePointer.addListener) coarsePointer.addListener(updateMobileMode);
document.addEventListener('fullscreenchange', fullscreenChanged);
document.addEventListener('webkitfullscreenchange', fullscreenChanged);
/* Mouse: left button fires, right button boosts, and the ship follows the
   cursor while it is over the play area. Touch/pen keep the drag-to-steer feel
   (the on-screen buttons handle fire/boost there). */
canvas.addEventListener('contextmenu', (e) => e.preventDefault());
/* Derive fire/boost from the whole button bitmask on every mouse event, so
   chording (right pressed while left is held, in either order) always resolves
   correctly regardless of per-button event quirks. buttons bit 0 = left,
   bit 1 = right. */
function syncMouseButtons(e) {
  if (e.buttons & 1) pressKey('SPACE'); else releaseKey('SPACE');
  if (e.buttons & 2) pressKey('SHIFT'); else releaseKey('SHIFT');
}
canvas.addEventListener('pointerdown', (e) => {
  bootAudio();
  if (state === ST_ENTRY) {
    syncEntryInput(true);
    e.preventDefault();
    return;
  }
  if (state === ST_TITLE) {
    tapKey('SPACE');
    e.preventDefault();
    return;
  }
  if (state === ST_PLAY && paused) {   /* click the screen to resume */
    paused = false; snd_pause(mobileOrientationSuspended);
    e.preventDefault();
    return;
  }
  if (e.pointerType === 'mouse') {
    syncMouseButtons(e);
    pointerAim.active = true;
    pointerAim.id = null;
    setPointerAim(e);
    /* capture so a button held while dragging past the edge keeps tracking
       (and its release is still delivered) */
    try { canvas.setPointerCapture(e.pointerId); } catch (_) {}
  } else {
    resetMobileMove();
    beginTouchAim(e);
    canvas.setPointerCapture(e.pointerId);
  }
  e.preventDefault();
});
canvas.addEventListener('pointermove', (e) => {
  if (e.pointerType === 'mouse') {
    syncMouseButtons(e);
    if (state === ST_PLAY) { pointerAim.active = true; setPointerAim(e); }
    return;
  }
  if (pointerAim.active && (pointerAim.id === null || pointerAim.id === e.pointerId)) {
    updateTouchAim(e);
    e.preventDefault();
  }
});
canvas.addEventListener('pointerrawupdate', (e) => {
  if (e.pointerType !== 'mouse' && pointerAim.active && pointerAim.id === e.pointerId) {
    updateTouchAim(e);
    e.preventDefault();
  }
});
function endPointerAim(e) {
  if (pointerAim.id === null || pointerAim.id === e.pointerId) {
    pointerAim.active = false;
    pointerAim.id = null;
  }
}
canvas.addEventListener('pointerup', (e) => {
  if (e.pointerType === 'mouse') {
    syncMouseButtons(e);
    /* keep pointerAim.active: the ship holds the last cursor position even
       after the button is released or the mouse leaves the play field, and
       resumes tracking when the cursor returns */
    return;
  }
  endPointerAim(e);
});
canvas.addEventListener('pointercancel', (e) => {
  if (e.pointerType === 'mouse') { releaseKey('SPACE'); releaseKey('SHIFT'); return; }
  endPointerAim(e);
});

function placeJoystick(e) {
  if (!joystick || !joystickBase) return;
  const zone = joystick.getBoundingClientRect();
  const base = joystickBase.getBoundingClientRect();
  const half = base.width * 0.5;
  const x = Math.max(half + 4, Math.min(zone.width - half - 4, e.clientX - zone.left));
  const y = Math.max(half + 4, Math.min(zone.height - half - 4, e.clientY - zone.top));
  joystickBase.style.left = `${x}px`;
  joystickBase.style.top = `${y}px`;
  joystickBase.style.bottom = 'auto';
  joystickBase.style.transform = 'translate(-50%, -50%)';
}
function updateJoystick(e) {
  if (!joystickBase || mobileMove.pointerId !== e.pointerId) return;
  const r = joystickBase.getBoundingClientRect();
  const radius = r.width * 0.5;
  const limit = radius * 0.58;
  let dx = e.clientX - (r.left + radius);
  let dy = e.clientY - (r.top + radius);
  const distance = Math.hypot(dx, dy);
  if (distance > limit) {
    dx = dx * limit / distance;
    dy = dy * limit / distance;
  }
  const nx = dx / limit;
  const ny = dy / limit;
  const dead = 0.15;
  mobileMove.x = nx < -dead ? -1 : nx > dead ? 1 : 0;
  mobileMove.y = ny < -dead ? -1 : ny > dead ? 1 : 0;
  if (joystickKnob) {
    joystickKnob.style.transform = `translate(calc(-50% + ${dx}px), calc(-50% + ${dy}px))`;
  }
}
function endJoystick(e) {
  if (mobileMove.pointerId !== e.pointerId) return;
  resetMobileMove();
  e.preventDefault();
}
if (joystick) {
  joystick.addEventListener('pointerdown', (e) => {
    if (!mobileMode || state !== ST_PLAY || mobileMove.active) return;
    bootAudio();
    pointerAim.active = false;
    pointerAim.id = null;
    placeJoystick(e);
    mobileMove.active = true;
    mobileMove.pointerId = e.pointerId;
    try { joystick.setPointerCapture(e.pointerId); } catch (_) {}
    updateJoystick(e);
    e.preventDefault();
  });
  joystick.addEventListener('pointermove', (e) => {
    if (mobileMove.pointerId !== e.pointerId) return;
    updateJoystick(e);
    e.preventDefault();
  });
  joystick.addEventListener('pointerrawupdate', (e) => {
    if (mobileMove.pointerId !== e.pointerId) return;
    updateJoystick(e);
    e.preventDefault();
  });
  joystick.addEventListener('pointerup', endJoystick);
  joystick.addEventListener('pointercancel', endJoystick);
  joystick.addEventListener('lostpointercapture', (e) => {
    if (mobileMove.pointerId === e.pointerId) resetMobileMove();
  });
}

function wireButton(btn) {
  const hold = btn.dataset.hold;
  const tap = btn.dataset.tap;
  const action = btn.dataset.action;
  const entry = btn.dataset.entry;
  btn.addEventListener('pointerdown', (e) => {
    bootAudio();
    if (hold) pressKey(hold);
    if (tap) tapKey(tap);
    if (action === 'fullscreen') toggleFullscreen();
    else if (action === 'play-fullscreen') {
      mobileLaunchDismissed = true;
      syncHtmlUi(false);
      toggleFullscreen();
    }
    else if (action === 'continue-windowed') {
      mobileLaunchDismissed = true;
      syncHtmlUi(false);
      rescale();
    }
    else if (action === 'newgame') begin_run();
    else if (action === 'music') snd_music_toggle();
    else if (action === 'sfx') snd_sfx_toggle();
    else if (action === 'scores') showScores();
    else if (action === 'title') returnToTitle();
    else if (action === 'replay') begin_run();
    else if (action === 'freeplay') { if (state === ST_WIN && win_t >= WIN_INPUT_DELAY) continueFreeplay(); }
    else if (action === 'save-title') { if (state === ST_WIN && win_t >= WIN_INPUT_DELAY) finishVictoryRun(); }
    if (entry === 'BKSP') backspaceEntry();
    if (entry === 'ENTER') submitEntry(false);
    btn.setPointerCapture(e.pointerId);
    e.preventDefault();
  });
  const release = (e) => {
    if (hold) releaseKey(hold);
    e.preventDefault();
  };
  btn.addEventListener('pointerup', release);
  btn.addEventListener('pointercancel', release);
  btn.addEventListener('lostpointercapture', () => { if (hold) releaseKey(hold); });
}
document.querySelectorAll('[data-hold], [data-tap], [data-action], [data-entry]').forEach(wireButton);
if (nameInput) {
  nameInput.addEventListener('input', () => {
    entry_name = cleanName(nameInput.value);
    entryNameError = false;
    if (nameInput.value !== entry_name) nameInput.value = entry_name;
  });
  nameInput.addEventListener('keydown', (e) => {
    if (e.code === 'Enter') { submitEntry(false); e.preventDefault(); }
    if (e.code === 'Escape') { submitEntry(true); e.preventDefault(); }
  });
}
/* Jump to the high-score table from any menu (not mid-run). */
function showScores() {
  if (state === ST_PLAY && !paused) return;
  syncScores();
  state = ST_SCORES;
}
function returnToTitle() {
  clearCombatFx();
  clearInput();
  state = ST_TITLE;
  paused = false;
  snd_music_set(MUS_TITLE);
  syncHtmlUi(false);
}
/* Reflect current mute state on the side-menu audio buttons. */
function updateAudioButtons() {
  document.querySelectorAll('[data-action="music"]').forEach(b => b.classList.toggle('off', musicMuted));
  document.querySelectorAll('[data-action="sfx"]').forEach(b => b.classList.toggle('off', sfxMuted));
  const m = document.querySelector('#sideMenu [data-action="music"]');
  const s = document.querySelector('#sideMenu [data-action="sfx"]');
  if (m) m.textContent = musicMuted ? 'MUSIC OFF' : 'MUSIC';
  if (s) s.textContent = sfxMuted ? 'SFX OFF' : 'SFX';
}
/* Touch layouts own the whole control surface. Desktop keeps the menu rail on
   menu screens only, never during play or fullscreen. */
const fsMedia = window.matchMedia('(display-mode: fullscreen)');
/* True for both the Fullscreen API (requestFullscreen) and native/F11 browser
   fullscreen. F11 never sets document.fullscreenElement, but it does match the
   display-mode media query, so check both. */
function isViewportFullscreen() {
  return !!fullscreenElement() || fsMedia.matches ||
         (screen.height && window.innerHeight >= screen.height);
}
function updateSideMenu() {
  const inPlay = state === ST_PLAY && !paused;
  const hide = coarsePointer.matches || window.innerWidth <= 720 || inPlay || state === ST_WIN || state === ST_ENTRY || isViewportFullscreen();
  document.body.classList.toggle('hide-side-menu', hide);
}

/* ================= main loop: fixed logic rate ================= */
/* LOGIC_HZ matches the paced DOS build. The game is frame-rate-locked
   (px/frame), so 35 Hz keeps the browser and native versions at the same
   on-screen speed on 33 MHz, 66 MHz, and faster machines. */
const STEP_MS = 1000 / LOGIC_HZ;
let acc = 0, lastT = 0;
function loop(t) {
  requestAnimationFrame(loop);
  if (!lastT) lastT = t;
  acc += t - lastT;
  lastT = t;
  if (acc > STEP_MS * 8) acc = STEP_MS * 8;   /* tab-switch catchup clamp */
  let ran = false;
  while (acc >= STEP_MS) { step(); acc -= STEP_MS; ran = true; }
  if (ran) present();
  scheduleMusic();
}

/* ================= boot ================= */
sprites_init();
set_palette();
hi_load();
syncScores();
init_stars();
init_dust();
gen_nebula();
snd_music_set(MUS_TITLE);
if (urlParams.get('shot') === 'win') {
  score = 987650; player.max_combo = 42; bosses_defeated = 15;
  state = ST_WIN; win_t = WIN_INPUT_DELAY + 30; mobileLaunchDismissed = true; snd_music_set(MUS_WIN);
} else if (urlParams.get('shot') === 'entry') {
  score = 987650; entry_rank = 0; mobileLaunchDismissed = true; enterHighScoreEntry(false);
}
updateFullscreenButtons();
updateAudioButtons();
syncHtmlUi(false);
rescale();
requestAnimationFrame(loop);
