/* game.h - top-level game entry points */
#ifndef GAME_H
#define GAME_H

void game_run(void);                 /* interactive game loop         */
void game_selftest(const char *bmp); /* render one frame, dump to BMP */
void game_selftest_title(const char *bmp);
void game_selftest_help(const char *bmp, int page);
void game_bench(void);         /* measure achieved FPS -> BENCH.TXT */

#endif
