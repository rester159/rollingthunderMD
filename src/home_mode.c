#include "rt_game_music.h"
#include "rt_home_progress.h"
#include "rt_save.h"
#include "rt_stage_raster.h"
#include "rt_home_mode.h"
#include "rt_banked_tiles.h"
#include "home_portraits.h"
#include "rt_title_menu.h"
#define PORTRAIT_BASE 512
#define BORDER_BASE 1130
#define CONFIRM (BUTTON_A | BUTTON_START)
enum { MODE, HOME, AGENTS, PENDING, PLAYERS, SETTINGS, GAME_OVER=6, CONTROLS, SOUND, ARCADE, ARCADE_OPTIONS, DIPS, SLOTS, OVERWRITE };
static u8 optionParent,slotLoad,dipIndex;
static u8 soundTrack=1,soundShown;
RT_SessionConfig rtSession;
u8 rtHomeStartingCredits=6;
extern const u32 rtMenuFont[768],rtMenuMd[48];
#define MENU_FONT_BASE 1152
static u8 page, cursor, ready[2];
static u16 previous[2];
static bool dirty,agentScreenReady;
u8 rtControlLayout;
static u16 fontBase=MENU_FONT_BASE;
static u8 fontPalette=PAL3;
void RT_resetSession(void)
{
    rtSession.mode = RT_MODE_ARCADE;
    rtSession.players = RT_HOME_SOLO;
    rtSession.agent[0] = RT_AGENT_ALBATROSS;
    rtSession.agent[1] = RT_AGENT_LEILA;
    rtSession.reserved = 0;
    rtSession.game = RT_HOME_CAMPAIGN;
    rtSession.costume[0] = rtSession.costume[1] = 0;
}
bool RT_isCoop(void)
{
    return rtSession.mode == RT_MODE_HOME && rtSession.players == RT_HOME_TWO_PLAYERS;
}
u16 RT_homePopulationScale(void) { return RT_isCoop() ? 384 : 256; }
u16 RT_homeAttackScale(void) { return RT_isCoop() ? 384 : 256; }
void RT_beginModeSelect(void)
{
    RT_titleMenuEnd();
    RT_stopStageRaster();
    /* Gameplay uses 64x64 maps at $2000/$4000 and tile scrolling.
       Menu patterns begin at $4000, so restore SGDK's complete front-end
       layout (including the font and blank tile) before uploading them.
       Drain old gameplay transfers before changing their destinations. */
    DMA_flushQueue();
    VDP_init();
    page = MODE; cursor = 0;agentScreenReady=FALSE;
    ready[0] = ready[1] = 0;
    previous[0] = previous[1] = 0xFFFF;
    dirty = TRUE;
}
void RT_beginHomeGameOver(void)
{
    RT_beginModeSelect();
    page = GAME_OVER;
}
void RT_showHomePending(void) { page = PENDING; dirty = TRUE; }
static void textAt(const char *text, u16 x, u16 y) {
    while(*text && x<40){
        u8 c=*text++;if(c>='a' && c<='z')c-=32;
        if(c<32 || c>127)c=' ';
        VDP_setTileMapXY(BG_A,TILE_ATTR_FULL(fontPalette,TRUE,FALSE,FALSE,fontBase+c-32),x++,y);
    }
}
/* White P1 and red P2 outlines and labels. */
#define P2_FONT_BASE 1256
static void loadBorders(void)
{
    static const u32 pieces[8][8] = {
        {0,0,0,0,0x00011111,0x00010000,0x00010000,0x00010000},
        {0,0,0,0,0x11111111,0,0,0},
        {0,0,0,0,0x11111000,0x00001000,0x00001000,0x00001000},
        {0x00010000,0x00010000,0x00010000,0x00010000,0x00010000,0x00010000,0x00010000,0x00010000},
        {0x00001000,0x00001000,0x00001000,0x00001000,0x00001000,0x00001000,0x00001000,0x00001000},
        {0x00010000,0x00010000,0x00010000,0x00011111,0,0,0,0},
        {0,0,0,0x11111111,0,0,0,0},
        {0x00001000,0x00001000,0x00001000,0x11111000,0,0,0,0}
    };
    u32 p2Tiles[8];
    u16 i, y;
    VDP_loadTileData((const u32 *)pieces, BORDER_BASE, 8, CPU);
    for (i = 0; i < 8; i++) {
        for (y = 0; y < 8; y++) p2Tiles[y] = pieces[i][y] * 2;
        VDP_loadTileData(p2Tiles, BORDER_BASE + 8 + i, 1, CPU);
    }
    for(i=0;i<96;i++){
        for(y=0;y<8;y++)p2Tiles[y]=rtMenuFont[i*8+y]&0x22222222UL;
        VDP_loadTileData(p2Tiles,P2_FONT_BASE+i,1,CPU);
    }
    PAL_setColor(49, RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(50, RGB24_TO_VDPCOLOR(0xFF3333));
    PAL_setColor(63, RGB24_TO_VDPCOLOR(0xFFFFFF));
}
static void borderTile(u16 x, u16 y, u16 piece, u16 color)
{
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE,
        BORDER_BASE + color * 8 + piece), x, y);
}
static void frame(u16 x, u16 y, u16 w, u16 h, u16 color, const char *label)
{
    u16 i;
    borderTile(x,y,0,color); borderTile(x+w-1,y,2,color);
    borderTile(x,y+h-1,5,color); borderTile(x+w-1,y+h-1,7,color);
    for (i=1;i<w-1;i++) { borderTile(x+i,y,1,color); borderTile(x+i,y+h-1,6,color); }
    for (i=1;i<h-1;i++) { borderTile(x,y+i,3,color); borderTile(x+w-1,y+i,4,color); }
    if (label) {
        u16 saved=fontBase;if(color)fontBase=P2_FONT_BASE;
        textAt(label,x+w/2-2,y);fontBase=saved;
    }
}
/* Portrait patterns stay resident. Selection changes publish only tilemap
 * rows in VBlank; switching/joining must never blank or reload the screen. */
static void agentFrameRow(u16 row[40],u16 y,u16 x,u16 top,u16 w,u16 h,u8 player)
{
    if(y<top || y>=top+h)return;
    u16 base=BORDER_BASE+player*8;
    if(y==top || y==top+h-1){
        u8 bottom=y!=top;
        for(u16 i=0;i<w;i++)row[x+i]=TILE_ATTR_FULL(PAL3,TRUE,FALSE,FALSE,
            base+(i==0?(bottom?5:0):i==w-1?(bottom?7:2):(bottom?6:1)));
    }else{
        row[x]=TILE_ATTR_FULL(PAL3,TRUE,FALSE,FALSE,base+3);
        row[x+w-1]=TILE_ATTR_FULL(PAL3,TRUE,FALSE,FALSE,base+4);
    }
    if(y==top){
        const char *label=player?" P2 ":" P1 ";
        for(u8 i=0;i<4;i++)row[x+w/2-2+i]=TILE_ATTR_FULL(PAL3,TRUE,FALSE,FALSE,
            (player?P2_FONT_BASE:MENU_FONT_BASE)+label[i]-32);
    }
}
static void updateAgentSelection(void)
{
    const u16 *map=(const u16 *)rtHomePortraitMap;
    for(u16 y=0;y<26;y++){
        u16 row[40]={0};
        if(y<2){
            for(u16 x=0;x<40;x++)if(x<23 || rtSession.players==RT_HOME_SOLO)
                row[x]=TILE_ATTR_FULL(PAL3,TRUE,FALSE,FALSE,16+(x/8)*16+y*8+x%8);
        }else{
            if(y>=3 && y<25)for(u8 p=0;p<2;p++)for(u16 x=0;x<14;x++)
                row[(p?22:4)+x]=map[p*308+(y-3)*14+x]+PORTRAIT_BASE;
            agentFrameRow(row,y,rtSession.agent[0]?21:3,2,16,24,0);
            if(rtSession.players==RT_HOME_TWO_PLAYERS){
                bool same=rtSession.agent[0]==rtSession.agent[1];
                agentFrameRow(row,y,(rtSession.agent[1]?21:3)+(same?1:0),same?3:2,same?14:16,same?22:24,1);
            }
        }
        DMA_transfer(DMA_QUEUE_COPY,DMA_VRAM,row,VDP_getBGAAddress()+y*128,40,2);
    }
}
u16 RT_jumpButtons(u8 player)
{
    u8 layout=(rtControlLayout>>(player*2))&3;
    return layout==2?BUTTON_B:layout==1?BUTTON_A:(BUTTON_A|BUTTON_C);
}
u16 RT_fireButtons(u8 player)
{
    u8 layout=(rtControlLayout>>(player*2))&3;
    return layout==2?(BUTTON_A|BUTTON_C):layout==1?(BUTTON_B|BUTTON_C):BUTTON_B;
}
void RT_drawModeSelect(void)
{
    u16 i;
    if(page==SOUND){
        RT_gameMusicPoll();
        u8 playing=RT_gameMusicPlaying();
        if(playing!=soundShown){soundShown=playing;dirty=TRUE;}
    }
    if(page!=AGENTS){
        agentScreenReady=FALSE;
        if(!RT_titleMenuActive()){
            if(!RT_titleMenuBegin())return;
            /* Opaque title font: transparent pen0 becomes pen1 (black in the
             * title palette), so the menu reads as a solid panel above the
             * walking cast instead of exposing the plane underneath. */
            /* Text is composed into tiles16..111 by RT_titleMenuTextCommit. */
            dirty=TRUE;
        }
        fontBase=16;fontPalette=PAL1;
        if(dirty){
            /* Restore plane cells before the title palette split begins.
             * A live ROM-reading sound driver changes DMA/bus timing. */
            VDP_waitVBlank(TRUE);
            /* Menu text is drawn as sprites on rows17..18, above the walking
             * cast (sprite tops at y153); the background stays untouched. */
            RT_titleMenuRestoreText();RT_titleMenuSetHome(page!=MODE && rtSession.mode==RT_MODE_HOME);
            RT_titleMenuTextClear();
            if(page==MODE){
                RT_titleMenuFooter();
                RT_titleMenuTextAt(cursor==0?"> ARCADE":"  ARCADE",15,16);
                RT_titleMenuTextAt(cursor==1?"> HOME":"  HOME",15,17);
                RT_titleMenuTextAt(cursor==2?"> CONTINUE":"  CONTINUE",15,18);
            }else if(page==HOME){
                static const char *items[]={"PLAY","SURVIVAL","SETTINGS"};
                u8 first=cursor>2?cursor-2:0;
                for(i=0;i<3;i++){
                    if(cursor==first+i)RT_titleMenuTextAt(">",13,16+i);
                    RT_titleMenuTextAt(items[first+i],15,16+i);
                }
            }else if(page==ARCADE || page==ARCADE_OPTIONS){
                static const char *items[2][3]={{"START","OPTIONS","DIP SWITCHES"},{"CONTROLS","SOUND TEST","RESET DIP DEFAULTS"}};
                u8 first=cursor>2?cursor-2:0;
                for(i=0;i<3;i++){
                    if(cursor==first+i)RT_titleMenuTextAt(">",10,16+i);
                    RT_titleMenuTextAt(items[page==ARCADE_OPTIONS][first+i],12,16+i);
                }
            }else if(page==DIPS){
                static const char *labels[]={"LIVES","BONUS LIFE","TIMER","DIFFICULTY","INVULNERABILITY","DEMO SOUND"};
                static const char *values[6][2]={{"3","5"},{"70000 / 200000","100000 / 300000"},{"150","120"},{"NORMAL","EASY"},{"OFF","ON"},{"ON","OFF"}};
                RT_titleMenuTextAt(labels[dipIndex],12,16);
                RT_titleMenuTextAt(values[dipIndex][(rtArcadeDips>>dipIndex)&1],12,17);
                RT_titleMenuTextAt("UP/DOWN  LEFT/RIGHT",10,18);
            }else if(page==SLOTS){
                if(!rtSaveAvailable){
                    RT_titleMenuTextAt("SRAM NOT AVAILABLE",11,16);
                    RT_titleMenuTextAt(slotLoad?"NO SAVES TO CONTINUE":"A: PLAY WITHOUT SAVE",10,17);
                    RT_titleMenuTextAt("B: BACK",15,18);
                }else for(i=0;i<3;i++){
                    char label[20]="SLOT 1  EMPTY";label[5]='1'+i;
                    u8 area=RT_homeSlotArea(i);
                    if(area){memcpy(label+8,"AREA ",5);label[13]='0'+area/10;label[14]='0'+area%10;label[15]=0;}
                    if(cursor==i)RT_titleMenuTextAt(">",10,16+i);
                    RT_titleMenuTextAt(label,12,16+i);
                }
            }else if(page==OVERWRITE){
                RT_titleMenuTextAt("REPLACE THIS SAVE?",11,16);
                RT_titleMenuTextAt("A/START: NEW GAME",11,17);
                RT_titleMenuTextAt("B: KEEP SAVE",11,18);
            }else if(page==PLAYERS){
                /* Only the requested player choice is a modal. */
                RT_titleMenuTextAt("PLAYERS",15,16);
                RT_titleMenuTextAt(cursor==0?"> 1P":"  1P",15,17);
                RT_titleMenuTextAt(cursor==1?"> 2P":"  2P",15,18);
            }else if(page==SETTINGS){
                static char value[2];value[0]='0'+rtHomeStartingCredits;value[1]=0;
                RT_titleMenuTextAt(cursor==0?"> CREDITS":"  CREDITS",12,16);
                RT_titleMenuTextAt(value,23,16);
                RT_titleMenuTextAt(cursor==1?"> CONTROLS":"  CONTROLS",12,17);
                RT_titleMenuTextAt(cursor==2?"> SOUND TEST":"  SOUND TEST",12,18);
            }else if(page==SOUND){
                static char track[]="< TRACK 00 >";
                track[8]='0'+soundTrack/10;track[9]='0'+soundTrack%10;
                RT_titleMenuTextAt("SOUND TEST",15,16);
                RT_titleMenuTextAt(track,14,17);
                RT_titleMenuTextAt(RT_gameMusicPsgMuted()?"C PSG OFF  B BACK":"C PSG ON   B BACK",12,18);
            }else if(page==CONTROLS){
                static const char *layouts[3]={"A/C JUMP  B FIRE","A JUMP  B/C FIRE","B JUMP  A/C FIRE"};
                RT_titleMenuTextAt(cursor?"UP/DOWN: PLAYER 2":"UP/DOWN: PLAYER 1",12,16);
                RT_titleMenuTextAt(layouts[(rtControlLayout>>(cursor*2))&3],12,17);
                RT_titleMenuTextAt("LEFT/RIGHT  B BACK",12,18);
            }else if(page==GAME_OVER){
                RT_titleMenuTextAt("GAME OVER",15,16);
                RT_titleMenuTextAt(rtSession.players==RT_HOME_SOLO?"NO LIVES LEFT":"NO CREDITS LEFT",12,17);
                RT_titleMenuTextAt("A/START: HOME",13,18);
            }else{
                RT_titleMenuTextAt(rtSession.game==RT_HOME_SURVIVAL?"SURVIVAL":"CAMPAIGN",15,16);
                RT_titleMenuTextAt("STILL IN DEVELOPMENT",10,17);
                RT_titleMenuTextAt("B: BACK",16,18);
            }
            RT_titleMenuTextCommit();
            dirty=FALSE;
        }
        RT_titleMenuStep();return;
    }
    if(RT_titleMenuActive()){
        RT_titleMenuEnd();VDP_init();dirty=TRUE;
    }
    fontBase=MENU_FONT_BASE;fontPalette=PAL3;
    if (!dirty) return;
    dirty = FALSE;
    if(agentScreenReady){updateAgentSelection();return;}
    VDP_setEnable(FALSE);
    VDP_setHorizontalScroll(BG_A,0); VDP_setHorizontalScroll(BG_B,0);
    VDP_setVerticalScroll(BG_A,0); VDP_setVerticalScroll(BG_B,0);
    VDP_clearPlane(BG_A,TRUE); VDP_clearPlane(BG_B,TRUE);
    VDP_updateSprites(0,CPU);
    VDP_setBackgroundColor(0);
    PAL_setColor(0,0);
    /* Plain white font for frame labels; page text is composed below. */
    VDP_loadTileData(rtMenuFont,MENU_FONT_BASE,96,CPU);
    PAL_setColor(63,RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(50,RGB24_TO_VDPCOLOR(0xAA2200));
    if (page == AGENTS) {
        const u16 *map = (const u16 *)rtHomePortraitMap;
        RT_loadBankedTiles(rtHomePortraitTiles,PORTRAIT_BASE,sizeof(rtHomePortraitTiles)/32);
        PAL_setColors(0,(const u16 *)rtHomePortraitPalette,48,CPU);
        loadBorders(); VDP_setTextPalette(PAL3);
        for (i=0;i<616;i++) {
            u16 p=i/308, cell=i%308;
            VDP_setTileMapXY(BG_A,map[i]+PORTRAIT_BASE,(p?22:4)+cell%14,3+cell/14);
        }
        {
            /* Red text (PAL3 pen2 = color50), centred over each portrait.
             * It sits on black, so the drop shadow uses transparent pen0. */
            const char *head[2]={"SELECT YOUR AGENT","P2: START"};
            const char *name[2]={"ALBATROSS","LEILA"};
            RT_MenuText header[2],names[2];
            for(u8 p=0;p<2;p++){
                s16 centre=p?232:88;
                header[p]=(RT_MenuText){head[p],centre-RT_menuTextWidth(head[p])/2,0};
                names[p]=(RT_MenuText){name[p],centre-RT_menuTextWidth(name[p])/2,0};
            }
            /* Five 8x2-tile chunks per strip through the shared 16-tile buffer. */
            for(u16 k=0;k<5;k++){
                RT_menuComposeOutline(rtMenuChunk,k*64,8,2,0,header,rtSession.players==RT_HOME_SOLO?2:1,2,0,10);
                VDP_loadTileData(rtMenuChunk,16+k*16,16,CPU);
                RT_menuComposeOutline(rtMenuChunk,k*64,8,2,0,names,2,2,0,10);
                VDP_loadTileData(rtMenuChunk,96+k*16,16,CPU);
            }
            for(i=0;i<80;i++){
                u16 tile=(i%40/8)*16+(i/40)*8+i%8;
                VDP_setTileMapXY(BG_A,TILE_ATTR_FULL(PAL3,TRUE,FALSE,FALSE,16+tile),i%40,i/40);
                VDP_setTileMapXY(BG_A,TILE_ATTR_FULL(PAL3,TRUE,FALSE,FALSE,96+tile),i%40,26+i/40);
            }
        }
        frame(rtSession.agent[0]?21:3,2,16,24,0," P1 ");
        if (rtSession.players == RT_HOME_TWO_PLAYERS) {
            bool same=rtSession.agent[0]==rtSession.agent[1];
            u16 x=rtSession.agent[1]?21:3;
            frame(x+(same?1:0),same?3:2,same?14:16,same?22:24,1,
                " P2 ");
        }
    }
    /* Publish the completed menu at VBlank, never midway down the screen. */
    VDP_waitVSync();
    VDP_setEnable(TRUE);agentScreenReady=TRUE;
}
bool RT_stepModeSelect(u16 pad1, u16 pad2)
{
    u16 edges[2]={pad1 & ~previous[0],pad2 & ~previous[1]};
    u16 edge=page==GAME_OVER ? edges[0]|edges[1] : edges[0];
    previous[0]=pad1; previous[1]=pad2;
    if (!(edge | (page==AGENTS?edges[1]:0))) return FALSE;
    dirty=TRUE;
    if (page==GAME_OVER) {
        if (edge & (CONFIRM | BUTTON_B)) { page=HOME; cursor=rtSession.game; }
    }
    else if (page==MODE) {
        if(edge & BUTTON_DOWN)cursor=(cursor+1)%3;
        if(edge & BUTTON_UP)cursor=(cursor+2)%3;
        if (edge & CONFIRM) {
            if(cursor==2){page=SLOTS;slotLoad=1;cursor=rtHomeSaveSlot;return FALSE;}
            rtSession.mode=cursor?RT_MODE_HOME:RT_MODE_ARCADE;
            if (cursor) { page=HOME; cursor=rtSession.game; }
            else {
                rtSession.players=RT_HOME_SOLO;
                rtSession.agent[0]=RT_AGENT_ALBATROSS;
                rtSession.costume[0]=rtSession.costume[1]=0;
                page=ARCADE;cursor=0;
            }
        }
    }
    else if (page==HOME) {
        if(edge & BUTTON_B){page=MODE;cursor=1;}
        else{
            if(edge & BUTTON_DOWN)cursor=(cursor+1)%3;
            if(edge & BUTTON_UP)cursor=(cursor+2)%3;
            if(edge & CONFIRM){
                if(cursor==2){page=SETTINGS;optionParent=SETTINGS;cursor=0;}
                else{
                    rtSession.game=cursor;rtHomeResume=0;page=cursor?AGENTS:SLOTS;slotLoad=0;cursor=rtHomeSaveSlot;rtSession.players=RT_HOME_SOLO;
                    ready[0]=ready[1]=0;
                    rtSession.agent[1]=rtSession.agent[0]^1;
                    rtSession.costume[0]=rtSession.costume[1]=0;
                }
            }
        }
    }
    else if(page==ARCADE){
        if(edge & BUTTON_B){page=MODE;cursor=0;}
        else{
            if(edge & BUTTON_DOWN)cursor=(cursor+1)%3;
            if(edge & BUTTON_UP)cursor=(cursor+2)%3;
            if(edge & CONFIRM){
                if(!cursor)return TRUE;
                if(cursor==1){page=ARCADE_OPTIONS;cursor=0;optionParent=ARCADE_OPTIONS;}
                else if(cursor==2){page=DIPS;dipIndex=0;}
            }
        }
    }
    else if(page==ARCADE_OPTIONS){
        if(edge & BUTTON_B){page=ARCADE;cursor=1;}
        else{
            if(edge & BUTTON_DOWN)cursor=(cursor+1)%3;
            if(edge & BUTTON_UP)cursor=(cursor+2)%3;
            if(edge & CONFIRM){
                if(cursor==2)rtArcadeDips=0;
                else if(cursor==1){page=SOUND;soundShown=0;}
                else{page=CONTROLS;cursor=0;}
            }
        }
    }
    else if(page==DIPS){
        if(edge & BUTTON_B){page=ARCADE;cursor=2;}
        else{
            if(edge & BUTTON_DOWN)dipIndex=(dipIndex+1)%6;
            if(edge & BUTTON_UP)dipIndex=(dipIndex+5)%6;
            if(edge & (BUTTON_LEFT|BUTTON_RIGHT|CONFIRM))rtArcadeDips^=1<<dipIndex;
        }
    }
    else if(page==SLOTS){
        if(edge & BUTTON_B){page=slotLoad?MODE:HOME;cursor=slotLoad?2:0;}
        else{
            if(edge & BUTTON_DOWN)cursor=(cursor+1)%3;
            if(edge & BUTTON_UP)cursor=(cursor+2)%3;
            if(edge & CONFIRM){
                if(!rtSaveAvailable){if(!slotLoad)page=AGENTS;}
                else if(slotLoad){if(RT_homeSelectSave(cursor))return TRUE;}
                else{rtHomeSaveSlot=cursor;page=RT_homeSlotArea(cursor)?OVERWRITE:AGENTS;}
            }
        }
    }
    else if(page==OVERWRITE){
        if(edge & BUTTON_B)page=SLOTS;
        else if(edge & CONFIRM)page=AGENTS;
    }
    else if(page==SETTINGS){
        if(edge & BUTTON_B){page=HOME;cursor=2;}
        else{
            if(edge & BUTTON_DOWN)cursor=(cursor+1)%3;
            if(edge & BUTTON_UP)cursor=(cursor+2)%3;
            if(cursor==2){if(edge & CONFIRM){page=SOUND;soundShown=0;}}
            else if(!cursor){
                if(edge & BUTTON_LEFT)rtHomeStartingCredits=rtHomeStartingCredits==1?9:rtHomeStartingCredits-1;
                if(edge & (BUTTON_RIGHT|CONFIRM))rtHomeStartingCredits=rtHomeStartingCredits==9?1:rtHomeStartingCredits+1;
            }else if(edge & CONFIRM){page=CONTROLS;cursor=0;}
        }
    }
    else if(page==CONTROLS){
        if(edge & BUTTON_B){page=optionParent;cursor=optionParent==SETTINGS?1:0;}
        else{
            if(edge & (BUTTON_UP|BUTTON_DOWN))cursor^=1;
            u8 shift=cursor*2,value=(rtControlLayout>>shift)&3;
            if(edge & BUTTON_LEFT)value=(value+2)%3;
            if(edge & (BUTTON_RIGHT|CONFIRM))value=(value+1)%3;
            rtControlLayout=(rtControlLayout&~(3<<shift))|(value<<shift);
        }
    }
    else if(page==SOUND){
        /* Tracks 1..13 of the arcade's music, played live on the YM2612/PSG. */
        if(edge & BUTTON_B){RT_gameMusicStop();soundShown=0;page=optionParent;cursor=optionParent==SETTINGS?2:1;}
        else{
            if(edge & BUTTON_LEFT)soundTrack=soundTrack==1?13:soundTrack-1;
            if(edge & BUTTON_RIGHT)soundTrack=soundTrack==13?1:soundTrack+1;
            if(edge & CONFIRM){
                /* Loading the Z80 driver masks interrupts. Start just after
                 * VBlank so it cannot suppress the title's palette split. */
                if(RT_titleMenuActive())SYS_doVBlankProcess();
                RT_gameMusicStart(soundTrack);soundShown=1;
            }
            if(edge & BUTTON_C)RT_gameMusicSetPsgMuted(!RT_gameMusicPsgMuted());
        }
    }
    else if (page==PLAYERS) {
        if (edge & BUTTON_B) { page=HOME; cursor=rtSession.game; }
        else {
            if (edge & BUTTON_DOWN) cursor^=1;
            if (edge & BUTTON_UP) cursor^=1;
            if (edge & CONFIRM) {
                rtSession.players=cursor; page=AGENTS;
                ready[0]=ready[1]=0;
                rtSession.costume[0]=rtSession.costume[1]=0;
            }
        }
    }
    else if (page==PENDING) {
        if (edge & BUTTON_B) { page=HOME; cursor=rtSession.game; }
    }
    else if(page==AGENTS) {
        u8 joined=0;
        if(rtSession.players==RT_HOME_SOLO && (edges[1]&CONFIRM)){
            rtSession.players=RT_HOME_TWO_PLAYERS;
            rtSession.agent[1]=rtSession.agent[0]^1;joined=1;
        }
        u8 count=rtSession.players==RT_HOME_TWO_PLAYERS?2:1;
        if(edges[0]&BUTTON_B){page=HOME;cursor=rtSession.game;return FALSE;}
        if(count==2 && (edges[1]&BUTTON_B)){
            rtSession.players=RT_HOME_SOLO;count=1;
        }
        for(u8 p=0;p<count;p++)
            if(edges[p]&(BUTTON_LEFT|BUTTON_RIGHT))rtSession.agent[p]^=1;
        /* A join press only joins. A later confirm from either controller wins.
         * If both confirm on one refresh, deterministic P1 order breaks the tie. */
        for(u8 p=0;p<count;p++)if((edges[p]&CONFIRM) && !(p==1 && joined)){
            if(count==2)rtSession.agent[p^1]=rtSession.agent[p]^1;
            rtSession.costume[0]=rtSession.costume[1]=0;
            return TRUE;
        }
    }
    return FALSE;
}
