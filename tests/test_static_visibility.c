#include <genesis.h>
#include "../src/stage_pool.c"
#include <assert.h>
#include <stdio.h>
u32 rtSpriteBirthEpoch;
static u32 seed=17;
static u32 rng(void){seed=seed*1664525+1013904223;return seed;}
int main(void){
    RT_StagePool cached={0},reference={0};
    for(u32 trial=0;trial<4000;trial++){
        RT_stagePoolInvalidateVisibility(&cached);cached.count=0;
        RT_WorldCamera camera={(s16)rng(),(s16)rng()};
        for(u8 i=0;i<48;i++){
            RT_Anchor *a=cached.objects+i;
            a->kind=i%9==0?255:rng()%32;
            a->x=((s32)camera.x+(s32)(rng()%512)-64)*16;
            a->y=((s32)camera.y+(s32)(rng()%512)-64)*16;
            if(trial%3){a->x&=~127;a->y&=~127;}
            a->visible=77;cached.count+=a->kind!=255;
        }
        for(u16 step=0;step<128;step++){
            camera.x+=(step%32<16?1:-1);camera.y+=(step%64<32?1:-1);
            if(step==64){
                RT_stagePoolInvalidateVisibility(&cached);
                for(u8 i=0;i<48;i++)cached.objects[i].x-=8192;
                camera.x-=512;
            }
            if(step==32){
                RT_stagePoolInvalidateVisibility(&cached);
                cached.objects[0]=(RT_Anchor){.x=(s32)camera.x*16,.y=((s32)camera.y+128)*16,.kind=8};
                cached.count++;
            }
            reference=cached;
            u8 expected=RT_stagePoolRefreshVisibility(&reference,&camera);
            assert(RT_stagePoolRefreshStaticVisibility(&cached,&camera)==expected);
            assert(!memcmp(cached.objects,reference.objects,sizeof cached.objects));
            assert(RT_stagePoolRefreshStaticVisibility(&cached,&camera)==expected);
        }
    }
    puts("512,000 cached visibility steps match full refresh, including aligned/unaligned anchors, boundaries, admission and rebasing.");
}
