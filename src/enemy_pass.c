#include "rt_enemy_pass.h"
#ifdef RT_PROFILE_ENEMIES
volatile u32 rtEnemyStateCosts[64],rtEnemyPassCosts[5];
volatile u16 rtEnemyStateCalls[64];
#endif
u8 RT_enemyTargetPlayer(const RT_PlayerTargets *players)
{
    if(!players || !players->agents || !players->combat || !players->players || players->players>2)return 255;
    for(u8 i=0;i<players->players;i++)if(!players->combat[i].removed && players->combat[i].health)return i;
    return 255;
}
#ifdef __m68k__
/* Avoid expanding every rare service into the per-frame pool traversal. */
__attribute__((optimize("Os")))
#endif
u8 RT_enemiesUpdateWithHigh(RT_EnemyPool *pool,const RT_EnemyPassContext *c,RT_EnemyPassResult *out,RT_HighEnemyPass highPass)
{
#ifdef RT_PROFILE_ENEMIES
    u32 wholeStart=getSubTick();
#endif
    if(!out)return 0;
    out->eventCount=out->visible=out->removed=out->hitEntries=out->doorTransitions=out->type10Deaths=0;
    out->faultSlot=out->faultAction=out->targetPlayer=255;
    if(!pool || !c || !c->stage || !c->camera || !c->terrain || !c->objects ||
        !c->objects->level || !c->projectiles || !c->terrain->ready ||
        c->terrain->camera.x!=c->camera->x || c->terrain->camera.y!=c->camera->y)return 0;
    out->targetPlayer=RT_enemyTargetPlayer(&c->players);
    if(out->targetPlayer==255)return 0;
    if(highPass && !highPass(pool,c,out))return 0;
    RT_Agent *target=c->players.agents+out->targetPlayer;
    RT_EnemyPerceptionTarget perception;RT_enemyPreparePerception(target,&perception);
    RT_PlayerTargets players=c->players;
    s32 left=(s32)c->camera->x*16-0x300,right=(s32)c->camera->x*16+0x1500;
    s32 origin=((s32)c->camera->y+256)*16;
    RT_EnemyTerrainContext probes={0,c->stage,c->camera,c->terrain};
    RT_EnemyUpdateContext update={c->camera,c->objects,target,&players,c->projectiles,
        RT_enemyTerrainProbeReady,&probes,0,c->visibleObjects,c->decisionCounter,
        c->objectCounter,c->workspaceFlags,c->fallbackSelector,0,c->throwMotion,c->visibleProjectiles,&perception};
#ifdef RT_PROFILE_ENEMIES
    u32 lowStart=getSubTick();rtEnemyPassCosts[1]+=lowStart-wholeStart;
#endif
    u8 remaining=pool->count;
    for(u16 i=0;i<RT_ENEMY_CAPACITY && remaining;i++){
        RT_Enemy *e=pool->slots+i;RT_DoorEnemy *a=&e->state;
        if(a->kind==RT_ENEMY_FREE)continue;
        remaining--;
        if(a->kind>=32){
            if(highPass)continue;
            out->faultSlot=i;out->faultAction=a->action;return 0;
        }
        u8 state=a->action&0xfc;
        if(c->actionCoverage)c->actionCoverage[state>>7]|=1UL<<((state>>2)&31);
        if(a->kind==16){
            out->faultSlot=i;out->faultAction=a->action;
            if(!c->type10Effects)return 0;
            RT_Type10PassEvent event;
            RT_EnemyResult result=RT_type10UpdateWithDelay(e,c->camera,target,c->scenePhase,c->sceneStep,c->type10Effects+i,&event,c->admissionDelays?c->admissionDelays+i:0);
            out->visible+=event.visibleDelta;
            if(result==RT_ENEMY_UNAVAILABLE)return 0;
            out->type10Deaths+=event.deaths;
            if(result==RT_ENEMY_REMOVED){pool->count--;out->removed++;a->hidden=1;}
            /* B298 also runs 962A for type10; its 78 entry keeps the state. */
            else if(!a->hidden)RT_enemyBodyContactPlayers(e,&perception,out->targetPlayer,&players);
            if(event.sound || event.command){
                if(out->eventCount>=RT_ENEMY_PASS_EVENT_CAPACITY)return 0;
                RT_EnemyPassEvent *record=out->events+out->eventCount++;
                *record=(RT_EnemyPassEvent){0,i,event.sound,event.command,pool->scoreOwner[i]};
            }
            continue;
        }
        if(state==0x80 && (a->spawnAction&0xfc)!=0x5c &&
            (a->spawnAction&0xfc)!=0x8c){
            out->faultSlot=i;out->faultAction=a->action;
#ifdef RT_PROFILE_ENEMIES
            u32 admissionStart=getSubTick();
#endif
            if(!RT_enemyActivateGrid(e,c->camera))return 0;
#ifdef RT_PROFILE_ENEMIES
            rtEnemyPassCosts[4]+=getSubTick()-admissionStart;
#endif
            continue;
        }
        a->hidden=a->x<left || (state==0x80?a->x>=right:a->x>right) || a->y>origin+0x400 || a->y<=origin-0x1000;
        if(a->hidden)continue;
        if(c->admissionDelays && c->admissionDelays[i]){
            c->admissionDelays[i]--;a->hidden=1;continue;
        }
        out->visible++;
        out->faultSlot=i;out->faultAction=a->action;
        if(!e->definition || (a->kind!=4 && a->kind!=8 && a->kind!=12))return 0;
        if(state==0x80 && (a->spawnAction&0xfc)==0x8c){
            if(!RT_enemyProximityEnter(e,target)){out->visible--;continue;}
            state=a->action&0xfc;
        }
        RT_WeaponEvent event={0};u16 award=0;RT_EnemyResult result;
        u8 pending=a->spawnAction&0xfc;
        if(a->action!=a->spawnAction && (pending==0x60 || pending==0x64 || pending==0x68)){
            RT_EnemyRecoilLinks links={0};
            RT_enemyThrownLinks(e,c->projectiles,&links);
            if(e->parent<RT_NATIVE_STAGE_CAPACITY && c->objects->objects[e->parent].kind<3){
                links.door=c->objects->objects+e->parent;
                links.doorOwnsEnemy=pool->doorChildren[e->parent]==i+1;
            }
            if(!RT_enemyPerceive(e,target) || !RT_enemyHitEnter(e,a->spawnAction,
                c->decisionCounter,&links,&award,&event.sound))return 0;
            out->hitEntries++;result=RT_ENEMY_CONTINUE;
        }else if(state==0x80 || state==0x5c || state==0x58){
#ifdef RT_PROFILE_ENEMIES
            u32 doorStart=getSubTick();
#endif
            result=RT_enemyUpdateDoor(e,c->objects,c->camera,target,
                (u8)(c->highVisible+out->visible),c->workspaceFlags);
#ifdef RT_PROFILE_ENEMIES
            rtEnemyPassCosts[3]+=getSubTick()-doorStart;
#endif
            if(result==RT_ENEMY_SELECT)out->doorTransitions++;
            if(a->hidden)out->visible--;
            else if(result!=RT_ENEMY_UNAVAILABLE && (state==0x58 || state==0x5c) &&
                RT_enemyBodyContactPlayers(e,&perception,out->targetPlayer,&players)==0x78){
                RT_EnemyRecoilLinks links={0};
                if(e->parent<RT_NATIVE_STAGE_CAPACITY && c->objects->objects[e->parent].kind<3){
                    links.door=c->objects->objects+e->parent;
                    links.doorOwnsEnemy=pool->doorChildren[e->parent]==i+1;
                }
                u8 prior=a->action&0xfc;
                if((prior!=0x5c || links.door) && !RT_enemyRecoilEnter(e,0x78,c->decisionCounter,&links))return 0;
            }
        }else{
            probes.enemy=e;update.accelerationY=e->definition->accelerationY;
            update.coopAttackPhase=c->coopAttackPhases?c->coopAttackPhases+i:0;
#ifdef RT_PROFILE_ENEMIES
            u32 serviceStart=getSubTick();
#endif
            result=RT_enemyUpdateReady(e,&update,&event);
#ifdef RT_PROFILE_ENEMIES
            rtEnemyStateCosts[state>>2]+=getSubTick()-serviceStart;
            rtEnemyStateCalls[state>>2]++;
#endif
        }
        if(result==RT_ENEMY_REMOVED){
            if(!pool->count)return 0;
            pool->count--;out->visible--;a->hidden=1;out->removed++;
        }
        if(result==RT_ENEMY_UNAVAILABLE)return 0;
        if(award || event.sound || event.command){
            if(out->eventCount>=RT_ENEMY_PASS_EVENT_CAPACITY)return 0;
            RT_EnemyPassEvent *record=out->events+out->eventCount++;
            *record=(RT_EnemyPassEvent){award,i,event.sound,event.command,pool->scoreOwner[i]};
        }
    }
#ifdef RT_PROFILE_ENEMIES
    u32 end=getSubTick();rtEnemyPassCosts[0]+=end-wholeStart;rtEnemyPassCosts[2]+=end-lowStart;
#endif
    if(remaining)return 0;
    out->faultSlot=out->faultAction=255;return 1;
}

u8 RT_enemiesUpdate(RT_EnemyPool *pool,const RT_EnemyPassContext *c,RT_EnemyPassResult *out)
{return RT_enemiesUpdateWithHigh(pool,c,out,0);}
