#include "stdafx.h"
#include "UserInfoBaseHandler.h"
#include "LimitEditDlg.h"
#include "../client/Util.h"
#include "../client/FavoriteUser.h"
#include "../client/UserInfoBase.h"

string UserInfoGuiTraits::g_hubHint;
UserPtr UserInfoBaseHandlerTraitsUser<UserPtr>::g_user = nullptr;
OnlineUserPtr UserInfoBaseHandlerTraitsUser<OnlineUserPtr>::g_user = nullptr;

OMenu UserInfoGuiTraits::copyUserMenu;
OMenu UserInfoGuiTraits::grantMenu;
OMenu UserInfoGuiTraits::speedMenu;
OMenu UserInfoGuiTraits::userSummaryMenu;
OMenu UserInfoGuiTraits::privateMenu;
OMenu UserInfoGuiTraits::favUserMenu;

static const uint16_t speeds[] = { 8, 16, 32, 64, 128, 256, 1024, 10*1024 };

int UserInfoGuiTraits::speedMenuCustomVal = -1;
int UserInfoGuiTraits::displayedSpeed[2];

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
	grantMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT_PERIOD, CTSTRING(SET_EXTRA_SLOT_TIME));
	grantMenu.AppendMenu(MF_SEPARATOR);
	grantMenu.AppendMenu(MF_STRING, IDC_UNGRANTSLOT, CTSTRING(REMOVE_EXTRA_SLOT));
	
	userSummaryMenu.CreatePopupMenu();
	
	speedMenu.CreatePopupMenu();
	MENUITEMINFO mii = { sizeof(mii) };
	mii.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID;
	mii.fType = MFT_RADIOCHECK;
	mii.wID = IDC_SPEED_NORMAL;
	mii.dwTypeData = const_cast<TCHAR*>(CTSTRING(NORMAL));
	speedMenu.InsertMenuItem(0, TRUE, &mii);
	mii.wID = IDC_SPEED_SUPER;
	mii.dwTypeData = const_cast<TCHAR*>(CTSTRING(SPEED_SUPER_USER));
	speedMenu.InsertMenuItem(1, TRUE, &mii);
	speedMenu.AppendMenu(MF_SEPARATOR);
	
	int pos = 3;
	for (int i = 0; i < _countof(speeds); i++)
	{
		tstring str;
		if (speeds[i] < 1024)
		{
			str = Util::toStringT(speeds[i]);
			str += _T(' ');
			str += TSTRING(KBPS);
		} else
		{
			str = Util::toStringT(speeds[i]>>10);
			str += _T(' ');
			str += TSTRING(MBPS);
		}
		mii.wID = IDC_SPEED_VALUE + i;
		mii.dwTypeData = const_cast<TCHAR*>(str.c_str());
		speedMenu.InsertMenuItem(pos++, TRUE, &mii);
	}
	
	mii.wID = IDC_SPEED_MANUAL;
	mii.dwTypeData = const_cast<TCHAR*>(CTSTRING(SPEED_LIMIT_MANUAL));
	speedMenu.InsertMenuItem(pos++, TRUE, &mii);
	mii.wID = IDC_SPEED_BAN;
	mii.dwTypeData = const_cast<TCHAR*>(CTSTRING(BAN_USER));
	speedMenu.InsertMenuItem(pos, TRUE, &mii);
	
	privateMenu.CreatePopupMenu();
	mii.wID = IDC_PM_NORMAL;
	mii.dwTypeData = const_cast<TCHAR*>(CTSTRING(NORMAL));
	privateMenu.InsertMenuItem(0, TRUE, &mii);
	mii.wID = IDC_PM_IGNORED;
	mii.dwTypeData = const_cast<TCHAR*>(CTSTRING(IGNORE_PRIVATE));
	privateMenu.InsertMenuItem(1, TRUE, &mii);
	mii.wID = IDC_PM_FREE;
	mii.dwTypeData = const_cast<TCHAR*>(CTSTRING(FREE_PM_ACCESS));
	privateMenu.InsertMenuItem(2, TRUE, &mii);

	favUserMenu.CreatePopupMenu();
}

void UserInfoGuiTraits::uninit()
{
	copyUserMenu.DestroyMenu();
	grantMenu.DestroyMenu();
	userSummaryMenu.DestroyMenu();
	speedMenu.DestroyMenu();
	privateMenu.DestroyMenu();
	favUserMenu.DestroyMenu();
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

int UserInfoGuiTraits::getSpeedLimitByCtrlId(WORD wID, int lim, const tstring& nick)
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
			LimitEditDlg dlg(true, nick, lim, 1, 1024);
			if (dlg.DoModal() == IDOK)
				lim = dlg.getLimit();
			break;
		}
		default:
			dcassert(0);
	}	
	return lim;
}

void UserInfoGuiTraits::updateSpeedMenuText(int customSpeed)
{
	int normalSpeed = SETTING(PER_USER_UPLOAD_SPEED_LIMIT);
	if (displayedSpeed[0] != normalSpeed)
	{
		displayedSpeed[0] = normalSpeed;
		tstring text = TSTRING(NORMAL);
		if (normalSpeed)
			text += _T(" (") + Util::toStringT(normalSpeed) + _T(' ') + TSTRING(KBPS) + _T(")");
		speedMenu.RenameItem(IDC_SPEED_NORMAL, text);
	}
	if (getCtrlIdBySpeedLimit(customSpeed) != IDC_SPEED_MANUAL)
		customSpeed = 0;
	if (displayedSpeed[1] != customSpeed)
	{
		displayedSpeed[1] = customSpeed;
		tstring text = TSTRING(SPEED_LIMIT_MANUAL);
		if (customSpeed)
			text += _T(" (") + Util::toStringT(customSpeed) + _T(' ') + TSTRING(KBPS) + _T(")");
		speedMenu.RenameItem(IDC_SPEED_MANUAL, text);
	}
}

void FavUserTraits::init(const UserInfoBase& ui)
{
	dcassert(ui.getUser());
	if (ui.getUser())
	{
		Flags::MaskType flags;
		isFav = FavoriteManager::getInstance()->getFavUserParam(ui.getUser(), flags, uploadLimit);
		
		if (isFav)
		{
			isAutoGrantSlot = (flags & FavoriteUser::FLAG_GRANT_SLOT) != 0;
			isIgnoredPm = (flags & FavoriteUser::FLAG_IGNORE_PRIVATE) != 0;
			isFreePm = (flags & FavoriteUser::FLAG_FREE_PM_ACCESS) != 0;
		}
		else
		{
			isAutoGrantSlot = false;			
			isIgnoredPm = false;
			isFreePm = false;
		}
		
		isIgnoredByName = UserManager::getInstance()->isInIgnoreList(ui.getUser()->getLastNick());
		isOnline = ui.getUser()->isOnline();

		isEmpty = false;
	}
}
