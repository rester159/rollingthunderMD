#include <genesis.h>
#include "rt_rom_layout.h"
#ifdef RT_STREAMED_PCM
#include "rt_pcm_stream_locations.h"
#include "rt_segmented_pcm.h"
#include "rt_z80_bus_policy.h"
extern const u8 pcm_rom_z80[4096];
#define P ((volatile u8*)0xa00100)
static bool active;
static u16 previousBank,initialPage;
static u8 lastPage;
static u32 generation;
static volatile u8 status;
static bool owns(void)
{
 /* The Z80 changes the hardware mapping without updating SDK's cache.
  * This checks our unchanged reservation, NOT the current physical page. */
 return active && Z80_getLoadedDriver()==Z80_DRIVER_CUSTOM &&
  RT_z80DriverGeneration()==generation && SYS_getBank(1)==initialPage;
}
static bool signature(void)
{return P[16]==0x52 && P[17]==0x54 && P[18]==0x50 && P[19]==0x43 && P[20]==9;}
static void word(u16 at,u16 v){P[at]=v;P[at+1]=v>>8;}
/* Construct immutable dispatch programs before starting playback. The Z80 IRQ
 * restores the one table word its automatic return-address push overwrites. */
static void dispatchTables(u8 delta)
{
 volatile u8 *ram=(volatile u8*)0xa00000;
 for(u16 mask=0;mask<256;mask++){
  u16 at=0x1200+mask*6;
  ram[0x1e00+mask]=at;ram[0x1f00+mask]=at>>8;
  for(u16 pair=0;pair<3;pair++){
   u16 index=pair<2?((mask>>(4-pair*4))&15)+(delta?17:0):16;
   ram[at+pair*2]=ram[0x120+index*2];
   ram[at+pair*2+1]=ram[0x121+index*2];
  }
 }
}
static void publish(const RT_SegmentedPcmPlan *p)
{
 initialPage=p->page;lastPage=p->lastPage;u8 delta=0;
 for(u16 i=0;i<4;i++)if(rtPcmStreamClips[i][0]==((u32)p->page<<19)+((u32)(p->bank-0x10)<<15)){lastPage=rtPcmStreamClips[i][2];delta=rtPcmStreamClips[i][3];}
 dispatchTables(delta);P[29]=delta;
 SYS_setBank(1,initialPage);
 Z80_upload(0x1000,(const u8*)((u32)p->bank<<15),p->preload);
 word(2,0x8000+p->preload);word(4,p->bank);word(6,p->remainingPages);
 P[8]=p->tail;P[9]=p->preloadEnd;
 word(22,((u32)&status>>15)&511);word(24,0x8000|((u32)&status&32767));
 P[26]=1;P[27]=p->page;status=RT_SEGPCM_STARTING;P[1]=RT_SEGPCM_STARTING;P[0]=1;
}
__attribute__((noipa,used,externally_visible)) bool RT_startSegmentedPcm(u32 address,u32 count,u32 romBytes)
{
 RT_SegmentedPcmPlan p;if(!RT_planSegmentedPcm(address,count,romBytes,&p))return FALSE;
 SYS_disableInts();
 if(active || Z80_isBusTaken()){SYS_enableInts();return FALSE;}
 previousBank=SYS_getBank(1);
 Z80_loadCustomDriver(pcm_rom_z80,4096);generation=RT_z80DriverGeneration();
 Z80_requestBus(TRUE);publish(&p);active=TRUE;Z80_releaseBus();SYS_enableInts();return TRUE;
}
__attribute__((noipa,used,externally_visible)) bool RT_rearmSegmentedPcm(u32 address,u32 count,u32 romBytes)
{
 RT_SegmentedPcmPlan p;if(!RT_planSegmentedPcm(address,count,romBytes,&p))return FALSE;
 SYS_disableInts();
 if(!owns() || Z80_isBusTaken() || status!=RT_SEGPCM_COMPLETE){SYS_enableInts();return FALSE;}
 Z80_requestBus(TRUE);
 if(!signature() || P[1]!=RT_SEGPCM_COMPLETE || P[0] || P[27]!=lastPage){
  Z80_releaseBus();SYS_enableInts();return FALSE;
 }
 publish(&p);Z80_releaseBus();SYS_enableInts();return TRUE;
}
__attribute__((noipa,used,externally_visible)) bool RT_replaceSegmentedPcm(u32 address,u32 count,u32 romBytes)
{
 RT_SegmentedPcmPlan p;if(!RT_planSegmentedPcm(address,count,romBytes,&p))return FALSE;
 SYS_disableInts();
 if(!owns() || Z80_isBusTaken()){SYS_enableInts();return FALSE;}
 Z80_requestBus(TRUE);
 if(!signature() || P[1]<RT_SEGPCM_STARTING || P[1]>RT_SEGPCM_COMPLETE){
  Z80_releaseBus();SYS_enableInts();return FALSE;
 }
 /* Publish under bus ownership, then reset before releasing the old cursors.
  * Z80 RAM writes while reset is asserted are not assumed to work. */
 Z80_upload(0,pcm_rom_z80,4096);
 publish(&p);Z80_startReset();Z80_releaseBus();waitSubTick(50);Z80_endReset();
 SYS_enableInts();return TRUE;
}
u8 RT_segmentedPcmStatus(void)
{
 SYS_disableInts();u8 result;
 if(!active)result=RT_SEGPCM_OFF;
 else if(!owns())result=RT_SEGPCM_LOST;
 else{
  result=status;
  if(result==RT_SEGPCM_COMPLETE){
   bool taken=Z80_getAndRequestBus(TRUE);
   if(!signature() || P[1]!=RT_SEGPCM_COMPLETE || P[0] || P[27]!=lastPage)result=RT_SEGPCM_LOST;
   if(!taken)Z80_releaseBus();
  }else if(result!=RT_SEGPCM_STARTING && result!=RT_SEGPCM_PLAYING)result=RT_SEGPCM_LOST;
 }
 SYS_enableInts();return result;
}
__attribute__((noipa,used,externally_visible)) bool RT_stopSegmentedPcm(void)
{
 SYS_disableInts();if(!active){SYS_enableInts();return TRUE;}
 if(!owns() || Z80_isBusTaken()){SYS_enableInts();return FALSE;}
 Z80_requestBus(TRUE);
 if(!signature()){Z80_releaseBus();SYS_enableInts();return FALSE;}
 Z80_unloadDriver();SYS_setBank(1,previousBank);active=FALSE;
 SYS_enableInts();return TRUE;
}

#endif
