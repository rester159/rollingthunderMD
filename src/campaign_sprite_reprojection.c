#include "rt_coop_resident_art.h"
/* Experimental separation of pose residency from per-update geometry. */
#include "rt_campaign_sprites.h"
#include <genesis.h>
#ifdef __m68k__
static inline u16 RT_emitPackedFast(RT_SpriteCommand *command,u8 *order,const RT_PackedSpritePiece *piece,u16 count,u16 n,u16 x,u16 y,u16 base,u8 priority,u8 key)
{
    register u32 d0 __asm__("d0")=count;
    register u32 d1 __asm__("d1")=n;
    register u32 d2 __asm__("d2")=x;
    register u32 d3 __asm__("d3")=y;
    register u32 d4 __asm__("d4")=base;
    register u32 d5 __asm__("d5")=((u16)key<<8)|priority;
    register RT_SpriteCommand *a0 __asm__("a0")=command;
    register const RT_PackedSpritePiece *a1 __asm__("a1")=piece;
    register u8 *a2 __asm__("a2")=order;
    __asm__ volatile("jsr RT_emitPackedRegisters" :
        "+d"(d0),"+d"(d1),"+a"(a0),"+a"(a1),"+a"(a2) :
        "d"(d2),"d"(d3),"d"(d4),"d"(d5) : "d6","d7","cc","memory");
    return d0;
}
#endif
#include "rt_enemy_emit_registers.h"
typedef RT_HeldGroup HeldGroup;
__attribute__((used,externally_visible)) u16 rtEnemyEmitCameraX,rtEnemyEmitCameraY;
static HeldGroup heldGroups[2+RT_ENEMY_CAPACITY+RT_PROJECTILE_CAPACITY+RT_NATIVE_STAGE_CAPACITY];
static u8 heldCount,heldReady;
static u8 heldPlayers[2],heldEnemies[RT_ENEMY_CAPACITY];
static u8 heldObjects[RT_NATIVE_STAGE_CAPACITY],heldProjectiles[RT_PROJECTILE_CAPACITY];
u32 rtSpriteBirthEpoch;
static u32 heldBirthEpoch;
typedef struct {u16 pose;u8 kind,color;} GraphicKey;
static GraphicKey graphicKeys[RT_ENEMY_CAPACITY];
static const RT_PackedSpritePose *graphics[RT_ENEMY_CAPACITY];
static void resetHeld(void)
{
    heldReady=heldCount=0;
    memset(heldPlayers,0,sizeof heldPlayers);memset(heldEnemies,0,sizeof heldEnemies);
    memset(heldObjects,0,sizeof heldObjects);memset(heldProjectiles,0,sizeof heldProjectiles);
}
void RT_reprojectionReset(void)
{resetHeld();memset(graphicKeys,255,sizeof graphicKeys);}
static inline const RT_PackedSpritePose *enemyGraphic(const RT_CampaignGraphics *g,const RT_Enemy *e,u8 index)
{
    u16 pose=e->state.pose;u8 kind=e->state.kind,color=e->byte4;
    GraphicKey *entry=graphicKeys+index;
    if(entry->pose==pose && entry->kind==kind && entry->color==color)return graphics[index];
    const RT_PackedSpritePose *graphic=kind>=32?g->highEnemy(kind,color,pose):g->enemy(kind,color,pose);
    *entry=(GraphicKey){pose,graphic?kind:255,color};return graphic;
}
static inline void hold(const RT_SpritePose *pose,const RT_PackedSpritePiece *packed,u16 tile,
    u8 type,u8 index,u8 kind,u8 tag)
{
    heldGroups[heldCount++]=(HeldGroup){pose,packed,tile,type,index,kind,tag};
}
static s16 streamStageId(const RT_Anchor *a)
{
    if(a->kind<3 && a->pose)return a->pose<=3?a->kind*3+a->pose-1:-2;
    if(a->kind==8)return a->pose<4?9+a->pose:-2;
    if(a->kind==11)return 9;
    return -1;
}
static u8 fail(RT_CampaignSprites *s,u8 code){s->fault=code;return 0;}
static inline u8 playerVisible(const RT_CampaignScene *scene,u8 p)
{
    const RT_Agent *a=scene->players.agents+p;
    /* The door state machine clears the actor layer once fully inside and
     * restores it when emerging. Apply this to full and retained-pose draws. */
    if(!a->body.layer && (a->action==RT_AGENT_DOOR_ENTER || a->action==RT_AGENT_DOOR_EXIT))return 0;
    const RT_AgentCombat *c=scene->players.combat+p;
    return RT_homeAgentVisible(c) || (!c->hidden && !c->removed &&
        ((scene->exiting&(1<<p)) || (scene->players.selection.mode==RT_MODE_ARCADE &&
         RT_agentHasFatalAnimation(scene->players.agents+p,c))));
}
/* RT_spriteSubmitPacked for an already validated visible actor-pool instance.
 * Identical SAT output; uploads only when the slot's resident pose changed. */
static inline u8 submitPacked(RT_SpriteBatch *b,RT_SpriteSlot *slot,const RT_SpritePose *pose,
    const RT_PackedSpritePiece *pieces,u16 x,u16 y,u8 facingLeft,u8 priority)
{
    u16 n=pose->pieceCount,count=b->count;
    if(!pieces || !pose->patterns || !n || !pose->tileCount || pose->tileCount>slot->capacity ||
       n>80-b->reserved-count){b->failed=1;return 0;}
    if(!RT_spritePixelsMatch(slot->pose,pose) && !RT_spriteUpload(b,slot,pose))return 0;
    u8 key=(priority<<2)|RT_SPRITE_POOL_ACTOR;
    u16 start=count;
    const RT_PackedSpritePiece *piece=pieces+(facingLeft?n:0);
    RT_SpriteCommand *command=b->commands+count;u8 *order=b->orderKeys+count;
#ifdef __m68k__
    count=RT_emitPackedFast(command,order,piece,count,n,x,y,slot->firstTile,priority,key);
#else
    for(;n;n--,piece++){
        if((u16)(x+piece->clipX)>=352 || (u16)(piece->clipY-y)>=256)continue;
        command->y=((y+piece->y)&255)+112;command->x=((x+piece->x)&511)+128;
        command->attribute=(piece->attribute+slot->firstTile)|((piece->priority|priority)>=3?0x8000:0);
        command->size=piece->size;command->link=(u8)(++count+2);*order++=key;command++;
    }
#endif
    b->count=count;RT_spritesMarkRun(b,start,key);return 1;
}
#ifdef __m68k__
/* Keep per-family preparation out of the compact frame dispatcher. */
__attribute__((noinline))
#endif
u8 RT_campaignSpritesQueue(RT_CampaignSprites *s,const RT_CampaignScene *scene,RT_CampaignDisplay *view)
{
    if(!s || !s->ready || !scene || !scene->ready || !view || !view->display.ready)return 0;
    const RT_CampaignGraphics *g=s->graphics;
    if(!g || g->stage!=view->display.stage)return fail(s,2);
    resetHeld();
    s->fault=0;if(s->hud)RT_spritesBeginHud(&s->batch);else RT_spritesBegin(&s->batch);
    RT_SpriteBatch *batch=&s->batch;
    const RT_WorldCamera *camera=&scene->camera;
    for(u8 p=0;p<scene->players.home.players;p++){
        const RT_Agent *a=scene->players.agents+p;
        const RT_AgentCombat *combat=scene->players.combat+p;
        u8 visible=playerVisible(scene,p);
        if(!visible && (!combat->health || combat->hidden || combat->removed))continue;
        u8 identity=scene->players.selection.agent[p],costume=scene->players.selection.costume[p];
        if(identity>1 || costume>1 || a->pose>=g->agentPoseCount)return fail(s,1);
        const RT_SpritePose *pose=g->agents[identity?1+costume:(costume?3:0)]+a->pose;
        if(!pose->patterns || !pose->tileCount)return fail(s,1);
        u16 heldFirst=batch->count;
        RT_SpriteInstance instance={pose,a->body.x>>4,(a->body.y+15)>>4,a->direction==2,a->body.layer,1,RT_SPRITE_POOL_ACTOR};
        if(!RT_spriteSubmit(batch,s->players+p,&instance,camera))return fail(s,1);
        hold(pose,0,s->players[p].firstTile,0,p,0,0);heldPlayers[p]=1;
        if(!visible)RT_spritesTruncate(batch,heldFirst);
    }
    /* Main-thread-only preparation workspace, kept off the interrupt-shared stack. */
    static const RT_SpritePose *poses[RT_ENEMY_CAPACITY+RT_PROJECTILE_CAPACITY+15];
    static u8 enemySlots[RT_ENEMY_CAPACITY];
    static u8 enemies[RT_ENEMY_CAPACITY],slots[RT_ENEMY_CAPACITY+RT_PROJECTILE_CAPACITY+15];
    static u8 shots[RT_PROJECTILE_CAPACITY],throwIndices[RT_PROJECTILE_CAPACITY];
    static u8 stageObjects[RT_NATIVE_STAGE_CAPACITY];
    u8 coop=scene->players.home.players==2;
retryDetail:
    ; /* Retry planning before any shared arena uploads or held enemy groups. */
    u8 enemyCount=0,shotCount=0,planned=0,remaining=scene->enemies.count;
    u16 piecesNeeded=batch->count;
    const RT_Enemy *e=scene->enemies.slots;
    for(u8 i=0;remaining && i<RT_ENEMY_CAPACITY;i++,e++){
        if(e->state.kind==RT_ENEMY_FREE)continue;
        remaining--;if(e->state.hidden)continue;
        u8 resident=coop && rtCoopCompact?RT_coopResidentIndex(e):255;
        const RT_PackedSpritePose *graphic=resident!=255?RT_coopResidentGraphic(resident):
            enemyGraphic(g,e,enemyCount);
        if(resident!=255)graphicKeys[enemyCount].kind=255;
        if(!graphic || !graphic->pose)return fail(s,3);
        piecesNeeded+=graphic->pose->pieceCount;
        enemySlots[enemyCount]=resident!=255?resident:10+planned;
        if(resident==255)poses[planned++]=graphic->pose;
        enemies[enemyCount]=i;graphics[enemyCount++]=graphic;
    }
    if(remaining)return fail(s,3);
    remaining=scene->projectiles.active;
    const RT_Projectile *shot=scene->projectiles.slots;
    for(u8 i=0;remaining && i<RT_PROJECTILE_CAPACITY;i++,shot++){
        if(!shot->kind)continue;
        remaining--;shots[shotCount++]=i;
        if(shot->kind<RT_PROJECTILE_NEW_THROW_DOWN){if(shot->pose>=g->bulletPoseCount)return fail(s,4);piecesNeeded+=g->bullets[shot->pose].pieceCount;continue;}
        if(shot->pose<g->throwFirstPose || shot->pose-g->throwFirstPose>=g->throwPoseCount)return fail(s,4);
        piecesNeeded+=g->throws[shot->pose-g->throwFirstPose].pieceCount;
        throwIndices[i]=planned;poses[planned++]=g->throws+shot->pose-g->throwFirstPose;
    }
    if(remaining)return fail(s,4);
    u8 staticIndices[2]={255,255},stageIndices[13];
    u8 stageObjectCount=0;
    for(u8 i=0;i<13;i++)stageIndices[i]=255;
    remaining=scene->objects.count;
    const RT_Anchor *a=scene->objects.objects;
    for(u8 i=0;remaining && i<RT_NATIVE_STAGE_CAPACITY;i++,a++){
        if(a->kind==255)continue;
        remaining--;
        if(!a->visible)continue;
        stageObjects[stageObjectCount++]=i;
        if(a->kind==9 || a->kind==10)piecesNeeded+=g->decorations[a->kind-9].pieceCount;
        else if(s->streamStages){s16 id=streamStageId(a);if(id>=0)piecesNeeded+=g->stagePoses[id].pieceCount;}
        else if(a->kind==8 || a->kind==11){u8 phase=a->kind==11?0:a->pose;if(phase<4)piecesNeeded+=g->fans[phase].pieceCount;}
        else if(a->kind<3 && a->pose && a->pose<=3)piecesNeeded+=g->doors[a->kind][a->pose-1].pieceCount;
        if(a->kind==9 || a->kind==10){
            u8 kind=a->kind-9;
            if(staticIndices[kind]==255){staticIndices[kind]=planned;poses[planned++]=g->decorations+kind;}
        }else if(s->streamStages){
            s16 id=streamStageId(a);if(id==-2)return fail(s,2);
            if(id>=0 && stageIndices[id]==255){stageIndices[id]=planned;poses[planned++]=g->stagePoses+id;}
        }
    }
    if(remaining)return fail(s,2);
    if(coop && !rtCoopCompact && piecesNeeded>80-batch->reserved){
        RT_coopDetailPressure();goto retryDetail;
    }
    if(!RT_spriteArenaPlan(&s->arena,poses,planned,slots)){
        if(coop && !rtCoopCompact){RT_coopDetailPressure();goto retryDetail;}
        return fail(s,5);
    }
    for(u8 n=0;n<stageObjectCount;n++){
        a=scene->objects.objects+stageObjects[n];
        u8 index=255;u16 tile=s->doorTile;const RT_SpritePose *pose=NULL;
        if(a->kind==9 || a->kind==10)index=staticIndices[a->kind-9];
        else if(s->streamStages){
            s16 id=streamStageId(a);
            if(id<0)continue;
            index=stageIndices[id];
        }
        if(index!=255){
            RT_SpriteSlot *slot=s->arena.slots+slots[index];
            RT_SpriteInstance instance={poses[index],a->x/16,a->y/16,0,a->layer,1,RT_SPRITE_POOL_STAGE};
            if(!RT_spriteSubmitStageCached(batch,slot,&instance,camera))return fail(s,2);
            hold(poses[index],0,slot->firstTile,2,stageObjects[n],a->kind,0);heldObjects[stageObjects[n]]=1;
            continue;
        }
        if(a->kind==8 || a->kind==11){
            u8 phase=a->kind==11?0:a->pose;
            if(phase>=4)return fail(s,2);
            pose=g->fans+phase;tile=s->fanTile;
        }
        else if(a->kind<3 && a->pose){if(a->pose>3)return fail(s,2);pose=&g->doors[a->kind][a->pose-1];}
        if(!pose)continue; /* Unsupported stage families reported by object pass. */
        RT_SpriteInstance instance={pose,a->x/16,a->y/16,0,a->layer,1,RT_SPRITE_POOL_STAGE};
        if(!RT_spriteSubmitStage(batch,tile,&instance,camera))return fail(s,2);
        hold(pose,0,tile,2,stageObjects[n],a->kind,0);heldObjects[stageObjects[n]]=1;
    }
    for(u8 n=0;n<enemyCount;n++){
        u8 i=enemies[n];e=scene->enemies.slots+i;
        RT_SpriteSlot *slot=enemySlots[n]<10?RT_coopResidentSlot(enemySlots[n]):
            s->arena.slots+slots[enemySlots[n]-10];
        const RT_PackedSpritePose *graphic=graphics[n];
        if(batch->failed)return fail(s,3);
        if(!submitPacked(batch,slot,graphic->pose,graphic->pieces,
            (u16)((e->state.x>>4)-camera->x),(u16)(((e->state.y+15)>>4)-camera->y),
            !!(e->state.action&2),e->state.attributes>>5))return fail(s,3);
        hold(graphic->pose,graphic->pieces,slot->firstTile,1,i,e->state.kind,e->byte4);heldEnemies[i]=1;
    }
    for(u8 n=0;n<shotCount;n++){
        u8 i=shots[n];shot=scene->projectiles.slots+i;
        RT_SpriteInstance instance={NULL,shot->x>>4,(shot->y+15)>>4,0,shot->layer,1,RT_SPRITE_POOL_ACTOR};
        if(shot->kind>=RT_PROJECTILE_NEW_THROW_DOWN){
            u8 index=throwIndices[i];instance.pose=poses[index];
            RT_SpriteSlot *slot=s->arena.slots+slots[index];
            if(!RT_spriteSubmit(batch,slot,&instance,camera))return fail(s,4);
            hold(instance.pose,0,slot->firstTile,3,i,shot->kind,0);
        }else{
            instance.pose=g->bullets+shot->pose;
            if(!RT_spriteSubmitResident(batch,s->bulletTile,&instance,camera))return fail(s,4);
            hold(instance.pose,0,s->bulletTile,3,i,shot->kind,0);
        }
        heldProjectiles[i]=1;
    }
    if(!RT_spritesQueue(batch))return fail(s,5);
    heldBirthEpoch=rtSpriteBirthEpoch;heldReady=1;
    if(!RT_spritesDeferred())view->display.queuedBytes+=batch->queuedBytes;
    return 1;
}
u8 RT_campaignSpritesReproject(RT_CampaignSprites *s,const RT_CampaignScene *scene,RT_CampaignDisplay *view)
{
    if(!heldReady || heldBirthEpoch!=rtSpriteBirthEpoch || !s->ready || !s->hud)return 0;
    /* A visibility transition does not necessarily allocate a slot. Never
     * silently omit a newly drawable actor just because its last full draw
     * submitted no pose. Fall back before preparing or publishing any SAT. */
    for(u8 p=0;p<scene->players.home.players;p++)if(!heldPlayers[p]){
        if(playerVisible(scene,p))return 0;
    }
    u8 remaining=scene->enemies.count;
    for(u8 i=0;i<RT_ENEMY_CAPACITY && remaining;i++){
        const RT_Enemy *e=scene->enemies.slots+i;
        if(e->state.kind==RT_ENEMY_FREE)continue;
        remaining--;
        if(!e->state.hidden && !heldEnemies[i])return 0;
    }
    if(remaining)return 0;
    remaining=scene->objects.count;
    for(u8 i=0;i<RT_NATIVE_STAGE_CAPACITY && remaining;i++){
        const RT_Anchor *a=scene->objects.objects+i;
        if(a->kind==255)continue;
        remaining--;
        if(a->visible && (a->kind==8 || a->kind==9 || a->kind==10 || a->kind==11 ||
            (a->kind<3 && a->pose)) && !heldObjects[i])return 0;
    }
    if(remaining)return 0;
    remaining=scene->projectiles.active;
    for(u8 i=0;i<RT_PROJECTILE_CAPACITY && remaining;i++){
        if(!scene->projectiles.slots[i].kind)continue;
        remaining--;
        if(!heldProjectiles[i])return 0;
    }
    if(remaining)return 0;
    RT_spritesBeginHud(&s->batch);
    rtEnemyEmitCameraX=scene->camera.x;rtEnemyEmitCameraY=scene->camera.y;
    for(u16 i=0;i<heldCount;i++){
        const HeldGroup *h=heldGroups+i;
        if(h->type==1 && h->packed){
            u16 j=i+1;
            while(j<heldCount && heldGroups[j].type==1 && heldGroups[j].packed)j++;
            if(!RT_emitHeldEnemies(&s->batch,scene->enemies.slots,h,heldGroups+j))return 0;
            i=j-1;continue;
        }
        /* A held packed enemy already owns immutable pose/tile residency.
         * Resolve current actor fields straight into the native SAT emitter. */
        if(h->type==1 && h->packed){
            const RT_Enemy *e=scene->enemies.slots+h->slot;
            if(e->state.kind!=RT_ENEMY_FREE && (e->state.kind!=h->kind || e->byte4!=h->tag))return 0;
            if(e->state.kind==RT_ENEMY_FREE || e->state.hidden)continue;
            u16 count=s->batch.count,n=h->pose->pieceCount;
            if(n>80-s->batch.reserved-count)return 0;
            u16 x=(u16)((e->state.x>>4)-scene->camera.x);
            u16 y=(u16)(((e->state.y+15)>>4)-scene->camera.y);
            u8 priority=e->state.attributes>>5,key=(priority<<2)|RT_SPRITE_POOL_ACTOR;
            const RT_PackedSpritePiece *piece=h->packed+((e->state.action&2)?n:0);
            RT_SpriteCommand *command=s->batch.commands+count;
            u8 *order=s->batch.orderKeys+count;
            u16 start=count;
#ifdef __m68k__
            count=RT_emitPackedFast(command,order,piece,count,n,x,y,h->tile,priority,key);
#else
            for(u16 k=0;k<n;k++,piece++){
                if((u16)(x+piece->clipX)>=352 || (u16)(piece->clipY-y)>=256)continue;
                command->y=((y+piece->y)&255)+112;command->x=((x+piece->x)&511)+128;
                command->attribute=(piece->attribute+h->tile)|((piece->priority|priority)>=3?0x8000:0);
                command->size=piece->size;command->link=(u8)(++count+2);*order++=key;command++;
            }
#endif
            s->batch.count=count;RT_spritesMarkRun(&s->batch,start,key);continue;
        }
        RT_SpriteInstance instance={h->pose,0,0,0,0,0,RT_SPRITE_POOL_ACTOR};
        if(h->type==0){
            const RT_Agent *a=scene->players.agents+h->slot;
            instance.visible=playerVisible(scene,h->slot);
            instance.x=a->body.x>>4;instance.y=(a->body.y+15)>>4;
            instance.facingLeft=a->direction==2;instance.priority=a->body.layer;
        }else if(h->type==1){
            const RT_Enemy *e=scene->enemies.slots+h->slot;
            if(e->state.kind!=RT_ENEMY_FREE && (e->state.kind!=h->kind || e->byte4!=h->tag))return 0;
            instance.visible=e->state.kind!=RT_ENEMY_FREE && !e->state.hidden;
            instance.x=e->state.x>>4;instance.y=(e->state.y+15)>>4;
            instance.facingLeft=!!(e->state.action&2);instance.priority=e->state.attributes>>5;
        }else if(h->type==2){
            const RT_Anchor *a=scene->objects.objects+h->slot;
            if(a->kind!=255 && a->kind!=h->kind)return 0;
            instance.visible=a->kind!=255 && a->visible && (a->kind>=3 || a->pose);
            instance.x=a->x/16;instance.y=a->y/16;instance.priority=a->layer;
            instance.poolRank=RT_SPRITE_POOL_STAGE;
        }else{
            const RT_Projectile *p=scene->projectiles.slots+h->slot;
            if(p->kind && p->kind!=h->kind)return 0;
            instance.visible=p->kind!=0;
            instance.x=p->x>>4;instance.y=(p->y+15)>>4;instance.priority=p->layer;
        }
        if(!instance.visible)continue;
        RT_SpriteSlot resident={h->pose,h->tile,h->pose->tileCount};
        if(h->packed){
            if(!RT_spriteSubmitPacked(&s->batch,&resident,&instance,&scene->camera,h->packed))return 0;
        }else if(h->type==2){
            if(!RT_spriteSubmitStage(&s->batch,h->tile,&instance,&scene->camera))return 0;
        }else if(!RT_spriteSubmitResident(&s->batch,h->tile,&instance,&scene->camera))return 0;
    }
    if(!RT_spritesQueue(&s->batch))return 0;
    if(!RT_spritesDeferred())view->display.queuedBytes+=s->batch.queuedBytes;
    return 1;
}

#include "rt_rom_layout.h"
#ifdef RT_LINEAR_ROM
#include "rt_packed_cache.h"
void RT_campaignSpritesPreload(const RT_CampaignSprites *s,const RT_CampaignScene *scene)
{
    if(scene->players.home.players==1)RT_packedExtraEnable();
    static const u8 priority[]={1,2,3,4,5,6,0,18,19,10,11,12,13,65,66,70};
    const RT_CampaignGraphics *g=s->graphics;
    if(scene->players.home.players==1){
        u8 identity=scene->players.selection.agent[0],costume=scene->players.selection.costume[0];
        const RT_SpritePose *poses=g->agents[identity?1+costume:(costume?3:0)];
        RT_packedPreload(poses[0].patterns,poses[0].tileCount*16);
        RT_packedPreload(poses[18].patterns,poses[18].tileCount*16);
    }
    for(u8 i=0;i<sizeof priority;i++){
        if(i==6){
            if(scene->players.home.players==2)break;
            /* Door constructors are not represented by the enemy spawn grid.
             * Keep the first regular door guard's short idle cycle resident. */
            u8 stage=scene->stage->story*5+scene->stage->area;
            for(u8 choice=0;choice<8;choice++){
                const RT_EnemyTemplate *d=rtDoorEnemyTemplates+rtDoorEnemyTemplateIds[stage][choice];
                if(d->kind!=4)continue;
                for(u16 pose=77;pose<=85;pose++){
                    const RT_PackedSpritePose *graphic=g->enemy(d->kind,d->initialByte4,pose);
                    if(graphic && graphic->pose)
                        RT_packedPreload(graphic->pose->patterns,graphic->pose->tileCount*16);
                }
                break;
            }
            /* Rank costumes from the complete authored spawn grid. Cache
             * common firing/door cycles before rare costumes, independent of
             * the player's current location or this frame's actor order. */
            u16 weights[2][128]={{0}};
            if(scene->spawnLevel)for(u16 n=0;n<scene->spawnLevel->eventCount;n++){
                const RT_EnemySpawn *spawn=scene->spawnLevel->events+n;
                if(spawn->definition>=rtGridEnemyTemplateCount)continue;
                const RT_EnemyTemplate *d=rtGridEnemyTemplates+spawn->definition;
                if(d->kind==4 || d->kind==8)weights[d->kind==8][d->initialByte4/2]++;
            }
            static const u16 guardPriority[]={91,92,93,94,95,96,
                204,205,206,207,77,78,79,80,81,82,83,84,85,69,70,71,72,73,74,75,76,0,1,2,3,4,5};
            for(u16 rank=0;rank<256;rank++){
                u16 best=0,color=0;u8 bank=0;
                for(u16 k=0;k<128;k++)for(u8 b=0;b<2;b++)
                    if(weights[b][k]>best){best=weights[b][k];color=k*2;bank=b;}
                if(!best)break;
                weights[bank][color/2]=0;
                for(u8 f=0;f<sizeof guardPriority/sizeof guardPriority[0];f++){
                    const RT_PackedSpritePose *graphic=g->enemy(bank?8:4,(u8)color,guardPriority[f]);
                    if(graphic && graphic->pose)
                        RT_packedPreload(graphic->pose->patterns,graphic->pose->tileCount*16);
                }
            }
        }
        for(u8 p=0;p<scene->players.home.players;p++){
        u8 identity=scene->players.selection.agent[p],costume=scene->players.selection.costume[p];
        const RT_SpritePose *pose=g->agents[identity?1+costume:(costume?3:0)]+priority[i];
        RT_packedPreload(pose->patterns,pose->tileCount*16);
        }
    }
}
#endif
