/* game.h - top-level game entry points */
#ifndef GAME_H
#define GAME_H

void game_run(void);                 /* interactive game loop         */
void game_selftest(const char *bmp); /* render one frame, dump to BMP */
void game_selftest_title(const char *bmp);
void game_selftest_help(const char *bmp, int page);
void game_selftest_stages(const char *bmp);
void game_selftest_bosses(const char *bmp, int page); /* 0: W04-W32, 1: W36-W60 */
void game_selftest_logic(void);      /* separation + boss envelopes -> SELFTEST.TXT */
void game_bench(int scene);    /* 0=boss 1=help; measure FPS -> BENCH.TXT */

#endif
