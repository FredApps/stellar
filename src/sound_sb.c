/* sound_sb.c - Sound Blaster 2.0+ 8-bit mono DMA tracker backend */
#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <malloc.h>
#include "sound_priv.h"

#define SB_RATE 11025U
#define DMA_BYTES 1024
#define DMA_HALF 512

typedef struct {
    const u8 __far *data;
    u32 phase, step;
    u16 length, loop_start, loop_end;
    u8 note, active, looped, volume, priority;
} MixVoice;

static MixVoice voices[4];
static u8 __far *dma_raw = NULL, __far *dma_buf = NULL;
static void (__interrupt __far *old_irq)(void);
static u16 sb_base;
static u8 sb_irq, sb_dma, old_pic_mask, fill_half;
static bool running = FALSE;
static const u32 ratios[12] = {65536UL,69433UL,73561UL,77936UL,82570UL,87480UL,
                               92682UL,98193UL,104032UL,110218UL,116772UL,123715UL};

static bool dsp_write(u8 value)
{
    u16 n; for(n=0;n<65000U;n++)if(!(inp(sb_base+0xC)&0x80)){outp(sb_base+0xC,value);return TRUE;} return FALSE;
}
static bool dsp_read(u8 *value)
{
    u16 n;for(n=0;n<65000U;n++)if(inp(sb_base+0xE)&0x80){*value=inp(sb_base+0xA);return TRUE;}return FALSE;
}
static bool dsp_reset(void)
{
    u16 n; outp(sb_base+6,1); for(n=0;n<1000;n++)inp(sb_base+6); outp(sb_base+6,0);
    for(n=0;n<65000U;n++)if((inp(sb_base+0xE)&0x80)&&inp(sb_base+0xA)==0xAA)return TRUE; return FALSE;
}

static u32 note_step(u8 note,u8 base)
{
    int delta=(int)note-(int)base,oct=delta/12,rem=delta%12;u32 step;
    if(rem<0){rem+=12;oct--;}step=ratios[rem];while(oct>0){step<<=1;oct--;}while(oct<0){step>>=1;oct++;}return step;
}

static void mix_half(u8 half)
{
    u16 i; u8 __far *out=dma_buf+(u16)half*DMA_HALF;
    for(i=0;i<DMA_HALF;i++){
        long mix=0;int v;
        for(v=0;v<4;v++)if(voices[v].active){
            u16 pos=(u16)(voices[v].phase>>16);
            if(pos>=voices[v].length){
                if(voices[v].looped&&voices[v].loop_end>voices[v].loop_start){
                    pos=(u16)(voices[v].loop_start+(pos-voices[v].loop_start)%(voices[v].loop_end-voices[v].loop_start));
                    voices[v].phase=(u32)pos<<16;
                }else{voices[v].active=0;continue;}
            }
            mix+=((int)voices[v].data[pos]-128)*(int)voices[v].volume/64;
            voices[v].phase+=voices[v].step;
        }
        mix+=128;if(mix<0)mix=0;if(mix>255)mix=255;out[i]=(u8)mix;
    }
}

static void __interrupt __far sb_interrupt(void)
{
    inp(sb_base+0xE);mix_half(fill_half);fill_half^=1;outp(0x20,0x20);
}

static void dma_program(u32 linear)
{
    static const u8 addr_port[4]={0,2,4,6},count_port[4]={1,3,5,7},page_port[4]={0x87,0x83,0x81,0x82};
    u16 addr=(u16)linear,count=DMA_BYTES-1;
    outp(0x0A,4|sb_dma);outp(0x0C,0);outp(0x0B,0x58|sb_dma);
    outp(addr_port[sb_dma],addr&0xFF);outp(addr_port[sb_dma],addr>>8);
    outp(page_port[sb_dma],(u8)(linear>>16));
    outp(count_port[sb_dma],count&0xFF);outp(count_port[sb_dma],count>>8);outp(0x0A,sb_dma);
}

bool sb_hw_init(const SoundConfig *cfg)
{
    u32 raw_linear,linear,adjust;u16 seg,off;u8 major,minor;int i;
    sb_base=cfg->sb_base;sb_irq=cfg->sb_irq;sb_dma=cfg->sb_dma;
    if(sb_irq<2||sb_irq>7||sb_dma>3||!dsp_reset()||!dsp_write(0xE1)||
       !dsp_read(&major)||!dsp_read(&minor)||major<2)return FALSE;
    dma_raw=(u8 __far *)_fmalloc(DMA_BYTES+1024);if(!dma_raw)return FALSE;
    raw_linear=((u32)FP_SEG(dma_raw)<<4)+FP_OFF(dma_raw);adjust=0;
    if((raw_linear&0xFFFFUL)>0xFC00UL)adjust=0x10000UL-(raw_linear&0xFFFFUL);
    linear=raw_linear+adjust;seg=(u16)(linear>>4);off=(u16)(linear&15);dma_buf=(u8 __far *)MK_FP(seg,off);
    for(i=0;i<4;i++)voices[i].active=0;for(i=0;i<DMA_BYTES;i++)dma_buf[i]=128;
    _disable();old_irq=_dos_getvect(8+sb_irq);old_pic_mask=inp(0x21);_dos_setvect(8+sb_irq,sb_interrupt);outp(0x21,old_pic_mask&~(1<<sb_irq));_enable();
    dma_program(linear);fill_half=0;
    if(!dsp_write(0x40)||!dsp_write((u8)(256-1000000UL/SB_RATE))||!dsp_write(0x48)||
       !dsp_write((DMA_HALF-1)&0xFF)||!dsp_write((DMA_HALF-1)>>8)||!dsp_write(0x1C)){sb_hw_shutdown();return FALSE;}
    running=TRUE;return TRUE;
}

void sb_hw_shutdown(void)
{
    if(running)dsp_write(0xDA);if(old_irq){_disable();outp(0x21,old_pic_mask);_dos_setvect(8+sb_irq,old_irq);old_irq=NULL;_enable();}
    if(dma_raw){_ffree(dma_raw);dma_raw=NULL;dma_buf=NULL;}running=FALSE;
}
void sb_hw_silence(void){int i;_disable();for(i=0;i<4;i++)voices[i].active=0;_enable();}

void sb_hw_note_on(u8 channel,u8 note,u8 instrument,u8 velocity)
{
    SoundSample s;MixVoice *v;if(channel>3||!snd_sample_get(instrument,&s))return;v=&voices[channel];
    _disable();v->data=s.data;v->length=s.length;v->loop_start=s.loop_start;v->loop_end=s.loop_end;
    v->looped=s.looped;v->phase=0;v->step=note_step(note,s.base_note);v->note=note;v->priority=velocity;
    v->volume=(u8)(((u16)velocity*56U)/127U);if(!v->volume)v->volume=1;v->active=1;_enable();
}
void sb_hw_note_off(u8 channel,u8 note){if(channel>3)return;_disable();if(!note||voices[channel].note==note)voices[channel].active=0;_enable();}
void sb_hw_sfx(u8 id)
{
    static const u8 sample[11]={0,4,0,4,3,5,0,1,0,3,5};
    static const u8 note[11]={82,38,76,45,36,42,84,72,88,43,79};
    sb_hw_note_on(3,note[id],sample[id],120);
}
