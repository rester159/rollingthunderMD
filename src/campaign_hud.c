#include "rt_campaign_hud.h"
/* All callers bound their fields to at most six decimal digits. Bounded
 * subtraction avoids 68000 software divide/modulo without changing glyphs. */
#ifdef __m68k__
__attribute__((noinline))
#endif
static void digits(RT_HudCell *cells,u32 value,u8 count,u8 group)
{
    static const u32 places[6]={100000,10000,1000,100,10,1};
    const u32 *place=places+6-count;
    u8 significant=0;
    for(u8 i=0;i<count;i++){
        u8 digit=0;u32 unit=*place++;
        while(value>=unit){value-=unit;digit++;}
        significant|=digit;
        cells[i]=(RT_HudCell){significant || i+1==count?digit:255,group};
    }
}
static inline void digitsUpdate(const RT_CampaignHudState *s,RT_HudCell *cells,u32 value,u8 count,u8 group)
{
    (void)s;digits(cells,value,count,group);
}
u8 RT_campaignHudScores(RT_CampaignHud *hud,const RT_CampaignScore *score,
    u8 lives0,u8 lives1)
{
    if(!hud || !score || score->high>999999 || score->players[0]>999999 ||
       score->players[1]>999999 || lives0>99 || lives1>99)return 0;
    /* CPU1 93E4/9477: six significant digits plus a fixed trailing zero. */
    const u32 values[3]={score->players[0],score->high,score->players[1]};
    const u8 columns[3]={2,14,27};
    for(u8 i=0;i<3;i++){
        RT_HudCell *cells=hud->cells+36+columns[i];u8 group=i==1?0xe4:0xfc;
        digits(cells,values[i],6,group);cells[6]=(RT_HudCell){0,group};
    }
    digits(hud->cells+72+4,lives0,2,0xfc);
    digits(hud->cells+72+33,lives1,2,0xfc);return 1;
}
u8 RT_campaignHudTimer(RT_CampaignHud *hud,u16 remaining)
{
    if(!hud || remaining>9999)return 0;
    /* 9561 compares the full timer; 9583 prints only its low three digits. */
    digits(hud->cells+108+32,remaining%1000,3,
        remaining>=60?0xe8:remaining>=30?0xf0:0xe4);return 1;
}
u8 RT_campaignHudAmmo(RT_CampaignHud *hud,u16 remaining)
{
    if(!hud || remaining>9999)return 0;
    digits(hud->cells+108+7,remaining%1000,3,0xfc);return 1;
}
u8 RT_campaignHudHealth(RT_CampaignHud *hud,u8 health)
{
    if(!hud || health>64)return 0;
    u8 full=health>>3,group=health>=32?0xe8:health>=16?0xf0:0xe4;
    RT_HudCell *cells=hud->cells+108+18;
    if(health<64)cells[full]=(RT_HudCell){0x68-(health&7),group};
    for(u8 i=0;i<full;i++)cells[i]=(RT_HudCell){0x61,group};
    return 1;
}

u8 RT_campaignHudBegin(RT_CampaignHudState *s,const RT_HudCell initial[144],u8 home,u8 players)
{
    if(!s || !initial || home>1 || !players || players>2 || (!home && players!=1))return 0;
    *s=(RT_CampaignHudState){0};memcpy(s->hud.cells,initial,sizeof s->hud);
    s->home=home;s->players=players;s->dirtyRows=15;
    if(home){
        for(u8 i=0;i<36;i++)s->hud.cells[72+i]=(RT_HudCell){255,3};
        const u8 label[6]={12,27,14,13,18,29}; /* CREDIT, source alphabet */
        for(u8 i=0;i<6;i++)s->hud.cells[72+12+i]=(RT_HudCell){label[i],252};
        if(players==2){
            for(u8 i=0;i<36;i++)s->hud.cells[108+i]=(RT_HudCell){255,3};
            s->hud.cells[108]=(RT_HudCell){1,252};s->hud.cells[109]=(RT_HudCell){25,252};
            s->hud.cells[128]=(RT_HudCell){2,252};s->hud.cells[129]=(RT_HudCell){25,252};
        }
    }
    return 1;
}
u8 RT_campaignHudUpdate(RT_CampaignHudState *s,const RT_CampaignScore *score,
    u16 timer,const u16 ammo[2],const u8 health[2],u16 credits,u8 reserves)
{
    if(!s || !score || !ammo || !health || !s->players || s->players>2 ||
       score->high>999999 || score->players[0]>999999 || score->players[1]>999999 ||
       timer>9999 || reserves>99)return 0;
    for(u8 p=0;p<s->players;p++)if(ammo[p]>9999 || health[p]>64)return 0;
    if(!s->valid || s->high!=score->high || s->score[0]!=score->players[0] ||
       s->score[1]!=score->players[1] || (!s->home && s->reserves!=reserves)){
        s->dirtyRows|=2;
        const u32 values[3]={score->players[0],score->high,score->players[1]};
        const u8 columns[3]={2,14,27};
        for(u8 i=0;i<3;i++){
            RT_HudCell *c=s->hud.cells+36+columns[i];u8 group=i==1?228:252;
            digitsUpdate(s,c,values[i],6,group);c[6]=(RT_HudCell){0,group};
        }
        if(!s->home){s->dirtyRows|=4;digitsUpdate(s,s->hud.cells+76,reserves,2,252);digitsUpdate(s,s->hud.cells+105,0,2,252);}
    }
    if(s->home && (!s->valid || s->credits!=credits)){s->dirtyRows|=4;digitsUpdate(s,s->hud.cells+72+19,credits,5,252);}
    if(!s->valid || s->timer!=timer){s->dirtyRows|=8;digitsUpdate(s,s->hud.cells+108+(s->players==2?16:32),timer%1000,3,timer>=60?232:timer>=30?240:228);}
    for(u8 p=0;p<s->players;p++){
        u8 column=s->players==2?(p?23:3):7;
        if(!s->valid || s->ammo[p]!=ammo[p]){s->dirtyRows|=8;digitsUpdate(s,s->hud.cells+108+column,ammo[p]%1000,3,252);}
        column=s->players==2?(p?27:7):18;
        if(!s->valid || s->health[p]!=health[p]){
            s->dirtyRows|=8;
            RT_HudCell *c=s->hud.cells+108+column;
            /* Initialize/repair a whole bar on entry or healing. During damage
             * replay every intermediate segment so skips cannot leave old ink. */
            if(!s->valid || health[p]>s->health[p])for(u8 i=0;i<8;i++)c[i]=(RT_HudCell){0x68,232};
            u8 h=!s->valid || health[p]>=s->health[p]?health[p]:s->health[p]-1;
            for(;;){
                u8 full=h>>3,group=h>=32?232:h>=16?240:228;
                if(h<64)c[full]=(RT_HudCell){0x68-(h&7),group};
                for(u8 i=0;i<full;i++)c[i]=(RT_HudCell){0x61,group};
                if(h==health[p])break;h--;
            }
        }
        s->ammo[p]=ammo[p];s->health[p]=health[p];
    }
    s->high=score->high;s->score[0]=score->players[0];s->score[1]=score->players[1];
    s->credits=credits;s->timer=timer;s->reserves=reserves;s->valid=1;return 1;
}
