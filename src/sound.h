/* sound.h - multi-device music and sound effects */
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
#define MUS_WIN   2

#define SND_NONE   0
#define SND_PCSPK  1
#define SND_ADLIB  2
#define SND_MT32   3
#define SND_SB     4

typedef struct {
    u8  device;
    u16 adlib_base;
    u16 mpu_base;
    u16 sb_base;
    u8  sb_irq;
    u8  sb_dma;
} SoundConfig;

/* Parse config/command-line options before VGA mode.  interactive controls
   whether a missing config may show the first-run text setup. */
void snd_configure(int argc, char **argv, bool interactive);
void snd_init(void);
void snd_shutdown(void);
void snd_mute_toggle(void);
bool snd_muted(void);
u8   snd_device(void);
const char *snd_device_name(void);
void snd_sfx(u8 id);
void snd_music_set(u8 track);
void snd_music_game(u8 chapter);
void snd_music_start(void);
void snd_music_stop(void);
void snd_pause(bool paused);
void snd_update(void);
void snd_silence(void);
i16 snd_last_freq(void);

#endif
