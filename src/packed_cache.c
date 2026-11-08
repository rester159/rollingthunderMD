#include "rt_packed_cache.h"
#include "rt_rom_layout.h"
#ifdef RT_LINEAR_ROM
/* Immutable, exact player pixels decoded while the display is disabled.
 * No eviction or background work can change a queued DMA source. */
static RT_PackedWorkspace *workspace;
typedef struct {const void *key;u16 *pixels;u16 words;} Entry;
static Entry entries[64];
static u16 used,staged,extraUsed;
static u16 *extra,*stagePixels;
static u16 stageWords;
static u8 allowStaging;
static u16 *borrowed[2],borrowedWords[2],borrowedUsed[2];
volatile u16 rtPackedExtraBytes;
static u8 published,preparing;
volatile u16 rtPackedPrepareHighWater;
volatile u32 rtPrefetchHits,rtPrefetchMisses,rtPrefetchCompleted;
void RT_packedWorkspaceBegin(void *memory,u16 bytes)
{workspace=bytes>=sizeof(RT_PackedWorkspace)?memory:0;used=staged=published=extraUsed=0;allowStaging=1;preparing=1;stagePixels=0;stageWords=0;memset(entries,0,sizeof entries);memset(borrowed,0,sizeof borrowed);memset(borrowedUsed,0,sizeof borrowedUsed);}
void RT_packedWorkspaceEnd(void)
{workspace=0;if(extra){MEM_free(extra);extra=0;}extraUsed=0;rtPackedExtraBytes=0;}
void RT_packedExtraEnable(void)
{
    /* Fonts need temporary heap space during display setup. Borrow it only
     * after setup, and release it in endHud before any later display reset. */
    allowStaging=1;MEM_pack();
    u16 bytes=MEM_getLargestFreeBlock();bytes=bytes>128?(bytes-128)&~31:0;
    if(bytes>3840)bytes=3840;
    extra=bytes?MEM_alloc(bytes):0;rtPackedExtraBytes=extra?bytes:0;
}
void RT_packedStaging(void *memory,u16 bytes){stagePixels=memory;stageWords=bytes/2;}
void RT_packedBorrow(u8 index,void *memory,u16 bytes)
{if(index<2){borrowed[index]=memory;borrowedWords[index]=bytes/2;borrowedUsed[index]=0;}}
static u8 hash(const void *key){return ((u32)key>>2)%64;}
u16 *RT_packedPrefetched(const void *key,u16 words)
{
    if(workspace){u8 index=hash(key);
        for(u8 n=0;n<64;n++,index=(index+1)&63){
            const Entry *e=entries+index;
            if(!e->key)break;
            if(e->key==key && e->words==words){rtPrefetchHits++;return e->pixels;}
        }
    }
    rtPrefetchMisses++;return 0;
}
void RT_packedPreload(const void *key,u16 words)
{
    if(!workspace || !words || ((u32)key&0xfe000000UL)!=0x84000000UL)return;
    u8 index=hash(key);
    for(u8 n=0;n<64;n++,index=(index+1)&63){
        Entry *e=entries+index;
        if(e->key==key)return;
        if(e->key)continue;
        u16 *output;
        if(used+words<=RT_PACKED_CACHE_WORDS){output=workspace->pixels+used;used+=words;}
        else if(borrowed[0] && borrowedUsed[0]+words<=borrowedWords[0]){output=borrowed[0]+borrowedUsed[0];borrowedUsed[0]+=words;}
        else if(borrowed[1] && borrowedUsed[1]+words<=borrowedWords[1]){output=borrowed[1]+borrowedUsed[1];borrowedUsed[1]+=words;}
        else if(stagePixels && extra && extraUsed+words<=rtPackedExtraBytes/2){output=extra+extraUsed;extraUsed+=words;}
        else return;
        RT_decodePackedPose(key,output,words);
        *e=(Entry){key,output,words};rtPrefetchCompleted++;return;
    }
}
void RT_packedPrepareBegin(void){preparing=published^1;staged=0;}
void RT_packedPreparePublish(void){published=preparing;}
u16 *RT_packedPrepareAllocate(u16 words)
{
    u16 capacity=stagePixels?stageWords/2:extra?rtPackedExtraBytes/4:(RT_PACKED_CACHE_WORDS-used)/2;
    if(!workspace || !allowStaging || !words || words>capacity-staged)return 0;
    u16 *result=(stagePixels?stagePixels:extra?extra:workspace->pixels+used)+(u16)preparing*capacity+staged;
    staged+=words;
    if(staged*2>rtPackedPrepareHighWater)rtPackedPrepareHighWater=staged*2;
    return result;
}
#endif
