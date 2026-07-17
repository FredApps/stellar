/* main.c - entry point, init/shutdown, /shot self-test */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "defs.h"
#include "vga.h"
#include "input.h"
#include "sound.h"
#include "sprites.h"
#include "game.h"

/* Diagnostic: run the music sequencer + sfx mixer headless and log the
   emitted speaker frequency each frame to AUDIO.TXT.  Sections:
     A: title track alone
     B00-B15: all game chapter tracks
     C: game track with sfx injected (preemption + resume check)
     D: fire sfx spam followed by one explosion (priority check)
     E: new pickup/combo/phase effects
     F: win track alone                                                */
static int audiodump(void)
{
    FILE *f = fopen("AUDIO.TXT", "w");
    int i, chapter;
    if (!f) return 1;
    snd_init();
    fprintf(f, "A\n");
    snd_music_set(MUS_TITLE);
    for (i = 0; i < 120; i++) { snd_update(); fprintf(f, "%d\n", snd_last_freq()); }
    for (chapter = 0; chapter < 16; chapter++) {
        fprintf(f, "B%02d\n", chapter);
        snd_music_game((u8)chapter);
        for (i = 0; i < 64; i++) { snd_update(); fprintf(f, "%d\n", snd_last_freq()); }
    }
    fprintf(f, "C\n");
    snd_music_game(7);
    for (i = 0; i < 90; i++) {
        if (i == 30) snd_sfx(SFX_EXPLODE);
        snd_update();
        fprintf(f, "%d\n", snd_last_freq());
    }
    fprintf(f, "D\n");
    for (i = 0; i < 40; i++) {
        if (i % 6 == 0) snd_sfx(SFX_FIRE);      /* constant fire spam */
        if (i == 12) snd_sfx(SFX_EXPLODE);      /* must not be stomped */
        snd_update();
        fprintf(f, "%d\n", snd_last_freq());
    }
    fprintf(f, "E\n");
    for (i = 0; i < 70; i++) {
        if (i == 0)  snd_sfx(SFX_PICK1);
        if (i == 16) snd_sfx(SFX_PICK2);
        if (i == 32) snd_sfx(SFX_COMBO);
        if (i == 46) snd_sfx(SFX_PHASE);
        if (i == 58) snd_sfx(SFX_BOOST);
        snd_update();
        fprintf(f, "%d\n", snd_last_freq());
    }
    fprintf(f, "F\n");
    snd_music_set(MUS_WIN);
    for (i = 0; i < 180; i++) { snd_update(); fprintf(f, "%d\n", snd_last_freq()); }
    snd_silence();
    fclose(f);
    return 0;
}

static bool kbd_on = FALSE;

static void cleanup(void)
{
    snd_silence();
    if (kbd_on) { kbd_remove(); kbd_on = FALSE; }
    vga_shutdown();
}

static void on_break(int sig) { (void)sig; exit(1); }

int main(int argc, char **argv)
{
    bool shot = FALSE, bench = FALSE; int bscene = 0;
    int i;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "/shot") || !strcmp(argv[i], "-shot")) shot = TRUE;
        if (!strcmp(argv[i], "/bench")) bench = TRUE;
        if (!strcmp(argv[i], "/benchhelp")) { bench = TRUE; bscene = 1; }
        if (!strcmp(argv[i], "/audiodump")) return audiodump();  /* no video */
    }

    if (vga_init() != 0) return 1;
    atexit(cleanup);
    signal(SIGINT,  on_break);
    signal(SIGABRT, on_break);

    sprites_init();
    snd_init();

    if (bench) { game_bench(bscene); return 0; }

    if (shot) {
        game_selftest_title("TITLE.BMP");
        game_selftest("FRAME.BMP");
        game_selftest_help("HELP1.BMP", 0);
        game_selftest_help("HELP2.BMP", 1);
        game_selftest_stages("STAGES.BMP");
        game_selftest_bosses("BOSSES1.BMP", 0);
        game_selftest_bosses("BOSSES2.BMP", 1);
        game_selftest_win("WIN.BMP");
        game_selftest_logic();    /* separation + boss envelopes -> SELFTEST.TXT */
        return 0;                 /* cleanup() restores text mode */
    }

    kbd_install(); kbd_on = TRUE;
    game_run();
    return 0;
}
