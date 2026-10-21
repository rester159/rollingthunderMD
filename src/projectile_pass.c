#include "rt_enemy_throw.h"
static u8 bulletTerrain(const RT_Projectile *shot,void *context)
{
    const RT_TerrainMaterial *m=RT_terrainWindowProbe(context,shot->x,shot->y,1,-1);
    /* CPU2 D426 masks D455's material flags with 01. Floor-change and
     * stair markers must not become invisible bullet walls. */
    return m->collision[shot->layer>>1]&1;
}
u8 RT_projectilesUpdateAll(RT_ProjectilePool *pool,const RT_ProjectilePassContext *c,
    RT_ProjectilePassResult *result)
{
    if(!result)return 0;
    result->soundCount=result->visible=result->removed=0;result->faultSlot=255;
    if(!pool || !c || !c->stage || !c->camera || !c->motion || !c->scene.terrain ||
       !c->scene.terrain->ready || c->scene.terrain->camera.x!=c->camera->x ||
       c->scene.terrain->camera.y!=c->camera->y || !c->scene.players.agents ||
       !c->scene.players.combat || !c->scene.players.players || c->scene.players.players>2)return 0;
    u8 remaining=pool->active;
    RT_ThrowSceneContext scene=c->scene;
    for(u8 slot=0;slot<RT_PROJECTILE_CAPACITY && remaining;slot++){
        RT_Projectile *o=pool->slots+slot;if(!o->kind)continue;
        remaining--;result->visible++;
        if(o->kind<=RT_PROJECTILE_IMPACT){
            RT_projectileStepWithTerrain(pool,slot,scene.players.agents,scene.players.players,
                c->stage,c->camera,c->bulletHit,c->bulletContext,bulletTerrain,(void*)scene.terrain);
        }else{
            RT_WeaponEvent event={0};
            RT_EnemyResult status=RT_throwMotionStep(pool,slot,c->motion,c->camera,
                RT_throwTerrainProbe,RT_throwSceneHit,&scene,&event);
            if(status==RT_ENEMY_UNAVAILABLE){result->faultSlot=slot;return 0;}
            if(event.sound)result->sounds[result->soundCount++]=(RT_ProjectileSound){slot,event.sound};
        }
        if(!o->kind){result->visible--;result->removed++;}
    }
    return remaining==0;
}
