#include "rt_campaign_completion_scene.h"
#include "rt_campaign_scene_collision.h"
#include "rt_ending_credits.h"
#ifdef __m68k__
#include "rt_stage_raster.h"
#include "rt_stage_transition_raster.h"
extern VoidCallback *vintCB,*vblankCB;
static VoidCallback *savedVInt,*savedVBlank;
static RT_CompletionScene *volatile owner;
static RT_EndingCredits endingCredits;
static void writeCredit(u8 plane,u8 column,u16 entry){VDP_setTileMapXY(plane?BG_B:BG_A,entry,column+2,27); }
u8 RT_completionSceneSetCredits(RT_CompletionScene *s,u16 credits){if(!s || owner!=s || s->fault || !s->clock.ending || s->background!=rtSceneBackgrounds+5)return 0;RT_endingCreditsPrepare(&endingCredits,credits);return 1;}
static void sceneVInt(void)
{
 RT_CompletionScene *s=owner;if(!s)return;
 if(s->enableOnVInt){s->enableOnVInt=0;s->visible=1;VDP_setEnable(TRUE);}
 if(s->visible)RT_serviceStageTransitionRasterVInt();
}
static void scenePublish(void)
{
 RT_CompletionScene *s=owner;if(!s || !s->pending){RT_releaseStageTransitionRasterIO();return;}
 s->pending=0;
 u8 published=RT_sceneArtPublish(&s->lowArt,&s->batch);
 RT_releaseStageTransitionRasterIO();
 if(!published){s->fault=6;return;}
 s->publicationLine=GET_VCOUNTER;
 if(!GET_VDP_STATUS(VDP_VBLANK_FLAG))s->latePublications++;
 s->lastRefresh=vtimer;
}
static u8 begin(RT_CompletionScene *s,u8 selector,u8 ending,void (*beginIO)(void),void (*endIO)(void))
{
 static const u8 groups[11]={5,1,2,3,4,5,3,4,3,4,6};
 if(!s || selector>10 || owner || (!beginIO != !endIO) || (beginIO && DMA_getQueueSize()))return 0;
 *s=(RT_CompletionScene){0};RT_spritesBegin(&s->batch);RT_endingCreditsReset(&endingCredits);
 s->background=rtSceneBackgrounds+groups[selector]-1;
 s->highArt=rtTransitionArt+groups[selector]-1;
 for(u8 i=0;i<48;i++)s->highActors.objects[i].kind=255;
 u8 spawned;
 if(!RT_transitionSceneConstruct(&s->highActors,selector,&spawned) ||
    !RT_sceneCastInitialize(&s->lowActors,selector) ||
    !RT_transitionClockBegin(&s->clock,ending)){s->fault=1;return 0;}
 u16 first=s->highArt->upperTile+(s->highArt->tileCount>256?s->highArt->tileCount-256:0);
 if(!RT_sceneArtInitialize(&s->lowArt,first,160)){s->fault=2;return 0;}
 RT_stopStageRaster();RT_stopStageTransitionRaster();VDP_setEnable(FALSE);
 /* The IO path rejected queued work above. Even an empty SDK flush takes
  * the Z80 bus outside the caller's window, interrupting live PCM. */
 if(!beginIO)DMA_flushQueue();
 savedVInt=vintCB;savedVBlank=vblankCB;owner=s;
 SYS_setVIntCallback(sceneVInt);SYS_setVBlankCallback(scenePublish);
 if(!RT_sceneBackgroundUploadIO(s->background,beginIO,endIO) || !RT_transitionArtUploadIO(s->highArt,beginIO,endIO)){
  s->fault=3;return 0;
 }
 return 1;
}
u8 RT_completionSceneBegin(RT_CompletionScene *s,u8 selector)
{return begin(s,selector,0,0,0);}
u8 RT_completionEndingBegin(RT_CompletionScene *s,u8 selector)
{return begin(s,selector,1,0,0);}
u8 RT_completionEndingBeginIO(RT_CompletionScene *s,u8 selector,void (*beginIO)(void),void (*endIO)(void))
{return begin(s,selector,1,beginIO,endIO);}
/* Low sprites append to the high prefix without changing its commands (only
 * the terminal link). Keep the prefix while every high geometry input matches.
 * Timers alone do not invalidate static portraits; a changed kind does. */
static u8 prepareHigh(RT_CompletionScene *s)
{
 /* Group5 alternates its portrait kinds every eight services. Rebuilding its
  * small SAT directly avoids cache maintenance on this already-fast path. */
 if(s->highArt==rtTransitionArt+4 && s->highActors.count)
  return RT_transitionSpritesPrepare(s->highArt,&s->highActors,&s->batch,s->scratch);
 u8 same=s->highCached && s->highPopulation==s->highActors.count;
 for(u8 i=0;same && i<RT_NATIVE_STAGE_CAPACITY;i++){
  const RT_Anchor *a=s->highActors.objects+i;
  const RT_CompletionGeometry *key=s->highGeometry+i;
  if(a->kind!=key->kind || (a->kind!=255 &&
      (a->x!=key->x || a->y!=key->y || a->visible!=key->visible || a->layer!=key->layer)))same=0;
 }
 if(same){
  s->batch.count=s->highCount;s->batch.failed=0;s->batch.queuedBytes=0;
  if(s->highCount)s->batch.commands[s->highCount-1].link=0;
  else s->batch.commands[0]=(RT_SpriteCommand){0};
  return 1;
 }
 s->highCached=0;
 if(!RT_transitionSpritesPrepare(s->highArt,&s->highActors,&s->batch,s->scratch))return 0;
 for(u8 i=0;i<RT_NATIVE_STAGE_CAPACITY;i++){
  const RT_Anchor *a=s->highActors.objects+i;
  RT_CompletionGeometry *key=s->highGeometry+i;key->kind=a->kind;
  if(a->kind!=255){key->x=a->x;key->y=a->y;key->visible=a->visible;key->layer=a->layer;}
 }
 u32 occupancy=0;
 for(u16 row=0;row<224;row++){occupancy+=s->scratch[row];s->highRowBudget[row]=occupancy;}
 s->highPopulation=s->highActors.count;s->highCount=s->batch.count;s->highCached=1;
 return 1;
}
/* Optional per-frame SAT extension (title menu text sprites). */
void (*rtCompletionSatDecorate)(RT_SpriteBatch *batch);
u8 RT_completionSceneStepIO(RT_CompletionScene *s,
 void (*beginIO)(u8),void (*endIO)(void))
{
 if(!s || owner!=s || s->fault || (!!beginIO!=!!endIO))return 0;
 if(s->clock.complete)return 2;
 if(!RT_transitionFrameRequest(&s->clock) ||
    RT_transitionSceneStep(&s->highActors,s->clock.remaining,!s->clock.ending)==255 ||
    RT_sceneCastStep(&s->lowActors,RT_sceneCollisionProbe,0)==255 ||
    !prepareHigh(s) ||
    !RT_sceneArtPrepareBudget(&s->lowArt,&rtSceneArt,&s->lowActors,s->highArt,&s->batch,s->scratch,
        s->highCached?s->highRowBudget:NULL)){
  s->fault=4;return 0;
 }
 if(rtCompletionSatDecorate)rtCompletionSatDecorate(&s->batch);
 if(beginIO){
  u16 end=s->lowArt.dirtyEnd<s->lowArt.used?s->lowArt.dirtyEnd:s->lowArt.used;
  beginIO(s->lowArt.dirty && s->lowArt.dirtyFirst<end?end-s->lowArt.dirtyFirst:0);
 }
 RT_endingCreditsCommit(&endingCredits,writeCredit);
 if(!s->frames){
  u8 published=RT_sceneArtPublish(&s->lowArt,&s->batch) &&
     RT_startStageTransitionRasterPalettes(s->background->palettes,
       s->background->footerFirst,s->background->footerCount);
  if(endIO)endIO();
  if(!published){s->fault=5;return 0;}
  s->enableOnVInt=1;while(s->enableOnVInt);
  s->firstRefresh=s->lastRefresh=vtimer;
  s->publicationLine=GET_VCOUNTER;
 }else{
  s->pending=1;
  if(beginIO)SYS_doVBlankProcessEx(IMMEDIATELY);else SYS_doVBlankProcess();
  if(endIO)endIO();
 }
 if(s->fault)return 0;
 if(RT_getStageTransitionRasterStats()->fault){
  /* Only the interactive title installs this decorator. Its sound-driver
   * loads and menu redraws can cross a palette deadline; the next VInt
   * restores the complete palette sequence. Do not permanently freeze the
   * menu for that transient miss. Ordinary cutscenes retain strict checks. */
  if(rtCompletionSatDecorate){
   extern volatile u32 rtTitleMenuRasterRecoveries;
   if(RT_acknowledgeStageTransitionRasterFault())rtTitleMenuRasterRecoveries++;
  }else{s->fault=7;return 0;}
 }
 s->frames++;
 RT_TransitionResult result=RT_transitionFramePresented(&s->clock);
 if(result==RT_TRANSITION_INVALID){s->fault=8;return 0;}
 return result==RT_TRANSITION_COMPLETE?2:1;
}
u8 RT_completionSceneStep(RT_CompletionScene *s)
{return RT_completionSceneStepIO(s,0,0);}
void RT_completionSceneEnd(RT_CompletionScene *s)
{
 if(!s || owner!=s)return;
 s->visible=s->enableOnVInt=s->pending=0;RT_endingCreditsReset(&endingCredits);
 RT_stopStageTransitionRaster();VDP_setEnable(FALSE);
 SYS_disableInts();owner=0;
 SYS_setVIntCallback(savedVInt);SYS_setVBlankCallback(savedVBlank);
 SYS_enableInts();
}
#endif
