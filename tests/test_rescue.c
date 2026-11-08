#include <assert.h>
#include <string.h>
#include "rt_rescue.h"
int main(void)
{
 RT_SessionConfig s={0};
 for(unsigned mode=0;mode<2;mode++)for(unsigned players=0;players<2;players++)
 for(unsigned first=0;first<2;first++)for(unsigned second=0;second<2;second++){
  s.mode=mode;s.players=players;s.agent[0]=first;s.agent[1]=second;
  bool margo=mode==RT_MODE_HOME && (players==RT_HOME_TWO_PLAYERS || first==RT_AGENT_LEILA);
  assert(RT_rescueIsMargo(&s)==margo);
  assert(!strcmp(RT_rescueMessage(&s,FALSE),margo?"RESCUE MARGO":"RESCUE LEILA"));
  assert(!strcmp(RT_rescueMessage(&s,TRUE),margo?"MARGO RESCUED":"LEILA RESCUED"));
 }
 assert(!RT_rescueIsMargo(0));
 return 0;
}
