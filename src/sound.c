/* sound.c - PC speaker driver, frame-paced */
#include <conio.h>
#include "sound.h"

static bool muted = FALSE;
static bool music_on = FALSE;

/* current speaker request */
static i16  sfx_timer = 0;      /* frames left of active sfx */
static i16  sfx_freq  = 0;
static i16  sfx_step  = 0;      /* freq delta per frame (sweeps) */
static u8   sfx_prio  = 0;      /* priority of the active sfx    */

static int  mus_idx = 0;
static i16  mus_timer = 0;
static i16  last_freq = 0;      /* frequency currently on the speaker */

static void tone(i16 freq)
{
    u16 div;
    last_freq = (freq > 0) ? freq : 0;
    if (freq <= 0) { outp(0x61, inp(0x61) & 0xFC); return; }
    div = (u16)(1193182L / freq);
    outp(0x43, 0xB6);
    outp(0x42, (u8)(div & 0xFF));
    outp(0x42, (u8)(div >> 8));
    outp(0x61, inp(0x61) | 0x03);
}

i16 snd_last_freq(void) { return last_freq; }

void snd_silence(void) { last_freq = 0; outp(0x61, inp(0x61) & 0xFC); }

void snd_init(void) { muted = FALSE; music_on = FALSE; sfx_timer = 0; snd_silence(); }

bool snd_muted(void) { return muted; }

void snd_mute_toggle(void)
{
    muted = !muted;
    if (muted) snd_silence();
}

/* higher value = more important; an active sfx is only replaced by an
   equal-or-higher priority one, so a stream of fire pips can't swallow
   an explosion. */
static const u8 sfx_priority[11] = {
    /* FIRE */ 1, /* EXPLODE */ 5, /* POWER */ 3,
    /* HIT  */ 2, /* BOSS    */ 4, /* MISSILE */ 3,
    /* PICK1 */ 3, /* PICK2 */ 3, /* COMBO */ 2, /* PHASE */ 4,
    /* BOOST */ 2
};

/* note table: {startfreq, frames, freqstep} */
void snd_sfx(u8 id)
{
    u8 p;
    if (id > SFX_BOOST) return;
    p = sfx_priority[id];
    if (sfx_timer > 0 && p < sfx_prio) return;   /* don't stomp a bigger sound */
    switch (id) {
        case SFX_FIRE:    sfx_freq = 900;  sfx_timer = 3;  sfx_step = -120; break;
        case SFX_EXPLODE: sfx_freq = 260;  sfx_timer = 10; sfx_step = -18;  break;
        case SFX_POWER:   sfx_freq = 500;  sfx_timer = 8;  sfx_step =  90;  break;
        case SFX_HIT:     sfx_freq = 160;  sfx_timer = 6;  sfx_step = -12;  break;
        case SFX_BOSS:    sfx_freq = 120;  sfx_timer = 16; sfx_step =  14;  break;
        case SFX_MISSILE: sfx_freq = 340;  sfx_timer = 9;  sfx_step = -28;  break;
        case SFX_PICK1:   sfx_freq = 620;  sfx_timer = 7;  sfx_step =  70;  break;
        case SFX_PICK2:   sfx_freq = 420;  sfx_timer = 9;  sfx_step =  55;  break;
        case SFX_COMBO:   sfx_freq = 760;  sfx_timer = 4;  sfx_step = 120;  break;
        case SFX_PHASE:   sfx_freq = 220;  sfx_timer = 14; sfx_step =  32;  break;
        case SFX_BOOST:   sfx_freq = 700;  sfx_timer = 5;  sfx_step =  80;  break;
    }
    sfx_prio = p;
}

/* Sixteen short chapter themes. The single PC-speaker voice needs air around
   the notes, so each phrase uses deliberate rests and leaves room for SFX. */
static const i16 game_mel[16][16][2] = {
  {{110,7},{131,7},{147,8},{165,12},{0,5},{147,6},{131,6},{110,10},{0,8},{98,6},{110,6},{123,8},{131,10},{110,8},{0,8},{110,14}},
  {{123,6},{147,6},{185,8},{165,10},{0,4},{147,6},{123,8},{98,10},{0,7},{123,6},{131,6},{147,8},{185,8},{165,10},{0,6},{123,14}},
  {{131,6},{165,6},{196,8},{262,10},{0,5},{220,6},{196,6},{165,10},{0,7},{147,6},{165,6},{196,8},{175,8},{147,10},{0,7},{131,14}},
  {{147,5},{185,5},{220,7},{294,10},{0,4},{262,6},{220,7},{185,9},{0,6},{147,5},{175,5},{220,7},{196,7},{175,9},{0,6},{147,13}},
  {{98,7},{123,7},{147,7},{196,11},{0,4},{185,6},{147,7},{123,9},{0,8},{110,6},{147,6},{165,8},{147,8},{123,10},{0,6},{98,14}},
  {{165,5},{196,5},{247,7},{330,10},{0,4},{294,6},{247,6},{196,9},{0,6},{165,5},{185,5},{220,7},{247,7},{220,9},{0,7},{165,13}},
  {{175,6},{220,6},{262,8},{349,10},{0,5},{330,5},{262,7},{220,9},{0,6},{196,6},{220,6},{277,8},{262,8},{220,10},{0,6},{175,14}},
  {{110,5},{165,5},{220,7},{330,9},{0,4},{294,5},{220,6},{165,9},{0,6},{123,5},{185,5},{247,7},{220,7},{165,9},{0,7},{110,13}},
  {{196,5},{247,5},{294,7},{392,10},{0,4},{349,5},{294,6},{247,9},{0,6},{220,5},{262,5},{330,7},{294,8},{247,9},{0,7},{196,13}},
  {{123,5},{185,5},{247,7},{370,9},{0,4},{330,5},{247,6},{185,9},{0,6},{147,5},{220,5},{294,7},{247,8},{185,9},{0,7},{123,13}},
  {{220,5},{262,5},{330,7},{440,10},{0,4},{392,5},{330,6},{262,9},{0,6},{247,5},{294,5},{349,7},{330,8},{262,9},{0,7},{220,13}},
  {{147,5},{220,5},{294,7},{440,9},{0,4},{392,5},{294,6},{220,9},{0,6},{175,5},{262,5},{349,7},{294,8},{220,9},{0,7},{147,13}},
  {{247,5},{294,5},{370,7},{494,10},{0,4},{440,5},{370,6},{294,9},{0,6},{262,5},{330,5},{392,7},{370,8},{294,9},{0,7},{247,13}},
  {{165,4},{247,4},{330,6},{494,9},{0,4},{440,5},{330,6},{247,8},{0,5},{196,4},{294,4},{392,6},{330,7},{247,8},{0,6},{165,12}},
  {{262,4},{330,4},{392,6},{523,9},{0,4},{494,5},{392,5},{330,8},{0,5},{294,4},{349,4},{440,6},{392,7},{330,8},{0,6},{262,12}},
  {{294,4},{370,4},{440,6},{587,9},{0,3},{523,5},{440,5},{370,8},{0,5},{330,4},{392,4},{494,6},{440,7},{370,8},{0,6},{294,12}}
};

/* heroic title theme - a rising fanfare that loops */
static const i16 title_mel[][2] = {
    {392,10},{523,10},{659,10},{784,16},{0,3},{659,8},{784,18},{0,5},
    {440,10},{587,10},{698,10},{880,16},{0,3},{784,8},{659,18},{0,6},
    {523,8},{523,8},{494,8},{440,8},{392,14},{0,3},
    {440,8},{494,10},{523,20},{0,10}
};

/* victory theme: bright ascending loop for the campaign clear screen */
static const i16 win_mel[][2] = {
    {523,8},{659,8},{784,8},{1047,16},{0,4},
    {988,8},{880,8},{784,12},{659,8},{784,18},{0,6},
    {698,8},{880,8},{1047,8},{1175,18},{0,6},
    {1047,10},{784,10},{880,10},{1047,24},{0,14}
};

static const i16 (*cur_mel)[2] = title_mel;
static int cur_n = (int)(sizeof(title_mel) / sizeof(title_mel[0]));

void snd_music_set(u8 track)
{
    if (track == MUS_TITLE) { cur_mel = title_mel; cur_n = (int)(sizeof(title_mel)/sizeof(title_mel[0])); }
    else if (track == MUS_WIN) { cur_mel = win_mel; cur_n = (int)(sizeof(win_mel)/sizeof(win_mel[0])); }
    else                    { cur_mel = game_mel[0]; cur_n = 16; }
    music_on = TRUE; mus_idx = 0; mus_timer = 0;
}

void snd_music_game(u8 chapter)
{
    if (chapter > 15) chapter = 15;
    cur_mel = game_mel[chapter]; cur_n = 16;
    music_on = TRUE; mus_idx = 0; mus_timer = 0;
}

void snd_music_start(void) { snd_music_game(0); }
void snd_music_stop(void)  { music_on = FALSE; }

void snd_update(void)
{
    if (muted) return;

    /* sfx takes priority over music */
    if (sfx_timer > 0) {
        tone(sfx_freq);
        sfx_freq += sfx_step;
        if (sfx_freq < 40) sfx_freq = 40;
        sfx_timer--;
        if (sfx_timer == 0) { sfx_prio = 0; if (!music_on) snd_silence(); }
        return;
    }

    if (!music_on) { snd_silence(); return; }

    if (mus_timer <= 0) {
        i16 f = cur_mel[mus_idx][0];
        mus_timer = cur_mel[mus_idx][1];
        mus_idx++;
        if (mus_idx >= cur_n) mus_idx = 0;
        if (f == 0) snd_silence(); else tone(f);
    }
    mus_timer--;
}
