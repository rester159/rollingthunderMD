#include "rt_stage_transition_raster.h"

typedef struct {u32 control,first,second,third,fourth;} RasterPacket;
static struct
{
    volatile bool footerDone,frameStarted,primed,active;
    u16 footerCount;
    const RasterPacket *volatile nextPacket;
} runtime;
static RasterPacket packets[7],footerPacket;
static u16 topPalette[64];
static RT_StageTransitionRasterStats stats;

static HINTERRUPT_CALLBACK transitionRasterInterrupt(void)
{
    u16 line;
    if (!runtime.active) return;
    line=GET_HVCOUNTER>>8;stats.interrupts++;
    if (line<100)
    {
        /* The counter reload precedes the ISR's register write. The first
           IRQ primes the counter that will run at the stable-color gap. */
        if (line<60) VDP_setHIntCounter(63);
        else if (!runtime.primed) {VDP_setHIntCounter(1);runtime.primed=TRUE;}
        return;
    }
    if (line>=129 && line<=143 && stats.index<7)
    {
        const RasterPacket *packet=runtime.nextPacket;
        if (!stats.index) stats.first=line;
        *(vu32 *)VDP_CTRL_PORT=packet->control;
        *(vu32 *)VDP_DATA_PORT=packet->first;
        *(vu32 *)VDP_DATA_PORT=packet->second;
        *(vu32 *)VDP_DATA_PORT=packet->third;
        *(vu32 *)VDP_DATA_PORT=packet->fourth;
        runtime.nextPacket=packet+1;stats.index++;stats.last=line;
        if (stats.index==7) VDP_setHIntCounter(68);
    }
    else if (line>=212 && line<216)
    {
        if (stats.index!=7) stats.fault=1;
        *(vu32 *)VDP_CTRL_PORT=footerPacket.control;
        *(vu32 *)VDP_DATA_PORT=footerPacket.first;
        if(runtime.footerCount==3)*(vu16 *)VDP_DATA_PORT=(u16)footerPacket.second;
        stats.footer=line;runtime.footerDone=TRUE;VDP_setHIntCounter(255);
    }
}

bool RT_startStageTransitionRaster(const RT_StageTransitionBackground *background)
{
    if (!background || !background->data || background->footerCount<2 || background->footerCount>3) return FALSE;
    u16 index=background->footerIndices[0];
    if (index>64-background->footerCount) return FALSE;
    for(u16 i=1;i<background->footerCount;i++)if(background->footerIndices[i]!=index+i)return FALSE;
    return RT_startStageTransitionRasterPalettes(background->palettes,index,background->footerCount);
}

bool RT_startStageTransitionRasterPalettes(const u16 palettes[3][64],
    u16 index,u16 footerCount)
{
    u16 i,p;
    if(!palettes || footerCount<2 || footerCount>3 || index>64-footerCount)return FALSE;
    for (p=0;p<3;p++) for (i=0;i<64;i++)
        if ((palettes[p][i]&~0x0EEE) || (!(i&15) && palettes[p][i])) return FALSE;
    for (i=49;i<=58;i++)
        if (palettes[0][i]!=palettes[1][i] || palettes[1][i]!=palettes[2][i]) return FALSE;
    for (i=0;i<64;i++)
        if ((palettes[1][i]!=palettes[2][i])!=(i>=index && i<index+footerCount)) return FALSE;
    SYS_disableInts();
    runtime.active=FALSE;VDP_setHInterrupt(FALSE);VDP_setHIntCounter(255);
    memcpy(topPalette,palettes[0],sizeof topPalette);
    for (i=0;i<7;i++)
    {
        u16 first=i<6?i*8:56;
        const u16 *colors=palettes[1]+first;
        packets[i].control=VDP_WRITE_CRAM_ADDR(first*2);
        packets[i].first=((u32)colors[0]<<16)|colors[1];
        packets[i].second=((u32)colors[2]<<16)|colors[3];
        packets[i].third=((u32)colors[4]<<16)|colors[5];
        packets[i].fourth=((u32)colors[6]<<16)|colors[7];
    }
    footerPacket.control=VDP_WRITE_CRAM_ADDR(index*2);
    footerPacket.first=((u32)palettes[2][index]<<16)|palettes[2][index+1];
    runtime.footerCount=footerCount;
    footerPacket.second=runtime.footerCount==3?palettes[2][index+2]:0;
    memset(&stats,0,sizeof stats);runtime.nextPacket=packets;runtime.primed=runtime.frameStarted=runtime.footerDone=FALSE;
    SYS_setHIntCallback(transitionRasterInterrupt);runtime.active=TRUE;
    SYS_enableInts();
    return TRUE;
}

void RT_serviceStageTransitionRasterVInt(void)
{
    if (!runtime.active) return;
    if (runtime.frameStarted && stats.index!=7) stats.fault=2;
    else if (runtime.frameStarted && !runtime.footerDone) stats.fault=3;
    runtime.frameStarted=TRUE;runtime.footerDone=FALSE;
    stats.index=0;runtime.primed=FALSE;runtime.nextPacket=packets;
    VDP_setAutoInc(2);PAL_setColors(0,topPalette,64,CPU);
    VDP_setHIntCounter(63);VDP_setHInterrupt(TRUE);
}

void RT_stopStageTransitionRaster(void)
{
    SYS_disableInts();
    runtime.active=FALSE;VDP_setHInterrupt(FALSE);
    SYS_setHIntCallback(NULL);VDP_setHIntCounter(255);
    SYS_enableInts();
}

const volatile RT_StageTransitionRasterStats *RT_getStageTransitionRasterStats(void)
{
    return &stats;
}

u16 RT_acknowledgeStageTransitionRasterFault(void)
{
    SYS_disableInts();u16 fault=stats.fault;stats.fault=0;SYS_enableInts();
    return fault;
}
