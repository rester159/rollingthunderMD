#include <genesis.h>
#include "../src/stage_pool.c"
#include <assert.h>
#include <stdio.h>
u32 rtSpriteBirthEpoch;
static u32 seed=0x52545649;
static u32 randomValue(void){seed=seed*1664525+1013904223;return seed;}
int main(void)
{
    RT_StagePool pool={0};
    for(u32 trial=0;trial<100000;trial++){
        RT_WorldCamera camera={0};camera.x=randomValue();camera.y=randomValue();
        s32 left=(s32)camera.x*16-0x200,right=(s32)camera.x*16+0x1400;
        s32 origin=((s32)camera.y+256)*16;u8 expected[48],count=0;
        pool.count=0;
        for(u8 i=0;i<48;i++){
            RT_Anchor *a=pool.objects+i;
            a->kind=(randomValue()>>24)&31;if((randomValue()&7)==0)a->kind=255;
            a->x=randomValue();a->y=randomValue();
            if(i<24){a->x=left+(s32)(randomValue()%0x1800)-0x100;a->y=origin+(s32)(randomValue()%0x2000)-0x1800;}
            s32 top=origin-(a->kind<8?0xe00:0xf00),bottom=origin+(a->kind<8?-0x300:0x100);
            if(i>=24 && i<36){a->x=(i&1?left:right)+((i%3)-1);a->y=top+1;}
            if(i>=36){a->x=left;a->y=(i&1?top:bottom)+((i%3)-1);}
            a->visible=77;
            expected[i]=a->kind==255?77:a->x>=left && a->x<right && a->y>top && a->y<=bottom;
            if(a->kind!=255){pool.count++;count+=expected[i];}
        }
        assert(RT_stagePoolRefreshVisibility(&pool,&camera)==count);
        for(u8 i=0;i<48;i++)assert(pool.objects[i].visible==expected[i]);
    }
    puts("4.8 million visibility decisions match signed bounds, including every band edge.");
}
