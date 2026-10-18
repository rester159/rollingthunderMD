#include "rt_survival_scene.h"
#include "rt_survival_attack.h"
#include <string.h>
static RT_CampaignInit populate(RT_CampaignScene *scene,const RT_SurvivalWorld *world,u8 entrance)
{
    for(u16 i=0;i<scene->spawnLevel->eventCount;i++){
        u16 definition=scene->spawnLevel->events[i].definition;
        if(definition>=rtGridEnemyTemplateCount)return RT_CAMPAIGN_INIT_ENEMIES;
        if(rtGridEnemyTemplates[definition].kind>=32)scene->hasHighEnemies=1;
    }
    for(u8 choice=0;choice<8;choice++){
        const RT_EnemyTemplate *definition=RT_doorEnemyTemplate(0,choice*4,0);
        if(!definition)return RT_CAMPAIGN_INIT_ENEMIES;
        if(definition->kind>=32)scene->hasHighEnemies=1;
    }
    RT_enemiesInitialize(&scene->enemies);
    if(!RT_stagePoolInitialize(&scene->objects,rtStageEventLevels) ||
       !RT_eventWindowInitialize(&scene->objectWindow,scene->stage,entrance))return RT_CAMPAIGN_INIT_OBJECTS;
    scene->enemyWindow=scene->objectWindow;
    if(!RT_terrainWindowInitialize(&scene->terrain,scene->stage,&scene->camera))return RT_CAMPAIGN_INIT_TERRAIN;
    RT_EventWindowRequest request={RT_EVENT_WINDOW_INITIAL,scene->objectWindow.gridX,scene->objectWindow.gridY};
    if(!RT_survivalSpawnObjects(world,&scene->objects,&request))return RT_CAMPAIGN_INIT_OBJECTS;
    if(!RT_survivalSpawnEnemies(world,&scene->enemies,scene->spawnLevel,&request,&scene->faultEvent,
        scene->players.home.players==2?&scene->population:0,scene->effects,15))return RT_CAMPAIGN_INIT_ENEMIES;
    /* No authored area countdown or exit gate in an endless session. */
    RT_survivalAttackConfigure(scene,world);
    scene->timer=(RT_CampaignTimer){0,0};scene->ready=1;return RT_CAMPAIGN_INIT_OK;
}
RT_CampaignInit RT_survivalSceneInitialize(RT_CampaignScene *scene,
    RT_SurvivalWindow *window,RT_SurvivalWorld *world,const RT_SessionConfig *selection,
    u32 seed,u8 enemySet,u16 credits)
{
    if(!scene || !window || !world || !selection || selection->mode!=RT_MODE_HOME ||
       selection->game!=RT_HOME_SURVIVAL || enemySet>1)return RT_CAMPAIGN_INIT_INVALID;
    if(!RT_prepareWorldBank(0,0))return RT_CAMPAIGN_INIT_TERRAIN;
    RT_SurvivalWindow next;RT_SurvivalWorld nextWorld;RT_ScenePlayers players;RT_WorldCamera camera;
    if(!RT_initSurvivalWindow(&next,seed) || !RT_survivalWorldBuild(&nextWorld,RT_worldStage(0,0),&next) ||
       !RT_scenePlayersInitialize(&players,selection,&nextWorld.stage,0,credits,&camera))return RT_CAMPAIGN_INIT_INVALID;
    *window=next;*world=nextWorld;world->stage.pages=world->pages;
    memset(scene,0,sizeof *scene);
    scene->stage=&world->stage;scene->spawnLevel=&rtEnemySpawnLevels[enemySet][0];
    scene->players=players;scene->camera=camera;scene->faultEvent=65535;
    return populate(scene,world,0);
}

RT_CampaignInit RT_survivalSceneContinue(RT_CampaignScene *scene,
    const RT_SurvivalWorld *world,u8 paidMask,const u16 pads[2])
{
    if(!scene || !world || !pads || !scene->ready || scene->stage!=&world->stage ||
       scene->players.selection.mode!=RT_MODE_HOME || scene->players.selection.game!=RT_HOME_SURVIVAL ||
       (scene->players.home.players!=1 && scene->players.home.players!=2) ||
       (paidMask!=1 && !(paidMask==2 && scene->players.home.players==2)) ||
       (scene->spawnLevel!=&rtEnemySpawnLevels[0][0] && scene->spawnLevel!=&rtEnemySpawnLevels[1][0]) ||
       world->stage.entrances[1].x<world->stage.cameraMinX ||
       world->stage.entrances[1].x>world->stage.cameraMaxX)return RT_CAMPAIGN_INIT_INVALID;
    for(u8 p=0;p<scene->players.home.players;p++){
        const RT_AgentCombat *combat=scene->players.combat+p;
        if(paidMask&(1<<p)){
            if(!combat->health || combat->removed || combat->invincibleTicks!=180)return RT_CAMPAIGN_INIT_INVALID;
        }else if(combat->health && !combat->removed)return RT_CAMPAIGN_INIT_INVALID;
    }
    RT_ScenePlayers players=scene->players;
    const RT_EnemySpawnLevel *spawns=scene->spawnLevel;
    for(u8 p=0;p<players.home.players;p++){
        players.previousButtons[p]=pads[p];
        if(!(paidMask&(1<<p))){players.combat[p].health=0;players.combat[p].removed=1;}
    }
    memset(scene,0,sizeof *scene);
    scene->stage=&world->stage;scene->spawnLevel=spawns;
    scene->players=players;scene->camera=world->stage.entrances[1];scene->faultEvent=65535;
    return populate(scene,world,1);
}
u8 RT_survivalSceneScrollPrepared(RT_CampaignScene *scene,const RT_SurvivalWorld *world,
    const RT_WorldCamera *previous,u8 visibilityReady)
{
    if(!scene || !scene->ready || !world || scene->stage!=&world->stage || !previous ||
       scene->players.selection.mode!=RT_MODE_HOME || scene->players.selection.game!=RT_HOME_SURVIVAL)return 0;
    RT_EventWindowRequest requests[2];
    u8 count=RT_eventWindowMove(&scene->enemyWindow,scene->stage,previous,&scene->camera,requests);
    if(count==255)return 0;
    u8 enemyMoved=count;
    u8 boundaries=0;
    for(u8 i=0;i<count;i++)boundaries|=1u<<(requests[i].edge-1);
    if(RT_enemiesCullWithProjectiles(&scene->enemies,&scene->objects,&scene->projectiles,&scene->camera,boundaries)==255)return 0;
    if(count)for(u8 i=0;i<RT_ENEMY_CAPACITY;i++)if(scene->enemies.slots[i].state.kind==RT_ENEMY_FREE)scene->attackPhases[i]=0;
    for(u8 i=0;i<count;i++)if(!RT_survivalSpawnEnemies(world,&scene->enemies,scene->spawnLevel,requests+i,
        &scene->faultEvent,scene->players.home.players==2?&scene->population:0,scene->effects,15))return 0;
    if(!visibilityReady && RT_stagePoolRefreshStaticVisibility(&scene->objects,&scene->camera)==255)return 0;
    count=RT_eventWindowMove(&scene->objectWindow,scene->stage,previous,&scene->camera,requests);
    if(count==255)return 0;
    boundaries=0;for(u8 i=0;i<count;i++)boundaries|=1u<<(requests[i].edge-1);
    if(RT_stagePoolCull(&scene->objects,&scene->camera,boundaries)==255)return 0;
    for(u8 i=0;i<count;i++)if(!RT_survivalSpawnObjects(world,&scene->objects,requests+i))return 0;
    if(enemyMoved)RT_survivalAttackConfigure(scene,world);
    return 1;
}

u8 RT_survivalSceneScroll(RT_CampaignScene *scene,const RT_SurvivalWorld *world,
    const RT_WorldCamera *previous)
{return RT_survivalSceneScrollPrepared(scene,world,previous,0);}
