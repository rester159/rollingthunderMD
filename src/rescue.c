#include "rt_rescue.h"
bool RT_rescueIsMargo(const RT_SessionConfig *s)
{
    return s && s->mode==RT_MODE_HOME &&
        (s->players==RT_HOME_TWO_PLAYERS || s->agent[0]==RT_AGENT_LEILA);
}
const char *RT_rescueMessage(const RT_SessionConfig *s,bool completed)
{
    if(RT_rescueIsMargo(s))return completed?"MARGO RESCUED":"RESCUE MARGO";
    return completed?"LEILA RESCUED":"RESCUE LEILA";
}
