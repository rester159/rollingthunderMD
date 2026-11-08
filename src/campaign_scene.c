#include "rt_campaign_scene.h"
#include "rt_home_progress.h"
#include <string.h>
RT_CampaignInit RT_campaignSceneInitialize(RT_CampaignScene *scene,
    const RT_SessionConfig *selection,u8 story,u8 area,u8 checkpoint,
    u8 enemySet,u16 credits)
{
    if(!scene || !selection || story>1 || area>4 || checkpoint>1 || enemySet>1 ||
       (selection->mode==RT_MODE_HOME && selection->game!=RT_HOME_CAMPAIGN))
        return RT_CAMPAIGN_INIT_INVALID;
    if(!RT_prepareWorldBank(story,area))return RT_CAMPAIGN_INIT_TERRAIN;
    const RT_WorldStage *stage=RT_worldStage(story,area);
    RT_ScenePlayers players;RT_WorldCamera camera;
    if(!stage || !RT_scenePlayersInitialize(&players,selection,stage,checkpoint,credits,&camera))
        return RT_CAMPAIGN_INIT_INVALID;
    /* Stage entry clears projectile, effect, delay and attack-phase ownership
     * together; no previous run's constructor state may leak into the next. */
    memset(scene,0,sizeof *scene);
    scene->stage=stage;scene->spawnLevel=&rtEnemySpawnLevels[enemySet][story*5+area];
    scene->camera=camera;scene->players=players;scene->faultEvent=65535;
    /* Determine the phase schedule once from the entire area's constructors.
     * High-family children require an existing high parent; door constructors
     * are included independently of whether a door is currently visible. */
    for(u16 i=0;i<scene->spawnLevel->eventCount;i++){
        u16 definition=scene->spawnLevel->events[i].definition;
        if(definition>=rtGridEnemyTemplateCount)return RT_CAMPAIGN_INIT_ENEMIES;
        if(rtGridEnemyTemplates[definition].kind>=32)scene->hasHighEnemies=1;
    }
    for(u8 choice=0;choice<8;choice++){
        const RT_EnemyTemplate *definition=RT_doorEnemyTemplate(story*5+area,choice*4,0);
        if(!definition)return RT_CAMPAIGN_INIT_ENEMIES;
        if(definition->kind>=32)scene->hasHighEnemies=1;
    }
    RT_enemiesInitialize(&scene->enemies);
    if(!RT_stagePoolInitialize(&scene->objects,&rtStageEventLevels[story*5+area]) ||
       !RT_eventWindowInitialize(&scene->objectWindow,stage,checkpoint))
        return RT_CAMPAIGN_INIT_OBJECTS;
    scene->enemyWindow=scene->objectWindow;
    if(!RT_terrainWindowInitialize(&scene->terrain,stage,&camera))return RT_CAMPAIGN_INIT_TERRAIN;
    RT_EventWindowRequest initial={RT_EVENT_WINDOW_INITIAL,
        scene->objectWindow.gridX,scene->objectWindow.gridY};
    if(!RT_stagePoolSpawn(&scene->objects,&initial))return RT_CAMPAIGN_INIT_OBJECTS;
    if(!RT_enemiesSpawnWindowWithFamilies(&scene->enemies,scene->spawnLevel,&initial,
        &scene->faultEvent,players.home.players==2?&scene->population:NULL,scene->effects,15))
        return RT_CAMPAIGN_INIT_ENEMIES;
    /* Cabinet default selects source150; this resets at every authored entry,
     * including checkpoint restart and Continue, never on a co-op respawn. */
    scene->timer=(RT_CampaignTimer){selection->mode==RT_MODE_ARCADE && (rtArcadeDips&RT_DIP_TIMER)?120:150,0};
    scene->ready=1;return RT_CAMPAIGN_INIT_OK;
}
