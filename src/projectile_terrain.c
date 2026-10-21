#include "rt_projectiles.h"
static u8 publishedBlocked(const RT_Projectile *shot,void *context)
{
    const RT_TerrainMaterial *material=RT_terrainWindowProbe(context,shot->x,shot->y,1,-1);
    return material->collision[shot->layer>>1]&1;
}
u8 RT_projectilesStepPublished(RT_ProjectilePool *pool,RT_Agent *agents,u8 players,
    const RT_WorldStage *stage,const RT_WorldCamera *camera,const RT_TerrainWindow *window,
    RT_ProjectileHit hit,void *context)
{
    if(!pool || !agents || !stage || !camera || !window || !window->ready ||
       window->camera.x!=camera->x || window->camera.y!=camera->y || players>2)return 0;
    RT_projectilesStepWithTerrain(pool,agents,players,stage,camera,hit,context,
        publishedBlocked,(void*)window);return 1;
}
