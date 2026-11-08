#include "rt_campaign_scene.h"
#include "rt_attract.h"
static void sharedCamera(RT_CampaignScene *scene)
{
    s16 low=0,high=0;u8 count=0;
    for(u8 p=0;p<2;p++){
        if(scene->players.combat[p].removed || !scene->players.combat[p].health)continue;
        s16 x=(s16)(scene->players.agents[p].body.x/16-scene->camera.x);
        if(!count || x<low)low=x;
        if(!count || x>high)high=x;
        count++;
    }
    if(!count)return;
    /* Same 288px framing as the existing Home camera; native actors retain
     * world coordinates so no actor/context rebinding is needed when it moves. */
    s16 delta=0;
    if(high>240)delta=high-240;
    else if(low<48 && scene->camera.x>0)delta=low-48;
    if(delta>low-8)delta=low-8;
    if(delta<high-280)delta=high-280;
    s16 maximum=scene->stage->widthCells*8-288;
    if(scene->camera.x+delta<0)delta=-scene->camera.x;
    if(scene->camera.x+delta>maximum)delta=maximum-scene->camera.x;
    /* Bound publication to the terrain window's accepted per-update span. */
    if(delta>16)delta=16;
    if(delta< -16)delta=-16;
    while(delta){
        u8 right=delta>0;
        if(RT_worldCameraBlocked(scene->stage,&scene->camera,right?RT_WORLD_RIGHT:RT_WORLD_LEFT))break;
        scene->camera.x+=right?1:-1;delta+=right?-1:1;
    }
}
u8 RT_campaignPlayersStep(RT_CampaignScene *scene,const u16 pads[2],
    RT_CampaignPlayerPass *result)
{
    if(!scene || !scene->ready || !pads || !result ||
       (scene->players.home.players!=1 && scene->players.home.players!=2))return 0;
    *result=(RT_CampaignPlayerPass){0};
    result->steps[0]=result->steps[1]=RT_HOME_AGENT_INACTIVE;
    RT_AnchorScene anchors={scene->objects.objects,RT_NATIVE_STAGE_CAPACITY,scene->players.home.players};
    u8 count=scene->players.home.players;
    for(u8 p=0;p<count;p++){
        RT_Agent *agent=scene->players.agents+p;
        u16 buttons=pads[p],pressed=buttons&~scene->players.previousButtons[p];
        scene->players.previousButtons[p]=buttons;
        if(scene->players.combat[p].removed || (!scene->players.combat[p].health &&
           !(scene->players.selection.mode==RT_MODE_ARCADE &&
             RT_agentHasFatalAnimation(agent,scene->players.combat+p)))){
            agent->shotRequest=0;continue;
        }
        if(rtArcadeAttract==RT_ATTRACT_DEMO){
            result->steps[p]=RT_attractAgentStep(agent,scene->players.combat+p,
                scene->stage,&scene->camera,&anchors,result->sounds+p);
        }else{
        u8 direction=buttons&BUTTON_UP?3:buttons&BUTTON_DOWN?4:
            buttons&BUTTON_RIGHT?1:buttons&BUTTON_LEFT?2:0;
        u8 input=RT_agentInput(agent,direction,!!(pressed&RT_jumpButtons(p)),
            !!(pressed&RT_fireButtons(p)),!!(buttons&RT_fireButtons(p)));
        result->steps[p]=RT_sceneStepAgent(agent,scene->players.combat+p,input,
            scene->stage,&scene->camera,&anchors,count==2,result->sounds+p);
        }
        if(result->steps[p]==RT_HOME_AGENT_INVALID)return 0;
        if(RT_projectilesRequest(&scene->projectiles,agent,p,result->weapons+p))result->shots|=1<<p;
    }
    if(count==2)sharedCamera(scene);
    return 1;
}
