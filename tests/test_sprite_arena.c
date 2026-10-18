#include <genesis.h>
#include "../src/sprite_arena.c"
#include <assert.h>
#include <stdio.h>
static u32 seed=77;
static u32 rng(void){seed=seed*1664525+1013904223;return seed;}
int main(void){
    RT_SpriteArena arena;RT_SpritePose poses[256];u16 image[1536];u32 patterns[128];
    for(u16 i=0;i<256;i++)poses[i]=(RT_SpritePose){patterns+i/2,0,1+(i/2*17)%64,1,0};
    for(u8 blocks=16;blocks<=45;blocks++){
        assert(RT_spriteArenaInitialize(&arena,512,blocks));memset(image,0,sizeof image);
        for(u16 frame=0;frame<2000;frame++){
            const RT_SpritePose *wanted[12];u8 slots[12],count=0,need=0,ids[12];
            for(u8 n=0;n<12;n++){
                u8 id=n<4?(frame/4+n)%12:rng()%256,size=(poses[id].tileCount+15)/16;
                u8 duplicate=0;for(u8 k=0;k<count;k++)if(ids[k]==id)duplicate=1;
                if(!duplicate && need+size>blocks)continue;
                if(!duplicate)need+=size;
                ids[count]=id;wanted[count++]=poses+id;
            }
            assert(RT_spriteArenaPlan(&arena,wanted,count,slots));
            for(u8 i=0;i<count;i++){
                RT_SpriteSlot *slot=arena.slots+slots[i];
                assert(slot->firstTile>=512 && slot->firstTile+slot->capacity<=512+blocks*16);
                assert(slot->capacity>=wanted[i]->tileCount);
                if(!RT_spritePixelsMatch(slot->pose,wanted[i])){
                    for(u16 t=0;t<wanted[i]->tileCount;t++)image[slot->firstTile+t]=ids[i]/2+1;
                    slot->pose=wanted[i];
                }
            }
            for(u8 i=0;i<count;i++)for(u16 t=0;t<wanted[i]->tileCount;t++)
                assert(image[arena.slots[slots[i]].firstTile+t]==ids[i]/2+1);
        }
    }
    puts("60,000 arena plans preserve every requested pose across eviction, reuse, compaction and timestamp wrap.");
}
