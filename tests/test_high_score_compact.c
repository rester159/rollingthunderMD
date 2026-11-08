#include <genesis.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef int16_t s16;
#define FAR_SAFE(p,n) (p)
#define DMA_QUEUE_COPY 1
#define DMA_VRAM 0
static u16 vram[32768],dmaMemory[4096],dmaUsed,dmaCount,temporaryUsed;
void SYS_disableInts(void){}
void SYS_enableInts(void){}
static u16 DMA_getQueueTransferSize(void){return dmaUsed;}
static u16 DMA_getQueueSize(void){return dmaCount;}
static u16 DMA_getMaxQueueSize(void){return 256;}
static bool DMA_canQueue(u8 location,u16 words){return temporaryUsed+words<=4096;}
static u16 *DMA_allocateTemp(u16 words){u16 *p=dmaMemory+temporaryUsed;temporaryUsed+=words;assert(temporaryUsed<=4096);return p;}
static bool DMA_queueDmaFast(u8 location,const void *source,u16 destination,u16 words,s16 step)
{assert(!location && step==2);memcpy((u8*)vram+destination,source,words*2);dmaUsed+=words*2;dmaCount++;return true;}
static bool DMA_transfer(u8 method,u8 location,const void *source,u16 destination,u16 words,s16 step)
{assert(method==DMA_QUEUE_COPY);return DMA_queueDmaFast(location,source,destination,words,step);}
bool RT_copyRomResource(void *out,u32 address,u16 bytes,u32 limit){assert(0);return false;}
#include "../src/game_over_panel_upload.c"
static void flush(void){dmaUsed=dmaCount=temporaryUsed=0;}
static u8 *readFile(const char *path,u32 *length)
{
 FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);*length=ftell(f);rewind(f);
 u8 *data=malloc(*length);assert(data && fread(data,1,*length,f)==*length);fclose(f);return data;
}
int main(int argc,char **argv)
{
 assert(argc==3);u32 oldSize,newSize;u8 *old=readFile(argv[1],&oldSize),*packed=readFile(argv[2],&newSize);
 u32 sceneLength=4176+(u32)word(old+10)*32;
 RT_GameOverPanelUpload states[2]={{0}};
 assert(RT_beginGameOverPanelUpload(states,old,sceneLength,old+sceneLength,oldSize-sceneLength-4032,0));
 assert(RT_beginGameOverPanelUpload(states+1,packed,sceneLength,packed+sceneLength,newSize-sceneLength-4032,0));
 assert(!states[0].sharedLookup && states[1].sharedLookup);
 u16 snapshots[2][32768];
 for(u8 variant=0;variant<2;variant++){
  memset(vram,0xa5,sizeof vram);flush();
  while(!states[variant].ready){assert(RT_queueGameOverPanelPatterns(states+variant,2048));flush();}
  memcpy(snapshots[variant],vram,sizeof vram);
 }
 assert(!memcmp(snapshots[0],snapshots[1],sizeof vram));
 for(u16 revision=0;revision<=90;revision++){
  u8 panel[672];
  for(u16 i=0;i<336;i++){
   u16 glyph=(revision+i)%90;
   panel[i*2]=glyph%45==44?255:glyph%45;panel[i*2+1]=glyph>=45?228:252;
   if(revision==90){panel[i*2]=255;panel[i*2+1]=0;}
  }
  for(u8 variant=0;variant<2;variant++){
   memcpy(vram,snapshots[variant],sizeof vram);flush();u16 cursor=0;
   for(u16 limit=0;cursor<336;limit++){
    assert(limit<100);RT_CompositePanelResult r=RT_queueGameOverCompositePanelChanges(states+variant,panel,&cursor,37,2048);
    assert(r!=RT_COMPOSITE_PANEL_INVALID);flush();
   }
   memcpy(snapshots[variant],vram,sizeof vram);
  }
  assert(!memcmp(snapshots[0],snapshots[1],sizeof vram));
 }
 RT_GameOverPanelUpload bad={0};packed[sceneLength+16]=255;packed[sceneLength+17]=255;
 assert(!RT_beginGameOverPanelUpload(&bad,packed,sceneLength,packed+sceneLength,newSize-sceneLength-4032,0));
 assert(!bad.valid);free(old);free(packed);
 puts("RTPC v1/v2: all 30,240 cell/glyph combinations and background restoration produce identical VRAM.");
}
