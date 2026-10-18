#include "rt_high_visibility.h"
#include "rt_type20.h"
#include "rt_type24.h"
#include "rt_type28.h"
#include "rt_type2c.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static u32 seed=123,selected;
static u32 rng(void){seed=seed*1664525+1013904223;return seed;}
RT_Type20EntryResult RT_type20SelectInContext(RT_Enemy *e,u8 a,const RT_Type20Motion *c){selected++;return RT_TYPE20_ENTRY_INSTALLED;}
RT_Type24EntryResult RT_type24Select(RT_Enemy *e,u8 a,const RT_Type24EntryContext *c){selected++;return RT_TYPE24_ENTRY_INSTALLED;}
RT_Type28EntryResult RT_type28Select(RT_Enemy *e,u8 a,const RT_Type28EntryContext *c){selected++;return RT_TYPE28_ENTRY_INSTALLED;}
RT_Type2cEntryResult RT_type2cSelect(RT_Enemy *e,u8 a,const RT_Type2cEntryContext *c){selected++;return RT_TYPE2C_ENTRY_INSTALLED;}
int main(void)
{
    const s16 edge[]={-32768,-1025,-1024,-769,-768,-767,-1,0,4095,4096,5375,5376,5377,32767};
    u32 skipped=0;
    for(u32 i=0;i<1000000;i++){
        RT_WorldCamera camera={(s16)rng(),(s16)rng()};
        s16 x=i%3?(s16)rng():edge[(i/3)%14],y=i%5?(s16)rng():edge[(i/5)%14];
        RT_Enemy e={0};e.state.kind=32+4*(i%4);e.state.hidden=(i>>2)&1;
        e.state.action=i%2?0x80+(i&3):rng();e.state.spawnAction=rng();
        e.state.x=(s32)camera.x*16+x;e.state.y=((s32)camera.y+256)*16-y;
        RT_Enemy input=e,old=e;selected=0;u8 counted=255;RT_EnemyResult result;
        switch(e.state.kind){
        case 32:{RT_Type20Motion c={.camera=&camera};result=RT_type20Prepare(&old,&c,&counted);break;}
        case 36:{RT_Type24MotionContext c={.camera=&camera};result=RT_type24Prepare(&old,&c,&counted);break;}
        case 40:{RT_Type28MotionContext c={.camera=&camera};result=RT_type28Prepare(&old,&c,&counted);break;}
        default:{RT_Type2cEntryContext c={.camera=&camera};result=RT_type2cPrepare(&old,&c,&counted);break;}
        }
        if(RT_highEnemyDormant(&e,(u16)camera.x*16,((u16)camera.y+256)*16)){
            assert(result==RT_ENEMY_CONTINUE && !selected && !counted && old.state.hidden);
            assert(!memcmp(&e,&old,sizeof e));skipped++;
        }else assert(!memcmp(&e,&input,sizeof e));
    }
    printf("1000000 exact visibility gates; %u dormant actors\n",skipped);
    return 0;
}
