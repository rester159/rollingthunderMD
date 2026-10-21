/* Original BF6D/BE9B/BEC9 control flow with native terrain/entry interfaces. */
#include "rt_type20.h"
#include "rt_high_enemy_motion.h"
#include "rt_high_animation.h"
static u8 walking(u8 state)
{
    switch(state){
    case 4:case 8:case 0x0c:case 0x10:case 0x14:case 0x3c:
    case 0x40:case 0x4c:case 0x50:case 0x58:case 0x5c:case 0x70:
    case 0x74:case 0x78:case 0x7c:case 0x84:case 0x88:case 0x8c:
    case 0x90:case 0x98:case 0x9c:case 0xa8:case 0xac:case 0xb0:
    case 0xb4:case 0xb8:case 0xbc:return 1;
    default:return 0;
    }
}
static RT_EnemyResult wallResponse(RT_Enemy *e,u8 code,const RT_Type20Motion *c)
{
    /* BA8D/BB2C enter BAE2 on contact. The sweep records the wall latch;
     * BAE2 also dispatches the terrain-specific entry for walking states.
     * Returning without that entry leaves the actor against the same wall
     * forever, with its animation timer never serviced. */
    u8 state=e->state.action&0xfc;
    if(state!=4 && state!=0x40)return RT_ENEMY_CONTINUE;
    u8 selector;
    switch(code){
    case 0x19:case 0x22:selector=0x48;break;
    case 0x25:case 0x27:selector=0x28;break;
    case 0x2d:selector=0x2c;break;
    default:selector=0x24;break;
    }
    RT_Type20EntryResult result=RT_type20SelectInContext(e,selector,c);
    if(result==RT_TYPE20_ENTRY_UNAVAILABLE)return RT_ENEMY_UNAVAILABLE;
    if(result==RT_TYPE20_ENTRY_DECLINED)
        return RT_type20SelectInContext(e,0x48,c)==RT_TYPE20_ENTRY_INSTALLED?
            RT_ENEMY_CONTINUE:RT_ENEMY_UNAVAILABLE;
    return RT_ENEMY_CONTINUE;
}
RT_EnemyResult RT_type20WalkStep(RT_Enemy *e,const RT_Type20Motion *c)
{
    if(!e || !c || !c->camera || e->state.kind!=32)return RT_ENEMY_UNAVAILABLE;
    u8 state=e->state.action&0xfc;
    if(state==0x80){
        return RT_type20StartupInContext(e,c)==RT_TYPE20_ENTRY_UNAVAILABLE?
            RT_ENEMY_UNAVAILABLE:RT_ENEMY_CONTINUE;
    }
    if(state && !walking(state))return RT_ENEMY_UNAVAILABLE;
    if(state){
        if(e->state.row>5)return RT_ENEMY_UNAVAILABLE;
        u8 hitCode;RT_EnemySweep result=RT_highEnemyHorizontal(e,c->camera,c->shadowXLow,c->probe,c->probeContext,&hitCode);
        if(result==RT_ENEMY_SWEEP_REMOVED){e->state.kind=RT_ENEMY_FREE;return RT_ENEMY_REMOVED;}
        if(result==RT_ENEMY_SWEEP_UNAVAILABLE)return RT_ENEMY_UNAVAILABLE;
        if(result==RT_ENEMY_SWEEP_HIT)return wallResponse(e,hitCode,c);
        u8 selector;
        if(!RT_highEnemyGround(e,c->camera,c->probe,c->probeContext,&selector))
            return RT_ENEMY_UNAVAILABLE;
        if(selector){
            /* BEA4's long BNE targets B88E, not the active fall handler. */
            return RT_type20Select(e,selector,c->probe,c->probeContext,c->sound)==RT_TYPE20_ENTRY_INSTALLED?
                RT_ENEMY_CONTINUE:RT_ENEMY_UNAVAILABLE;
        }
        RT_EnemyResult decision=RT_type20WalkDecision(e,c->targetX,c->probe,c->probeContext,c->sound);
        if(decision!=RT_ENEMY_CONTINUE)return decision==RT_ENEMY_SELECT?RT_ENEMY_CONTINUE:decision;
    }
    e->state.timer--;
    if(e->state.timer)return RT_ENEMY_CONTINUE;
    if(!state){
        RT_type20EnterWalk(e,e->state.decisions&4?5:6);
    }else{
        if(e->state.row==5){e->state.row=0;e->byte12&=0xdf;}
        else e->state.row++;
        const RT_HighAnimationRow *r=rtHighCycleRows+rtHighCycleOffsets[0]+e->state.row;
        e->state.timer=r->timer;e->state.flags=r->flags;
        e->state.pose=r->pose;
    }
    return RT_ENEMY_CONTINUE;
}
