#include <genesis.h>
extern u32 rtSpriteBirthEpoch;
#include "rt_projectiles.h"

u8 RT_projectilesRequest(RT_ProjectilePool *pool,RT_Agent *agent,u8 owner,RT_WeaponEvent *event)
{
    u8 request=agent->shotRequest;agent->shotRequest=0;event->sound=event->command=0;
    if(!(request&RT_SHOT_PENDING))return 0;
    u8 alternate=!!(request&RT_SHOT_ALTERNATE),slot;
    if(!alternate && !agent->ammoPrimary){event->sound=0x2b;return 0;}
    for(slot=0;slot<RT_PROJECTILE_CAPACITY-1;slot++)
        if(!pool->slots[slot].kind && !pool->slots[slot+1].kind)break;
    if(slot==RT_PROJECTILE_CAPACITY-1){event->sound=0x2b;return 0;}
    u8 last=!alternate && agent->ammoPrimary==1;
    u8 index=(request&RT_SHOT_KNEEL?6:0)+(alternate?4:0)+
        (last?2+!!(request&RT_SHOT_PRIOR_LEFT):!!(request&RT_SHOT_LEFT));
    const RT_ShotTemplate *row=rtShotTemplates+index;
    RT_Projectile *shot=pool->slots+slot,*flash=shot+1;
    rtSpriteBirthEpoch++;
    *shot=(RT_Projectile){agent->body.x+row->x,agent->body.y+row->y,row->velocityX,
        RT_PROJECTILE_NEW_SHOT,0,0,row->pose,row->flags,owner,agent->body.layer,0};
    *flash=(RT_Projectile){agent->body.x+row->flashX,agent->body.y+row->flashY,0,
        RT_PROJECTILE_NEW_FLASH,0,0,row->flashPose,row->flags&RT_PROJECTILE_LEFT,owner,flash->layer,0};
    u16 *ammo=alternate?&agent->ammoAlternate:&agent->ammoPrimary;
    *ammo=*ammo?*ammo-1:9999;pool->active+=2;
    if(last)event->command=1;else event->sound=alternate?7:5;
    return 1;
}
static void returnRound(const RT_Projectile *shot,RT_Agent *agents,u8 players)
{
    if((shot->flags&RT_PROJECTILE_LAST) && shot->owner<players && !agents[shot->owner].ammoPrimary)
        agents[shot->owner].ammoPrimary=1;
}
static void removeShot(RT_ProjectilePool *pool,RT_Projectile *shot)
{
    shot->kind=RT_PROJECTILE_FREE;pool->active--;
}
void RT_projectileStepWithTerrain(RT_ProjectilePool *pool,u8 slot,RT_Agent *agents,u8 players,
    const RT_WorldStage *stage,const RT_WorldCamera *camera,RT_ProjectileHit hit,void *context,
    RT_ProjectileTerrain terrain,void *terrainContext)
{
    if(!pool || slot>=RT_PROJECTILE_CAPACITY)return;
        RT_Projectile *shot=pool->slots+slot;
        u8 left=!!(shot->flags&RT_PROJECTILE_LEFT);
        switch(shot->kind){
        case RT_PROJECTILE_NEW_SHOT:shot->kind=RT_PROJECTILE_SHOT;break;
        case RT_PROJECTILE_NEW_FLASH:shot->kind=RT_PROJECTILE_FLASH;break;
        case RT_PROJECTILE_FLASH:
            if(shot->phase>=2)removeShot(pool,shot);
            else shot->pose=rtShotFlashPoses[left][shot->phase++];
            break;
        case RT_PROJECTILE_IMPACT:
            if(!--shot->timer)removeShot(pool,shot);
            break;
        case RT_PROJECTILE_SHOT:{
            s32 next=shot->x+shot->velocityX;
            s16 local=(s16)(next-(s32)camera->x*16);
            if(shot->velocityX<0?local<-256:local>=4608){
                returnRound(shot,agents,players);removeShot(pool,shot);break;
            }
            shot->x=next;
            RT_MotionBody anchor;anchor.x=shot->x;anchor.y=shot->y;
            u8 blocked=terrain?terrain(shot,terrainContext):
                RT_motionProbe(stage,&anchor,camera,1,-1)->collision[shot->layer>>1]&1;
            u8 struck=!blocked && hit && hit(shot,context);
            if(blocked || struck){
                returnRound(shot,agents,players);
                if(struck)shot->x+=left?-128:128;
                shot->kind=RT_PROJECTILE_IMPACT;shot->timer=6;shot->pose=rtShotImpactPoses[left];
            }else{
                shot->timer=(shot->timer+1)&7;
                if(!shot->timer){shot->phase++;if(shot->phase==3)shot->phase=0;shot->pose=(shot->flags&RT_PROJECTILE_ENEMY?rtEnemyShotCyclePoses:rtShotCyclePoses)[left][shot->phase];}
            }
            break;
        }
        default:break;
        }
}
void RT_projectilesStepWithTerrain(RT_ProjectilePool *pool,RT_Agent *agents,u8 players,
    const RT_WorldStage *stage,const RT_WorldCamera *camera,RT_ProjectileHit hit,void *context,
    RT_ProjectileTerrain terrain,void *terrainContext)
{
    if(!pool->active)return;
    for(u8 slot=0;slot<RT_PROJECTILE_CAPACITY;slot++)
        RT_projectileStepWithTerrain(pool,slot,agents,players,stage,camera,hit,context,terrain,terrainContext);
}

void RT_projectilesStep(RT_ProjectilePool *pool,RT_Agent *agents,u8 players,
    const RT_WorldStage *stage,const RT_WorldCamera *camera,RT_ProjectileHit hit,void *context)
{
    RT_projectilesStepWithTerrain(pool,agents,players,stage,camera,hit,context,0,0);
}
