/* sound.h - PC speaker SFX + music sequencer (frame-driven) */
#ifndef SOUND_H
#define SOUND_H
#include "defs.h"

#define SFX_FIRE    0
#define SFX_EXPLODE 1
#define SFX_POWER   2
#define SFX_HIT     3
#define SFX_BOSS    4
#define SFX_MISSILE 5
#define SFX_PICK1   6
#define SFX_PICK2   7
#define SFX_COMBO   8
#define SFX_PHASE   9
#define SFX_BOOST  10

#define MUS_TITLE 0
#define MUS_GAME  1

void snd_init(void);
void snd_mute_toggle(void);
bool snd_muted(void);
void snd_sfx(u8 id);
void snd_music_set(u8 track);   /* choose + (re)start a track */
void snd_music_start(void);     /* start in-game track        */
void snd_music_stop(void);
void snd_update(void);      /* call once per frame */
void snd_silence(void);     /* speaker off (on exit) */
i16  snd_last_freq(void);   /* freq currently driven to the speaker, 0=off */

#endif
