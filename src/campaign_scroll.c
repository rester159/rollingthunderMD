#include "rt_campaign_scene.h"
u8 RT_campaignScrollPrepared(RT_CampaignScene *scene,const RT_WorldCamera *previous,u8 visibilityReady)
{
    if(!scene || !scene->ready || !previous)return 0;
    RT_EventWindowRequest requests[2];
    u8 count=RT_eventWindowMove(&scene->enemyWindow,scene->stage,previous,&scene->camera,requests);
    if(count==255)return 0;
    u8 boundaries=0;
    for(u8 i=0;i<count;i++)boundaries|=1u<<(requests[i].edge-1);
    if(RT_enemiesCullWithProjectiles(&scene->enemies,&scene->objects,
        &scene->projectiles,&scene->camera,boundaries)==255)return 0;
    /* Reused slots must not inherit another actor's co-op attack phase. */
    if(count)for(u8 i=0;i<RT_ENEMY_CAPACITY;i++)
        if(scene->enemies.slots[i].state.kind==RT_ENEMY_FREE)scene->attackPhases[i]=0;
    for(u8 i=0;i<count;i++)
        if(!RT_enemiesSpawnWindowWithFamilies(&scene->enemies,scene->spawnLevel,requests+i,
            &scene->faultEvent,scene->players.home.players==2?&scene->population:NULL,
            scene->effects,15))return 0;
    if(!visibilityReady && RT_stagePoolRefreshStaticVisibility(&scene->objects,&scene->camera)==255)return 0;
    return RT_stagePoolAdvanceWindow(&scene->objects,&scene->objectWindow,
        scene->stage,previous,&scene->camera);
}

u8 RT_campaignScroll(RT_CampaignScene *scene,const RT_WorldCamera *previous)
{return RT_campaignScrollPrepared(scene,previous,0);}
