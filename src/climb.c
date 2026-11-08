#include "rt_climb.h"

static void frame(RT_Agent *agent,u8 row)
{
    const RT_AgentFrame *f=rtAgentSequences[agent->action].frames+row;
    agent->phase=row;agent->timer=f->duration;
    agent->pose=f->pose;agent->contactPose=f->contactPose;
}
static u8 exitDoor(RT_Agent *agent,const RT_WorldStage *stage,
    const RT_WorldCamera *camera,RT_AnchorScene *scene)
{
    RT_AgentCommand next={RT_AGENT_DOOR_EXIT,0};
    return RT_agentSelectInScene(agent,next,stage,camera,scene);
}
u8 RT_agentClimbStep(RT_Agent *agent,u8 input,const RT_WorldStage *stage,
    const RT_WorldCamera *camera,RT_AnchorScene *scene)
{
    u8 action=agent->action,row=agent->phase;
    if(!scene || !scene->anchors || !agent->anchorIndex ||
       agent->anchorIndex>scene->count || row>(action==RT_AGENT_DOOR_ENTER?7:5)){
        agent->unsupported=action+1;return 0;
    }
    RT_Anchor *anchor=scene->anchors+agent->anchorIndex-1;
    u8 down=action==RT_AGENT_DOOR_EXIT || action==RT_AGENT_CLIMB_DOWN;
    /* Direction reversal precedes the open-door wait and every animation row. */
    if(row<=5 && (input&28)==(down?12:16)){
        static const u8 reversedRow[6]={5,5,4,3,2,0};
        agent->action=action+(down?-1:1);frame(agent,reversedRow[row]);return 1;
    }
    if(action==RT_AGENT_DOOR_ENTER && row>=5){
        if(row==5){
            /* This wait tests before decrementing; zero advances next call. */
            if(agent->timer)agent->timer--;
            else if(anchor->flags&24)agent->phase++;
            else return exitDoor(agent,stage,camera,scene);
        }else if(row==6){
            u8 level=(anchor->flags&24)>>3;
            /* Another player may consume the shared door after row 5. Do
             * not start a zero-duration refill (which would wrap to 255). */
            if(!level)return exitDoor(agent,stage,camera,scene);
            agent->doorSupply=anchor->flags&56;
            if(anchor->flags&32){
                static const u8 duration[4]={0,40,80,100};
                agent->weaponMode=level?1:0;agent->timer=duration[level];
                agent->interactionEvent=RT_INTERACTION_ALTERNATE;
            }else{
                static const u8 duration[4]={0,20,30,50};
                agent->timer=duration[level];agent->interactionEvent=RT_INTERACTION_VOICE;
            }
            agent->phase++;
        }else{
            /* Source 88B2 is BCD +0001 with wrap. Native inventory is decimal,
             * so branch at 9999 instead of carrying packed decimal at runtime. */
            /* Consuming a primary door also sets its shared bit 5. Keep the
             * supply type captured at refill start until this player exits. */
            u16 *ammo=agent->doorSupply&32?&agent->ammoAlternate:&agent->ammoPrimary;
            *ammo=*ammo==9999?0:*ammo+1;
            /* Co-op receives 50% more supplies without extending the door wait. */
            if(scene->players==2 && (agent->timer&1))
                *ammo=*ammo==9999?0:*ammo+1;
            if(!--agent->timer){
                /* Bit5 becomes the consumed latch, including primary doors. */
                anchor->flags=(anchor->flags&7)|32;
                return exitDoor(agent,stage,camera,scene);
            }
        }
        return 1;
    }
    if((action==RT_AGENT_DOOR_ENTER || action==RT_AGENT_DOOR_EXIT) && row<5)
        anchor->flags|=1;
    if(action==RT_AGENT_DOOR_EXIT && !row){
        if(anchor->phase==4)frame(agent,1);
        return 1;
    }
    if(--agent->timer)return 1;
    if(row==5){
        agent->fireLatch=agent->jumpLatch=0;input&=0xfc;
        if(action==RT_AGENT_DOOR_EXIT && (anchor->flags&32) && !(anchor->flags&24)){
            anchor->flags&=7;anchor->spawnDelay=191;
        }
        return RT_agentSelectInScene(agent,rtAgentInputMaps[0][input],stage,camera,scene);
    }
    if(row){
        if(!(anchor->flags&4))agent->body.y+=down?32:-32;
        if(down && (action==RT_AGENT_DOOR_EXIT || row<4))agent->body.layer=anchor->layer;
    }
    if(action==RT_AGENT_DOOR_ENTER && row==4){
        agent->body.layer=0;agent->timer=32;agent->phase++;
    }else{
        if(action==RT_AGENT_CLIMB_UP && row==4)agent->body.layer=2;
        frame(agent,row+1);
    }
    return 1;
}
