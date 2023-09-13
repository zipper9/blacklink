#ifndef USER_MESSAGES_H_
#define USER_MESSAGES_H_

#include "../client/w.h"
#include "../client/FeatureDef.h"

#ifdef BL_UI_FEATURE_EMOTICONS
static const UINT WM_ANIM_CHANGE_FRAME  = WM_USER + 1;
static const UINT WMU_PASTE_TEXT        = WM_USER + 108;
#endif

static const UINT WMU_TRAY_ICON         = WM_USER + 109;
static const UINT WMU_LINK_ACTIVATED    = WM_USER + 110;
static const UINT WMU_DIALOG_CREATED    = WM_USER + 111;
static const UINT WMU_CHAT_LINK_CLICKED = WM_USER + 112;
static const UINT WMU_UPDATE_LAYOUT     = WM_USER + 113;
static const UINT WMU_DIALOG_CLOSED     = WM_USER + 114;
static const UINT WMU_SHOW_QUEUE_ITEM   = WM_USER + 120;
static const UINT WMU_LISTENER_INIT     = WM_USER + 121;

static const UINT WMU_USER_INITDIALOG   = WM_APP + 501;
static const UINT WMU_RESTART_REQUIRED  = WM_APP + 502;

#endif // USER_MESSAGES_H_
