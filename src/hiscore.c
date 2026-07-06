/* hiscore.c - load/save high scores */
#include <stdio.h>
#include <string.h>
#include "hiscore.h"

HiEntry g_hi[HISCORE_N];

static const char *DEF_NAMES[HISCORE_N] =
    { "ACE", "NOVA", "COMET", "ORION", "VEGA", "PULSAR", "ROOKIE", "CADET" };

static void defaults(void)
{
    int i;
    for (i = 0; i < HISCORE_N; i++) {
        strcpy(g_hi[i].name, DEF_NAMES[i]);
        g_hi[i].score = (u32)(HISCORE_N - i) * 1000UL;
    }
}

static int valid_name_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ';
}

static void sanitize(void)
{
    int i, j;
    for (i = 0; i < HISCORE_N; i++) {
        for (j = 0; j < NAME_LEN; j++) {
            char c = g_hi[i].name[j];
            if (c == 0) break;
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            if (!valid_name_char(c)) c = ' ';
            g_hi[i].name[j] = c;
        }
        g_hi[i].name[NAME_LEN] = 0;
        if (g_hi[i].name[0] == 0) strcpy(g_hi[i].name, "PLAYER");
    }
}

void hi_load(void)
{
    FILE *f = fopen("HISCORE.DAT", "rb");
    if (!f) { defaults(); return; }
    if (fread(g_hi, sizeof(HiEntry), HISCORE_N, f) != HISCORE_N) defaults();
    else sanitize();
    fclose(f);
}

void hi_save(void)
{
    FILE *f = fopen("HISCORE.DAT", "wb");
    if (!f) return;
    fwrite(g_hi, sizeof(HiEntry), HISCORE_N, f);
    fclose(f);
}

int hi_qualifies(u32 score)
{
    int i;
    for (i = 0; i < HISCORE_N; i++)
        if (score > g_hi[i].score) return i;
    return -1;
}

void hi_insert(int rank, const char *name, u32 score)
{
    int i;
    for (i = HISCORE_N - 1; i > rank; i--) g_hi[i] = g_hi[i - 1];
    strncpy(g_hi[rank].name, name, NAME_LEN);
    g_hi[rank].name[NAME_LEN] = 0;
    g_hi[rank].score = score;
}
