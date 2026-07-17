/* sound.c - shared sequencer, PC speaker, OPL2 and MPU-401 backends */
#include <conio.h>
#include <dos.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sound.h"
#include "sound_priv.h"
#include "music_data.h"

#define BANK_TRACKS 18
#define BANK_SAMPLES 6

typedef struct {
    char magic[4]; u8 version, tracks, samples, hz;
    u32 track_dir, sample_dir, event_data, total_size, checksum, reserved;
} BankHeader;
typedef struct { u32 offset; u16 count, loop_ticks; u32 reserved; } BankTrack;
typedef struct { u32 offset; u16 length, loop_start, loop_end; u8 base_note, flags; u32 reserved; } BankSample;
typedef struct { u16 tick; u8 channel, command, note, instrument; } BankEvent;
typedef struct { char magic[4]; u8 version; SoundConfig cfg; u8 check; } ConfigFile;

static SoundConfig cfg = { SND_PCSPK, 0x388, 0x330, 0x220, 7, 1 };
static u8 active_device = SND_PCSPK;
static bool muted = FALSE, music_on = FALSE, paused = FALSE;
static bool bank_ok = FALSE;
static u8 __far *bank_data = NULL;
static u32 bank_size = 0;
static BankTrack bank_tracks[BANK_TRACKS];
static BankSample bank_samples[BANK_SAMPLES];

static u8 music_track = 0;
static u16 music_tick = 0, music_event = 0;
static u16 lead_index = 0, lead_left = 0, lead_gate = 0;
static u8 lead_note = 0;
static i16 last_freq = 0;
static i16 sfx_timer = 0, sfx_freq = 0, sfx_step = 0;
static u8 sfx_prio = 0, rich_sfx_note = 0;
static bool pc_resume = FALSE;

static const u8 sfx_priority[11] = { 1,5,3,2,4,3,3,3,2,4,2 };
static const u8 sfx_midi_note[11] = { 81,38,72,36,43,48,76,67,84,55,79 };

/* OPL operator offsets for channels 0..8. */
static const u8 opl_op[9] = { 0,1,2,8,9,10,16,17,18 };
/* mod 20,40,60,80,E0; car 20,40,60,80,E0; feedback */
static const u8 opl_patch[6][11] = {
    {0x21,0x22,0xF2,0x73,0x00, 0x01,0x08,0xF4,0x36,0x00,0x06}, /* lead */
    {0x01,0x18,0xA4,0x74,0x00, 0x01,0x10,0xA4,0x74,0x00,0x04}, /* pad  */
    {0x21,0x12,0xF3,0x45,0x00, 0x01,0x08,0xF2,0x35,0x00,0x02}, /* bass */
    {0x01,0x16,0xF8,0x08,0x00, 0x01,0x08,0xF8,0x08,0x00,0x0E}, /* kick */
    {0x01,0x10,0xF6,0x16,0x00, 0x01,0x06,0xF3,0x08,0x00,0x0F}, /* snare*/
    {0x01,0x18,0xF2,0x04,0x00, 0x01,0x0A,0xF1,0x03,0x00,0x0F}  /* hat  */
};
static const u16 opl_fnum[12] = {343,363,385,408,432,458,485,514,544,577,611,647};

static void pc_off(void) { outp(0x61, inp(0x61) & 0xFC); }
static void pc_tone(i16 freq)
{
    u16 div;
    last_freq = freq > 0 ? freq : 0;
    if (freq <= 0) { pc_off(); return; }
    div = (u16)(1193182L / freq);
    outp(0x43, 0xB6); outp(0x42, div & 0xFF); outp(0x42, div >> 8);
    outp(0x61, inp(0x61) | 3);
}

static i16 note_freq(u8 note)
{
    static const u16 c4[12] = {262,277,294,311,330,349,370,392,415,440,466,494};
    i16 octave = (i16)(note / 12) - 5;
    u32 f = c4[note % 12];
    while (octave > 0) { f <<= 1; octave--; }
    while (octave < 0) { f >>= 1; octave++; }
    return (i16)f;
}

static void opl_write(u8 reg, u8 value)
{
    int i;
    outp(cfg.adlib_base, reg); for (i = 0; i < 6; i++) inp(cfg.adlib_base);
    outp(cfg.adlib_base + 1, value); for (i = 0; i < 35; i++) inp(cfg.adlib_base);
}

static bool opl_detect(void)
{
    u8 a, b; int i;
    opl_write(4, 0x60); opl_write(4, 0x80); a = inp(cfg.adlib_base) & 0xE0;
    opl_write(2, 0xFF); opl_write(4, 0x21);
    for (i = 0; i < 100; i++) inp(cfg.adlib_base);
    b = inp(cfg.adlib_base) & 0xE0;
    opl_write(4, 0x60); opl_write(4, 0x80);
    return a == 0 && b == 0xC0;
}

static void opl_instrument(u8 channel, u8 instrument, u8 velocity)
{
    u8 m, c, carrier_level, attenuation; unsigned level; const u8 *p;
    if (channel > 8) return;
    instrument %= 6; p = opl_patch[instrument]; m = opl_op[channel]; c = m + 3;
    opl_write(0x20+m,p[0]); opl_write(0x40+m,p[1]); opl_write(0x60+m,p[2]); opl_write(0x80+m,p[3]); opl_write(0xE0+m,p[4]);
    attenuation=(u8)((127-velocity)>>2);level=(p[6]&0x3F)+attenuation;if(level>63)level=63;
    carrier_level=(u8)((p[6]&0xC0)|level);
    opl_write(0x20+c,p[5]); opl_write(0x40+c,carrier_level); opl_write(0x60+c,p[7]); opl_write(0x80+c,p[8]); opl_write(0xE0+c,p[9]);
    opl_write(0xC0+channel,p[10]);
}

static void opl_note_off(u8 channel) { if (channel < 9) opl_write(0xB0+channel, 0); }
static void opl_note_on(u8 channel, u8 note, u8 instrument, u8 velocity)
{
    u8 block; u16 fnum;
    if (channel > 8 || note < 12) return;
    block = (u8)(note / 12 - 1); if (block > 7) block = 7;
    fnum = opl_fnum[note % 12];
    opl_note_off(channel); opl_instrument(channel, instrument, velocity);
    opl_write(0xA0+channel, fnum & 0xFF);
    opl_write(0xB0+channel, 0x20 | (block << 2) | ((fnum >> 8) & 3));
}

static void opl_reset(void)
{
    int i; for (i = 0; i < 9; i++) opl_note_off((u8)i);
    opl_write(1, 0x20); opl_write(0xBD, 0);
}

static bool mpu_wait_write(void)
{
    u16 i; for (i = 0; i < 60000U; i++) if (!(inp(cfg.mpu_base + 1) & 0x40)) return TRUE;
    return FALSE;
}
static bool mpu_send(u8 value) { if (!mpu_wait_write()) return FALSE; outp(cfg.mpu_base, value); return TRUE; }
static bool mpu_command(u8 value) { if (!mpu_wait_write()) return FALSE; outp(cfg.mpu_base + 1, value); return TRUE; }
static bool mpu_init(void)
{
    int ch;
    if (!mpu_command(0x3F)) return FALSE;
    for (ch = 1; ch <= 8; ch++) { mpu_send(0xB0 | ch); mpu_send(123); mpu_send(0); }
    mpu_send(0xC1); mpu_send(81); mpu_send(0xC2); mpu_send(48); mpu_send(0xC3); mpu_send(38);
    return TRUE;
}
static void mpu_note_on(u8 lane, u8 note, u8 instrument, u8 velocity)
{
    static const u8 channels[4] = {1,2,3,9};
    u8 ch = channels[lane & 3]; (void)instrument;
    mpu_send(0x90 | ch); mpu_send(note); mpu_send(velocity);
}
static void mpu_note_off(u8 lane, u8 note)
{
    static const u8 channels[4] = {1,2,3,9}; u8 ch = channels[lane & 3];
    mpu_send(0x80 | ch); mpu_send(note); mpu_send(0);
}
static void mpu_all_off(void)
{
    int ch; for (ch = 1; ch <= 9; ch++) { mpu_send(0xB0 | ch); mpu_send(123); mpu_send(0); }
}

static void free_bank(void)
{
    if (bank_data) _ffree(bank_data);
    bank_data=NULL; bank_size=0; bank_ok=FALSE;
}

static bool load_bank(void)
{
    FILE *f; BankHeader h; u8 temp[512]; u32 pos = 0, sum = 0; size_t n, want; int i;
    f = fopen("AYRIEN.SND", "rb"); if (!f) f = fopen("dist\\AYRIEN.SND", "rb");
    if (!f) f = fopen("STELLAR.SND", "rb");
    if (!f) return FALSE;
    if (fread(&h, 1, sizeof(h), f) != sizeof(h) ||
        (memcmp(h.magic,"AYSN",4) && memcmp(h.magic,"STSN",4)) || h.version != 1 ||
        h.tracks != BANK_TRACKS || h.samples != BANK_SAMPLES || h.hz != 35 ||
        h.total_size < sizeof(h) || h.total_size > 524288UL ||
        h.track_dir > h.total_size || sizeof(bank_tracks) > h.total_size-h.track_dir ||
        h.sample_dir > h.total_size || sizeof(bank_samples) > h.total_size-h.sample_dir ||
        h.event_data > h.total_size) { fclose(f); return FALSE; }
    bank_data = (u8 __far *)_fmalloc(h.total_size); if (!bank_data) { fclose(f); return FALSE; }
    rewind(f);
    while (pos < h.total_size) {
        want=(size_t)((h.total_size-pos)>sizeof(temp)?sizeof(temp):(h.total_size-pos));
        n=fread(temp,1,want,f); if(!n)break; _fmemcpy(bank_data+pos,temp,n); pos+=n;
    }
    if(pos==h.total_size && fgetc(f)!=EOF)pos++;
    fclose(f); bank_size = pos;
    if (bank_size != h.total_size) { free_bank(); return FALSE; }
    for (pos = 0; pos < bank_size; pos++) if (pos < 24 || pos >= 28) sum += bank_data[pos];
    if (sum != h.checksum) { free_bank(); return FALSE; }
    _fmemcpy(bank_tracks, bank_data + h.track_dir, sizeof(bank_tracks));
    _fmemcpy(bank_samples, bank_data + h.sample_dir, sizeof(bank_samples));
    for (i=0;i<BANK_TRACKS;i++) {
        if (!bank_tracks[i].loop_ticks || bank_tracks[i].offset > bank_size ||
            (u32)bank_tracks[i].count*6UL > bank_size-bank_tracks[i].offset) {
            free_bank(); return FALSE;
        }
    }
    for(i=0;i<BANK_SAMPLES;i++) {
        if(bank_samples[i].offset>bank_size || bank_samples[i].length>bank_size-bank_samples[i].offset ||
           bank_samples[i].loop_start>bank_samples[i].loop_end ||
           bank_samples[i].loop_end>bank_samples[i].length) { free_bank(); return FALSE; }
    }
    return TRUE;
}

bool snd_sample_get(u8 id, SoundSample *out)
{
    BankSample *s; if (!bank_ok || id >= BANK_SAMPLES) return FALSE; s=&bank_samples[id];
    if (s->offset + s->length > bank_size) return FALSE;
    out->data=bank_data+s->offset; out->length=s->length; out->loop_start=s->loop_start;
    out->loop_end=s->loop_end; out->base_note=s->base_note; out->looped=(s->flags&1)!=0; return TRUE;
}

static u8 config_check(const ConfigFile *c)
{
    const u8 *p=(const u8 *)c; u8 sum=0; unsigned i;
    for(i=0;i<sizeof(*c)-1;i++) sum=(u8)(sum+p[i]); return sum;
}
static bool config_load(void)
{
    FILE *f=fopen("AYRIEN.CFG","rb"); ConfigFile c;
    if(!f)f=fopen("STELLAR.CFG","rb");
    if(!f) return FALSE; if(fread(&c,1,sizeof(c),f)!=sizeof(c)){fclose(f);return FALSE;} fclose(f);
    if((memcmp(c.magic,"AYCF",4)&&memcmp(c.magic,"STCF",4))||c.version!=1||c.check!=config_check(&c)) return FALSE; cfg=c.cfg; return TRUE;
}
static void config_save(void)
{
    FILE *f; ConfigFile c; memcpy(c.magic,"AYCF",4); c.version=1; c.cfg=cfg; c.check=config_check(&c);
    f=fopen("AYRIEN.CFG","wb"); if(f){fwrite(&c,1,sizeof(c),f);fclose(f);}
}
static void parse_blaster(void)
{
    char *env=getenv("BLASTER"), *p; if(!env) return; p=env;
    while(*p){ while(*p==' ')p++; if(*p=='A'||*p=='a')cfg.sb_base=(u16)strtoul(p+1,&p,16);
        else if(*p=='I'||*p=='i')cfg.sb_irq=(u8)strtoul(p+1,&p,10);
        else if(*p=='D'||*p=='d')cfg.sb_dma=(u8)strtoul(p+1,&p,10); else while(*p&&*p!=' ')p++; }
}
static bool starts(const char *s,const char *p){return strnicmp(s,p,strlen(p))==0;}

void snd_configure(int argc, char **argv, bool interactive)
{
    int i, choice=0; unsigned base,irq,dma; bool setup=FALSE, loaded=config_load(); char *v;
    parse_blaster();
    if(cfg.device>SND_SB)cfg.device=SND_PCSPK;
    if(cfg.sb_irq<2||cfg.sb_irq>7)cfg.sb_irq=7;if(cfg.sb_dma>3)cfg.sb_dma=1;
    for(i=1;i<argc;i++){
        if(!stricmp(argv[i],"/SETUP")||!stricmp(argv[i],"-SETUP"))setup=TRUE;
        else if(!stricmp(argv[i],"/PCSPK"))cfg.device=SND_PCSPK;
        else if(!stricmp(argv[i],"/ADLIB"))cfg.device=SND_ADLIB;
        else if(starts(argv[i],"/MT32")){cfg.device=SND_MT32;v=strchr(argv[i],':');if(v)cfg.mpu_base=(u16)strtoul(v+1,NULL,16);}
        else if(starts(argv[i],"/SB")){cfg.device=SND_SB;v=strchr(argv[i],':');if(v&&sscanf(v+1,"%x,%u,%u",&base,&irq,&dma)==3){cfg.sb_base=(u16)base;cfg.sb_irq=(u8)irq;cfg.sb_dma=(u8)dma;}}
        else if(!stricmp(argv[i],"/NOSOUND"))cfg.device=SND_NONE;
    }
    if(interactive && (setup||!loaded)){
        printf("AYRIEN ASSAULT SOUND SETUP\n\n1 PC SPEAKER\n2 ADLIB / OPL2\n3 ROLAND MT-32 (MPU-401)\n4 SOUND BLASTER 2.0+\n5 NO SOUND\n\nSELECT [1-5]: ");
        while(choice<'1'||choice>'5')choice=getch(); printf("%c\n",choice);
        cfg.device=choice=='5'?SND_NONE:(u8)(choice-'0'); config_save();
    }
}

u8 snd_device(void){return active_device;}
const char *snd_device_name(void)
{
    static const char *names[]={"NONE","PC SPEAKER","ADLIB OPL2","ROLAND MT-32","SOUND BLASTER"};
    return active_device<=SND_SB?names[active_device]:names[0];
}

static void backend_music_off(void)
{
    int i;
    if(active_device==SND_ADLIB)for(i=0;i<8;i++)opl_note_off((u8)i);
    else if(active_device==SND_MT32)mpu_all_off();
    else if(active_device==SND_SB)for(i=0;i<4;i++)sb_hw_note_off((u8)i,0);
    else if(active_device==SND_PCSPK)pc_off();
}

void snd_init(void)
{
    active_device=cfg.device; muted=FALSE; music_on=FALSE; paused=FALSE; sfx_timer=0; last_freq=0;
    if(active_device==SND_ADLIB){if(!opl_detect())active_device=SND_PCSPK;else opl_reset();}
    if(active_device==SND_MT32 && !mpu_init())active_device=SND_PCSPK;
    if(active_device==SND_ADLIB||active_device==SND_MT32||active_device==SND_SB)bank_ok=load_bank();
    if((active_device==SND_ADLIB||active_device==SND_MT32||active_device==SND_SB)&&!bank_ok)active_device=SND_PCSPK;
    if(active_device==SND_SB && !sb_hw_init(&cfg))active_device=SND_PCSPK;
}

void snd_shutdown(void)
{
    snd_silence(); if(active_device==SND_SB)sb_hw_shutdown(); if(active_device==SND_ADLIB)opl_reset(); if(active_device==SND_MT32)mpu_all_off();
    free_bank();
}
bool snd_muted(void){return muted;}
void snd_mute_toggle(void){muted=!muted;if(muted)snd_silence();else{music_tick=music_event=lead_index=lead_left=lead_gate=0;pc_resume=FALSE;}}
void snd_pause(bool value){paused=value;if(value)snd_silence();else{music_tick=music_event=lead_index=lead_left=lead_gate=0;pc_resume=FALSE;}}

void snd_silence(void)
{
    backend_music_off(); pc_off(); if(active_device==SND_SB)sb_hw_silence();
    if(active_device==SND_ADLIB)opl_note_off(8);
    if(active_device==SND_MT32){mpu_send(0xB8);mpu_send(123);mpu_send(0);}
    sfx_timer=0;sfx_prio=0;pc_resume=FALSE;
}

static void start_lead(void)
{
    const MusicLead *m=&music_leads[music_track]; const MusicNote __far *n;
    if(lead_index>=m->count)lead_index=0; n=&m->notes[lead_index++];
    lead_note=n->note; lead_left=n->duration; lead_gate=n->gate;
    if(lead_note)pc_tone(note_freq(lead_note));else pc_off();
}

static void backend_event(const BankEvent *e)
{
    u8 channel=e->channel;
    if(active_device==SND_ADLIB){channel=(channel==3)?7:channel;if(e->command)opl_note_on(channel,e->note,e->instrument,e->command);else opl_note_off(channel);}
    else if(active_device==SND_MT32){if(e->command)mpu_note_on(channel,e->note,e->instrument,e->command);else mpu_note_off(channel,e->note);}
    else if(active_device==SND_SB){if(e->command)sb_hw_note_on(channel,e->note,e->instrument,e->command);else sb_hw_note_off(channel,e->note);}
    if(channel==0&&e->command)last_freq=note_freq(e->note);
}

static void update_rich_music(void)
{
    BankTrack *t=&bank_tracks[music_track]; BankEvent e;
    while(music_event<t->count){_fmemcpy(&e,bank_data+t->offset+(u32)music_event*6UL,6);if(e.tick!=music_tick)break;backend_event(&e);music_event++;}
    music_tick++; if(music_tick>=t->loop_ticks){backend_music_off();music_tick=0;music_event=0;}
}

void snd_sfx(u8 id)
{
    u8 p; if(id>SFX_BOOST||active_device==SND_NONE)return;p=sfx_priority[id];if(sfx_timer>0&&p<sfx_prio)return;
    sfx_prio=p;sfx_timer=(id==SFX_EXPLODE||id==SFX_BOSS||id==SFX_PHASE)?12:6;rich_sfx_note=sfx_midi_note[id];
    if(active_device==SND_PCSPK){
        switch(id){case SFX_FIRE:sfx_freq=900;sfx_step=-120;sfx_timer=3;break;case SFX_EXPLODE:sfx_freq=260;sfx_step=-18;break;
        case SFX_POWER:sfx_freq=500;sfx_step=90;break;case SFX_HIT:sfx_freq=160;sfx_step=-12;break;case SFX_BOSS:sfx_freq=120;sfx_step=14;break;
        case SFX_MISSILE:sfx_freq=340;sfx_step=-28;break;case SFX_PICK1:sfx_freq=620;sfx_step=70;break;case SFX_PICK2:sfx_freq=420;sfx_step=55;break;
        case SFX_COMBO:sfx_freq=760;sfx_step=120;sfx_timer=4;break;case SFX_PHASE:sfx_freq=220;sfx_step=32;break;default:sfx_freq=700;sfx_step=80;sfx_timer=5;break;}
    }else if(active_device==SND_ADLIB)opl_note_on(8,rich_sfx_note,id==SFX_EXPLODE?4:id==SFX_MISSILE?5:0,112);
    else if(active_device==SND_MT32){mpu_send(0x98);mpu_send(rich_sfx_note);mpu_send(110);}
    else if(active_device==SND_SB)sb_hw_sfx(id);
}

void snd_music_set(u8 track)
{
    paused=FALSE;music_track=track==MUS_WIN?17:0;music_on=TRUE;music_tick=music_event=lead_index=lead_left=lead_gate=0;backend_music_off();
}
void snd_music_game(u8 chapter){paused=FALSE;if(chapter>15)chapter=15;music_track=(u8)(chapter+1);music_on=TRUE;music_tick=music_event=lead_index=lead_left=lead_gate=0;backend_music_off();}
void snd_music_start(void){snd_music_game(0);}
void snd_music_stop(void){music_on=FALSE;snd_silence();}

void snd_update(void)
{
    if(muted||paused||active_device==SND_NONE)return;
    if(sfx_timer>0){
        if(active_device==SND_PCSPK){pc_tone(sfx_freq);sfx_freq+=sfx_step;if(sfx_freq<40)sfx_freq=40;}
        sfx_timer--;if(sfx_timer==0){sfx_prio=0;if(active_device==SND_PCSPK){if(music_on)pc_resume=TRUE;else pc_off();}else if(active_device==SND_ADLIB)opl_note_off(8);
            else if(active_device==SND_MT32){mpu_send(0x88);mpu_send(rich_sfx_note);mpu_send(0);}}
        if(active_device==SND_PCSPK)return;
    }
    if(!music_on)return;
    if(active_device==SND_PCSPK){
        if(pc_resume){if(lead_note&&lead_gate)pc_tone(note_freq(lead_note));else pc_off();pc_resume=FALSE;}
        if(!lead_left)start_lead();
        if(lead_gate&&--lead_gate==0)pc_off();
        if(lead_left)--lead_left;
    }else update_rich_music();
}
i16 snd_last_freq(void){return last_freq;}
