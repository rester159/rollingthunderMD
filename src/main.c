#include "rt_home_progress.h"
#include "rt_save.h"
#include "rt_cart_probe.h"
#include "rt_attract.h"
#include "rt_rom_layout.h"
#include "rt_packed_cache.h"
#include "rt_coop_resident_art.h"
#include "rt_coop_resident_art.h"
extern unsigned char rtHomeStartingCredits;
#include "rt_title_menu.h"
#include <genesis.h>
extern u32 rtSpriteBirthEpoch;
#include "rt_high_score.h"
#include "rt_high_score_input.h"
#include "rt_game_over_panel_upload.h"
#include "rt_high_score_assets.h"
#include "rt_campaign_ending.h"
#include "rt_ending_admission.h"
#include "rt_pcm_scene_frame.h"
#include "rt_vblank_policy.h"
#include "rt_stage_raster.h"
#include "rt_stage_transition_raster.h"
#include "rt_music_playback.h"
#include "rt_game_music.h"
#include "rt_segmented_music_playback.h"
#include "rt_z80_bus_policy.h"
#include "rt_segmented_pcm.h"
#include "rt_final_audio_locations.h"
#define RT_GAMEPLAY_HUD 1
#define RT_BANKED_CAMPAIGN 1
#define RT_STREAM_TERRAIN 1
#include <genesis.h>
#include "rt_home_mode.h"
#include "rt_campaign_loop.h"
#include "rt_survival_loop.h"
#include "rt_survival_view.h"
#include "rt_survival_retire_native.h"
#include "rt_campaign_sprites.h"
#include "rt_frame_commit.h"
#include "rt_frame_pacer.h"
#include "rt_arcade_session.h"
#include "rt_campaign_score.h"
#include "rt_campaign_completion_scene.h"
#include "rt_campaign_completion_monitor.h"
#ifdef RT_GAMEPLAY_HUD
#include "rt_campaign_hud_display.h"
#include "rt_campaign_hud_banks.h"
static RT_CampaignHudDisplay hudDisplay;
static RT_CampaignHudState hudState;
#endif
#ifdef RT_BANKED_CAMPAIGN
#include "rt_stage_bank_image.h"
#include "rt_campaign_bank_pages.h"
#endif
#ifdef RT_CAMPAIGN_BANKS
#include "rt_campaign_area01.h"
#endif
static RT_CampaignScene scene;
static RT_SurvivalWindow survivalWindow;
static RT_SurvivalWorld survivalWorld;
static RT_SurvivalDisplay survivalDisplay;
static u8 survival,survivalRetirePending,rtSinceSkip,homeAutoContinue;
volatile u32 rtAppSurvivalRetirements;
static RT_CampaignUpdate update;
static RT_CampaignDisplay view;
static RT_CampaignSprites sprites;
#ifdef RT_STREAM_TERRAIN
static RT_DisplayStream terrainStream;
#endif
volatile u16 rtAppState,rtAppFault;
volatile u32 rtAppUpdates,rtAppPresentations,rtAppRefreshes;
volatile u32 rtAppSkippedPresentations,rtAppMaxUpdateLag;
static RT_ArcadeSession arcade;
static RT_CampaignScore score;
static RT_CampaignBonus bonus;
typedef struct {
    u8 panel[672],published[672];
    RT_HighScoreInput input;
    RT_GameOverPanelUpload upload;
} RT_HighWorkspace;
#ifdef RT_LINEAR_ROM
/* These screens are mutually exclusive. endHud drains queued sprite DMA
 * before the high-score or completion code initializes its own workspace. */
static union {struct {RT_CompletionScene scene;RT_HighWorkspace high;} screens;RT_PackedWorkspace prefetch;} screenWorkspace;
_Static_assert(sizeof(RT_PackedWorkspace)<=sizeof(RT_CompletionScene)+sizeof(RT_HighWorkspace),"Shared screens must not increase the RAM budget");
#define highPanel screenWorkspace.screens.high.panel
#define highPublished screenWorkspace.screens.high.published
#define highInput screenWorkspace.screens.high.input
#define highUpload screenWorkspace.screens.high.upload
#define completion screenWorkspace.screens.scene
void *RT_sceneDecodeWorkspace(void){return &screenWorkspace.screens.high;}
_Static_assert(sizeof(RT_HighWorkspace)>=1024,"Scene decode scratch fits inactive score screen");
extern void RT_campaignSpritesPreload(const RT_CampaignSprites*,const RT_CampaignScene*);
#else
static RT_CompletionScene completion;
static RT_HighWorkspace highWorkspace;
#define highPanel highWorkspace.panel
#define highPublished highWorkspace.published
#define highInput highWorkspace.input
#define highUpload highWorkspace.upload
#endif
RT_CompletionScene *RT_titleMenuStorage(void){return &completion;}
/* Published source events await the native HUD/audio consumers. Static clear
 * text is implemented; bonus timer/score HUD and sound remain unimplemented. */
volatile u8 rtAppBonusEvents;
RT_TransitionCommands rtAppTransitionCommands;
static u8 transitionSelector,transitionEffect;
static u32 readyStarted;
static u32 lastRefresh;
static RT_FramePacer pacer;
static u8 tick;
volatile u8 rtAppPaused;
static u16 pausePrevious[2],pausePalette[64];
static void saveProgress(void);
static u8 beginAttract(void);
static u8 attractScores(void);
static u32 attractStarted;
static u16 attractPrevious;
static volatile u16 attractEdges;
static JoyEventCallback *attractSavedJoy;
extern VoidCallback *vintCB;
static VoidCallback *attractLoadPrevious;
static u8 attractLoadHook;
static void attractLoadVInt(void)
{
    if(attractLoadPrevious)attractLoadPrevious();
    /* Stage preparation spans several refreshes without a VBlank process.
     * Poll while the screen is disabled; normal rendering owns input later. */
    if(!(VDP_getReg(1)&0x40))JOY_update();
}
static void endAttractLoad(void)
{
    if(attractLoadHook){SYS_setVIntCallback(attractLoadPrevious);attractLoadHook=0;}
}
static void attractJoy(u16 joy,u16 changed,u16 state)
{
    if(joy==JOY_1)attractEdges|=changed&state&(BUTTON_START|BUTTON_A|BUTTON_B);
    if(attractSavedJoy)attractSavedJoy(joy,changed,state);
}
static void releaseAttractInput(void)
{JOY_setEventHandler(attractSavedJoy);attractEdges=0;}
static u16 held;
static RT_MusicPlayback highMusic;
static RT_MusicSequence highSequence;
static u8 highAck;
volatile u32 rtHighAudioStarted,rtHighAudioStopped,rtHighAudioRestored;
static u16 highPreviousBank;static u32 highPreviousWord;
static RT_MusicPlayback finalMusic;
static RT_MusicSequence finalSequence;
static u8 finalAck;
volatile u16 rtFinalCommandSeen,rtFinalSourceStatus,rtFinalStatusAvailable,rtFinalAdmissionReady;
volatile u32 rtFinalRegisterWrites,rtFinalCommandRefresh,rtFinalSourceEnd;
static void finalRegister(u8 reg,u8 value,void *context)
{(void)reg;(void)value;(void)context;rtFinalRegisterWrites++;}
static u8 serviceFinalMusic(void)
{
    if(!finalMusic.active)return 1;
    u32 now=vtimer;
    RT_MusicRefreshResult result;
    do{result=RT_serviceMusicPlayback(&finalMusic,now,8,1024,finalRegister,0);}
    while(result==RT_MUSIC_REFRESH_PENDING);
    u8 status;bool drained;
    if(result==RT_MUSIC_REFRESH_FAULT || !RT_readMusicPlaybackStatus(&finalMusic,&status,&drained))return 0;
    rtFinalSourceStatus=status;rtFinalStatusAvailable=1;rtFinalSourceEnd=finalMusic.clock.endRefresh;
    return 1;
}
static u8 consumeFinalCommands(void)
{
    if(scene.stage->story!=1 || scene.stage->area!=4)return 1;
    if(update.battle.enemies.eventCount>RT_ENEMY_PASS_EVENT_CAPACITY)return 0;
    for(u8 i=0;i<update.battle.enemies.eventCount;i++){
        if(update.battle.enemies.events[i].command!=7 || rtFinalCommandSeen)continue;
        RT_gameMusicStop();
        if(!RT_beginMusicPlaybackWindows(&finalMusic,&finalSequence,&rtFinalMusicBank,7,&finalAck,vtimer,
            RT_FINAL_VICTORY_ADDRESS,RT_FINAL_VICTORY_SAMPLES,RT_ROM_BYTES,RT_FINAL_VICTORY_WINDOWS,1024,finalRegister,0))return 0;
        rtFinalCommandSeen=1;rtFinalCommandRefresh=vtimer;
    }
    return 1;
}

static RT_EndingAdmission finalAdmission;
static RT_CampaignEnding finalEnding;
static u32 finalIORefresh;
volatile u16 rtFinalAdmissionActions,rtFinalEndingFrames,rtFinalEndingLate,rtFinalEndingPhase,rtFinalEntryReady;
volatile u32 rtFinalEndingStarted,rtFinalVictoryServices;
static void endFinalFlow(void);
volatile u16 rtHighStarted,rtHighDone,rtHighPlayer,rtHighPhase,rtHighMusicRequest;
volatile u32 rtHighSteps;
static u8 beginHighEntry(void);
static void endHighClock(void);
enum {APP_MENU,APP_PLAY,APP_CONTINUE,APP_UNFINISHED,APP_READY,APP_BONUS,APP_TRANSITION,APP_FINAL_WAIT,APP_ENDING,APP_HIGH_ENTRY,APP_ATTRACT};

/* Campaign publication owner. Once armed, the DMA queue and display descriptor
 * are immutable until finishPipeline; only the next logical update overlaps. */
extern VoidCallback *vintCB,*vblankCB;
static VoidCallback *pipelinePrevious;
static volatile u8 pipelinePending;
static u8 pipelineActive,pipelineInFlight,pipelineReprojected;
volatile u32 rtAppReusedPresentations,rtAppFullPresentations;
volatile u32 rtPipelineCommits,rtPipelineSpills;
volatile u16 rtPipelineEndLine,rtPipelineMaxBytes;
static void pipelineVInt(void)
{
    while(GET_VCOUNTER<225);
    RT_setZ80LocalBusOnly(TRUE);
    if(pipelinePrevious)pipelinePrevious();
    if(pipelinePending){
        Z80_enableBusProtection();DMA_flushQueue();Z80_disableBusProtection();
        RT_displayCommit(&view.display);
        if(vblankCB)vblankCB();
        rtPipelineEndLine=GET_VCOUNTER;
        if(rtPipelineEndLine<224)rtPipelineSpills++;
        rtPipelineCommits++;rtAppPresentations++;
        if(pipelineReprojected)rtAppReusedPresentations++;else rtAppFullPresentations++;
        pipelinePending=0;
    }
    JOY_update();
    RT_setZ80LocalBusOnly(FALSE);
}
static void beginPipeline(void)
{
    if(pipelineActive)return;
    SYS_disableInts();pipelinePrevious=vintCB;pipelineActive=1;
    SYS_setVIntCallback(pipelineVInt);SYS_enableInts();
}
static void finishPipeline(void)
{
    if(!pipelineInFlight)return;
    while(pipelinePending){__asm__ volatile("nop");}
    RT_campaignDisplayCommitted(&view);pipelineInFlight=0;
}
static void endPipeline(void)
{
    if(!pipelineActive)return;
    finishPipeline();SYS_disableInts();SYS_setVIntCallback(pipelinePrevious);
    pipelineActive=0;SYS_enableInts();
}
static void armPipeline(void)
{
    u16 bytes=DMA_getQueueTransferSize();
    if(bytes>rtPipelineMaxBytes)rtPipelineMaxBytes=bytes;
    pipelineInFlight=1;
    __asm__ volatile("" ::: "memory");
    pipelinePending=1;
}

/* Every sound byte of one update, in source phase order. */
static void postUpdateSounds(const RT_CampaignUpdate *u)
{
    u8 bytes[48],n=0;
    /* The final victory/ending audio owns the Z80 from its command onward. */
    if(!u->updated || finalMusic.active || rtFinalCommandSeen || (rtArcadeAttract && (rtArcadeDips&RT_DIP_DEMO_MUTE)))return;
    for(u8 p=0;p<2;p++){bytes[n++]=u->players.sounds[p];bytes[n++]=u->players.weapons[p].sound;}
    bytes[n++]=u->exitSounds[0];bytes[n++]=u->exitSounds[1];
    for(u8 i=0;i<u->battle.enemies.eventCount && n<48;i++)bytes[n++]=u->battle.enemies.events[i].sound;
    for(u8 i=0;i<u->battle.projectiles.soundCount && n<48;i++)bytes[n++]=u->battle.projectiles.sounds[i].sound;
    for(u8 i=0;i<u->objects.count && n<48;i++)bytes[n++]=u->objects.events[i].sound;
    RT_gameSoundPost(bytes,n);
}
static void endHud(void){
    RT_gameMusicStop();
    RT_spritesDiscardPrepared();
    endPipeline();
    DMA_flushQueue();saveProgress();
#ifdef RT_LINEAR_ROM
    RT_packedWorkspaceEnd();
#endif
#ifdef RT_GAMEPLAY_HUD
    RT_campaignHudDisplayEnd(&hudDisplay);
#endif
}
static u8 updateHud(void){
#ifdef RT_GAMEPLAY_HUD
    u16 ammo[2],machineAmmo[2];u8 health[2],machineMask=0;
    for(u8 p=0;p<2;p++){
        const RT_Agent *a=scene.players.agents+p;
        ammo[p]=rtSession.mode==RT_MODE_HOME?a->ammoPrimary:
            a->weaponMode==1?a->ammoAlternate:a->ammoPrimary;
        machineAmmo[p]=a->ammoAlternate;
        if(a->weaponMode==1)machineMask|=1<<p;
        health[p]=scene.players.combat[p].health;
    }
    return RT_campaignHudUpdate(&hudState,&score,scene.timer.remaining,ammo,health,
        scene.players.home.credits,rtSession.mode==RT_MODE_ARCADE?arcade.reserves:0,
        machineAmmo,machineMask);
#else
    return 1;
#endif
}
static void menu(void){endHighClock();endHud();VDP_setEnable(FALSE);if(finalMusic.active && !RT_stopMusicPlayback(&finalMusic)){rtAppFault=94;return;}RT_beginModeSelect();rtAppState=APP_MENU;}
static void notice(const char *title,const char *detail,u8 state){
    endHighClock();endFinalFlow();
    endHud();VDP_setEnable(FALSE);if(finalMusic.active && !RT_stopMusicPlayback(&finalMusic))rtAppFault=94;RT_beginModeSelect();VDP_clearPlane(BG_A,TRUE);VDP_clearPlane(BG_B,TRUE);
    PAL_setColor(0,0);VDP_setTextPalette(PAL0);
    VDP_drawText(title,5,10);VDP_drawText(detail,5,13);
    if(state!=APP_CONTINUE)VDP_drawText("START: RETURN TO MENU",5,18);
    rtAppState=state;held=0xFFFF;
}
u8 RT_prepareWorldBank(u8 story,u8 area)
{
    if(story>1 || area>=5)return 0;
    u8 page=rtCampaignBankPages[story*5+area];if(!page)return 0;
    endHud();VDP_setEnable(FALSE);DMA_flushQueue();
    memset(&sprites,0,sizeof sprites);memset(&view,0,sizeof view);
    memset(&terrainStream,0,sizeof terrainStream);
    if(!RT_stageBankSelect(page))return 0;
    const RT_StageBankImage *image=(const RT_StageBankImage*)(RT_STAGE_BANK_BASE+rtCampaignBankOffsets[story*5+area]);
    if(image->magic!=RT_STAGE_BANK_MAGIC || image->story!=story || image->area!=area ||
       !image->worldPageCount || (u32)image->worldCells<0x100000UL ||
       (u32)image->worldCells+(u32)image->worldPageCount*512>0x400000UL)return 0;
    return RT_worldBind(image->worldCells,image->worldDirectory,image->worldPageCount,story,area);
}
static u8 video(void){
    survivalRetirePending=0;endHud();
#ifdef RT_BANKED_CAMPAIGN
    if(scene.stage->story>1 || scene.stage->area>=5)return 0;
    u8 bankIndex=scene.stage->story*5+scene.stage->area;
    if(!rtCampaignBankPages[bankIndex])return 0;
    /* End transition callbacks before entering here. Drain old ROM DMA before
     * replacing its address window, then discard every stage-owned cache. */
    VDP_setEnable(FALSE);DMA_flushQueue();
    memset(&sprites,0,sizeof sprites);memset(&view,0,sizeof view);
    memset(&terrainStream,0,sizeof terrainStream);
    if(!RT_stageBankSelect(rtCampaignBankPages[bankIndex]))return 0;
    const RT_StageBankImage *image=(const RT_StageBankImage*)(RT_STAGE_BANK_BASE+rtCampaignBankOffsets[bankIndex]);
    if(image->magic!=RT_STAGE_BANK_MAGIC)return 0;
    const RT_CampaignGraphics *graphics=image->graphics;
    const RT_DisplayPatch (*patches)[2]=image->patches;
    if(!graphics || !graphics->stage || !patches)return 0;
#else
    const RT_CampaignGraphics *graphics=&rtCampaignOpeningGraphics;
    const RT_DisplayPatch (*patches)[2]=rtDisplayDoorPatches;
    if(scene.stage->story || scene.stage->area){
#ifdef RT_CAMPAIGN_BANKS
    if(scene.stage->story==0 && scene.stage->area==1){
        graphics=&rtCampaignArea01Graphics;patches=area01_rtDisplayDoorPatches;
    }else return 0;
#else
    return 0;
#endif
    }
 #endif
    u8 displayReady;
    if(survival)displayReady=RT_survivalViewInitialize(&view,&survivalDisplay,&scene,&survivalWorld,graphics->stage,patches);
    else
#ifdef RT_STREAM_TERRAIN
       displayReady=RT_campaignDisplayInitializeStreamed(&view,&scene,graphics->stage,patches,&terrainStream,640);
#else
       displayReady=RT_campaignDisplayInitialize(&view,&scene,graphics->stage,patches);
#endif
    if(!displayReady || !(survival?RT_campaignSpritesInitializeGenerated(&sprites,&view,graphics,640):
        RT_campaignSpritesInitializeBank(&sprites,&view,graphics)))return 0;
#ifdef RT_LINEAR_ROM
    RT_packedWorkspaceBegin(&screenWorkspace.prefetch,sizeof screenWorkspace.prefetch);
    /* A resident background never reads the streaming cell ring or tile
     * lookup arrays. Their control fields remain intact; endHud drains all
     * sprite transfers before the next stage initializes these arrays. */
    if(scene.players.home.players==1 && !survival && terrainStream.resident){
        RT_packedBorrow(0,terrainStream.cells,4096);
        RT_packedStaging(terrainStream.cells+2048,4096);
        RT_packedBorrow(1,terrainStream.tiles.mapping,
            (u8*)&terrainStream.tiles.patternCount-(u8*)terrainStream.tiles.mapping);
    }
    RT_campaignSpritesPreload(&sprites,&scene);
#endif
#ifdef RT_GAMEPLAY_HUD
    if(scene.stage->story>1 || scene.stage->area>=5)return 0;
    const RT_CampaignHudArt *art=rtCampaignHudBanks[scene.stage->story*5+scene.stage->area];
    if(!art)return 0;
    VDP_setEnable(FALSE);DMA_flushQueue();
    if(!RT_campaignSpritesUseHud(&sprites,scene.players.home.players) ||
       !RT_coopResidentBegin(&sprites,scene.players.home.players) ||
       !RT_campaignHudBegin(&hudState,art->initial,rtSession.mode==RT_MODE_HOME,scene.players.home.players) ||
       !updateHud() || !RT_campaignHudDisplayBegin(&hudDisplay,art,1440) ||
       !RT_campaignHudDisplayQueue(&hudDisplay,&hudState.hud))return 0;
    SYS_doVBlankProcess();VDP_setEnable(TRUE);
#endif
    lastRefresh=vtimer;RT_framePacerReset(&pacer,lastRefresh);return 1;
}
static u8 advanceFinalAdmission(void)
{
    RT_EndingAdmissionAction action=RT_endingAdmissionStep(&finalAdmission,
        rtFinalStatusAvailable,(u8)rtFinalSourceStatus,!pipelineInFlight && !DMA_getQueueSize(),1);
    if(action==RT_ENDING_ADMISSION_WAIT)return 1;
    if(action==RT_ENDING_ADMISSION_INVALID)return 0;
    if(action==RT_ENDING_ADMISSION_EVENTS){
        endHud();RT_stopStageRaster();RT_stopStageTransitionRaster();VDP_setEnable(FALSE);
        if(!RT_stopMusicPlaybackCommand(&finalMusic,&finalAck) ||
           RT_serviceMusicSequence(&finalSequence,&rtFinalMusicBank,1024,finalRegister,0)==RT_MUSIC_CONTROL_INVALID)return 0;
        update.battle.enemies.eventCount=0;update.battle.projectiles.soundCount=0;
        update.objects.count=0;scene.players.agents[0].shotRequest=0;scene.players.agents[1].shotRequest=0;
    }else if(action==RT_ENDING_ADMISSION_MAP){
        DMA_flushQueue();RT_spritesDiscardPrepared();
    }else if(action==RT_ENDING_ADMISSION_SCENE){
        RT_completionSceneEnd(&completion);
        scene.ready=0;
        memset(&scene.enemies,0,sizeof scene.enemies);
        memset(&scene.projectiles,0,sizeof scene.projectiles);
        memset(&scene.objects,0,sizeof scene.objects);
        VDP_clearPlane(BG_A,FALSE);VDP_clearPlane(BG_B,FALSE);
    }else if(action==RT_ENDING_ADMISSION_READY){
        u8 player=scene.exitPlayer;
        if(player>=scene.players.home.players)return 0;
        RT_setJoyBeforeVBlankCallback(TRUE);
        if(!RT_campaignEndingBegin(&finalEnding,&completion,&score,
            scene.players.agents+player,scene.players.home.spawn+player,
            rtSession.mode==RT_MODE_ARCADE?&arcade.reserves:0,player,scene.timer.remaining))return 0;
        rtFinalVictoryServices=finalMusic.clock.services+1;
        if(!RT_beginMusicPlaybackWindows(&finalMusic,&finalSequence,&rtFinalMusicBank,9,&finalAck,vtimer,
            RT_FINAL_ENDING_AFTER_VICTORY_ADDRESS,RT_FINAL_ENDING_AFTER_VICTORY_SAMPLES,RT_ROM_BYTES,2,1024,finalRegister,0))return 0;
        rtFinalAdmissionReady=1;rtFinalEndingStarted=vtimer;
        finalIORefresh=0;rtAppState=APP_ENDING;return 1;
    }
    rtFinalAdmissionActions|=1u<<action;
    return RT_endingAdmissionAcknowledge(&finalAdmission,action);
}
static void finalFrameBegin(u8 tiles)
{(void)tiles;RT_beginExclusivePcmSceneFrameUntil(&finalIORefresh,225);}
static void finalLoadBegin(void){finalFrameBegin(160);}
static void endFinalFlow(void)
{
    RT_campaignEndingEnd(&finalEnding);
    RT_setJoyBeforeVBlankCallback(FALSE);
}
static u8 advanceFinalEnding(void)
{
    if(rtSession.mode==RT_MODE_HOME && finalEnding.sceneLoaded &&
       !RT_completionSceneSetCredits(&completion,scene.players.home.credits))return 0;
    RT_EndingResult result=RT_campaignEndingStep(&finalEnding,vtimer,
        rtFinalStatusAvailable,(u8)rtFinalSourceStatus,finalFrameBegin,finalLoadBegin,RT_endExclusivePcmSceneFrame);
    rtFinalEndingFrames=completion.frames;rtFinalEndingLate=completion.latePublications;
    rtFinalEndingPhase=finalEnding.flow.phase;
    if(finalEnding.earnedReserves){
        u32 credits=(u32)scene.players.home.credits+finalEnding.earnedReserves;
        scene.players.home.credits=credits>65535?65535:credits;
    }
    rtAppBonusEvents=finalEnding.events.bonusEvents;
    if(result==RT_ENDING_FAULT)return 0;
    if(result==RT_ENDING_ENTRY_READY){
        rtFinalEntryReady=1;endFinalFlow();
        if(!beginHighEntry())return 0;
    }
    return 1;
}

/* Source qualification/initials routines with native ROM-bank/VDP ownership. */
static u8 highRecords[50],highScores[8],highPlayer,highRemaining,highArea;
/* Menu saves are event driven. Rechecking journal CRCs on every idle frame
 * halves the cast animation rate even when no SRAM write is needed. */
static u32 menuSaveState(void)
{
    return (u32)rtHomeStartingCredits | ((u32)rtControlLayout<<4) |
        ((u32)RT_gameMusicPsgMuted()<<8) | ((u32)rtArcadeDips<<9) |
        ((u32)rtHomeSaveSlot<<15);
}
static void saveProgress(void)
{
    /* Entry IRQ owns the table until its current initials are complete. */
    if(rtArcadeAttract || highRecords[0x2d])return;
    RT_saveStore(highRecords,score.high,rtHomeStartingCredits,RT_gameMusicPsgMuted(),rtControlLayout);
    RT_homeProfileStore();
}
static VoidCallback *highPreviousVInt;
static u8 highClockInstalled;
static volatile u8 highClockRunning,highInputFault;
static void highEntryVInt(void)
{
    if(highPreviousVInt)highPreviousVInt();
    if(!highClockRunning)return;
    /* Align controller bus access with the PCM-local I/O interval.
     * Capability remains disabled until a PCM owner is actually admitted. */
    while(GET_VCOUNTER<225);
    if(highMusic.active)RT_setZ80LocalBusOnly(TRUE);
    JOY_update();
    if(highMusic.active)RT_setZ80LocalBusOnly(FALSE);
    u8 input[22];
    if(!RT_sampleHighScoreInput(&highInput,JOY_readJoypad(JOY_1),JOY_readJoypad(JOY_2)) ||
       !RT_takeHighScoreInput(&highInput,input) ||
       !RT_stepHighScoreEntry(highRecords,highPanel+(3+highRecords[0x2a]*2)*56+4,input,highPlayer,1,(u8)vtimer)){
        highInputFault=1;highClockRunning=0;return;
    }
    rtHighSteps++;rtHighPhase=highRecords[0x2d];
    if(!highRecords[0x31])highClockRunning=0;
}
static void endHighClock(void)
{
    highClockRunning=0;
    if(highMusic.active){
        if(!RT_stopMusicPlaybackCommand(&highMusic,&highAck))rtAppFault=98;
        rtHighAudioStopped=vtimer;
        rtHighAudioRestored=SYS_getBank(1)==highPreviousBank && *(volatile u32*)0x80000==highPreviousWord;
        if(!rtHighAudioRestored)rtAppFault=98;
    }
    if(!highClockInstalled)return;
    SYS_disableInts();highClockRunning=0;SYS_setVIntCallback(highPreviousVInt);
    highClockInstalled=0;SYS_enableInts();
}
static void beginHighClock(void)
{
    SYS_disableInts();
    if(!highClockInstalled){highPreviousVInt=vintCB;SYS_setVIntCallback(highEntryVInt);highClockInstalled=1;}
    highInputFault=0;highClockRunning=1;SYS_enableInts();
}
static u32 highIORefresh;
static u8 *highRow(void){return highPanel+(3+highRecords[0x2a]*2)*56+4;}
static u8 publishHighPanel(void)
{
    SYS_disableInts();memcpy(highPublished,highPanel,sizeof highPanel);SYS_enableInts();
    u16 cursor=0;
    RT_CompositePanelResult result;
    do{
        result=RT_queueGameOverCompositePanelChanges(&highUpload,highPublished,&cursor,336,2048);
        if(result==RT_COMPOSITE_PANEL_INVALID)return 0;
        /* Publication is independent of the physical-refresh input/entry IRQ.
         * Audio integration must select the exclusive PCM window here
         * only while that driver owns the sound bus. */
        while(vtimer==highIORefresh);
        if(GET_VCOUNTER>225){
            u32 missed=vtimer;while(vtimer==missed);
        }
        while(GET_VCOUNTER<225);
        highIORefresh=vtimer;
        if(highMusic.active)RT_setZ80LocalBusOnly(TRUE);
        SYS_doVBlankProcessEx(IMMEDIATELY);
        if(highMusic.active)RT_setZ80LocalBusOnly(FALSE);
    }while(result!=RT_COMPOSITE_PANEL_QUEUED);
    return 1;
}
static u8 chooseHighPlayer(void)
{
    while(highRemaining){
        highRemaining--;highPlayer=scene.players.home.players-1-highRemaining;
        RT_HighScoreAction action=RT_beginHighScoreEntry(highRecords,highScores,highPlayer,highArea);
        if(action==RT_HIGH_SCORE_INVALID)return 0;
        if(action==RT_HIGH_SCORE_NOT_QUALIFIED)continue;
        u8 musicRequest;
        if(!RT_drawHighScorePanel(highRecords,RT_writeHighScorePanelByte,highPanel) ||
           !RT_armHighScoreEntry(highRecords,highRow(),&musicRequest))return 0;
        /* Preserve a continuing0B request across the two qualifiers. */
        rtHighMusicRequest=musicRequest;
        /* Experimental warm asset preserves measured07/stop/09 history.
         * Loading-gap chip phase remains a separate fidelity validation. */
        if(!highMusic.active){
            highPreviousBank=SYS_getBank(1);highPreviousWord=*(volatile u32*)0x80000;
            rtHighAudioRestored=0;
            if(!RT_beginSegmentedMusicPlayback(&highMusic,&highSequence,
                rtFinalCommandSeen?&rtFinalMusicBank:&rtArcadeMusicBank,musicRequest,&highAck,vtimer,
                (rtFinalCommandSeen?RT_HIGH_WARM_ADDRESS:RT_HIGH_COLD_ADDRESS),1118805,RT_ROM_BYTES,1024,finalRegister,0))return 0;
            rtHighAudioStarted=vtimer;
        }
        RT_initHighScoreInput(&highInput);rtHighPlayer=highPlayer;
        if(!publishHighPanel())return 0;beginHighClock();return 1;
    }
    rtHighDone=1;
    if(rtSession.mode==RT_MODE_ARCADE)return beginAttract();
    menu();return 1;
}
static u8 beginHighEntry(void)
{
    if(!scene.stage || scene.stage->story>1 || scene.stage->area>4)return 0;
    u8 reached=scene.stage->story*5+scene.stage->area+1;
    highArea=reached==10?0x10:reached;
    endFinalFlow();endHud();VDP_setEnable(FALSE);
    if(finalMusic.active && !RT_stopMusicPlaybackCommand(&finalMusic,&finalAck))return 0;
    DMA_flushQueue();
    if(!RT_stageBankSelect(RT_HIGH_SCORE_PAGE))return 0;
    static const RT_SceneBackground background={
        (const u32 *)(RT_HIGH_SCORE_BASE+4176),
        (const u16 (*)[1008])RT_HIGH_MAPS_ADDRESS,
        (const u16 (*)[64])(RT_HIGH_SCORE_BASE+16),610,0,0};
    if(!RT_sceneBackgroundUpload(&background))return 0;
    highUpload=(RT_GameOverPanelUpload){0};
    if(!RT_beginGameOverPanelUpload(&highUpload,(const u8 *)RT_HIGH_SCORE_BASE,RT_HIGH_SCENE_BYTES,
        (const u8 *)RT_HIGH_PANEL_ADDRESS,RT_HIGH_PANEL_BYTES,0))return 0;
    while(!highUpload.ready){
        if(!RT_queueGameOverPanelPatterns(&highUpload,2048))return 0;
        SYS_doVBlankProcess();
    }
    for(u8 p=0;p<2;p++){
        u32 value=score.players[p];if(value>999999)return 0;
        static const u32 places[3]={10000,100,1};
        for(u8 i=0;i<3;i++){
            u8 pair=0;while(value>=places[i]){value-=places[i];pair++;}
            highScores[p*4+i]=((pair/10)<<4)|(pair%10);
        }
        highScores[p*4+3]=0;
    }
    highMusic=(RT_MusicPlayback){0};highSequence=rtFinalCommandSeen?finalSequence:(RT_MusicSequence){0};highAck=0;
    highRemaining=scene.players.home.players;rtHighDone=0;rtHighStarted=1;
    rtAppState=APP_HIGH_ENTRY;
    if(!chooseHighPlayer())return 0;
    VDP_setEnable(TRUE);return 1;
}
static u8 stepHighEntry(const u16 pads[2])
{
    (void)pads;
    if(highInputFault)return 0;
    if(highMusic.active){
        RT_MusicRefreshResult r;
        do{r=RT_serviceMusicPlayback(&highMusic,vtimer,8,1024,finalRegister,0);}
        while(r==RT_MUSIC_REFRESH_PENDING);
        if(r==RT_MUSIC_REFRESH_FAULT)return 0;
    }
    if(!highClockRunning){saveProgress();return chooseHighPlayer();}
    return publishHighPanel();
}

static u8 start(void){
    if(finalMusic.active)return 0;
    finalAdmission=(RT_EndingAdmission){0};finalEnding=(RT_CampaignEnding){0};
    rtFinalAdmissionActions=rtFinalEntryReady=0;
    finalMusic=(RT_MusicPlayback){0};finalSequence=(RT_MusicSequence){0};finalAck=0;
    rtFinalCommandSeen=rtFinalStatusAvailable=rtFinalAdmissionReady=0;rtFinalSourceStatus=255;
    survival=rtSession.mode==RT_MODE_HOME && rtSession.game==RT_HOME_SURVIVAL;
    rtAppSurvivalRetirements=0;
    u8 resume=rtHomeResume && rtSession.mode==RT_MODE_HOME && !survival;
    score=(RT_CampaignScore){.high=score.high};
    RT_CampaignInit initialized=resume?(!RT_homeRestore(&scene,&score)):survival?
        RT_survivalSceneInitialize(&scene,&survivalWindow,&survivalWorld,&rtSession,vtimer,0,rtHomeStartingCredits):
        RT_campaignSceneInitialize(&scene,&rtSession,0,0,0,rtSession.mode==RT_MODE_ARCADE && (rtArcadeDips&RT_DIP_EASY)?1:0,rtSession.mode==RT_MODE_HOME?rtHomeStartingCredits:6);
    if(initialized){
        rtAppFault=1;notice("NATIVE BUILD ERROR","STAGE INITIALIZATION",APP_UNFINISHED);return 0;
    }
    rtHomeResume=0;
    if(!resume)RT_homeCheckpoint(&scene,&score);
    if(rtArcadeAttract==RT_ATTRACT_DEMO)RT_attractDemoInitialize(&scene);
    if(rtSession.mode==RT_MODE_ARCADE && !RT_arcadeSessionInitialize(&arcade,&scene,(rtArcadeDips&RT_DIP_LIVES)?1:0)){rtAppFault=4;return 0;}
    if(!video()){rtAppFault=1;notice("NATIVE BUILD ERROR","STAGE INITIALIZATION",APP_UNFINISHED);return 0;}
    transitionSelector=1;transitionEffect=0;rtAppBonusEvents=0;
    tick=0;rtAppState=APP_PLAY;
    if(rtSession.mode==RT_MODE_ARCADE && !rtArcadeAttract){
        readyStarted=vtimer;rtAppState=APP_READY;
    }
    return 1;
}
/* Arcade attract owns no credits, score entry or save transaction. All demo
 * gameplay is discarded before a real Start. B returns to the mode selector. */
static u8 attractTitle(void)
{
    endHighClock();endHud();RT_titleMenuEnd();RT_completionSceneEnd(&completion);
    if(!RT_titleMenuBegin())return 0;
    RT_titleMenuTextClear();RT_titleMenuTextAt("PRESS START",14,16);
    RT_titleMenuTextCommit();
    rtArcadeAttract=RT_ATTRACT_TITLE;attractStarted=vtimer;rtAppState=APP_ATTRACT;
    return 1;
}
static u8 beginAttract(void)
{
    rtArcadeAttract=RT_ATTRACT_TITLE;attractPrevious=0xffff;attractEdges=0;
    attractSavedJoy=JOY_getEventHandler();JOY_setEventHandler(attractJoy);
    return attractTitle();
}
static u8 attractScores(void)
{
    endHud();endAttractLoad();RT_titleMenuEnd();VDP_setEnable(FALSE);DMA_flushQueue();
    if(!RT_stageBankSelect(RT_HIGH_SCORE_PAGE))return 0;
    static const RT_SceneBackground background={
        (const u32 *)(RT_HIGH_SCORE_BASE+4176),
        (const u16 (*)[1008])RT_HIGH_MAPS_ADDRESS,
        (const u16 (*)[64])(RT_HIGH_SCORE_BASE+16),610,0,0};
    if(!RT_sceneBackgroundUpload(&background))return 0;
    highUpload=(RT_GameOverPanelUpload){0};
    if(!RT_beginGameOverPanelUpload(&highUpload,(const u8 *)RT_HIGH_SCORE_BASE,RT_HIGH_SCENE_BYTES,
        (const u8 *)RT_HIGH_PANEL_ADDRESS,RT_HIGH_PANEL_BYTES,0))return 0;
    while(!highUpload.ready){
        if(!RT_queueGameOverPanelPatterns(&highUpload,2048))return 0;
        SYS_doVBlankProcess();
    }
    if(!RT_drawHighScorePanel(highRecords,RT_writeHighScorePanelByte,highPanel) || !publishHighPanel())return 0;
    VDP_setEnable(TRUE);rtArcadeAttract=RT_ATTRACT_SCORES;
    attractStarted=vtimer;rtAppState=APP_ATTRACT;return 1;
}
static u8 stepAttract(void)
{
    u32 age=vtimer-attractStarted;
    /* Original visible holds: title 512, TOP 5 256, Maboo 192 services.
     * PAL uses the same real-time holds as the arcade's 60 Hz presentation. */
    u16 rate=SYS_isPAL()?50:60;
    if(rtArcadeAttract==RT_ATTRACT_TITLE){
        if(age<(512UL*rate)/60)return RT_titleMenuStep();
        RT_titleMenuEnd();rtArcadeAttract=RT_ATTRACT_DEMO;
        attractLoadPrevious=vintCB;attractLoadHook=1;SYS_setVIntCallback(attractLoadVInt);
        if(!start())return 0;
        attractStarted=vtimer;return 1;
    }
    if(rtArcadeAttract==RT_ATTRACT_SCORES){
        if(age<(256UL*rate)/60){SYS_doVBlankProcess();return 1;}
        VDP_setEnable(FALSE);DMA_flushQueue();
        if(!RT_stageBankSelect(RT_COMPLETION_PAGE) || !RT_completionEndingBegin(&completion,5))return 0;
        rtArcadeAttract=RT_ATTRACT_MABOO;attractStarted=vtimer;return 1;
    }
    if(rtArcadeAttract==RT_ATTRACT_MABOO){
        if(age<(192UL*rate)/60)return RT_completionSceneStep(&completion)==1;
        RT_completionSceneEnd(&completion);rtAttractLoops++;return attractTitle();
    }
    return 0;
}
int main(bool hardReset){
    (void)hardReset;rtAppUpdates=rtAppPresentations=rtAppRefreshes=0;rtAppFault=0;
    rtAppSkippedPresentations=rtAppMaxUpdateLag=0;
    u8 initialHigh[3];if(!RT_initializeHighScores(highRecords,initialHigh))return 0;score.high=3000;
    RT_resetSession();
    if(!RT_cartridgeBankingAvailable()){
        VDP_init();PAL_setColor(15,RGB24_TO_VDPCOLOR(0xFFFFFF));VDP_setTextPalette(PAL0);
        VDP_drawText("THIS EMULATOR OR CARTRIDGE",7,9);
        VDP_drawText("DOES NOT SUPPORT THIS ROM",8,11);
        VDP_drawText("TRY PICODRIVE OR GENESIS PLUS GX",5,15);
        VDP_drawText("7 MB SSF2 BANKING REQUIRED",7,18);
        VDP_setEnable(TRUE);
        while(TRUE)SYS_doVBlankProcess();
    }
    u8 muted=0;
    RT_saveLoad(highRecords,&score.high,&rtHomeStartingCredits,&muted,&rtControlLayout);
    RT_homeProgressLoad();
    RT_resetMarkReady();
    RT_gameMusicSetPsgMuted(muted);menu();
    while(TRUE){
        if(finalMusic.active && !serviceFinalMusic()){
            rtAppFault=92;notice("AUDIO ERROR","SOURCE CLOCK",APP_UNFINISHED);continue;
        }
        if(rtAppState==APP_FINAL_WAIT){
            if(!advanceFinalAdmission()){rtAppFault=95;notice("ENDING ERROR","ADMISSION",APP_UNFINISHED);}
            else if(!finalAdmission.entered){u32 now=vtimer;while(vtimer==now){__asm__ volatile("nop");}}
            continue;
        }
        if(rtAppState==APP_ENDING){
            if(!advanceFinalEnding()){rtAppFault=96;notice("ENDING ERROR","SEQUENCE",APP_UNFINISHED);}
            continue;
        }
        u16 pads[2]={JOY_readJoypad(JOY_1),JOY_readJoypad(JOY_2)};
        if(rtArcadeAttract){
            /* Preserve brief Start/Back presses sampled during bank/video loads. */
            SYS_disableInts();u16 edge=attractEdges;attractEdges=0;SYS_enableInts();
            edge|=pads[0]&~attractPrevious;attractPrevious=pads[0];
            if(edge&(BUTTON_START|BUTTON_A|BUTTON_B)){
                endHud();endAttractLoad();RT_titleMenuEnd();RT_completionSceneEnd(&completion);
                releaseAttractInput();rtArcadeAttract=RT_ATTRACT_OFF;
                if(edge&BUTTON_B)menu();
                else if(!start()){rtAppFault=100;continue;}
                pausePrevious[0]=pads[0];pausePrevious[1]=pads[1];continue;
            }
            if(rtAppState==APP_ATTRACT){
                if(!stepAttract()){rtAppFault=101;releaseAttractInput();rtArcadeAttract=0;notice("ATTRACT ERROR","SCENE",APP_UNFINISHED);}
                continue;
            }
            /* No live input, pausing or credit use during demonstration. */
            pads[0]=pads[1]=0;
            if(rtAttractDemoUpdates>=1536){
                if(!attractScores())rtAppFault=102;
                continue;
            }
        }
        u16 startEdge=(pads[0]&~pausePrevious[0]) |
            (RT_isCoop()?(pads[1]&~pausePrevious[1]):0);
        pausePrevious[0]=pads[0];pausePrevious[1]=pads[1];
        if(rtAppState==APP_PLAY && (startEdge&BUTTON_START)){
            finishPipeline();DMA_flushQueue();
            rtAppPaused^=1;
            if(rtAppPaused){
                saveProgress();PAL_getColors(0,pausePalette,64);
                u16 dim[64];for(u8 i=0;i<64;i++)dim[i]=(pausePalette[i]>>1)&0x666;
                PAL_setColors(0,dim,64,CPU);
            }else{
                PAL_setColors(0,pausePalette,64,CPU);
                lastRefresh=vtimer;RT_framePacerReset(&pacer,lastRefresh);
                for(u8 p=0;p<2;p++)scene.players.previousButtons[p]=pads[p];
            }
        }
        if(rtAppPaused){
            u32 now=vtimer;while(vtimer==now){__asm__ volatile("nop");}
            continue;
        }
        if(rtAppState==APP_HIGH_ENTRY){if(!stepHighEntry(pads)){rtAppFault=97;notice("HIGH SCORE ERROR","ENTRY",APP_UNFINISHED);}continue;}
        if(rtAppState==APP_MENU){
            u32 settings=menuSaveState();
            if(RT_stepModeSelect(pads[0],pads[1])){saveProgress();RT_titleMenuEnd();
                if(rtSession.mode==RT_MODE_ARCADE){if(!beginAttract())rtAppFault=103;}
                else start();}
            else if(settings!=menuSaveState())saveProgress();
            if(rtAppState==APP_MENU){RT_drawModeSelect();if(!RT_titleMenuActive())SYS_doVBlankProcess();}
            continue;
        }
        if(rtAppState==APP_BONUS){
            SYS_doVBlankProcess();
            u32 now=vtimer;RT_campaignBonusRefresh(&bonus,now-lastRefresh);lastRefresh=now;
            u8 earned=0,events=0;
            u8 *reserves=rtSession.mode==RT_MODE_ARCADE?&arcade.reserves:&earned;
            RT_BonusResult result=RT_campaignBonusStep(&bonus,&score,scene.exitPlayer,rtSession.mode==RT_MODE_ARCADE && (rtArcadeDips&RT_DIP_BONUS)?1:0,reserves,&events);
            rtAppBonusEvents=events;scene.timer.remaining=bonus.remaining;
            /* CPU1 bonus queues 0A, followed by 10 on an extra life. */
            if(events&(RT_BONUS_SOUND|RT_BONUS_EXTRA_LIFE_SOUND)){
                u8 sounds[2],count=0;
                if(events&RT_BONUS_SOUND)sounds[count++]=0x0a;
                if(events&RT_BONUS_EXTRA_LIFE_SOUND)sounds[count++]=0x10;
                RT_gameSoundPost(sounds,count);
            }
            if(earned && scene.players.home.credits<65535)scene.players.home.credits++;
            if(result==RT_BONUS_INVALID){rtAppFault=40;notice("NATIVE BUILD ERROR","TIME BONUS",APP_UNFINISHED);continue;}
            if(events&(RT_BONUS_SCORE_HUD|RT_BONUS_TIMER_HUD))
                RT_completionMonitorUpdate(score.high,score.players[0],score.players[1],bonus.remaining);
            if(result==RT_BONUS_COMPLETE){
                if(!RT_transitionCommandsBegin(transitionSelector,&transitionEffect,&rtAppTransitionCommands) ||
                   !RT_completionSceneBegin(&completion,transitionSelector)){
                    RT_completionSceneEnd(&completion);rtAppFault=41;
                    notice("NATIVE BUILD ERROR","TRANSITION LOAD",APP_UNFINISHED);continue;
                }
                /* Ordinary A466 transitions select music effect 5. The
                 * selector-5 scene deliberately preserves the prior effect. */
                if(transitionEffect==5)RT_gameMusicStart(5);
                rtAppState=APP_TRANSITION;
            }
            continue;
        }
        if(rtAppState==APP_TRANSITION){
            u8 result=RT_completionSceneStep(&completion);
            if(result==1)continue;
            RT_gameMusicStop();
            RT_completionSceneEnd(&completion);
            if(!result){rtAppFault=42+completion.fault;notice("NATIVE BUILD ERROR","TRANSITION FRAME",APP_UNFINISHED);continue;}
            if(!RT_transitionCommandsEnd(&transitionSelector,&transitionEffect,&rtAppTransitionCommands) ||
               RT_campaignSceneAdvance(&scene,rtSession.mode==RT_MODE_ARCADE && (rtArcadeDips&RT_DIP_EASY)?1:0)){
                rtAppFault=51;notice("NATIVE BUILD ERROR","NEXT AREA",APP_UNFINISHED);continue;
            }
            RT_homeCheckpoint(&scene,&score);
            if(!video()){notice("DEVELOPMENT BUILD","AREA ART NOT LINKED",APP_UNFINISHED);continue;}
            tick=0;rtAppState=APP_PLAY;
            if(rtSession.mode==RT_MODE_ARCADE){
                arcade.story=scene.stage->story;arcade.area=scene.stage->area;
                arcade.checkpoint=0;arcade.previousWindow=scene.objectWindow;
                readyStarted=vtimer;rtAppState=APP_READY;
            }
            continue;
        }
        if(rtAppState==APP_READY){
            if(vtimer-readyStarted>=128){lastRefresh=vtimer;RT_framePacerReset(&pacer,lastRefresh);rtAppState=APP_PLAY;}
            SYS_doVBlankProcess();continue;
        }
        if(rtAppState==APP_UNFINISHED){
            u16 now=pads[0]|pads[1],edge=now&~held;held=now;
            if(edge&BUTTON_START)menu();
            SYS_doVBlankProcess();continue;
        }
        if(rtAppState==APP_CONTINUE){
            u8 mask;
            if(homeAutoContinue){
                /* Player 1 pays and restarts at the area entrance; a dead
                 * partner rejoins on the next survivor-respawn pass. */
                homeAutoContinue=0;
                u8 dead=(1<<scene.players.home.players)-1;
                mask=RT_homeRespawnAgent(&scene.players.home,scene.players.agents,
                    scene.players.combat,0,dead,1)?1:0;
                if(!mask){RT_beginHomeGameOver();rtAppState=APP_MENU;continue;}
            }else mask=RT_homeResolveDeaths(&scene.players.home,scene.players.agents,
                scene.players.combat,(pads[0]&BUTTON_START?1:0)|(pads[1]&BUTTON_START?2:0));
            if(mask){
                u16 credits=scene.players.home.credits;
                RT_SessionConfig selection=scene.players.selection;
                u8 story=scene.stage->story,area=scene.stage->area;
                u8 enemySet=scene.spawnLevel==&rtEnemySpawnLevels[1][story*5+area];
                /* Reload the current area's authored entrance and preserve
                 * which player paid. Other dead slot costs a separate credit
                 * on the next survivor-respawn pass. */
                if(survival?RT_survivalSceneContinue(&scene,&survivalWorld,mask,pads):
                   RT_campaignSceneInitialize(&scene,&selection,story,area,0,enemySet,credits)){
                    rtAppFault=2;notice("NATIVE BUILD ERROR","CONTINUE INITIALIZATION",APP_UNFINISHED);
                }else{
                    for(u8 p=0;p<scene.players.home.players;p++){
                        if(mask&(1<<p))scene.players.combat[p].invincibleTicks=180;
                        else {scene.players.combat[p].health=0;scene.players.combat[p].removed=1;}
                        scene.players.previousButtons[p]=pads[p];
                    }
                    if(video())rtAppState=APP_PLAY;
                    else {rtAppFault=3;notice("NATIVE BUILD ERROR","CONTINUE DISPLAY",APP_UNFINISHED);}
                }
            }
            SYS_doVBlankProcess();continue;
        }
        /* Survival shares Campaign's overlapped publication: the next update
         * runs while VInt commits the previous frame, instead of idling for
         * VBlank after every presentation. */
        beginPipeline();
        if(!RT_gameMusicActive() && !finalMusic.active && !rtFinalCommandSeen && !(rtArcadeAttract && (rtArcadeDips&RT_DIP_DEMO_MUTE)))
            RT_gameMusicStart(RT_gameMusicTrackFor(scene.stage->story,survival?0:scene.stage->area));
        u32 refresh=vtimer;
        if(!RT_framePacerStep(&pacer,refresh)){
            while(vtimer==refresh){__asm__ volatile("nop");}
            continue;
        }
        if(survivalRetirePending){
            /* Same logical point as the former synchronous path: after the
             * previous presentation's commit, before the next update. Only
             * drain early when a rebase can actually proceed. */
            survivalRetirePending=0;
            if(RT_survivalRetireSpatiallyReady(&scene)){
                finishPipeline();update.objects.count=0;
                if(RT_survivalRetireTwo(&survivalWindow,&survivalWorld,&scene,&view,
                    &survivalDisplay,&update.objects,RT_worldStage(0,0))){
                    /* Held sprite groups refer to pre-rebase records. */
                    rtAppSurvivalRetirements++;rtSpriteBirthEpoch++;
                }
            }
        }
        u32 lag=refresh-pacer.serviced;
        if(lag>rtAppMaxUpdateLag)rtAppMaxUpdateLag=lag;
        u32 elapsed=refresh-lastRefresh;lastRefresh=refresh;
        rtAppRefreshes+=elapsed;RT_campaignProtectionAdvance(&scene,elapsed);
        if(rtSession.mode==RT_MODE_ARCADE && !rtArcadeAttract && (rtArcadeDips&RT_DIP_INVINCIBLE))scene.players.combat[0].invincibleTicks=2;
        RT_CampaignRunState state=survival?
            RT_survivalUpdate(&scene,&survivalWorld,pads,tick++,(u8)refresh,&update):
            RT_campaignUpdate(&scene,pads,tick++,(u8)refresh,&update);
        if(update.respawnMask)rtSpriteBirthEpoch++;
        postUpdateSounds(&update);
        u8 spritePrepared=0,drawChosen=0;
        if(state==RT_CAMPAIGN_RUNNING && update.updated){
            if(scene.players.home.players==2)RT_coopDetailUpdate(2,
                update.battle.enemies.visible,scene.projectiles.active,vtimer-pacer.serviced>1);
            drawChosen=1;
            /* Co-op frameskip: when well behind, give one update's draw time
             * to game speed, but show at least 2 of every 3 updates. */
            if(scene.players.home.players==2 && vtimer-pacer.serviced>2 && rtSinceSkip>=2){rtSinceSkip=0;}
            else if((rtSinceSkip<255?rtSinceSkip++:0),((scene.players.home.players==2 && !rtCoopCompact) || RT_framePacerDrawWithLag(&pacer,vtimer,1))){
                RT_spritesDefer();
                if(!RT_campaignSpritesQueue(&sprites,&scene,&view)){
                    finishPipeline();RT_spritesDiscardPrepared();
                    rtAppFault=30+sprites.fault;
                    notice("UNFINISHED GRAPHICS","PREPARE FAILED",APP_UNFINISHED);continue;
                }
                spritePrepared=1;
            }else{
                RT_spritesDefer();
                if(RT_campaignSpritesReproject(&sprites,&scene,&view))spritePrepared=2;
                else{
                    RT_spritesDiscardPrepared();RT_spritesDefer();
                    if(!RT_campaignSpritesQueue(&sprites,&scene,&view)){
                        finishPipeline();RT_spritesDiscardPrepared();
                        rtAppFault=30+sprites.fault;
                        notice("UNFINISHED GRAPHICS","PREPARE FAILED",APP_UNFINISHED);continue;
                    }
                    spritePrepared=1;
                }
            }
        }
        /* Score bookkeeping overlaps the previous video publication. */
        /* A completed battle can also end a life. Publish its awards before
         * handling that terminal state, once, even on a skipped presentation. */
        if(update.worldUpdated && !rtArcadeAttract){
            u8 earned=0;
            u8 *reserves=rtSession.mode==RT_MODE_ARCADE?&arcade.reserves:&earned;
            if(!RT_campaignScoreEnemies(&score,&update.battle.enemies,
                scene.players.home.players,rtSession.mode==RT_MODE_ARCADE && (rtArcadeDips&RT_DIP_BONUS)?1:0,reserves)){
                finishPipeline();rtAppFault=6;notice("NATIVE BUILD ERROR","SCORE EVENTS",APP_UNFINISHED);continue;
            }
            if(earned){
                u32 credits=(u32)scene.players.home.credits+earned;
                scene.players.home.credits=credits>65535?65535:(u16)credits;
                if(state==RT_CAMPAIGN_GAME_OVER)state=RT_CAMPAIGN_CONTINUE;
            }
        }
        finishPipeline();
        if(!survival && update.worldUpdated && !consumeFinalCommands()){rtAppFault=90;notice("AUDIO ERROR","FINAL COMMAND",APP_UNFINISHED);continue;}
        if(state==RT_CAMPAIGN_FAULT){
            rtAppFault=10+update.faultPhase;notice("NATIVE BUILD ERROR","GAMEPLAY PHASE",APP_UNFINISHED);continue;
        }
        if(rtArcadeAttract && state!=RT_CAMPAIGN_RUNNING){
            if(!attractScores())rtAppFault=104;
            continue;
        }
        if(state==RT_CAMPAIGN_ARCADE_DEATH){
            if(!RT_arcadeClaimRestart(&arcade)){
                if(!beginHighEntry()){
                    rtAppFault=94;notice("NATIVE BUILD ERROR","ARCADE HIGH SCORE",APP_UNFINISHED);
                }
                continue;
            }
            if(RT_campaignSceneInitialize(&scene,&rtSession,arcade.story,arcade.area,arcade.checkpoint,(rtArcadeDips&RT_DIP_EASY)?1:0,0) || !video()){
                rtAppFault=5;notice("NATIVE BUILD ERROR","ARCADE RESTART",APP_UNFINISHED);continue;
            }
            arcade.previousWindow=scene.objectWindow;readyStarted=vtimer;rtAppState=APP_READY;continue;
        }
        if(state==RT_CAMPAIGN_FINAL_EXIT){
            if(!rtFinalCommandSeen){rtAppFault=91;notice("AUDIO ERROR","MISSING FINAL COMMAND",APP_UNFINISHED);continue;}
            rtAppState=APP_FINAL_WAIT;continue;
        }
        if(state==RT_CAMPAIGN_STAGE_CLEAR){
            endHud();
            if(!RT_campaignBonusBegin(&bonus,scene.timer.remaining,0,0) ||
               !RT_completionMonitorShow(scene.stage->story,scene.stage->area)){
                rtAppFault=40;notice("NATIVE BUILD ERROR","TIME BONUS",APP_UNFINISHED);continue;
            }
            SYS_doVBlankProcess();
            RT_completionMonitorUpdate(score.high,score.players[0],score.players[1],bonus.remaining);
            rtAppBonusEvents=0;lastRefresh=vtimer;rtAppState=APP_BONUS;continue;
        }
        if(state==RT_CAMPAIGN_CONTINUE){
            if(rtSession.mode==RT_MODE_HOME){
                /* Home credits come from Settings and are spent without a
                 * prompt; the session reaches GAME_OVER once none remain. */
                endHud();homeAutoContinue=1;rtAppState=APP_CONTINUE;continue;
            }
            notice("CONTINUE?","START: SPEND ONE CREDIT",APP_CONTINUE);
            /* No held Start may accept a newly opened Continue prompt. */
            scene.players.home.previousStart=(pads[0]&BUTTON_START?1:0)|(pads[1]&BUTTON_START?2:0);
            scene.players.home.continueWaiting=1;continue;
        }
        if(state==RT_CAMPAIGN_GAME_OVER){endHud();RT_beginHomeGameOver();rtAppState=APP_MENU;continue;}
        if(!update.updated)continue;
        if(rtSession.mode==RT_MODE_ARCADE)RT_arcadeObserveWindow(&arcade,&scene);
        rtAppUpdates++;
        if(!updateHud()){rtAppFault=60;notice("NATIVE BUILD ERROR","HUD VALUES",APP_UNFINISHED);continue;}
        if(!(survival?RT_survivalViewQueue(&view,&survivalDisplay,&scene.camera,&update.objects):
            RT_campaignDisplayQueue(&view,&scene.camera,&update.objects))){
            rtAppFault=20;notice("NATIVE BUILD ERROR","BACKGROUND QUEUE",APP_UNFINISHED);continue;
        }
        if(drawChosen?!spritePrepared:!RT_framePacerDrawWithLag(&pacer,vtimer,1)){rtAppSkippedPresentations++;continue;}
#ifdef RT_GAMEPLAY_HUD
        if(!RT_campaignHudDisplayQueueRows(&hudDisplay,&hudState.hud,hudState.dirtyRows)){
            rtAppFault=61;notice("NATIVE BUILD ERROR","HUD QUEUE",APP_UNFINISHED);continue;
        }
        hudState.dirtyRows=0;
#endif
        if(!(spritePrepared?RT_spritesPublishPrepared():RT_campaignSpritesQueue(&sprites,&scene,&view))){
            rtAppFault=30+sprites.fault;notice("UNFINISHED GRAPHICS","DEVELOPMENT BUILD",APP_UNFINISHED);continue;
        }
        if(spritePrepared)view.display.queuedBytes+=sprites.batch.queuedBytes;
        pipelineReprojected=spritePrepared==2;armPipeline();
        /* Tile events have been copied and published; rebase after commit. */
        if(survival)survivalRetirePending=1;
    }
}
