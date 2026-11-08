#ifndef RT_RESCUE_H
#define RT_RESCUE_H
#include "rt_home_mode.h"
/* All Home co-op teams rescue Margo, including duplicate Albatross teams. */
bool RT_rescueIsMargo(const RT_SessionConfig *session);
const char *RT_rescueMessage(const RT_SessionConfig *session,bool completed);
#endif
