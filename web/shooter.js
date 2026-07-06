/* STELLAR ASSAULT - browser port of the MS-DOS/VGA original.
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

const MAX_PBULLET=48, MAX_EBULLET=64, MAX_ENEMY=28, MAX_POWERUP=8,
      MAX_PART=160, MAX_STARS=96, MAX_MISSILE=6;

const HISCORE_N=10, NAME_LEN=8;

const SH_SHIP_W=16, SH_SHIP_H=16, SH_EN_W=16, SH_EN_H=14,
      SH_PB_W=3, SH_PB_H=8, SH_EB_W=5, SH_EB_H=7,
      SH_PU_W=12, SH_PU_H=12, SH_BOSS_W=48, SH_BOSS_H=34,
      SH_MSL_W=5, SH_MSL_H=10;

const BOOST_MAX=140, BOOST_MIN_START=12, BOOST_DRAIN=2,
      BOOST_RECHARGE=1, BOOST_RECHARGE_CD=25;

const SFX_FIRE=0, SFX_EXPLODE=1, SFX_POWER=2, SFX_HIT=3, SFX_BOSS=4,
      SFX_MISSILE=5, SFX_PICK1=6, SFX_PICK2=7, SFX_COMBO=8, SFX_PHASE=9,
      SFX_BOOST=10;
const MUS_TITLE=0, MUS_GAME=1;

const ST_TITLE=0, ST_PLAY=1, ST_OVER=2, ST_ENTRY=3, ST_SCORES=4, ST_HELP=5;

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
const spr_enemy = [build(SH_EN_W,SH_EN_H,SCOUT_ART), build(SH_EN_W,SH_EN_H,WEAVER_ART), build(SH_EN_W,SH_EN_H,SHOOTER_ART)];
const spr_missile = build(SH_MSL_W, SH_MSL_H, MISSILE_ART);
const spr_ebullet = build(SH_EB_W, SH_EB_H, EBULLET_ART);
const spr_pbullet = [build(SH_PB_W, SH_PB_H, PBULLET_ART), null, null];
const spr_powerup = [];
const spr_boss = [null, null, null];

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
function build_boss(kind) {
  const W = SH_BOSS_W, H = SH_BOSS_H;
  const d = new Uint8Array(W * H);
  const hull = kind === 1 ? C_LRED : kind === 2 ? C_LGREEN : C_LGRAY;
  const edge = kind === 1 ? C_RED  : kind === 2 ? C_GREEN  : C_DGRAY;
  const core = kind === 2 ? PAL_GLOW+4 : PAL_FIRE+4;
  for (let y = 0; y < H; y++) {
    let hw;
    if (y < 4) hw = kind === 2 ? 8 : 4;
    else if (y < 10) hw = 6 + (y-4)*3;
    else if (y < 24) hw = 22;
    else hw = 22 - (y-24)*2;
    if (hw < 0) hw = 0;
    for (let x = 24-hw; x <= 24+hw; x++)
      d[y*W+x] = (x < 24-hw+2 || x > 24+hw-2) ? edge : hull;
  }
  for (let y = 8; y < 16; y++) {
    for (let x = 1; x < 7; x++)  d[y*W+x] = x < 3 ? edge : hull;
    for (let x = 41; x < 47; x++) d[y*W+x] = x > 44 ? edge : hull;
  }
  for (let x = 2; x < 6; x++)  d[15*W+x] = PAL_GLOW+12;
  for (let x = 42; x < 46; x++) d[15*W+x] = PAL_GLOW+12;
  for (let x = 8; x < 40; x++) if (d[21*W+x]) d[21*W+x] = PAL_GLOW+8;
  for (let y = 12; y < 20; y++) for (let x = 20; x < 28; x++) d[y*W+x] = core;
  for (let y = 24; y < 30; y++) {
    d[y*W+14] = PAL_FIRE+10; d[y*W+15] = PAL_FIRE+10;
    d[y*W+32] = PAL_FIRE+10; d[y*W+33] = PAL_FIRE+10;
  }
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
function sprites_init() {
  make_banked_ships();
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
  spr_boss[0] = build_boss(0); spr_boss[1] = build_boss(1); spr_boss[2] = build_boss(2);
}

/* ================= input ================= */
const keyState = {}, keyEdge = {};
const typedQueue = [];
const pointerAim = { active:false, x:SCRW >> 1, y:SCRH - 30, id:null };
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
let muted = false;
let musicTrack = -1, musIdx = 0, nextNoteTime = 0, musicPaused = false;

/* DOS melodies (freq Hz, duration frames @70Hz) - the arrangement adds voices */
const title_mel = [
  [392,10],[523,10],[659,10],[784,16],[0,3],[659,8],[784,18],[0,5],
  [440,10],[587,10],[698,10],[880,16],[0,3],[784,8],[659,18],[0,6],
  [523,8],[523,8],[494,8],[440,8],[392,14],[0,3],
  [440,8],[494,10],[523,20],[0,10]];
const game_mel = [
  [110,7],[0,9],[110,7],[0,9],[131,7],[0,9],[110,7],[0,9],
  [147,7],[0,9],[131,7],[0,9],[110,8],[0,24],
  [98,7],[0,9],[110,7],[0,9],[123,7],[0,9],[131,7],[0,9],
  [110,7],[0,9],[98,7],[0,9],[110,8],[0,24]];

function bootAudio() {
  if (ac) { if (ac.state === 'suspended') ac.resume(); return; }
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
  if (ac.state === 'suspended') ac.resume();
  nextNoteTime = ac.currentTime + 0.05;
  const boot = document.getElementById('boot');
  if (boot) boot.remove();
}
function snd_muted() { return muted; }
function snd_mute_toggle() {
  muted = !muted;
  if (masterGain) masterGain.gain.setTargetAtTime(muted ? 0 : 0.5, ac.currentTime, 0.02);
  if (!muted && ac) nextNoteTime = ac.currentTime + 0.05;   /* don't burst-schedule missed notes */
}
function duck(amount, secs) {
  if (!ac || muted) return;
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
}
function snd_sfx(id) {
  if (!ac || muted) return;
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
  musicTrack = track; musIdx = 0; musicPaused = false;
  if (ac) {
    /* clear any pending SFX "duck" ramps and force full music level, so
       switching tracks (or returning to the menu) always restores volume */
    musicGain.gain.cancelScheduledValues(ac.currentTime);
    musicGain.gain.setValueAtTime(0.42, ac.currentTime);
    nextNoteTime = ac.currentTime + 0.08;
  }
}
function snd_music_stop() { musicTrack = -1; }

/* Pause/resume audio to mirror the DOS build's snd_silence() on pause.
   Ramps music to silence (killing already-scheduled notes routed through
   musicGain) and stops new notes from being scheduled. */
function snd_pause(p) {
  musicPaused = p;
  if (!ac) return;
  if (p) musicGain.gain.cancelScheduledValues(ac.currentTime);
  musicGain.gain.setTargetAtTime(p ? 0 : 0.42, ac.currentTime, 0.015);
  if (!p) nextNoteTime = ac.currentTime + 0.05;   /* resume without burst catch-up */
}

/* three-voice arrangement: lead + sub-octave bass + noise hat */
function scheduleMusic() {
  if (!ac || muted || musicPaused || musicTrack < 0) return;
  if (nextNoteTime < ac.currentTime - 0.1) nextNoteTime = ac.currentTime + 0.05; /* tab-switch snap */
  const mel = musicTrack === MUS_TITLE ? title_mel : game_mel;
  while (nextNoteTime < ac.currentTime + 0.30) {
    const [f, frames] = mel[musIdx];
    const dur = frames / LOGIC_HZ;   /* frame-locked tempo, like snd_update() */
    const t = nextNoteTime;
    if (f > 0) {
      if (musicTrack === MUS_TITLE) {
        voice('square', f, f, t, dur * 0.92, 0.16, musicGain);
        voice('square', f * 1.005, f * 1.005, t, dur * 0.92, 0.10, musicGain); /* chorus */
        voice('triangle', f / 2, f / 2, t, dur, 0.22, musicGain);              /* bass   */
        noise(t, 0.03, 0.06, 6000, 6000, 'highpass', musicGain);               /* hat    */
      } else {
        /* in-game groove, transposed up an octave so it's audible on small
           speakers (the DOS pulse sat at 98-147 Hz = near-inaudible in a
           browser). Lead + fifth + original bass body + hat. */
        voice('triangle', f * 2, f * 2, t, dur, 0.24, musicGain);              /* audible lead */
        voice('square', f * 3, f * 3, t, dur * 0.5, 0.06, musicGain);          /* fifth-ish sparkle */
        voice('square', f, f, t, dur, 0.16, musicGain);                        /* bass body    */
        if ((musIdx & 3) === 0) noise(t, 0.03, 0.05, 4000, 4000, 'highpass', musicGain); /* hat */
      }
    }
    nextNoteTime += dur;
    musIdx = (musIdx + 1) % mel.length;
  }
}

/* ================= high scores (server + local fallback) ================= */
const HS_KEY = 'stellar_assault_hiscores';
const SCORE_API = '/api/stellar-scores.ashx';
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
    const v = cleanScoreList(JSON.parse(localStorage.getItem(HS_KEY)));
    if (v) { g_hi = v; return; }
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
const pbul = [], ebul = [], enemies = [], powr = [], part = [], stars = [], mslA = [], blast = [], dust = [];
for (let i = 0; i < MAX_PBULLET; i++) pbul.push({active:false});
for (let i = 0; i < MAX_EBULLET; i++) ebul.push({active:false});
for (let i = 0; i < MAX_ENEMY; i++) enemies.push({active:false});
for (let i = 0; i < MAX_POWERUP; i++) powr.push({active:false, t:0, type:0});
for (let i = 0; i < MAX_PART; i++) part.push({active:false});
for (let i = 0; i < MAX_STARS; i++) stars.push({});
for (let i = 0; i < MAX_MISSILE; i++) mslA.push({active:false});
for (let i = 0; i < 10; i++) blast.push({active:false});
for (let i = 0; i < 36; i++) dust.push({});

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
let state = ST_TITLE, paused = false;
let entry_rank = -1, entry_name = '', over_timer = 0, help_page = 0;
let entrySubmitted = false, uiState = null;

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
  if (g_diff === DIF_EASY) return 56 - boss.phase * 8;
  if (g_diff === DIF_HARD) return 38 - boss.phase * 10;
  return 46 - boss.phase * 10;
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
  for (const a of [pbul, ebul, enemies, powr, part, mslA, blast]) for (const o of a) o.active = false;
  boss.active = false;
  player.x = 152; player.y = 168;
  player.lives = g_diff === DIF_EASY ? 5 : g_diff === DIF_HARD ? 2 : 3;
  player.gun = GUN_MIN; player.wtype = WT_CANNON;
  player.msl = 5; player.bombs = g_diff === DIF_HARD ? 1 : 2;
  player.shield = g_diff === DIF_EASY ? 400 : 0;
  player.invuln = 0; player.firecd = 0; player.rapid = 0;
  player.boost = BOOST_MAX; player.boost_cd = 0; player.boosting = false;
  player.combo = 0; player.combo_t = 0; player.max_combo = 0; player.alive = true;
  score = 0; wave = 0; flash = 0; shk = 0;
  wave_banner = 0; msg_timer = 0; msg_text = ''; ship_bank = 1;
  wave_kills = wave_missed = wave_hit = combo_broken = 0;
  risk_spawned = 0; bosses_defeated = 0;
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
  if (wave % 4 === 0) {
    const boss_order = [1, 0, 2];
    const boss_index = Math.floor(wave / 4) - 1;
    boss.active = true; boss.entering = true;
    boss.kind = boss_order[boss_index % 3];
    boss.phase = 0; boss.last_phase = 0; boss.summons = 0;
    boss.x = (SCRW - SH_BOSS_W) >> 1; boss.y = -SH_BOSS_H;
    boss.maxhp = boss.hp = 36 + wave * diff_boss_hp_mul() + boss_index * 8;
    boss.dir = 1; boss.t = 0; boss.firecd = 60; boss.charge = 0;
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
  e.vy = 1 + (wave > 6 ? 1 : 0) + (g_diff === DIF_HARD ? 1 : 0);
  e.firecd = rrange(40, 90) + diff_enemy_fire_adjust();
  let elite_chance = wave >= 13 ? 18 : 12;
  if (g_diff === DIF_EASY) elite_chance -= 4;
  if (g_diff === DIF_HARD) elite_chance += 5;
  e.elite = (wave >= 7 && (rnd() % 100) < elite_chance) ? 1 : 0;
  e.mode = 0; e.aux = 0;
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
    return true;
  }
  return false;
}
function summon_escort() {
  const type = boss.kind === 2 ? E_WEAVER : E_SCOUT;
  const x0 = boss.x < 80 ? boss.x + 36 : boss.x - 28;
  for (let n = 0; n < 3; n++) spawn_one(type, x0 + n*20, -SH_EN_H - n*12, 0);
  set_msg('ESCORTS INBOUND');
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
    init_enemy(e, pick_type(), rrange(8, SCRW - SH_EN_W - 8));
    to_spawn--;
    return;
  }
}

/* ---------------- firing ---------------- */
function add_pbullet(x, y, dx, dy, kind) {
  const b = free_bullet(pbul);
  if (b) { b.active = true; b.x = x; b.y = y; b.dx = dx; b.dy = dy; b.kind = kind;
           b.grazed = 0; b.dmg = kind === WT_LASER ? 2 : 1; }
}
function player_fire() {
  const cx = player.x + (SH_SHIP_W>>1) - (SH_PB_W>>1);
  const cy = player.y - 4;
  const g = player.gun;
  let cd;
  switch (player.wtype) {
  case WT_LASER:
    add_pbullet(cx, cy-2, 0, -12, WT_LASER);
    if (g >= 3) { add_pbullet(cx-6, cy, 0, -12, WT_LASER); add_pbullet(cx+6, cy, 0, -12, WT_LASER); }
    cd = 7;
    break;
  case WT_WAVE: {
    const spread = g + 1;
    for (let k = -spread; k <= spread; k++) add_pbullet(cx + k*3, cy, k, -5, WT_WAVE);
    cd = 15;
    break; }
  default:
    switch (g) {
    case 1: add_pbullet(cx-3, cy, 0, -7, 0); add_pbullet(cx+3, cy, 0, -7, 0); break;
    case 2: add_pbullet(cx, cy-2, 0, -7, 0);
            add_pbullet(cx-5, cy, 0, -7, 0); add_pbullet(cx+5, cy, 0, -7, 0); break;
    case 3: add_pbullet(cx, cy-2, 0, -7, 0);
            add_pbullet(cx-5, cy, -1, -7, 0); add_pbullet(cx+5, cy, 1, -7, 0); break;
    default: add_pbullet(cx, cy-2, 0, -7, 0);
             add_pbullet(cx-5, cy, 0, -7, 0); add_pbullet(cx+5, cy, 0, -7, 0);
             add_pbullet(cx-7, cy+2, -2, -7, 0); add_pbullet(cx+7, cy+2, 2, -7, 0); break;
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
    m.y = player.y - SH_MSL_H;
    m.dx = 0;
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
  const bx = boss.x + (SH_BOSS_W>>1), by = boss.y + SH_BOSS_H - 6;
  switch (boss.kind) {
  case 1: {
    const dir = player.x + 8 > bx ? 1 : -1;
    add_ebullet(bx-8, by, dir, 3); add_ebullet(bx+8, by, dir, 3);
    add_ebullet(bx, by, 0, 4);
    if (g_diff !== DIF_EASY && boss.phase >= 1) add_ebullet(bx, by, dir*2, 3);
    if (g_diff === DIF_HARD && boss.phase >= 1) {
      add_ebullet(bx-18, by-2, -2, 3); add_ebullet(bx+18, by-2, 2, 3);
    }
    break; }
  case 2:
    if (g_diff === DIF_EASY) {
      for (let k = -2; k <= 2; k++) add_ebullet(bx, by, k, 3);
    } else if (g_diff === DIF_HARD) {
      for (let k = -4; k <= 4; k++) add_ebullet(bx, by, k, 3);
      if (boss.phase >= 1) {
        for (let k = -2; k <= 2; k++) add_ebullet(bx, by, k*2, 2);
        add_ebullet(bx-20, by-4, 1, 4); add_ebullet(bx+20, by-4, -1, 4);
      }
    } else {
      for (let k = -3; k <= 3; k++) add_ebullet(bx, by, k, 3);
      if (boss.phase >= 1) for (let k = -2; k <= 2; k++) add_ebullet(bx, by, k*2, 2);
    }
    break;
  default:
    if (g_diff === DIF_EASY) {
      for (let k = -1; k <= 1; k++) add_ebullet(bx, by, k, 3);
      if (boss.phase >= 2) add_ebullet(bx, by, 0, 5);
    } else {
      for (let k = -2; k <= 2; k++) add_ebullet(bx, by, k, 3);
      if ((g_diff === DIF_HARD && boss.phase >= 1) || boss.phase >= 2)
        for (let k = -2; k <= 2; k++) add_ebullet(bx, by, k, 5);
    }
    break;
  }
  snd_sfx(SFX_HIT);
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
  switch (type) {
    case PU_GUN:     if (player.gun < GUN_MAX) player.gun++; break;
    case PU_RAPID:   player.rapid = 700; break;
    case PU_SHIELD:  player.shield = 500; break;
    case PU_LIFE:    if (player.lives < 9) player.lives++; break;
    case PU_MISSILE: player.msl = Math.min(30, player.msl + 4); break;
    case PU_LASER:   player.wtype = WT_LASER; break;
    case PU_WAVE:    player.wtype = WT_WAVE; break;
    case PU_BOMB:    if (player.bombs < 10) player.bombs++; break;
    case PU_SCORE:   score_add(500 + wave * 50); break;
  }
  if (type === PU_LIFE || type === PU_SHIELD) snd_sfx(SFX_PICK1);
  else if (type === PU_SCORE) snd_sfx(SFX_COMBO);
  else snd_sfx(SFX_PICK2);
  score_add(50);
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
  if (boss.active) { boss.hp -= 18; if (boss.hp <= 0) boss_die(); }
  snd_sfx(SFX_EXPLODE);
}

/* ---------------- damage ---------------- */
function hurt_player() {
  if (player.invuln > 0) return;
  wave_hit = 1; combo_broken = 1;
  player.combo = 0; player.combo_t = 0;
  if (player.shield > 0) {
    player.shield = 0; player.invuln = 40;
    burst(player.x+8, player.y+8, 12, C_LBLUE, C_WHITE);
    snd_sfx(SFX_HIT);
    return;
  }
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
      player.shield = g_diff === DIF_EASY ? 360 : 240;
    last_death_score = score;
  }
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
  score_add(enemy_score(e) * combo_mult());
  if (!risk_spawned && player.combo >= 10) {
    drop_powerup(rrange(96, 212), rrange(38, 72), PU_SCORE);
    risk_spawned = 1;
  }
  const drop_chance = g_diff === DIF_EASY ? 18 : g_diff === DIF_HARD ? 12 : 15;
  if (rnd() % 100 < drop_chance) {
    const r = rnd() % 100;
    const t = r < 16 ? PU_GUN : r < 30 ? PU_RAPID : r < 44 ? PU_SHIELD
            : r < 64 ? PU_MISSILE : r < 76 ? PU_LASER : r < 88 ? PU_WAVE
            : r < 95 ? PU_BOMB : PU_LIFE;
    drop_powerup(cx - (SH_PU_W>>1), cy, t);
  }
  e.active = false;
  snd_sfx(tier_up ? SFX_COMBO : SFX_EXPLODE);
}
function boss_die() {
  fireburst(boss.x + (SH_BOSS_W>>1), boss.y + (SH_BOSS_H>>1), 60);
  spawn_blast(boss.x + (SH_BOSS_W>>1), boss.y + (SH_BOSS_H>>1), 3);
  score_add(5000 * combo_mult());
  flash = 8; shk = 24;
  bosses_defeated++;
  force_powerup(boss.x + 10, boss.y + 10, PU_LIFE);
  force_powerup(boss.x + SH_BOSS_W - 22, boss.y + 10, PU_BOMB);
  force_powerup(boss.x + (SH_BOSS_W>>1) - (SH_PU_W>>1), boss.y + 20, PU_MISSILE);
  boss.active = false;
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
  if (boss.active && overlap(mx-26, my-26, 52, 52, boss.x, boss.y, SH_BOSS_W, SH_BOSS_H)) {
    boss.hp -= 18;
    if (boss.hp <= 0) boss_die();
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
    if (key_pressed('LEFT'))  { player.x -= sp; ship_bank = 0; }
    if (key_pressed('RIGHT')) { player.x += sp; ship_bank = 2; }
    if (key_pressed('UP'))    player.y -= sp;
    if (key_pressed('DOWN'))  player.y += sp;
    if (pointerAim.active) {
      const tx = pointerAim.x - (SH_SHIP_W >> 1);
      const ty = pointerAim.y - (SH_SHIP_H >> 1);
      const dx = tx - player.x;
      const dy = ty - player.y;
      if (Math.abs(dx) > 1) {
        const mx = Math.max(-sp, Math.min(sp, dx));
        player.x += mx;
        ship_bank = mx < 0 ? 0 : 2;
      }
      if (Math.abs(dy) > 1) player.y += Math.max(-sp, Math.min(sp, dy));
    }
    if (player.boosting && (frame & 1)) {
      spawn_part(player.x+4, player.y+SH_SHIP_H+1, 0);
      spawn_part(player.x+12, player.y+SH_SHIP_H+1, 0);
    }
    player.x = Math.max(0, Math.min(SCRW - SH_SHIP_W, player.x));
    player.y = Math.max(8, Math.min(SCRH - SH_SHIP_H, player.y));
    if (player.firecd > 0) player.firecd--;
    if (key_pressed('SPACE') && player.firecd <= 0) player_fire();
    if (key_hit('CTRL')) fire_missile();
    if (key_hit('B')) smart_bomb();
    if (player.invuln > 0) player.invuln--;
    if (player.shield > 0) player.shield--;
    if (player.rapid > 0) player.rapid--;
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
    for (const e of enemies) if (e.active && e.y < m.y) {
      const dx = e.x + (SH_EN_W>>1) - m.x, dy = e.y + (SH_EN_H>>1) - m.y;
      const d = dx*dx + dy*dy;
      if (d < best) { best = d; tx = e.x + (SH_EN_W>>1); }
    }
    if (boss.active && boss.y < m.y) tx = boss.x + (SH_BOSS_W>>1);
    if (tx >= 0) {
      if (tx > m.x + 2 && m.dx < 2) m.dx++;
      if (tx < m.x - 2 && m.dx > -2) m.dx--;
    }
    m.x += m.dx; m.y -= 4;
    spawn_part(m.x + (SH_MSL_W>>1), m.y + SH_MSL_H, 0);
    if (m.y < -SH_MSL_H || m.x < -8 || m.x > SCRW) { m.active = false; continue; }
    let hit = false;
    for (const e of enemies) if (e.active &&
        overlap(m.x, m.y, SH_MSL_W, SH_MSL_H, e.x, e.y, SH_EN_W, SH_EN_H)) {
      m.active = false; missile_boom(m.x + (SH_MSL_W>>1), m.y); hit = true; break;
    }
    if (!hit && m.active && boss.active &&
        overlap(m.x, m.y, SH_MSL_W, SH_MSL_H, boss.x, boss.y, SH_BOSS_W, SH_BOSS_H)) {
      m.active = false; missile_boom(m.x + (SH_MSL_W>>1), m.y);
    }
  }

  /* player bullets */
  for (const b of pbul) if (b.active) {
    b.x += b.dx; b.y += b.dy;
    if (b.y < -8 || b.x < -4 || b.x > SCRW) b.active = false;
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
      if (e.y > 40 && e.y < 120) e.y -= e.vy;
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
    if (player.alive && player.invuln === 0 &&
        overlap(e.x+2, e.y+2, SH_EN_W-4, SH_EN_H-4, player.x+3, player.y+3, SH_SHIP_W-6, SH_SHIP_H-6)) {
      kill_enemy(e); hurt_player(); continue;
    }
    for (const b of pbul) if (b.active &&
        overlap(b.x, b.y, SH_PB_W, SH_PB_H, e.x+2, e.y, SH_EN_W-4, SH_EN_H)) {
      if (b.kind !== WT_LASER) b.active = false;
      burst(b.x, b.y, 3, C_WHITE, C_YELLOW);
      e.hp -= b.dmg;
      if (e.hp <= 0) { kill_enemy(e); break; }
    }
  }

  /* boss */
  if (boss.active) {
    if (boss.entering) {
      boss.y += 2;
      if (boss.y >= 14) { boss.y = 14; boss.entering = false; }
    } else {
      const spd = 2 + boss.phase;
      boss.x += boss.dir * spd;
      if (boss.x < 4) { boss.x = 4; boss.dir = 1; }
      if (boss.x > SCRW - SH_BOSS_W - 4) { boss.x = SCRW - SH_BOSS_W - 4; boss.dir = -1; }
      if (boss.firecd === 18) { boss.charge = 18; snd_sfx(SFX_PHASE); }
      if (--boss.firecd <= 0) { boss_fire(); boss.firecd = diff_boss_fire_cd(); }
    }
    boss.t++;
    if (boss.charge > 0) boss.charge--;
    boss.phase = boss.hp * 3 <= boss.maxhp ? 2 : boss.hp * 3 <= boss.maxhp * 2 ? 1 : 0;
    if (boss.phase !== boss.last_phase) {
      boss.last_phase = boss.phase;
      snd_sfx(SFX_PHASE);
      set_msg('BOSS PHASE');
      if ((boss.kind === 0 || boss.kind === 2) && boss.phase > 0 && !(boss.summons & (1 << boss.phase))) {
        boss.summons |= 1 << boss.phase;
        summon_escort();
      }
    }
    for (const b of pbul) if (b.active &&
        overlap(b.x, b.y, SH_PB_W, SH_PB_H, boss.x+2, boss.y, SH_BOSS_W-4, SH_BOSS_H)) {
      if (b.kind !== WT_LASER) b.active = false;
      burst(b.x, b.y, 2, C_WHITE, C_YELLOW);
      boss.hp -= b.dmg;
      if (boss.hp <= 0) { boss_die(); break; }
    }
    if (boss.active && player.alive && player.invuln === 0 &&
        overlap(boss.x+2, boss.y, SH_BOSS_W-4, SH_BOSS_H, player.x+3, player.y+3, SH_SHIP_W-6, SH_SHIP_H-6))
      hurt_player();
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
  if (to_spawn > 0) {
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
const BOSSNAME = ['DREADNOUGHT', 'WARSHIP', 'HIVE'];
const DIFNAME = ['EASY', 'NORMAL', 'HARD'];

function draw_hud() {
  text_draw(4, 2, 'SCORE ' + pad6(score), C_WHITE);
  if (player.combo >= 2)
    text_draw(116, 2, 'X' + combo_mult(), combo_mult() >= 3 ? C_YELLOW : C_LGRAY);
  const wv = 'WAVE ' + wave;
  text_draw(SCRW - 8*wv.length - 4, 2, wv, C_LCYAN);
  text_draw(4, 190, 'SH' + player.lives, C_LGREEN);
  text_draw(36, 190, WNAME[player.wtype] + player.gun,
            player.wtype === WT_LASER ? C_LCYAN : player.wtype === WT_WAVE ? C_LMAG : PAL_FIRE+11);
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
  if (player.shield > 0) text_draw(268, 190, 'S', C_LBLUE);
  if (boss.active && !boss.entering) {
    const w = Math.floor(boss.hp * 200 / boss.maxhp);
    const c = boss.phase >= 2 ? C_LRED : boss.phase >= 1 ? C_YELLOW : C_LGREEN;
    vga_frame(59, 12, 202, 6, C_DGRAY);
    vga_rect(60, 13, Math.max(0, w), 4, c);
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
  }
  if (boss.active) {
    DS(boss.x, boss.y, SH_BOSS_W, SH_BOSS_H, spr_boss[boss.kind]);
    if (boss.phase >= 1) {
      vga_hline(boss.x+9+shx, boss.y+9+shy, 30, C_DGRAY);
      vga_hline(boss.x+13+shx, boss.y+25+shy, 22, C_DGRAY);
    }
    if (boss.phase >= 2) {
      vga_frame(boss.x+15+shx, boss.y+7+shy, 18, 22, C_LRED);
      vga_pixel(boss.x+10+shx, boss.y+18+shy, C_YELLOW);
      vga_pixel(boss.x+37+shx, boss.y+18+shy, C_YELLOW);
    }
    if (boss.charge > 0)
      vga_frame(boss.x-2+shx, boss.y-2+shy, SH_BOSS_W+4, SH_BOSS_H+4, boss.charge & 2 ? C_WHITE : C_LRED);
    vga_rect(boss.x+21+shx, boss.y+13+shy, 6, 6,
             PAL_FIRE + 8 + ((frame >> (boss.phase >= 2 ? 0 : 1)) & 7));
  }
  for (const m of mslA) if (m.active) DS(m.x, m.y, SH_MSL_W, SH_MSL_H, spr_missile);
  for (const b of ebul) if (b.active) DS(b.x, b.y, SH_EB_W, SH_EB_H, spr_ebullet);
  for (const b of pbul) if (b.active) DS(b.x, b.y, SH_PB_W, SH_PB_H, spr_pbullet[b.kind]);
  if (player.alive && !(player.invuln > 0 && frame & 2)) {
    if (player.shield > 0)
      vga_frame(player.x-2+shx, player.y-2+shy, SH_SHIP_W+4, SH_SHIP_H+4,
                frame & 2 ? PAL_GLOW+14 : PAL_GLOW+8);
    DS(player.x, player.y, SH_SHIP_W, SH_SHIP_H, spr_ship[ship_bank]);
    const fc = frame & 2 ? PAL_FIRE+12 : PAL_FIRE+8;
    const fx = player.x + shx, fy = player.y + SH_SHIP_H + shy;
    vga_pixel(fx+3, fy, fc); vga_pixel(fx+12, fy, fc);
    vga_pixel(fx+3, fy+1, PAL_FIRE+5); vga_pixel(fx+12, fy+1, PAL_FIRE+5);
    if (frame & 1) { vga_pixel(fx+3, fy+2, PAL_FIRE+3); vga_pixel(fx+12, fy+2, PAL_FIRE+3); }
    if (player.boosting) {
      vga_hline(fx+1, fy+3, 5, PAL_FIRE+11); vga_hline(fx+10, fy+3, 5, PAL_FIRE+11);
      vga_pixel(fx+3, fy+4, PAL_FIRE+6); vga_pixel(fx+12, fy+4, PAL_FIRE+6);
    }
  }
  draw_hud();
  if (wave_banner > 0) {
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
  text_center(34, 'STELLAR ASSAULT', C_YELLOW);
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
  if (frame & 16) text_center(150, 'PRESS SPACE', C_LGRAY);
}
function help_row(y, pu, txt) {
  vga_sprite(52, y, SH_PU_W, SH_PU_H, spr_powerup[pu]);
  text_draw(72, y+2, txt, C_LGRAY);
}
function draw_help() {
  if (help_page === 0) {
    text_center(8, 'PICKUPS', C_YELLOW);
    help_row(24,  PU_GUN,     'GUN +1 LEVEL. -1 WHEN YOU DIE');
    help_row(40,  PU_RAPID,   'RAPID FIRE FOR A WHILE');
    help_row(56,  PU_SHIELD,  'SHIELD: ABSORBS ONE HIT');
    help_row(72,  PU_LIFE,    'EXTRA SHIP');
    help_row(88,  PU_MISSILE, '+4 MISSILES (MAX 30)');
    help_row(104, PU_LASER,   'LASER GUN: FAST, PIERCES');
    help_row(120, PU_WAVE,    'WAVE GUN: WIDE ARC');
    help_row(136, PU_BOMB,    '+1 SMART BOMB (MAX 10)');
    help_row(152, PU_SCORE,   'SCORE GEM: RISKY BONUS');
    text_center(176, 'SPACE NEXT PAGE   ESC BACK', C_LCYAN);
  } else {
    text_center(8, 'COMBAT MANUAL', C_YELLOW);
    text_draw(28, 26,  'SPACE  FIRE. GUN LEVELS 1-4', C_LGRAY);
    text_draw(28, 40,  '       WIDEN YOUR PATTERN', C_DGRAY);
    vga_sprite(32, 54, SH_MSL_W, SH_MSL_H, spr_missile);
    text_draw(44, 56,  'CTRL  HOMING MISSILE, BIG', C_LGRAY);
    text_draw(44, 68,  '      BLAST. BEST VS BOSSES', C_DGRAY);
    text_draw(28, 86,  'B     SMART BOMB: CLEARS ENEMY', C_LGRAY);
    text_draw(28, 98,  '      SHOTS, HURTS EVERYTHING', C_DGRAY);
    text_draw(28, 114, 'SHIFT BOOST: SHORT SPEED BURST', C_LGRAY);
    text_draw(28, 130, 'COMBO FAST KILLS MULTIPLY SCORE', C_LGRAY);
    text_draw(28, 142, '      UP TO X5. DYING RESETS IT', C_DGRAY);
    text_draw(28, 158, 'GRAZE NEAR-MISS SHOTS FOR BONUS', C_LGRAY);
    text_center(180, 'SPACE FIRST PAGE   ESC BACK', C_LCYAN);
  }
}
function draw_scores() {
  text_center(16, 'HIGH SCORES', C_YELLOW);
  for (let i = 0; i < HISCORE_N; i++) {
    const b = (i+1) + '. ' + g_hi[i].name.padEnd(8) + ' ' + pad6(g_hi[i].score);
    text_center(34 + i*12, b, i === 0 ? C_WHITE : C_LCYAN);
  }
  text_center(164, scoreStatus, scoresOnline ? C_LGREEN : C_YELLOW);
  if (frame & 16) text_center(180, 'SPACE TITLE   CTRL REPLAY', C_LGREEN);
}
function draw_entry() {
  text_center(60, 'NEW HIGH SCORE!', C_YELLOW);
  text_center(80, 'SCORE ' + pad6(score), C_WHITE);
  text_center(108, 'ENTER NAME:', C_LCYAN);
  text_center(124, entry_name + (frame & 8 ? '_' : ' '), C_WHITE);
  text_center(150, 'TYPE THEN PRESS ENTER', C_LGRAY);
  text_center(164, 'ESC SAVES + REPLAYS', C_DGRAY);
}

/* ================= state machine (game_run) ================= */
function begin_run() {
  reset_game(); start_wave();
  state = ST_PLAY; paused = false;
  snd_music_set(MUS_GAME);
}
function syncEntryInput(focus) {
  if (!nameInput) return;
  if (nameInput.value !== entry_name) nameInput.value = entry_name;
  if (focus) setTimeout(() => { nameInput.focus(); nameInput.select(); }, 0);
}
function syncHtmlUi(forceFocus) {
  const entering = state === ST_ENTRY;
  document.body.classList.toggle('entry-mode', entering);
  if (entering) syncEntryInput(forceFocus || uiState !== state);
  else if (nameInput) nameInput.blur();
  uiState = state;
}
function enterHighScoreEntry() {
  entry_name = '';
  typedQueue.length = 0;
  entrySubmitted = false;
  state = ST_ENTRY;
  syncHtmlUi(true);
}
function backspaceEntry() {
  if (state !== ST_ENTRY || entrySubmitted) return;
  entry_name = entry_name.slice(0, -1);
  syncEntryInput(false);
}
function submitEntry(replay) {
  if (state !== ST_ENTRY || entrySubmitted) return;
  entrySubmitted = true;
  const savedName = cleanName(entry_name) || 'PLAYER';
  entry_name = savedName;
  syncEntryInput(false);
  hi_insert(entry_rank, savedName, score);
  hi_save();
  submitScore(savedName, score);
  if (replay) begin_run();
  else state = ST_SCORES;
  syncHtmlUi(false);
}
function step() {
  if (key_hit('M')) snd_mute_toggle();
  if (!(state === ST_PLAY && paused)) frame++;
  if (state === ST_PLAY && key_hit('P')) { paused = !paused; snd_pause(paused); }
  if (state !== ST_PLAY || !paused) { update_stars(); update_dust(); }

  switch (state) {
  case ST_TITLE:
    if (key_hit('UP') && g_diff > 0) g_diff--;
    if (key_hit('DOWN') && g_diff < 2) g_diff++;
    if (key_hit('H')) { help_page = 0; state = ST_HELP; }
    if (key_hit('SPACE')) begin_run();
    break;
  case ST_HELP:
    if (key_hit('SPACE')) help_page ^= 1;
    if (key_hit('ESC') || key_hit('H')) state = ST_TITLE;
    break;
  case ST_PLAY:
    if (!paused) update_play();
    if (key_hit('ESC')) { state = ST_TITLE; paused = false; snd_music_set(MUS_TITLE); }
    if (!player.alive) {
      remember_run();
      snd_music_set(MUS_TITLE);
      over_timer = 160;
      state = ST_OVER;
    }
    break;
  case ST_OVER:
    if (over_timer > 0) over_timer--;
    if ((over_timer <= 130 && (key_hit('SPACE') || key_hit('ENTER'))) || over_timer === 0) {
      entry_rank = hi_qualifies(score);
      if (entry_rank >= 0) enterHighScoreEntry();
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
  case ST_SCORES: draw_scores(); break;
  case ST_ENTRY:  draw_entry(); break;
  }
  if (muted) text_draw(SCRW-40, 190, 'MUTE', C_LRED);
  if (!paused || state !== ST_PLAY) {
    if ((frame & 3) === 0) vga_cycle_palette();
  }
  syncHtmlUi(false);
}

/* ================= presentation ================= */
const wrapEl = document.getElementById('wrap');
const canvas = document.getElementById('screen');
const nameInput = document.getElementById('nameInput');
const ctx = canvas.getContext('2d');
const imageData = ctx.createImageData(SCRW, SCRH);

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
  document.querySelectorAll('[data-action="fullscreen"], #fsbtn, #bootFullBtn').forEach(btn => {
    btn.disabled = !supported;
    btn.title = supported ? 'Fullscreen' : 'Fullscreen unavailable';
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
window.addEventListener('resize', rescale);
document.addEventListener('fullscreenchange', () => { rescale(); updateFullscreenButtons(); });
document.addEventListener('webkitfullscreenchange', () => { rescale(); updateFullscreenButtons(); });
canvas.addEventListener('pointerdown', (e) => {
  bootAudio();
  if (state === ST_TITLE) {
    tapKey('SPACE');
    e.preventDefault();
    return;
  }
  pointerAim.active = true;
  pointerAim.id = e.pointerId;
  setPointerAim(e);
  canvas.setPointerCapture(e.pointerId);
  e.preventDefault();
});
canvas.addEventListener('pointermove', (e) => {
  if (pointerAim.active && (pointerAim.id === null || pointerAim.id === e.pointerId)) {
    setPointerAim(e);
    e.preventDefault();
  }
});
function endPointerAim(e) {
  if (pointerAim.id === null || pointerAim.id === e.pointerId) {
    pointerAim.active = false;
    pointerAim.id = null;
  }
}
canvas.addEventListener('pointerup', endPointerAim);
canvas.addEventListener('pointercancel', endPointerAim);

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
    if (nameInput.value !== entry_name) nameInput.value = entry_name;
  });
  nameInput.addEventListener('keydown', (e) => {
    if (e.code === 'Enter') { submitEntry(false); e.preventDefault(); }
    if (e.code === 'Escape') { submitEntry(true); e.preventDefault(); }
  });
}
document.getElementById('fsbtn').addEventListener('click', () => { bootAudio(); toggleFullscreen(); });
document.getElementById('startBtn').addEventListener('click', () => { bootAudio(); tapKey('SPACE'); });
document.getElementById('bootFullBtn').addEventListener('click', () => { bootAudio(); toggleFullscreen(); });
document.getElementById('bootMuteBtn').addEventListener('click', () => { bootAudio(); snd_mute_toggle(); });
document.getElementById('bootHelpBtn').addEventListener('click', () => { bootAudio(); tapKey('H'); });
document.getElementById('boot').addEventListener('pointerdown', (e) => {
  if (e.target.closest('button, a')) return;
  bootAudio();
  tapKey('SPACE');
  e.preventDefault();
});

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
updateFullscreenButtons();
syncHtmlUi(false);
rescale();
requestAnimationFrame(loop);
