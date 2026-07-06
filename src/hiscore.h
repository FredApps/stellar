/* hiscore.h - top-N high score table persisted to HISCORE.DAT */
#ifndef HISCORE_H
#define HISCORE_H
#include "defs.h"

extern HiEntry g_hi[HISCORE_N];

void hi_load(void);
void hi_save(void);
int  hi_qualifies(u32 score);      /* returns rank index 0..N-1, or -1 */
void hi_insert(int rank, const char *name, u32 score);

#endif
