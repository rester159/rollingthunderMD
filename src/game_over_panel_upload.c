#include "rt_game_over_panel_upload.h"
#include "rt_rom_resource_copy.h"
static u16 word(const u8 *p){return ((u16)p[0]<<8)|p[1];}
static bool span(const u8 *p,u32 size,u32 limit)
{return p && !((u32)p&1) && (!limit || ((u32)p<limit && size<=limit-(u32)p));}
static bool readBytes(void *out,const u8 *p,u16 size,u32 limit)
{
 if(limit)return RT_copyRomResource(out,(u32)p,size,limit);
 memcpy(out,FAR_SAFE(p,size),size);return TRUE;
}
bool RT_beginGameOverPanelUpload(RT_GameOverPanelUpload *s,
 const u8 *scene,u32 length,const u8 *prepared,u32 size,u32 limit)
{
 u8 a[128],b[128];u16 count,i;u32 offset,records;
 if(!s || s->valid || limit>0x2000000UL || length<4176 ||
    length>4176+768UL*32 || size<16 || !span(scene,length,limit) ||
    !span(prepared,size,limit))return FALSE;
 if(!readBytes(a,scene,16,limit) || !readBytes(b,prepared,16,limit))return FALSE;
 if(memcmp(a,"RTGC",4) || word(a+4)!=1 || word(a+6)!=36 || word(a+8)!=28 ||
    word(a+12)!=2 || word(a+14) || !word(a+10) || length!=4176+(u32)word(a+10)*32 ||
    memcmp(b,"RTPC",4) || (word(b+4)!=1 && word(b+4)!=2) || word(b+6)!=336 || word(b+8)!=90 ||
    (((u32)word(b+12)<<16)|word(b+14))!=length)return FALSE;
 bool sharedLookup=word(b+4)==2;
 count=word(b+10);records=sharedLookup?16+90*2:16+length+336UL*90*2;
 if(!count || count>128 || size!=records+(u32)count*68)return FALSE;
 for(offset=0;!sharedLookup && offset<length;offset+=128){
  u16 n=length-offset<128?length-offset:128;
  if(!readBytes(a,scene+offset,n,limit) || !readBytes(b,prepared+16+offset,n,limit) || memcmp(a,b,n))return FALSE;
 }
 if(sharedLookup)for(i=0;i<90;i++){
  if(!readBytes(a,prepared+16+i*2,2,limit) || word(a)>=count)return FALSE;
 }
 /* Validate all headers before changing state. */
 for(i=0;i<count;i++){
  if(!readBytes(a,prepared+records+(u32)i*68,4,limit))return FALSE;
  if((word(a)>=4 || word(a+2)>=4) && !(word(a)==65535 && word(a+2)==65535))return FALSE;
 }
 for(i=0;i<count;i++){
  if(!readBytes(a,prepared+records+(u32)i*68,4,limit))return FALSE;
  s->entries[i*2]=word(a)==65535?65535:256+i*2+(word(a)<<13);
  s->entries[i*2+1]=word(a)==65535?65535:(257+i*2+(word(a+2)<<13))|0x8000;
 }
 s->scene=scene;s->prepared=prepared;s->romBytes=limit;s->lookup=sharedLookup?16:16+length;
 s->sharedLookup=sharedLookup;
 s->records=records;s->count=count;s->index=65535;s->ready=FALSE;
 memset(s->cells,254,sizeof s->cells);s->valid=TRUE;return TRUE;
}
bool RT_queueGameOverPanelPatterns(RT_GameOverPanelUpload *s,u16 budget)
{
 u32 data[16];
 if(!s || !s->valid || budget<64)return FALSE;
 if(s->ready)return TRUE;
 if(s->index==65535){
  memset(data,0,8);
  if((u32)DMA_getQueueTransferSize()+8>budget || !DMA_transfer(DMA_QUEUE_COPY,DMA_VRAM,data,0xF000,4,2))return TRUE;
  s->index=0;
 }
 while(s->index<s->count){
  if((u32)DMA_getQueueTransferSize()+64>budget)return TRUE;
  if(!readBytes(data,s->prepared+s->records+(u32)s->index*68+4,64,s->romBytes))return FALSE;
  if(!DMA_transfer(DMA_QUEUE_COPY,DMA_VRAM,data,(256+s->index*2)*32,32,2))return TRUE;
  s->index++;
 }
 if(!DMA_getQueueSize())s->ready=TRUE;
 return TRUE;
}
static bool queueRun(u16 cell,const u16 *a,const u16 *b,u16 count,u16 budget)
{
 u16 *copy;u16 destination=((cell/36)*64+cell%36+2)*2;
 SYS_disableInts();
 if((u32)DMA_getQueueTransferSize()+count*4>budget ||
    (u32)DMA_getQueueSize()+2>DMA_getMaxQueueSize() || !DMA_canQueue(DMA_VRAM,count*2)){
  SYS_enableInts();return FALSE;
 }
 copy=DMA_allocateTemp(count*2);
 if(!copy){SYS_enableInts();return FALSE;}
 memcpy(copy,a,count*2);memcpy(copy+count,b,count*2);
 DMA_queueDmaFast(DMA_VRAM,copy,0xC000+destination,count,2);
 DMA_queueDmaFast(DMA_VRAM,copy+count,0xE000+destination,count,2);
 SYS_enableInts();return TRUE;
}
RT_CompositePanelResult RT_queueGameOverCompositePanelRange(
 RT_GameOverPanelUpload *s,const u8 panel[672],u16 first,u16 count,
 u16 *cursor,u16 cellBudget,u16 byteBudget)
{
 u16 i,done=0,end;
 if(!s || !s->valid || !s->ready || !panel || !cursor || first>336 ||
    count>336-first || *cursor<first || *cursor>first+count || !cellBudget)return RT_COMPOSITE_PANEL_INVALID;
 end=first+count;
 for(i=first;i<end;i++){
  u8 glyph=panel[i*2],group=panel[i*2+1];
  if((glyph>=44 && glyph!=255) || (group!=252 && group!=228 && !(glyph==255 && !group)))return RT_COMPOSITE_PANEL_INVALID;
 }
 while(*cursor<end){
  u8 bytes[2];u16 a[28],b[28],entry,cell,run=0,limit,used;
  i=*cursor;
  if(!memcmp(s->cells+i*2,panel+i*2,2)){(*cursor)++;continue;}
  if(done==cellBudget)return RT_COMPOSITE_PANEL_MORE;
  cell=(4+i/28)*36+4+i%28;
  used=DMA_getQueueTransferSize();
  if(used>=byteBudget || byteBudget-used<4)return RT_COMPOSITE_PANEL_RETRY;
  limit=(byteBudget-used)/4;
  if(limit>28-i%28)limit=28-i%28;
  if(limit>cellBudget-done)limit=cellBudget-done;
  if(limit>end-i)limit=end-i;
  /* A contiguous source row becomes one copied transfer per plane. Never
     cross the panel row boundary or accept half of the two-plane update. */
  while(run<limit && memcmp(s->cells+(i+run)*2,panel+(i+run)*2,2)){
   u16 index=i+run;
   a[run]=b[run]=65535;
   if(panel[index*2+1]){
   u16 glyph=(panel[index*2+1]==228?45:0)+(panel[index*2]==255?44:panel[index*2]);
   if(!readBytes(bytes,s->prepared+s->lookup+((s->sharedLookup?0:(u32)index*90)+glyph)*2,2,s->romBytes))return RT_COMPOSITE_PANEL_INVALID;
   entry=word(bytes);if(entry>=s->count)return RT_COMPOSITE_PANEL_INVALID;
   a[run]=s->entries[entry*2];b[run]=s->entries[entry*2+1];
  }
  if(a[run]==65535){
   if(!readBytes(bytes,s->scene+144+(cell+run)*2,2,s->romBytes))return RT_COMPOSITE_PANEL_INVALID;
   a[run]=word(bytes)+512;
   if(!readBytes(bytes,s->scene+2160+(cell+run)*2,2,s->romBytes))return RT_COMPOSITE_PANEL_INVALID;
   b[run]=(word(bytes)+512)|0x8000;
  }
   run++;
  }
  if(!queueRun(cell,a,b,run,byteBudget))return RT_COMPOSITE_PANEL_RETRY;
  memcpy(s->cells+i*2,panel+i*2,run*2);*cursor+=run;done+=run;
 }
 return RT_COMPOSITE_PANEL_QUEUED;
}
RT_CompositePanelResult RT_queueGameOverCompositePanelChanges(
 RT_GameOverPanelUpload *s,const u8 panel[672],u16 *cursor,u16 cellBudget,u16 byteBudget)
{
 return RT_queueGameOverCompositePanelRange(s,panel,0,336,cursor,cellBudget,byteBudget);
}
