#include "rt_attract.h"
volatile u8 rtArcadeAttract,rtAttractScriptIndex;
volatile u32 rtAttractLoops,rtAttractDemoUpdates;
/* Original CPU2 EB00: action selectors, not a recording of human inputs.
 * 98CD advances when the current action signals completion. */
static const u8 commands[]={
 0x01,0x05,0x1d,0x05,0x19,0x29,0x19,0x05,0x19,0x1d,0x05,0x05,
 0x2d,0x19,0x1d,0x1e,0x1e,0x1d,0x1a,0x05,0x19,0x05,0x08,0x29,
 0x19,0x19,0x05,0x05,0x1d,0x1d,0x2d,0x1d,0x19,0x19,0x05,0x1d,0x19,0x19
};
void RT_attractDemoInitialize(RT_CampaignScene *scene)
{
 RT_Agent *a=scene->players.agents;
 /* Demo template 850F starts at X0580; normal play starts at X0600. */
 a->body.x=(s32)scene->camera.x*16+0x580;
 rtAttractScriptIndex=0;rtAttractDemoUpdates=0;
}
RT_HomeAgentStep RT_attractAgentStep(RT_Agent *a,RT_AgentCombat *combat,
 const RT_WorldStage *stage,RT_WorldCamera *camera,RT_AnchorScene *anchors,u8 *sound)
{
 u8 action=a->action,phase=a->phase,done=0;
 if(a->timer==1){
  if(action==RT_AGENT_IDLE || (action==RT_AGENT_WALK && phase==5) ||
     (action>=RT_AGENT_LOOK && action<=RT_AGENT_RISE) ||
     ((action==RT_AGENT_FIRE || action==RT_AGENT_KNEEL_FIRE) && phase==3) ||
     (action==RT_AGENT_VAULT && phase==9) || (action==RT_AGENT_DROP && phase==6) ||
     ((action==RT_AGENT_FALL || action==RT_AGENT_VAULT_FALL) && phase==2) ||
     ((action==RT_AGENT_DOOR_EXIT || action==RT_AGENT_CLIMB_UP || action==RT_AGENT_CLIMB_DOWN) && phase==5))done=1;
 }
 /* Direction holds only the walk; no controller or saved layout feeds a demo. */
 u8 input=action==RT_AGENT_WALK?(a->direction==2?8:4):0;
 RT_HomeAgentStep result=RT_sceneStepAgent(a,combat,input,stage,camera,anchors,0,sound);
 if(result==RT_HOME_AGENT_INVALID || result==RT_HOME_AGENT_REMOVED || combat->removed)return result;
 if((action==RT_AGENT_JUMP || action==RT_AGENT_RUNNING_JUMP || action==RT_AGENT_CONTACT) && a->action==RT_AGENT_IDLE)done=1;
 if(done && result==RT_HOME_AGENT_NORMAL){
  if(++rtAttractScriptIndex==sizeof commands)rtAttractScriptIndex=0;
  u8 command=commands[rtAttractScriptIndex];
  /* A173 gives scripted fire one pending shot, as a human fire edge would. */
  a->fireLatch=((command&0xfc)==0x18 || (command&0xfc)==0x1c)?1:0;
  if(!RT_agentSelectInScene(a,(RT_AgentCommand){command>>2,command&3},stage,camera,anchors))return RT_HOME_AGENT_INVALID;
 }
 rtAttractDemoUpdates++;
 return result;
}
