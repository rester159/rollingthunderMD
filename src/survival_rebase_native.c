#include "rt_survival_rebase_native.h"
static u8 subtractable(s32 x){return x>=(-2147483647L-1+8192);}
u8 RT_survivalSceneCanShift512(const RT_CampaignScene *s)
{
    if(!s || !s->ready || !s->terrain.ready || s->camera.x<512 ||
       s->terrain.camera.x!=s->camera.x || s->terrain.camera.y!=s->camera.y ||
       s->objectWindow.gridX<2 || s->enemyWindow.gridX<2 ||
       s->players.selection.mode!=RT_MODE_HOME || s->players.selection.game!=RT_HOME_SURVIVAL ||
       (s->players.home.players!=1 && s->players.home.players!=2) || s->exiting || s->bossProgress)return 0;
    for(u8 p=0;p<s->players.home.players;p++)
        if(((s->players.home.players==1 || !s->players.combat[p].removed) &&
            !subtractable(s->players.agents[p].body.x)) || !subtractable(s->players.home.spawn[p].body.x))return 0;
    u8 objects=0,enemies=0,shots=0;
    for(u8 i=0;i<RT_NATIVE_STAGE_CAPACITY;i++)if(s->objects.objects[i].kind!=255){
        if(!subtractable(s->objects.objects[i].x))return 0;
        objects++;
    }
    for(u8 i=0;i<RT_ENEMY_CAPACITY;i++)if(s->enemies.slots[i].state.kind!=RT_ENEMY_FREE){
        if(!subtractable(s->enemies.slots[i].state.x))return 0;
        enemies++;
    }
    for(u8 i=0;i<RT_PROJECTILE_CAPACITY;i++)if(s->projectiles.slots[i].kind){
        if(!subtractable(s->projectiles.slots[i].x))return 0;
        shots++;
    }
    return objects==s->objects.count && enemies==s->enemies.count && shots==s->projectiles.active;
}
u8 RT_survivalSceneShift512(RT_CampaignScene *s)
{
    if(!RT_survivalSceneCanShift512(s))return 0;
    for(u8 p=0;p<s->players.home.players;p++){
        if(s->players.home.players==1 || !s->players.combat[p].removed)s->players.agents[p].body.x-=8192;
        s->players.home.spawn[p].body.x-=8192;
    }
    RT_stagePoolInvalidateVisibility(&s->objects);
    for(u8 i=0;i<RT_NATIVE_STAGE_CAPACITY;i++)if(s->objects.objects[i].kind!=255)s->objects.objects[i].x-=8192;
    for(u8 i=0;i<RT_ENEMY_CAPACITY;i++)if(s->enemies.slots[i].state.kind!=RT_ENEMY_FREE)s->enemies.slots[i].state.x-=8192;
    for(u8 i=0;i<RT_PROJECTILE_CAPACITY;i++)if(s->projectiles.slots[i].kind)s->projectiles.slots[i].x-=8192;
    s->camera.x-=512;s->terrain.camera.x-=512;
    s->objectWindow.gridX-=2;s->enemyWindow.gridX-=2;
    return 1;
}
