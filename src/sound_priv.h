#ifndef SOUND_PRIV_H
#define SOUND_PRIV_H
#include "sound.h"

typedef struct {
    const u8 __far *data;
    u16 length, loop_start, loop_end;
    u8 base_note, looped;
} SoundSample;

bool snd_sample_get(u8 id, SoundSample *out);
bool sb_hw_init(const SoundConfig *cfg);
void sb_hw_shutdown(void);
void sb_hw_silence(void);
void sb_hw_note_on(u8 channel, u8 note, u8 instrument, u8 velocity);
void sb_hw_note_off(u8 channel, u8 note);
void sb_hw_sfx(u8 id);

#endif
