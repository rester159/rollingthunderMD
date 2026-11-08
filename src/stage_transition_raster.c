#include "rt_stage_transition_raster.h"
#include "rt_z80_bus_policy.h"

static struct
{
    volatile bool bodyPrepared,footerPrepared,bodyDone,footerDone,frameStarted,primed,active;
    u8 audioGuard;
    u16 packetCount,footerCount;
} runtime;
static u16 packetColors[50],footerColors[3],footerFirst;
static u8 packetIndices[50];
extern void RT_transitionRasterWriteColors(const u8 *indices,const u16 *colors,u32 pairs);
static u16 topPalette[64];
static RT_StageTransitionRasterStats stats;
volatile u32 rtTransitionRasterBusHolds;

static HINTERRUPT_CALLBACK transitionRasterInterrupt(void)
{
    u16 line;
    if (!runtime.active) return;
    line=GET_HVCOUNTER>>8;stats.interrupts++;
    if (line<90)
    {
        /* If the main loop missed its VBlank publication, bound the carried
         * audio reservation rather than keeping RAM playback parked forever. */
        RT_z80DmaGuardEnd(runtime.audioGuard);runtime.audioGuard=0;
        /* The counter reload precedes the ISR's register write. The first
           IRQ primes the counter that will run at the stable-color gap. */
        if (line<46) VDP_setHIntCounter(49);
        else if (!runtime.primed) {VDP_setHIntCounter(0);runtime.primed=TRUE;}
        return;
    }
    if (line>=99 && line<=128 && !runtime.bodyDone)
    {
        if(!runtime.bodyPrepared){
            /* Short preparation ticks tolerate a delayed/coalesced IRQ.
             * Let the main loop run between preparing audio and the split. */
            VDP_setHIntCounter(3);
            runtime.audioGuard=RT_z80DmaGuardBegin();
            if(runtime.audioGuard==1)rtTransitionRasterBusHolds++;
            runtime.bodyPrepared=TRUE;
        }
        /* If a blocking transfer swallowed the first IRQ, the old 50-line
         * reload is still running. Service this frame in the same handler;
         * its next 149/157/... IRQs still provide a safe footer preparation. */
        if(runtime.primed && line<118)return;
        VDP_setHIntCounter(7);
        while((GET_HVCOUNTER>>8)<127){}
        stats.first=GET_HVCOUNTER>>8;
        if(runtime.packetCount)RT_transitionRasterWriteColors(packetIndices,packetColors,runtime.packetCount);
        stats.index=runtime.packetCount;
        stats.last=GET_HVCOUNTER>>8;runtime.bodyDone=TRUE;
        RT_z80DmaGuardEnd(runtime.audioGuard);runtime.audioGuard=0;
    }
    else if (line>=170 && line<216 && runtime.bodyDone && !runtime.footerDone)
    {
        if(!runtime.footerPrepared){
            /* Audio can finish a ROM refill before the actual split. Four
             * line preparation ticks do not depend on IRQ entry latency. */
            VDP_setHIntCounter(3);
            runtime.audioGuard=RT_z80DmaGuardBegin();
            if(runtime.audioGuard==1)rtTransitionRasterBusHolds++;
            runtime.footerPrepared=TRUE;
        }
        if(line<206)return;
        u8 indices[4];u16 colors[4];
        for(u8 i=0;i<4;i++){
            u8 offset=i<runtime.footerCount?i:runtime.footerCount-1;
            indices[i]=footerFirst+offset;colors[i]=footerColors[offset];
        }
        /* Enter ahead of the split so the audio parking handshake cannot put
         * the footer palette behind its first row. The helper waits for the
         * next full blank: begin on line211 for a line212-or-later write. */
        while((GET_HVCOUNTER>>8)<211){}
        RT_transitionRasterWriteColors(indices,colors,(runtime.footerCount+1)/2);
        /* Keep this acknowledged reservation through the next VBlank upload.
         * Waiting for a fresh ROM-refill handshake inside DMA can consume
         * the blanking budget while interrupts are masked. */
        stats.footer=GET_HVCOUNTER>>8;runtime.footerDone=TRUE;VDP_setHInterrupt(FALSE);VDP_setHIntCounter(255);
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
    u16 i,p,count=0;u8 changed[50];
    if(!palettes || footerCount<2 || footerCount>3 || index>64-footerCount)return FALSE;
    for (p=0;p<3;p++) for (i=0;i<64;i++)
        if ((palettes[p][i]&~0x0EEE) || (!(i&15) && palettes[p][i])) return FALSE;
    for (i=49;i<=58;i++)
        if (palettes[0][i]!=palettes[1][i] || palettes[1][i]!=palettes[2][i]) return FALSE;
    for (i=0;i<64;i++)
        if ((palettes[1][i]!=palettes[2][i])!=(i>=index && i<index+footerCount)) return FALSE;
    /* The stable console colors and transparent pens leave at most 50
     * changed entries. Two writes per blank fit the 128..152 palette gap. */
    for(i=0;i<64;i++)if(palettes[0][i]!=palettes[1][i]){
        if(count==50)return FALSE;
        changed[count++]=i;
    }
    SYS_disableInts();
    runtime.active=FALSE;VDP_setHInterrupt(FALSE);VDP_setHIntCounter(255);
    RT_z80DmaGuardEnd(runtime.audioGuard);
    memcpy(topPalette,palettes[0],sizeof topPalette);
    runtime.packetCount=(count+1)/2;
    for (i=0;i<runtime.packetCount;i++)
    {
        u16 first=changed[i*2],second=changed[i*2+1<count?i*2+1:i*2];
        packetIndices[i*2]=first;packetColors[i*2]=palettes[1][first];
        packetIndices[i*2+1]=second;packetColors[i*2+1]=palettes[1][second];
    }
    runtime.footerCount=footerCount;footerFirst=index;
    for(i=0;i<footerCount;i++)footerColors[i]=palettes[2][index+i];
    memset(&stats,0,sizeof stats);
    runtime.audioGuard=0;
    runtime.primed=runtime.frameStarted=runtime.bodyPrepared=runtime.footerPrepared=
        runtime.bodyDone=runtime.footerDone=FALSE;
    SYS_setHIntCallback(transitionRasterInterrupt);runtime.active=TRUE;
    SYS_enableInts();
    return TRUE;
}

void RT_serviceStageTransitionRasterVInt(void)
{
    if (!runtime.active) return;
    if (runtime.frameStarted && !runtime.bodyDone) stats.fault=2;
    else if (runtime.frameStarted && !runtime.footerDone) stats.fault=3;
    if(!runtime.footerDone){RT_z80DmaGuardEnd(runtime.audioGuard);runtime.audioGuard=0;}
    runtime.frameStarted=TRUE;runtime.bodyDone=runtime.footerDone=FALSE;
    runtime.bodyPrepared=runtime.footerPrepared=FALSE;
    stats.index=0;runtime.primed=FALSE;
    VDP_setAutoInc(2);PAL_setColors(0,topPalette,64,CPU);
    VDP_setHIntCounter(49);VDP_setHInterrupt(TRUE);
}

void RT_releaseStageTransitionRasterIO(void)
{
    SYS_disableInts();
    RT_z80DmaGuardEnd(runtime.audioGuard);runtime.audioGuard=0;
    SYS_enableInts();
}

void RT_stopStageTransitionRaster(void)
{
    SYS_disableInts();
    runtime.active=FALSE;VDP_setHInterrupt(FALSE);
    RT_z80DmaGuardEnd(runtime.audioGuard);runtime.audioGuard=0;
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
