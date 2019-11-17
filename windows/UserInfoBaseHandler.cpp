#include "stdafx.h"
#include "UserInfoBaseHandler.h"
#include "LimitEditDlg.h"
#include "../client/Util.h"
#include "../client/FavoriteUser.h"

static const uint16_t speeds[] = { 8, 16, 32, 64, 128, 256, 1024, 10*1024 };

void UserInfoGuiTraits::init()
{
	copyUserMenu.CreatePopupMenu();
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_NICK, CTSTRING(COPY_NICK));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_EXACT_SHARE, CTSTRING(COPY_EXACT_SHARE));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_DESCRIPTION, CTSTRING(COPY_DESCRIPTION));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_APPLICATION, CTSTRING(COPY_APPLICATION));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_TAG, CTSTRING(COPY_TAG));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_CID, CTSTRING(COPY_CID));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_EMAIL_ADDRESS, CTSTRING(COPY_EMAIL_ADDRESS));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_GEO_LOCATION, CTSTRING(COPY_GEO_LOCATION));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_IP, CTSTRING(COPY_IP));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_NICK_IP, CTSTRING(COPY_NICK_IP));
	
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_ALL, CTSTRING(COPY_ALL));
	
	grantMenu.CreatePopupMenu();
	grantMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT, CTSTRING(GRANT_EXTRA_SLOT));
	grantMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT_HOUR, CTSTRING(GRANT_EXTRA_SLOT_HOUR));
	grantMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT_DAY, CTSTRING(GRANT_EXTRA_SLOT_DAY));
	grantMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT_WEEK, CTSTRING(GRANT_EXTRA_SLOT_WEEK));
	grantMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT_PERIOD, CTSTRING(EXTRA_SLOT_TIMEOUT));
	grantMenu.AppendMenu(MF_SEPARATOR);
	grantMenu.AppendMenu(MF_STRING, IDC_UNGRANTSLOT, CTSTRING(REMOVE_EXTRA_SLOT));
	
	userSummaryMenu.CreatePopupMenu();
	
	speedMenu.CreatePopupMenu();
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_NORMAL, CTSTRING(NORMAL));
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_SUPER, CTSTRING(SPEED_SUPER_USER));
	speedMenu.AppendMenu(MF_SEPARATOR);
	
	for (int i = 0; i < _countof(speeds); i++)
	{
		wstring str;
		if (speeds[i] < 1024)
		{
			str = Util::toStringW(speeds[i]);
			str += L' ';
			str += WSTRING(KBPS);
		} else
		{
			str = Util::toStringW(speeds[i]>>10);
			str += L' ';
			str += WSTRING(MBPS);
		}
		speedMenu.AppendMenu(MF_STRING, IDC_SPEED_VALUE + i, str.c_str());
	}
	
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_MANUAL, CTSTRING(SPEED_LIMIT_MANUAL));	
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_BAN,  CTSTRING(BAN_USER));
	
	privateMenu.CreatePopupMenu();
	privateMenu.AppendMenu(MF_STRING, IDC_PM_NORMAL,  CTSTRING(NORMAL));
	privateMenu.AppendMenu(MF_STRING, IDC_PM_IGNORED, CTSTRING(IGNORE_S));
	privateMenu.AppendMenu(MF_STRING, IDC_PM_FREE,    CTSTRING(FREE_PM_ACCESS));
}

void UserInfoGuiTraits::uninit()
{
	copyUserMenu.DestroyMenu();
	grantMenu.DestroyMenu();
	userSummaryMenu.DestroyMenu();
	speedMenu.DestroyMenu();
	privateMenu.DestroyMenu();
}

int UserInfoGuiTraits::getCtrlIdBySpeedLimit(const int limit)
{
	switch (limit)
	{
		case FavoriteUser::UL_SU:
			return IDC_SPEED_SUPER;
		case FavoriteUser::UL_BAN:
			return IDC_SPEED_BAN;
		case FavoriteUser::UL_NONE:
			return IDC_SPEED_NORMAL;
	}
	for (int i = 0; i < _countof(speeds); i++)
		if (limit == speeds[i])
			return IDC_SPEED_VALUE + i;
	return IDC_SPEED_MANUAL;
}

int UserInfoGuiTraits::getSpeedLimitByCtrlId(WORD wID, int lim)
{
	if (wID >= IDC_SPEED_VALUE)
	{
		int index = wID - IDC_SPEED_VALUE;
		dcassert(index < _countof(speeds));
		return speeds[index];
	}
	switch (wID)
	{
		case IDC_SPEED_NORMAL:
			lim = FavoriteUser::UL_NONE;
			break;
		case IDC_SPEED_SUPER:
			lim = FavoriteUser::UL_SU;
			break;
		case IDC_SPEED_BAN:
			lim = FavoriteUser::UL_BAN;
			break;
		case IDC_SPEED_MANUAL:
		{
			if (lim < 0)
				lim = 0;
			LimitEditDlg dlg(lim);
			if (dlg.DoModal() == IDOK)
			{
				lim = dlg.GetLimit();
			}
			break;
		}
		default:
			dcassert(0);
	}	
	return lim;
}
