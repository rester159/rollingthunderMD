#include "rt_enemy_update.h"
#include "rt_enemy_throw.h"
#ifdef __m68k__
/* Keep cold state handlers out of the 68000 hot dispatcher. */
__attribute__((optimize("Os")))
#endif
RT_EnemyResult RT_enemyUpdateReady(RT_Enemy *e,const RT_EnemyUpdateContext *c,
    RT_WeaponEvent *event)
{
    event->sound=event->command=0;
    u8 state=e->state.action&0xfc,selector=0,object=255,shot=0;
    RT_EnemyPerceptionTarget local;const RT_EnemyPerceptionTarget *perception=c->perception;
    if(!perception){RT_enemyPreparePerception(c->target,&local);perception=&local;}
    RT_enemyPerceivePrepared(e,perception);
    /* CPU2 B298 runs 962A's body contact before the state handler; a
     * resulting 78 entry is serviced by this same update, as B2B2 is. */
    if((e->state.decisions&0x40) && c->targets && c->targets->combat && c->targets->agents &&
       c->target>=c->targets->agents && c->target<c->targets->agents+c->targets->players &&
       RT_enemyBodyContact(e,perception,c->targets->combat+(c->target-c->targets->agents))){
        RT_EnemyRecoilLinks links={0};RT_enemyThrownLinks(e,c->projectiles,&links);
        if(!RT_enemyRecoilEnter(e,0x78,c->decisionCounter,&links))return RT_ENEMY_UNAVAILABLE;
        state=0x78;
    }
    RT_EnemyResult result;
    switch(state){
    case 0:case 0x6c:case 0x70:case 0x74:
        if(--e->state.timer)return RT_ENEMY_CONTINUE;
        selector=4;result=RT_ENEMY_SELECT;break;
    case 4:case 0x40:case 0x9c:case 0xa0:case 0xa4:
        result=RT_enemyWalkStep(e,c->camera,c->probe,c->probeContext,c->objects,
            c->visibleObjects,c->target->body.x,c->decisionCounter,c->objectCounter,
            &selector,&object,c->coopAttackPhase);
        /* B6AC/B6CA return selector zero for matched ordinary doors, then
         * walking continues through decision/animation. Only source43 emits
         * A8/AC; neither path retains an object ownership link. */
        break;
    case 8:case 0x3c:
        result=RT_enemyPauseStep(e,c->target->body.x,c->projectiles->active,
            c->probe,c->probeContext,&selector);break;
    case 0x38:result=RT_enemyIdleStep(e,&selector);break;
    case 0x48:case 0x54:result=RT_enemyTurnStep(e,&selector);break;
    case 0x4c:case 0x50:
        result=RT_enemySlopeStep(e,c->camera,c->probe,c->probeContext,&selector);break;
    case 0x28:case 0x2c:case 0x34:
        result=RT_enemyJumpStep(e,c->camera,c->accelerationY,c->probe,c->probeContext,&selector);break;
    case 0x20:case 0x24:case 0x30:case 0x44:
        result=RT_enemyHopStep(e,c->camera,c->accelerationY,c->probe,c->probeContext,&selector);break;
    case 0x18:case 0x1c:
        result=RT_enemyRangedStep(e,c->workspaceFlags,&shot,&selector);
        if(shot)RT_enemyProjectileRequest(c->projectiles,e,state==0x1c,event,0);
        break;
    case 0x60:case 0x64:case 0x68:
        result=RT_enemyHitStep(e,c->camera,c->accelerationY,c->probe,c->probeContext,&selector,&event->command);break;
    case 0x84:case 0x88:
        result=RT_enemyThrowStep(e,c->projectiles,c->throwMotion,c->visibleProjectiles,event,&selector);break;
    case 0xb4:
        result=RT_enemyScriptWalkStep(e,c->camera,c->probe,c->probeContext,&selector);break;
    case 0x90:case 0x98:
        result=RT_enemyTimedExitStep(e,&selector);break;
    case 0xb0:case 0xb8:case 0xbc:
        result=RT_enemyApproachStep(e,c->target->body.x,&selector);break;
    case 0xa8:case 0xac:
        result=RT_enemyObjectTransitionStep(e,&selector);break;
    case 0x8c:
        result=RT_enemyProximityStep(e,c->camera,c->accelerationY,c->probe,c->probeContext,&selector);break;
    case 0x94:result=RT_enemyCloseStep(e,c->targets,&selector);break;
    case 0x78:case 0x7c:
        result=RT_enemyRecoilStep(e,c->camera,c->accelerationY,c->probe,c->probeContext,&selector);break;
    case 0x0c:case 0x10:case 0x14:
        result=RT_enemyRecoveryStep(e,c->fallbackSelector,&selector);break;
    default:return RT_ENEMY_UNAVAILABLE;
    }
    if(result!=RT_ENEMY_SELECT)return result;
    if((selector&0xfc)==0x78 || (selector&0xfc)==0x7c){
        RT_EnemyRecoilLinks links={0};RT_enemyThrownLinks(e,c->projectiles,&links);
        if(!RT_enemyRecoilEnter(e,selector,c->decisionCounter,&links))return RT_ENEMY_UNAVAILABLE;
    }else if(!RT_enemySelect(e,c->camera,selector,c->workspaceFlags))return RT_ENEMY_UNAVAILABLE;
    return result;
}

RT_EnemyResult RT_enemyUpdate(RT_Enemy *e,const RT_EnemyUpdateContext *c,RT_WeaponEvent *event)
{
    if(!e || !c || !event || !c->camera || !c->objects || !c->target ||
        !c->projectiles || !c->probe ||
        (e->state.kind!=4 && e->state.kind!=8 && e->state.kind!=12))return RT_ENEMY_UNAVAILABLE;
    return RT_enemyUpdateReady(e,c,event);
}
