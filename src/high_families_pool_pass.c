#include "rt_enemy_pass.h"
#include "rt_type20.h"
#include "rt_type2c.h"
#include "rt_high_visibility.h"
#include "rt_type24.h"
#include "rt_type28.h"
typedef struct {RT_EnemyPool *pool;const RT_EnemyPassContext *context;u8 parent;} ChildSpawn;
static u8 spawnChild(const RT_Enemy *parent,const RT_Type28Child *child,void *opaque)
{
    ChildSpawn *s=opaque;u8 slot;
    if(parent!=s->pool->slots+s->parent || !RT_type28SpawnChild(s->pool,s->parent,s->context->camera,child,&slot))return 0;
    if(s->context->admissionDelays)s->context->admissionDelays[slot]=0;
    if(s->context->coopAttackPhases)s->context->coopAttackPhases[slot]=0;
    return 1;
}
#ifdef RT_PROFILE_HIGH
volatile u32 rtHighPassCosts[4];
#endif
#ifdef __m68k__
__attribute__((optimize("Os"),noinline))
#endif
u8 RT_highFamiliesPoolPass(RT_EnemyPool *pool,const RT_EnemyPassContext *c,RT_EnemyPassResult *out)
{
    if(!pool || !c || !out || !c->players.agents || !c->players.combat || !c->stage ||
        out->targetPlayer>=c->players.players || !c->camera || !c->terrain || !c->objects || pool->count>RT_ENEMY_CAPACITY)return 0;
#ifdef RT_PROFILE_HIGH
    u32 wholeStart=getSubTick();
#endif
    RT_Agent *target=c->players.agents+out->targetPlayer;
    RT_EnemyPerceptionTarget perception;RT_enemyPreparePerception(target,&perception);
    RT_EnemyTerrainContext probes={0,c->stage,c->camera,c->terrain};
    const u8 shadowX=(u8)(c->camera->x*16),shadowY=(u8)(0-c->camera->y*16);
    const u16 cameraX=(u16)c->camera->x*16,cameraY=((u16)c->camera->y+256)*16;
    union {
        RT_Type20Motion walking;
        RT_Type24MotionContext flying;
        RT_Type28MotionContext splitting;
        RT_Type2cMotionContext large;
    } context;
    for(u8 i=0;i<RT_ENEMY_CAPACITY;i++){
        RT_Enemy *e=pool->slots+i;
        if(e->state.kind==RT_ENEMY_FREE || e->state.kind<32)continue;
        out->faultSlot=i;out->faultAction=e->state.action;
        if((e->state.kind!=32 && e->state.kind!=36 && e->state.kind!=40 && e->state.kind!=44) || !e->definition)return 0;
        RT_EnemyPassEvent event={.slot=i,.player=pool->scoreOwner[i]};u8 counted=0;
        probes.enemy=e;
        if(c->actionCoverage){u8 state=e->state.action&0xfc;c->actionCoverage[state>>7]|=1UL<<((state>>2)&31);}
        /* The four prepare services share this exact signed-word visibility
         * gate. Offscreen actors need no temporary motion context or event
         * service. Preserve the entry-state hidden bit: an unusual visible
         * pending entry still follows the complete original service path. */
        if(RT_highEnemyDormant(e,cameraX,cameraY))continue;
        /* Only the selected family owns this context during its service. */
        if(e->state.kind==32){
            context.walking=(RT_Type20Motion){.camera=c->camera,.probe=RT_enemyTerrainProbeReady,
                .probeContext=&probes,.shadowXLow=shadowX,.shadowYLow=shadowY,
                .targetX=target->body.x,.accelerationY=e->definition->accelerationY,
                .sound=&event.sound,.award=&event.scoreBCD};
        }else if(e->state.kind==36){
            context.flying=(RT_Type24MotionContext){.entry={.accelerationY=pool->accelerationY+i,
                .command=&event.command,.sound=&event.sound,.award=&event.scoreBCD},
                .camera=c->camera,.probe=RT_enemyTerrainProbeReady,.probeContext=&probes,
                .shadowXLow=shadowX,.shadowYLow=shadowY,
                .targetX=target->body.x,.targetY=target->body.y};
        }else if(e->state.kind==40){
            context.splitting=(RT_Type28MotionContext){.entry={.accelerationY=pool->accelerationY+i,
                .command=&event.command,.award=&event.scoreBCD},.camera=c->camera,
                .probe=RT_enemyTerrainProbeReady,.probeContext=&probes,.shadowXLow=shadowX,.shadowYLow=shadowY};
        }else{
            context.large=(RT_Type2cMotionContext){.entry={.camera=c->camera,.probe=RT_enemyTerrainProbeReady,
                .probeContext=&probes,.phase=c->vblankPhase,.sound=&event.sound,.award=&event.scoreBCD},
                .accelerationY=e->definition->accelerationY,.shadowXLow=shadowX,.shadowYLow=shadowY};
        }
#ifdef RT_PROFILE_HIGH
        u32 partStart=getSubTick();
#endif
        RT_EnemyResult result=e->state.kind==32?RT_type20Prepare(e,&context.walking,&counted):
            e->state.kind==36?RT_type24Prepare(e,&context.flying,&counted):
            e->state.kind==40?RT_type28Prepare(e,&context.splitting,&counted):RT_type2cPrepare(e,&context.large.entry,&counted);
#ifdef RT_PROFILE_HIGH
        rtHighPassCosts[0]+=getSubTick()-partStart;
#endif
        if(result==RT_ENEMY_UNAVAILABLE || !RT_enemyPassFlush(out,&event))return 0;
        if(!e->state.hidden){
            if(c->admissionDelays && c->admissionDelays[i]){
                c->admissionDelays[i]--;e->state.hidden=1;counted=0;
            }else{
#ifdef RT_PROFILE_HIGH
                partStart=getSubTick();
#endif
                if(!RT_highEnemyPerceivePrepared(e,&perception,c->players.combat+out->targetPlayer,&event.command))return 0;
                if(!RT_enemyPassFlush(out,&event))return 0;
#ifdef RT_PROFILE_HIGH
                rtHighPassCosts[1]+=getSubTick()-partStart;partStart=getSubTick();
#endif
                ChildSpawn children={pool,c,i};
                result=e->state.kind==32?RT_type20Step(e,&context.walking):e->state.kind==36?
                    RT_type24Step(e,&context.flying):e->state.kind==40?
                    RT_type28Step(e,&context.splitting,pool->count,spawnChild,&children):RT_type2cStep(e,&context.large,c->objects,target->body.x);
#ifdef RT_PROFILE_HIGH
                rtHighPassCosts[2]+=getSubTick()-partStart;
#endif
                if(result==RT_ENEMY_UNAVAILABLE)return 0;
                if(result==RT_ENEMY_REMOVED){
                    if(!pool->count)return 0;
                    pool->count--;out->removed++;e->state.hidden=1;counted=0;
                }
            }
        }
        out->visible+=counted;
        if(!RT_enemyPassFlush(out,&event))return 0;
    }
#ifdef RT_PROFILE_HIGH
    rtHighPassCosts[3]+=getSubTick()-wholeStart;
#endif
    return 1;
}
