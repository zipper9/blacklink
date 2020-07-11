#ifndef USER_MESSAGES_H_
#define USER_MESSAGES_H_

#include "../client/w.h"

#ifdef IRAINMAN_INCLUDE_SMILE
static const UINT WM_UPDATE_SMILE      = WM_USER + 1;
static const UINT WM_ANIM_CHANGE_FRAME = WM_USER + 1;
#endif

static const UINT WMU_LINK_ACTIVATED  = WM_USER + 110;
static const UINT WMU_SHOW_QUEUE_ITEM = WM_USER + 120;

#endif // USER_MESSAGES_H_
