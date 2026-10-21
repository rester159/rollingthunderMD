#include "rt_game_music.h"
#include "rt_z80_bus_policy.h"
#include "src/rt_music_z80.h"

/* Level music runs entirely on the Z80 (src/rt_music_z80.s80): the native
 * source sequencer plus the YM2151 -> YM2612/PSG translation, clocked by
 * YM2612 Timer A. The 68000 only loads the driver and posts commands. */

/* CPU1 $A866: music selector per story*5+area. */
static const u8 areaTrack[10]={6,6,10,10,6,6,6,10,10,6};
extern const u8 rtMusicZ80Blob[],rtVoiceZ80Blob[];

#define MB_TRACK  0x1F00
#define MB_SEQ    0x1F01
#define MB_PSG    0x1F02
#define MB_BANKLO 0x1F04
#define MB_BANKHI 0x1F05
#define MB_BANKOK 0x1F06
#define MB_STATUS 0x1F08
#define MB_VBANKLO 0x1F0C
#define MB_VBANKHI 0x1F0D
#define MB_SFXRING 0x1F10
#define MB_SFXWR  0x1F20

static u32 generation;
static u8 loaded,active,psgMuted,sequence,sfxWrite;
volatile u8 rtGameMusicTrack;

static vu8 *mailbox(u16 address){return (vu8*)(Z80_RAM+address);}

static bool owns(void)
{
    return loaded && Z80_getLoadedDriver()==Z80_DRIVER_CUSTOM && RT_z80DriverGeneration()==generation;
}

static void ensureDriver(void)
{
    if(owns())return;
    Z80_loadCustomDriver(rt_music_z80,sizeof(rt_music_z80));
    generation=RT_z80DriverGeneration();
    /* This driver reads music data from ROM: DMA must halt it. */
    RT_setZ80RomDriver(TRUE);
    u32 bank=(u32)rtMusicZ80Blob>>15,voices=(u32)rtVoiceZ80Blob>>15;
    SYS_disableInts();
    Z80_requestBus(TRUE);
    *mailbox(MB_PSG)=psgMuted;
    *mailbox(MB_VBANKLO)=voices;*mailbox(MB_VBANKHI)=voices>>8;
    *mailbox(MB_BANKLO)=bank;*mailbox(MB_BANKHI)=bank>>8;
    *mailbox(MB_BANKOK)=0x5A;
    Z80_releaseBus();
    SYS_enableInts();
    sequence=sfxWrite=0;loaded=1;
}

static void post(u8 track)
{
    SYS_disableInts();
    bool taken=Z80_getAndRequestBus(TRUE);
    *mailbox(MB_TRACK)=track;*mailbox(MB_PSG)=psgMuted;
    *mailbox(MB_SEQ)=++sequence;
    if(!taken)Z80_releaseBus();
    SYS_enableInts();
}

u8 RT_gameMusicTrackFor(u8 story,u8 area)
{
    return story<2 && area<5?areaTrack[story*5+area]:6;
}

void RT_gameMusicPrepare(void){ensureDriver();}

void RT_gameMusicStart(u8 track)
{
    ensureDriver();
    post(track);
    rtGameMusicTrack=track;active=1;
}

void RT_gameMusicStop(void)
{
    if(!active)return;
    active=0;
    if(owns())post(0);
}

u8 RT_gameMusicActive(void){return active && owns();}

u8 RT_gameMusicPlaying(void)
{
    if(!RT_gameMusicActive())return 0;
    SYS_disableInts();
    bool taken=Z80_getAndRequestBus(TRUE);
    u8 status=*mailbox(MB_STATUS);
    if(!taken)Z80_releaseBus();
    SYS_enableInts();
    return status;
}

void RT_gameMusicSetPsgMuted(u8 muted)
{
    psgMuted=muted;
    if(owns()){
        SYS_disableInts();
        bool taken=Z80_getAndRequestBus(TRUE);
        *mailbox(MB_PSG)=muted;
        if(!taken)Z80_releaseBus();
        SYS_enableInts();
    }
}
u8 RT_gameMusicPsgMuted(void){return psgMuted;}

void RT_gameMusicPoll(void){}

void RT_gameSoundPost(const u8 *bytes,u8 count)
{
    u8 n=0;
    for(u8 i=0;i<count;i++)if(bytes[i])n++;
    if(!n)return;
    ensureDriver();
    SYS_disableInts();
    bool taken=Z80_getAndRequestBus(TRUE);
    /* At most 15 in flight; the Z80 drains the ring every few samples. */
    for(u8 i=0;i<count && n;i++)if(bytes[i]){
        *mailbox(MB_SFXRING+(sfxWrite&15))=bytes[i];sfxWrite++;n--;
    }
    *mailbox(MB_SFXWR)=sfxWrite&15;
    if(!taken)Z80_releaseBus();
    SYS_enableInts();
}
