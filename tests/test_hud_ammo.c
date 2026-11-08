#include <stdint.h>
typedef int8_t s8;
#include "../src/campaign_hud.c"
#include <assert.h>
#include <stdio.h>
static unsigned number(const RT_HudCell *p){unsigned n=0;for(unsigned i=0;i<3;i++)n=n*10+(p[i].glyph==255?0:p[i].glyph);return n;}
int main(void){
 RT_HudCell initial[144];for(unsigned i=0;i<144;i++)initial[i]=(RT_HudCell){255,3};
 RT_CampaignHudState s;RT_CampaignScore score={0};u16 primary[2]={50,75},machine[2]={80,100};u8 health[2]={64,64};
 assert(RT_campaignHudBegin(&s,initial,1,2));
 assert(RT_campaignHudUpdate(&s,&score,150,primary,health,6,0,machine,3));
 assert(number(s.hud.cells+111)==50 && number(s.hud.cells+131)==75);
 assert(number(s.hud.cells+75)==80 && number(s.hud.cells+105)==100);
 RT_HudCell credit[12];memcpy(credit,s.hud.cells+84,sizeof credit);
 s.dirtyRows=0;primary[0]+=20;
 assert(RT_campaignHudUpdate(&s,&score,150,primary,health,6,0,machine,3));
 assert(s.dirtyRows==8 && number(s.hud.cells+111)==70 && number(s.hud.cells+75)==80);
 s.dirtyRows=0;machine[1]--;
 assert(RT_campaignHudUpdate(&s,&score,150,primary,health,6,0,machine,3));assert(s.dirtyRows==4);
 s.dirtyRows=0;
 assert(RT_campaignHudUpdate(&s,&score,150,primary,health,6,0,machine,2));
 for(unsigned i=0;i<6;i++)assert(s.hud.cells[72+i].glyph==255);
 assert(s.hud.cells[102].glyph==22 && number(s.hud.cells+105)==99);
 assert(!memcmp(credit,s.hud.cells+84,sizeof credit));
 s.dirtyRows=0;
 assert(RT_campaignHudUpdate(&s,&score,150,primary,health,6,0,machine,2));assert(!s.dirtyRows);
 s.valid=0;assert(RT_campaignHudUpdate(&s,&score,150,primary,health,6,0,machine,2));
 assert(!memcmp(credit,s.hud.cells+84,sizeof credit));
 puts("Independent Home ammo counters, refill visibility, MG removal, credits and dirty-row behavior passed.");
}
