#include "rt_campaign_scene.h"
u8 RT_campaignObjectsStep(RT_CampaignScene *scene,u8 tick,RT_CampaignObjectPass *result)
{
    if(!scene || !scene->ready || !result)return 0;
    result->count=result->spawned=0;result->unservicedKinds=0;
    if(RT_stagePoolRefreshStaticVisibility(&scene->objects,&scene->camera)==255)return 0;
    u8 remaining=scene->objects.count;
    for(u8 i=0;i<RT_NATIVE_STAGE_CAPACITY && remaining;i++){
        RT_Anchor *object=scene->objects.objects+i;
        if(object->kind==255)continue;
        remaining--;
        if(!object->visible)continue;
        if(object->kind==8){RT_stageFanStep(object);continue;}
        /* CPU1 $49/$4A/$4B run only common visibility/delay service then RTS. */
        if(object->kind>=9 && object->kind<=11){
            if(object->spawnDelay)object->spawnDelay--;
            continue;
        }
        if(object->kind==3)continue; /* Climbs are serviced by the linked player. */
        if(object->kind>2){
            if(object->kind>31)return 0;
            result->unservicedKinds|=1UL<<object->kind;continue;
        }
        u8 sound;RT_AnchorAction action=RT_anchorStep(object,&sound);
        if(action==RT_ANCHOR_INVALID)return 0;
        if(action==RT_ANCHOR_SPAWN){
            u8 slot;
            if(RT_enemiesSpawnDoor(&scene->enemies,&scene->objects,i,
                scene->stage->story*5+scene->stage->area,(u8)scene->camera.x,tick,&slot)){
                result->spawned++;
                scene->population.admissionDelays[slot]=scene->attackPhases[slot]=0;
                scene->effects[slot]=(RT_Type10Effects){0};
            }else if(scene->enemies.count<RT_ENEMY_CAPACITY)return 0;
        }
        if(action==RT_ANCHOR_RESTORE || action==RT_ANCHOR_REPLACE){
            RT_CampaignObjectEvent *event=result->events+result->count++;
            if(!RT_anchorPatchCell(object,&event->column,&event->row))return 0;
            event->slot=i;event->kind=object->kind;
            event->replace=action==RT_ANCHOR_REPLACE;event->sound=sound;
        }
    }
    return remaining==0;
}
