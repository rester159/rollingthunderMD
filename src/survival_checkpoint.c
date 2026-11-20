#include "rt_survival_checkpoint.h"
u8 RT_survivalCheckpointCapture(RT_CampaignScene *scene,RT_SurvivalWorld *world)
{
    if(!scene || !world || !scene->ready || scene->stage!=&world->stage ||
       scene->players.selection.mode!=RT_MODE_HOME ||
       scene->players.selection.game!=RT_HOME_SURVIVAL ||
       (scene->players.home.players!=1 && scene->players.home.players!=2) ||
       scene->camera.x<world->stage.cameraMinX || scene->camera.x>world->stage.cameraMaxX)return 0;
    /* At most one successful capture per128px of forward camera travel.
     * Ordinary updates avoid the standing-clearance probes entirely. */
    if((s32)scene->camera.x-world->stage.entrances[1].x<128)return 0;
    u8 selected=255;
    s32 minimum=scene->players.home.spawn[0].body.x;
    for(u8 p=1;p<scene->players.home.players;p++)
        if(scene->players.home.spawn[p].body.x>minimum)minimum=scene->players.home.spawn[p].body.x;
    for(u8 p=0;p<scene->players.home.players;p++){
        const RT_Agent *a=scene->players.agents+p;
        const RT_AgentCombat *c=scene->players.combat+p;
        /* anchorIndex is a latch of the last door/climb used, not "in a door
         * now": it stays set after the player walks away. Testing it here
         * stopped every later capture once any door or ladder was used, which
         * froze retirement and walled the camera at cameraMaxX until a death
         * reset the agent. Door and climb states are already excluded by the
         * action, hidden and layer tests. */
        if(!c->health || c->removed || c->hidden || c->reaction || c->damagePending ||
           (a->action!=RT_AGENT_IDLE && a->action!=RT_AGENT_WALK) ||
           a->body.layer>6 || (a->body.layer&1) || (a->body.y&127) || a->body.x<=minimum)continue;
        s32 localX=a->body.x-(s32)scene->camera.x*16;
        s32 localY=a->body.y-(s32)scene->camera.y*16;
        if(localX<24*16 || localX>264*16 || localY<40*16 || localY>240*16)continue;
        /* Ordinary flat support under both feet; exclude stair transitions.
         * Test actual world cells, without collision-window vertical wrap. */
        u8 safe=1;
        for(s16 dx=-8;dx<=8;dx+=8){
            const RT_TerrainMaterial *floor=RT_worldFixed(scene->stage,a->body.x+dx*16,a->body.y);
            if(!(floor->collision[a->body.layer>>1]&2) || (floor->probeCode&0xee)==0x20){safe=0;break;}
            for(s16 dy=8;dy<=24;dy+=8)
                if(RT_worldFixed(scene->stage,a->body.x+dx*16,a->body.y-dy*16)->collision[a->body.layer>>1]&1){safe=0;break;}
            if(!safe)break;
        }
        if(safe){selected=p;minimum=a->body.x;}
    }
    if(selected==255)return 0;
    const RT_MotionBody *body=&scene->players.agents[selected].body;
    for(u8 p=0;p<scene->players.home.players;p++){
        RT_MotionBody *spawn=&scene->players.home.spawn[p].body;
        spawn->x=body->x;spawn->y=body->y;spawn->layer=body->layer;
    }
    world->stage.entrances[1]=scene->camera;
    return 1;
}
