#include "rt_enemies.h"
#ifdef __m68k__
_Static_assert(sizeof(RT_Enemy)==36 && __builtin_offsetof(RT_Enemy,velocityX)==24 &&
    __builtin_offsetof(RT_Enemy,variation)==29 && __builtin_offsetof(RT_Enemy,byte12)==32 &&
    __builtin_offsetof(RT_Enemy,delay)==35 && sizeof(RT_EnemyEntry)==4,"grid admission ABI");
#else
/* Rare per-actor service: keep it out of the hot pool traversal. */
__attribute__((noinline,optimize("Os")))
u8 RT_enemyActivateGrid(RT_Enemy *enemy,const RT_WorldCamera *camera)
{
    if(!enemy || !camera || (enemy->state.action&0xfc)!=0x80)return 0;
    RT_DoorEnemy *a=&enemy->state;
    u8 selector=a->spawnAction,base=selector&0xfc;
    if((a->kind!=4 && a->kind!=8 && a->kind!=12) ||
        (base!=0 && base!=4 && base!=8 && base!=0x3c) ||
        (a->kind==12 && selector!=6))return 0;
    s16 x=(s16)(a->x-(s32)camera->x*16);
    s16 y=(s16)(((s32)camera->y+256)*16-a->y);
    if(y < -0x400 || y >= 0x1000)return 1;
    u8 entering=selector&2 ? x<0x1500 : x>=-0x300;
    if(!entering && !(selector&3))selector+=a->action&3;
    if(base==4){
        s16 speed=20+((enemy->variation&0x30)>>2);
        enemy->velocityX=selector&2?-speed:speed;
        enemy->velocityY=0;enemy->delay=selector&2?30:20;
    }else if(!entering && (base==8 || base==0x3c)){
        if(enemy->variation&3)enemy->delay=0;
        else{enemy->delay=2;enemy->byte12|=4;}
    }
    a->row=0;
    if(entering){
        const RT_EnemyEntry *entry=rtEnemyEntries+(base>>2);
        a->action=selector;a->timer=entry->timer;a->flags=entry->flags;
        a->pose=12+(base>>2);
    }else a->spawnAction=selector;
    return 1;
}

#endif

static u8 selectEntry(RT_Enemy *enemy,u8 selector)
{
    if(!enemy || (enemy->state.kind!=4 && enemy->state.kind!=8 && enemy->state.kind!=12))return 0;
    u8 state=selector&0xfc;
    if(state!=0x84 && state!=0x88 && state!=0xb4 && state!=0x90 && state!=0x98 && state!=0xb0 && state!=0xb8 && state!=0xbc && state!=0x40 && state!=0x9c && state!=0xa0 && state!=0xa4 && state!=0xa8 && state!=0xac && state!=0x0c && state!=0x94 && state!=0 && state!=0x10 && state!=0x14 && state!=0x18 && state!=0x1c && state!=4 && state!=8 && state!=0x38 && state!=0x3c && state!=0x48 && state!=0x54 && state!=0x4c && state!=0x50 && state!=0x28 && state!=0x2c && state!=0x34 && state!=0x20 && state!=0x24 && state!=0x30 && state!=0x44)return 0;
    if(!(selector&3))selector+=enemy->state.action&3;
    if(state==0x18 || state==0x1c || state==0xa0 || state==0xa4){
        if(state==0x1c || state==0xa4)enemy->byte12|=8;
        enemy->delay=enemy->traits&0x10?3:1;
    }else if(state==0x10)enemy->delay=2;
    else if(state==4 || state==0x40 || state==0x9c){
        s16 speed=20+((enemy->variation&0x30)>>2);
        enemy->velocityX=selector&2?-speed:speed;enemy->velocityY=0;
        enemy->delay=selector&2?30:20;
    }else if(state==0x20 || state==0x24 || state==0x30 || state==0x44){
        enemy->velocityY=state==0x30?-32:-112;
        if(state==0x20 || state==0x24)enemy->byte12&=0x7e;
        if(state==0x44)enemy->velocityX=selector&2?-48:48;
    }else if(state==0x28 || state==0x2c){
        enemy->velocityY=state==0x28?-160:0;
    }else if(state==0x4c || state==0x50){
        enemy->velocityX=selector&2?-16:16;
        if(state==0x50 || !(selector&2))enemy->velocityY=state==0x50?16:-16;
    }else if(state==8 || state==0x3c){
        if(enemy->variation&3)enemy->delay=0;
        else{enemy->delay=2;enemy->byte12|=4;}
    }
    const RT_EnemyEntry *entry=rtEnemyEntries+(state>>2);
    enemy->state.action=enemy->state.spawnAction=selector;
    enemy->state.row=0;enemy->state.timer=entry->timer;enemy->state.flags=entry->flags;
    enemy->state.pose=12+(state>>2);
    return 1;
}

u8 RT_enemySelectAfterDoor(RT_Enemy *enemy,u8 selector)
{
    /* Standing attacks need camera-relative offscreen redirection. */
    if((selector&0xfc)==0x18 || (selector&0xfc)==0xa0)return 0;
    return selectEntry(enemy,selector);
}
u8 RT_enemySelect(RT_Enemy *enemy,const RT_WorldCamera *camera,u8 selector,u8 workspaceFlags)
{
    if(!enemy || !camera)return 0;
    if(((selector&0xfc)==0x18 || (selector&0xfc)==0xa0) && enemy->state.x-(s32)camera->x*16>=0x1400)
        selector=!(enemy->state.decisions&1)?0x70:workspaceFlags&0x80?0:4;
    if((selector&0xfc)==0x74)
        selector=((s32)camera->y+256)*16-enemy->state.y<0x700?0x28:0x2c;
    if((selector&0xfc)==0x70){
        u8 flags=enemy->state.decisions;
        selector=flags&0x20?0x38:flags&1?4:flags&8?8:0x3c;
    }
    return selectEntry(enemy,selector);
}
