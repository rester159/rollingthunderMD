#include <genesis.h>
#include "rt_sprites.h"
#include "rt_rom_layout.h"
#include "rt_packed_cache.h"
#ifdef RT_PACKED_PATTERNS
extern u16 RT_queueCanonicalPose(const void*,u16,u16);
#endif
_Static_assert(sizeof(RT_PackedSpritePiece)==12 && sizeof(RT_SpriteCommand)==8,"packed hardware ABI");
#ifdef __m68k__
#include "rt_packed_emit_registers.h"
#include "rt_piece_emit_registers.h"
#endif


typedef struct {void *source;u16 dest,len;u8 method;} RT_PreparedTransfer;
static RT_PreparedTransfer preparedTransfers[83];
static u8 prepareTransfers,preparedCount;
u8 RT_spritesDeferred(void){return prepareTransfers;}
void RT_spritesDefer(void){
    preparedCount=0;prepareTransfers=1;
#ifdef RT_LINEAR_ROM
    RT_packedPrepareBegin();
#endif
}
static u16 publishTransfer(u8 method,u8 location,void *source,u16 dest,u16 len,s16 step)
{
#ifdef RT_PACKED_PATTERNS
    if(((u32)source&0xfe000000UL)==0x84000000UL){
        if(method!=DMA_QUEUE || location!=DMA_VRAM || step!=2)return 0;
        return RT_queueCanonicalPose(source,dest,len);
    }
#endif
    return DMA_transfer(method,location,source,dest,len,step);
}
static u16 prepareTransfer(u8 method,u8 location,void *source,u16 dest,u16 len,s16 step)
{
    if(!prepareTransfers)return publishTransfer(method,location,source,dest,len,step);
    if(location!=DMA_VRAM || step!=2 || preparedCount>=83)return 0;
#ifdef RT_LINEAR_ROM
    if(method==DMA_QUEUE && ((u32)source&0xfe000000UL)==0x84000000UL){
        void *decoded=RT_prepareCanonicalPose(source,len);
        if(decoded)source=decoded;
    }
#endif
    preparedTransfers[preparedCount++]=(RT_PreparedTransfer){source,dest,len,method};return 1;
}
u8 RT_spritesPublishPrepared(void)
{
    prepareTransfers=0;
    for(u8 i=0;i<preparedCount;i++){
        RT_PreparedTransfer *p=preparedTransfers+i;
        if(!publishTransfer(p->method,DMA_VRAM,p->source,p->dest,p->len,2))return 0;
    }
#ifdef RT_LINEAR_ROM
    RT_packedPreparePublish();
#endif
    preparedCount=0;return 1;
}
void RT_spritesDiscardPrepared(void){prepareTransfers=0;preparedCount=0;}
/* Queue a pose's patterns into its slot and publish its residency. */
u8 RT_spriteUpload(RT_SpriteBatch *batch,RT_SpriteSlot *slot,const RT_SpritePose *pose)
{
    if(!prepareTransfer(DMA_QUEUE,DMA_VRAM,(void *)pose->patterns,slot->firstTile*32,
        pose->tileCount*16,2)){batch->failed=1;return 0;}
    slot->pose=pose;batch->queuedBytes+=pose->tileCount*32;return 1;
}

u8 RT_spriteSlotInitialize(RT_SpriteSlot *slot,u16 firstTile,u16 capacity)
{
    if(!slot || !capacity || capacity>64 || firstTile>=1536 ||
       capacity>1536-firstTile)return 0;
    slot->pose=NULL;slot->firstTile=firstTile;slot->capacity=capacity;
    return 1;
}
static RT_SpriteCommand published[80];
static u8 nextPublication;
#ifndef __m68k__
static RT_SpriteCommand sdkCache[80];
#endif
static RT_SpriteCommand *nextCommands(void)
{
#ifdef __m68k__
    _Static_assert(sizeof(VDPSprite)==sizeof(RT_SpriteCommand),"SDK SAT stride");
    RT_SpriteCommand *second=(RT_SpriteCommand*)(void*)vdpSpriteCache;
#else
    RT_SpriteCommand *second=sdkCache;
#endif
    return (nextPublication?second:published)+4;
}

/* Begin is called with display disabled and no outstanding DMA. The HUD
 * prefix is immutable for the whole area, so initialize both publication
 * buffers once instead of copying it on every gameplay frame. */
void RT_spritesInitializeHud(const RT_SpriteCommand *prefix)
{
    memcpy(published,prefix,4*sizeof *prefix);
#ifdef __m68k__
    memcpy(vdpSpriteCache,prefix,4*sizeof *prefix);
#else
    memcpy(sdkCache,prefix,4*sizeof *prefix);
#endif
}

void RT_spritesBegin(RT_SpriteBatch *batch)
{
    if(!batch)return;
    batch->commands=batch->storage;batch->count=0;batch->queuedBytes=0;batch->failed=0;batch->reserved=0;batch->runCount=0;
}
void RT_spritesBeginHud(RT_SpriteBatch *batch)
{RT_spritesBegin(batch);if(batch){batch->reserved=22;batch->commands=nextCommands();}}
u8 RT_spriteSubmit(RT_SpriteBatch *batch,RT_SpriteSlot *slot,
    const RT_SpriteInstance *instance,const RT_WorldCamera *camera)
{
    if(!batch || batch->failed)return 0;
    if(!slot || !instance || !camera){batch->failed=1;return 0;}
    if(!instance->visible)return 1;
    const RT_SpritePose *pose=instance->pose;
    if(!pose || !pose->patterns || !pose->pieces || !pose->pieceCount ||
       !pose->tileCount || pose->tileCount>slot->capacity ||
       pose->pieceCount>80-batch->reserved-batch->count || instance->facingLeft>1 ||
       instance->priority>7 || instance->poolRank>RT_SPRITE_POOL_RELATED){batch->failed=1;return 0;}
    if(!RT_spritePixelsMatch(slot->pose,pose)){
        if(!prepareTransfer(DMA_QUEUE,DMA_VRAM,(void *)pose->patterns,slot->firstTile*32,
            pose->tileCount*16,2)){batch->failed=1;return 0;}
        slot->pose=pose;batch->queuedBytes+=pose->tileCount*32;
    }
    s16 localX=instance->x-camera->x,localY=instance->y-camera->y;
    u16 facing=instance->facingLeft;
#ifdef __m68k__
    {
        u16 start=batch->count;u8 key=(instance->priority<<2)|instance->poolRank;
        batch->count=RT_emitPiecesCall(0,batch->commands+start,batch->orderKeys+start,
            pose->pieces,start,pose->pieceCount,(u16)localX,(u16)localY,slot->firstTile,
            instance->priority,key,facing);
        RT_spritesMarkRun(batch,start,key);
        return 1;
    }
#endif
    u16 start=batch->count;
    for(u16 i=0;i<pose->pieceCount;i++){
        const RT_SpritePiece *piece=pose->pieces+i;
        if(piece->clipRule==0){
            /* Authored acceptance rectangle precedes device wrapping. This
             * retains CPU2's clipping without a source actor record. */
            s16 x=(s16)((u16)localX+(u16)piece->clipX[facing]);
            s16 y=(s16)(256-(u16)localY+(u16)piece->clipY);
            if(x < -32 || x>=320 || y < -32 || y>=224)continue;
        }
        if(piece->clipRule==3){
            /* Stage objects use CPU2's narrower inclusive -31..287 X
             * and -31..223 Y acceptance, before device wrapping. */
            s16 x=(s16)((u16)localX+(u16)piece->clipX[0]);
            s16 y=(s16)(256-(u16)localY+(u16)piece->clipY);
            if(x < -31 || x>287 || y < -31 || y>223)continue;
        }
        if(piece->clipRule==2 && (localX < -32 || localX>=320 || localY<=0 || localY>256))continue;
        u16 x=((u16)localX+16+(u16)piece->x[facing])&511;
        s16 y=(s16)((((u16)localY-32+(u16)piece->y+16)&255)-16);
        if(piece->clipRule==1){
            u16 width=(((piece->size>>2)&3)+1)*8;
            u16 height=((piece->size&3)+1)*8;
            if(x>=304 || x+width<=16 || y>=224 || y+(s16)height<=0)continue;
        }
        if(piece->clipRule==2 && (x>=304 || x+piece->clipX[0]<=16 || y>=224 || y+piece->clipY<=0))continue;
        RT_SpriteCommand *command=batch->commands+batch->count;
        batch->orderKeys[batch->count]=(instance->priority<<2)|instance->poolRank;
        command->y=y+128;command->x=x+128;
        command->size=piece->size;command->link=(u8)(++batch->count+2);
        command->attribute=(piece->attribute[facing]+slot->firstTile+piece->tile)|
            (((piece->priority|instance->priority)>=3)?0x8000:0);
    }
    RT_spritesMarkRun(batch,start,(instance->priority<<2)|instance->poolRank);
    return 1;
}
u8 RT_spriteSubmitResident(RT_SpriteBatch *batch,u16 firstTile,
    const RT_SpriteInstance *instance,const RT_WorldCamera *camera)
{
    if(!instance || !instance->pose || firstTile>=1536 ||
       instance->pose->tileCount>1536-firstTile){if(batch)batch->failed=1;return 0;}
    RT_SpriteSlot resident={instance->pose,firstTile,instance->pose->tileCount};
    return RT_spriteSubmit(batch,&resident,instance,camera);
}
static void linkSpriteOrder(RT_SpriteBatch *batch)
{
    u8 heads[32],tails[32],first=255,last=255,zeroPredecessor=255;
    u16 i;
    for(i=0;i<32;i++)heads[i]=255;
    for(i=0;i<batch->count;i++){
        u8 key=batch->orderKeys[i];
        if(heads[key]==255)tails[key]=i;
        if(heads[key]==0)zeroPredecessor=i;
        batch->commands[i].link=heads[key];
        heads[key]=i;
    }
    /* The source sorts records ascending and later entries win overlaps.
     * Prepending each component reverses equal-key records/components. */
    for(i=32;i>0;i--){
        u8 head=heads[i-1];
        if(head==255)continue;
        if(first==255)first=head;
        else {batch->commands[last].link=head;if(head==0)zeroPredecessor=last;}
        last=tails[i-1];
    }
    batch->commands[last].link=0;
    /* Genesis starts at entry zero. Exchange only the head, repairing the
     * single edge into old entry zero; all other native SAT records stay put. */
    if(first){
        RT_SpriteCommand saved=batch->commands[0];u8 key=batch->orderKeys[0];
        batch->commands[0]=batch->commands[first];batch->commands[first]=saved;
        batch->orderKeys[0]=batch->orderKeys[first];batch->orderKeys[first]=key;
        batch->commands[zeroPredecessor==first?0:zeroPredecessor].link=first;
    }
}
u8 RT_spritesQueue(RT_SpriteBatch *batch)
{
    if(!batch || batch->failed)return 0;
    u16 count=batch->count;
    if((batch->reserved && batch->reserved!=22) || count>80-batch->reserved)return 0;
    if(!batch->reserved && count>1){
        /* Emitters record keys per run; expand them for the generic sort. */
        for(u16 r=0;r<batch->runCount;r++){
            u16 end=r+1<batch->runCount?batch->runStarts[r+1]:count;
            for(u16 i=batch->runStarts[r];i<end;i++)batch->orderKeys[i]=batch->runKeys[r];
        }
        linkSpriteOrder(batch);
    }
    else if(!batch->reserved && count)batch->commands[0].link=0;
    else if(!batch->reserved){
        batch->commands[0]=(RT_SpriteCommand){0,0,0,0,0};
        count=1;
    }
    if(batch->reserved){
        /* Two persistent SAT buffers permit one pending DMA publication while
         * the next frame is prepared. Synchronous clients keep queue-copy
         * semantics; the campaign drains its old queue before publishing the
         * prepared frame. Never use the SDK sprite engine concurrently. */
        RT_SpriteCommand *masked=batch->commands-4;
        /* The HUD prefix already owns hardware entry zero. Link buckets
         * directly in their final SAT positions; no head exchange or rebasing. */
        /* Records already link to their predecessor (emitter writes
         * count+3), which is the previous same-key record inside a run of
         * equal keys. Patch only run starts: each links to the latest
         * earlier record with its key, or begins its key's chain. Keys then
         * chain in descending order; the source's later-entry-wins order
         * within a key is preserved exactly. */
        u8 lastOfKey[32],firstOfKey[32],seen[32],keys[32],distinct=0;
        for(u16 r=0;r<batch->runCount;r++){
            u8 key=batch->runKeys[r];
            if(distinct<32){u8 k=0;while(k<distinct && keys[k]!=key)k++;if(k==distinct){keys[distinct++]=key;seen[key]=0;}}
        }
        u8 zeroPredecessor=count>1 && (batch->runCount==1 || batch->runStarts[1]>1)?5:79;
        for(u16 r=0;r<batch->runCount;r++){
            u8 key=batch->runKeys[r],start=batch->runStarts[r];
            u16 end=r+1<batch->runCount?batch->runStarts[r+1]:count;
            if(seen[key]){
                masked[start+4].link=lastOfKey[key];
                if(lastOfKey[key]==4)zeroPredecessor=start+4;
            }
            else {seen[key]=1;firstOfKey[key]=start+4;}
            lastOfKey[key]=end+3;
        }
        /* Distinct keys are few: insertion-sort them descending. */
        for(u8 i=1;i<distinct;i++){
            u8 key=keys[i],j=i;
            while(j && keys[j-1]<key){keys[j]=keys[j-1];j--;}
            keys[j]=key;
        }
        u8 last=79;
        for(u8 i=0;i<distinct;i++){
            u8 head=lastOfKey[keys[i]];
            masked[last].link=head;
            if(head==4)zeroPredecessor=last;
            last=firstOfKey[keys[i]];
        }
        masked[last].link=0;
        /* Keep the HUD tail's link constant. Exchange the sorted gameplay
         * head with entry four and repair its single incoming edge, removing
         * the extra per-frame DMA command for the old dynamic tail link. */
        if(count){
            u8 first=lastOfKey[keys[0]];
            if(first!=4){
                RT_SpriteCommand saved=masked[4];
                masked[4]=masked[first];masked[first]=saved;
                masked[zeroPredecessor==first?4:zeroPredecessor].link=first;
            }
        }else{masked[4]=(RT_SpriteCommand){0,0,0,0,128};count=1;}
        if(!prepareTransfer(prepareTransfers?DMA_QUEUE:DMA_QUEUE_COPY,DMA_VRAM,masked,0xf400,(count+4)*4,2)){
            batch->failed=1;return 0;
        }
        nextPublication^=1;
        batch->queuedBytes+=(count+4)*8;return 1;
    }
    if(!prepareTransfer(DMA_QUEUE_COPY,DMA_VRAM,batch->commands,0xf400,count*4,2)){
        batch->failed=1;return 0;
    }
    batch->queuedBytes+=count*8;
    return 1;
}

u8 RT_spriteSubmitPacked(RT_SpriteBatch *batch,RT_SpriteSlot *slot,
    const RT_SpriteInstance *instance,const RT_WorldCamera *camera,
    const RT_PackedSpritePiece *pieces)
{
    if(!batch || batch->failed)return 0;
    if(!slot || !instance || !camera || !pieces){batch->failed=1;return 0;}
    if(!instance->visible)return 1;
    const RT_SpritePose *pose=instance->pose;
    if(!pose || !pose->patterns || !pose->pieceCount || !pose->tileCount ||
       pose->tileCount>slot->capacity || pose->pieceCount>80-batch->reserved-batch->count ||
       instance->facingLeft>1 || instance->priority>7 || instance->poolRank>3){batch->failed=1;return 0;}
    if(!RT_spritePixelsMatch(slot->pose,pose)){
        if(!prepareTransfer(DMA_QUEUE,DMA_VRAM,(void*)pose->patterns,slot->firstTile*32,
            pose->tileCount*16,2)){batch->failed=1;return 0;}
        slot->pose=pose;batch->queuedBytes+=pose->tileCount*32;
    }
    u16 x=(u16)(instance->x-camera->x),y=(u16)(instance->y-camera->y);
    u16 count=batch->count,base=slot->firstTile;
    u8 key=(instance->priority<<2)|instance->poolRank;
    u8 priority=instance->priority;
    RT_SpriteCommand *command=batch->commands+count;u8 *order=batch->orderKeys+count;
    const RT_PackedSpritePiece *piece=pieces+(instance->facingLeft?pose->pieceCount:0);
    u16 start=count;
#ifdef __m68k__
    count=RT_emitPackedFast(command,order,piece,count,pose->pieceCount,x,y,base,priority,key);
#else
    for(u16 remaining=pose->pieceCount;remaining;remaining--,piece++){
        if((u16)(x+piece->clipX)>=352 || (u16)(piece->clipY-y)>=256)continue;
        command->y=((y+piece->y)&255)+112;
        command->x=((x+piece->x)&511)+128;
        command->attribute=(piece->attribute+base)|((piece->priority|priority)>=3?0x8000:0);
        command->size=piece->size;command->link=(u8)(++count+2);
        *order++=key;command++;
    }
#endif
    batch->count=count;RT_spritesMarkRun(batch,start,key);return 1;
}

u8 RT_spriteSubmitStageCached(RT_SpriteBatch *batch,RT_SpriteSlot *slot,
    const RT_SpriteInstance *instance,const RT_WorldCamera *camera)
{
    if(!batch || batch->failed)return 0;
    if(!slot || !instance || !camera){batch->failed=1;return 0;}
    if(!instance->visible)return 1;
    const RT_SpritePose *pose=instance->pose;
    if(!pose || !pose->patterns || !pose->pieces || !pose->pieceCount ||
       !pose->tileCount || pose->tileCount>slot->capacity ||
       pose->pieceCount>80-batch->reserved-batch->count || instance->facingLeft>1 ||
       instance->priority>7 || instance->poolRank>3){batch->failed=1;return 0;}
    if(!RT_spritePixelsMatch(slot->pose,pose)){
        if(!prepareTransfer(DMA_QUEUE,DMA_VRAM,(void*)pose->patterns,slot->firstTile*32,
            pose->tileCount*16,2)){batch->failed=1;return 0;}
        slot->pose=pose;batch->queuedBytes+=pose->tileCount*32;
    }
    return RT_spriteSubmitStage(batch,slot->firstTile,instance,camera);
}

/* Resident authored stage components use rule3 and a single facing. */
u8 RT_spriteSubmitStage(RT_SpriteBatch *batch,u16 firstTile,
    const RT_SpriteInstance *instance,const RT_WorldCamera *camera)
{
    if(!batch || batch->failed)return 0;
    if(!instance || !camera || !instance->pose || firstTile>=1536){batch->failed=1;return 0;}
    const RT_SpritePose *pose=instance->pose;
    if(pose->tileCount>1536-firstTile){batch->failed=1;return 0;}
    if(!instance->visible)return 1;
    if(!pose->patterns || !pose->pieces || !pose->pieceCount || !pose->tileCount ||
       pose->pieceCount>80-batch->reserved-batch->count || instance->facingLeft>1 ||
       instance->priority>7 || instance->poolRank>3){batch->failed=1;return 0;}
    u16 x=(u16)(instance->x-camera->x),y=(u16)(instance->y-camera->y);
    u16 count=batch->count;
    u8 key=(instance->priority<<2)|instance->poolRank;
    RT_SpriteCommand *command=batch->commands+count;u8 *order=batch->orderKeys+count;
    const RT_SpritePiece *piece=pose->pieces;
#ifdef __m68k__
    batch->count=RT_emitPiecesCall(1,command,order,piece,count,pose->pieceCount,x,y,firstTile,
        instance->priority,key,0);
    RT_spritesMarkRun(batch,count,key);
    return 1;
#endif
    u16 start=count;
    for(u16 remaining=pose->pieceCount;remaining;remaining--,piece++){
        if((u16)(x+piece->clipX[0]+31)>=319 ||
           (u16)(287-y+piece->clipY)>=255)continue;
        command->y=((y+piece->y-16)&255)+112;
        command->x=((x+piece->x[0]+16)&511)+128;
        command->attribute=(piece->attribute[0]+firstTile+piece->tile)|
            (((piece->priority|instance->priority)>=3)?0x8000:0);
        command->size=piece->size;command->link=(u8)(++count+2);
        *order++=key;command++;
    }
    batch->count=count;RT_spritesMarkRun(batch,start,key);return 1;
}
