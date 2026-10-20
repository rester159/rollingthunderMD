#include "rt_sprite_arena.h"
#define NONE 255
/* Pose assigned to each slot by the planner. RT_spriteSubmit publishes the
 * same pointer into slot.pose once its upload is queued. */
static const RT_SpritePose *planned[RT_SPRITE_ARENA_BLOCKS];
static inline u8 hashPose(const RT_SpritePose *pose){return ((u32)pose->patterns>>2)&31;}
u8 RT_spriteArenaInitialize(RT_SpriteArena *a,u16 first,u8 blocks)
{
    if(!a || !blocks || blocks>RT_SPRITE_ARENA_BLOCKS || first>=1536 ||
       (u16)blocks*16>1536-first)return 0;
    a->firstTile=first;a->blocks=blocks;a->stamp=1;a->cursor=0;
    for(u8 i=0;i<RT_SPRITE_ARENA_BLOCKS;i++){
        a->slots[i]=(RT_SpriteSlot){0,first,0};
        a->owner[i]=a->chain[i]=NONE;a->used[i]=0;planned[i]=0;
    }
    for(u8 i=0;i<32;i++)a->heads[i]=NONE;
    for(u8 i=0;i<80;i++)a->hints[i]=NONE;
    return 1;
}
static u8 lookup(const RT_SpriteArena *a,const RT_SpritePose *pose)
{
    u8 s=a->heads[hashPose(pose)];
    while(s!=NONE && !RT_spritePixelsMatch(planned[s],pose))s=a->chain[s];
    return s;
}
/* Drop a slot's range, residency and hash entry. */
static void release(RT_SpriteArena *a,u8 slot)
{
    RT_SpriteSlot *s=a->slots+slot;
    if(planned[slot]){
        u8 *link=a->heads+hashPose(planned[slot]);
        while(*link!=slot)link=a->chain+*link;
        *link=a->chain[slot];planned[slot]=0;
    }
    if(s->capacity){
        u8 start=(s->firstTile-a->firstTile)>>4,end=start+(s->capacity>>4);
        for(u8 b=start;b<end;b++)if(a->owner[b]==slot)a->owner[b]=NONE;
    }
    s->pose=0;s->capacity=0;
}
static void assign(RT_SpriteArena *a,u8 slot,const RT_SpritePose *pose,u8 start,u8 size)
{
    for(u8 b=start;b<start+size;b++)a->owner[b]=slot;
    a->slots[slot]=(RT_SpriteSlot){0,a->firstTile+(u16)start*16,(u16)size*16};
    planned[slot]=pose;
    u8 h=hashPose(pose);a->chain[slot]=a->heads[h];a->heads[h]=slot;
}
#ifdef __m68k__
/* Keep the allocator's temporary arrays and register pressure out of the
 * per-frame actor traversal. Planning still sees the complete visible set. */
__attribute__((noinline))
#endif
u8 RT_spriteArenaPlan(RT_SpriteArena *a,const RT_SpritePose *const *poses,u8 count,u8 *slots)
{
    if(!a || !a->blocks || a->blocks>RT_SPRITE_ARENA_BLOCKS || count>80 ||
       (count && (!poses || !slots)))return 0;
    u8 stamp=++a->stamp;
    if(!stamp){for(u8 s=0;s<RT_SPRITE_ARENA_BLOCKS;s++)a->used[s]=0;stamp=a->stamp=1;}
    /* Pass 1: resident poses. A hint hit needs no validation: only validated
     * poses are ever planned. Count each slot's blocks once. */
    const RT_SpritePose *fresh[RT_SPRITE_ARENA_BLOCKS];u8 freshSize[RT_SPRITE_ARENA_BLOCKS];
    u8 freshSlot[RT_SPRITE_ARENA_BLOCKS],pending[80];
    u8 need=0,unique=0,freshCount=0,pendingCount=0;
    for(u8 i=0;i<count;i++){
        const RT_SpritePose *pose=poses[i];
        u8 s=a->hints[i];
        if(s==NONE || !RT_spritePixelsMatch(planned[s],pose)){
            if(!pose || !pose->patterns || !pose->tileCount || pose->tileCount>64)return 0;
            s=lookup(a,pose);
            if(s==NONE){
                u8 j=0;for(;j<freshCount;j++)if(RT_spritePixelsMatch(fresh[j],pose))break;
                if(j==freshCount){
                    u8 size=(pose->tileCount+15)>>4;
                    if(unique==RT_SPRITE_ARENA_BLOCKS || need+size>a->blocks)return 0;
                    unique++;need+=size;fresh[j]=pose;freshSize[j]=size;freshCount++;
                }
                pending[pendingCount++]=i;slots[i]=j;continue;
            }
            a->hints[i]=s;
        }
        if(a->used[s]!=stamp){
            u8 size=a->slots[s].capacity>>4;
            if(unique==RT_SPRITE_ARENA_BLOCKS || need+size>a->blocks)return 0;
            a->used[s]=stamp;unique++;need+=size;
        }
        slots[i]=s;
    }
    if(!freshCount)return 1;
    /* Pass 2: allocate new poses. Pinned blocks belong to slots used by this plan. */
    for(u8 j=0;j<freshCount;j++){
        /* FIFO ring: continue from the last allocation, skipping blocks
         * pinned by this plan, so the oldest cached art is evicted first. */
        u8 size=freshSize[j],best=NONE,start=a->cursor,scanned=0;
        while(scanned<a->blocks+size){
            if(start+size>a->blocks){scanned+=a->blocks-start;start=0;continue;}
            u8 b=0;
            for(;b<size;b++){
                u8 o=a->owner[start+b];
                if(o!=NONE && a->used[o]==stamp)break;
            }
            if(b==size){best=start;break;}
            start+=b+1;scanned+=b+1;
        }
        if(best==NONE){freshCount=j;goto compact;}
        u8 slot=NONE;
        for(u8 s=0;s<RT_SPRITE_ARENA_BLOCKS;s++)if(a->used[s]!=stamp){
            if(!planned[s] && !a->slots[s].capacity){slot=s;break;}
            if(slot==NONE)slot=s;
        }
        release(a,slot);
        for(u8 b=best;b<best+size;b++)if(a->owner[b]!=NONE)release(a,a->owner[b]);
        assign(a,slot,fresh[j],best,size);a->cursor=best+size>=a->blocks?0:best+size;
        a->used[slot]=stamp;freshSlot[j]=slot;
    }
    for(u8 k=0;k<pendingCount;k++){
        u8 i=pending[k];
        slots[i]=a->hints[i]=freshSlot[slots[i]];
    }
    return 1;
compact:
    {
        /* Fragmented: lay the complete set out contiguously. A survivor that
         * keeps its exact range stays resident; everything else reuploads. */
        const RT_SpritePose *set[RT_SPRITE_ARENA_BLOCKS];u8 sizes[RT_SPRITE_ARENA_BLOCKS];
        u8 total=0,first=0,keep[RT_SPRITE_ARENA_BLOCKS];
        for(u8 i=0;i<count;i++){
            const RT_SpritePose *pose=poses[i];
            u8 j=0;for(;j<total;j++)if(RT_spritePixelsMatch(set[j],pose))break;
            if(j==total){set[j]=pose;sizes[j]=(pose->tileCount+15)>>4;total++;}
        }
        for(u8 s=0;s<RT_SPRITE_ARENA_BLOCKS;s++)keep[s]=NONE;
        for(u8 j=0;j<total;j++){
            u8 s=lookup(a,set[j]);
            if(s!=NONE && RT_spritePixelsMatch(a->slots[s].pose,set[j]) &&
               a->slots[s].firstTile==a->firstTile+(u16)first*16)keep[s]=j;
            first+=sizes[j];
        }
        for(u8 s=0;s<RT_SPRITE_ARENA_BLOCKS;s++){
            if(keep[s]==NONE)release(a,s);
            a->used[s]=keep[s]==NONE?0:stamp;
        }
        first=0;
        for(u8 j=0;j<total;j++){
            u8 s=lookup(a,set[j]);
            if(s==NONE){
                for(s=0;a->used[s]==stamp;s++);
                a->used[s]=stamp;assign(a,s,set[j],first,sizes[j]);
            }
            first+=sizes[j];
        }
        a->cursor=first>=a->blocks?0:first;
        for(u8 i=0;i<count;i++)slots[i]=a->hints[i]=lookup(a,poses[i]);
        return 1;
    }
}
