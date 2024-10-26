#include "stdafx.h"
#include "UsersFrame.h"
#include "LimitEditDlg.h"
#include "HubFrame.h"
#include "QueueFrame.h"
#include "../client/LocationUtil.h"
#include "../client/FormatUtil.h"
#include "../client/Util.h"
#include "../client/UploadManager.h"
#include "../client/QueueManager.h"
#include "../client/UserManager.h"
#include "../client/ConfCore.h"

string UserInfoGuiTraits::g_hubHint;
UserPtr UserInfoBaseHandlerTraitsUser<UserPtr>::g_user = nullptr;
OnlineUserPtr UserInfoBaseHandlerTraitsUser<OnlineUserPtr>::g_user = nullptr;

OMenu UserInfoGuiTraits::copyUserMenu;
OMenu UserInfoGuiTraits::grantMenu;
OMenu UserInfoGuiTraits::speedMenu;
OMenu UserInfoGuiTraits::userSummaryMenu;
OMenu UserInfoGuiTraits::privateMenu;
OMenu UserInfoGuiTraits::favUserMenu;

vector<UserInfoGuiTraits::DetailsItem> UserInfoGuiTraits::detailsItems;
UINT UserInfoGuiTraits::detailsItemMaxId = 0;

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

	MenuHelper::addStaticMenu(grantMenu);
	MenuHelper::addStaticMenu(copyUserMenu);
	MenuHelper::addStaticMenu(userSummaryMenu);
	MenuHelper::addStaticMenu(speedMenu);
	MenuHelper::addStaticMenu(privateMenu);
	MenuHelper::addStaticMenu(favUserMenu);
}

void UserInfoGuiTraits::uninit()
{
	// NOTE: no need to call MenuHelper::removeStaticMenu
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

bool UserInfoGuiTraits::getSpeedLimitByCtrlId(WORD wID, int& lim, const tstring& nick)
{
	if (wID >= IDC_SPEED_VALUE)
	{
		int index = wID - IDC_SPEED_VALUE;
		dcassert(index < _countof(speeds));
		lim = speeds[index];
		return true;
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
			if (dlg.DoModal() != IDOK) return false;
			lim = dlg.getLimit();
			break;
		}
		default:
			dcassert(0);
			return false;
	}	
	return true;
}

void UserInfoGuiTraits::updateSpeedMenuText(int customSpeed)
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const int normalSpeed = ss->getInt(Conf::PER_USER_UPLOAD_SPEED_LIMIT);
	ss->unlockRead();
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

void UserInfoGuiTraits::processDetailsMenu(WORD id)
{
	for (const auto& item : detailsItems)
		if (item.id == id)
		{
			if (item.type == DetailsItem::TYPE_USER)
			{
				UserPtr user = ClientManager::findUser(item.cid);
				if (user)
					UserManager::getInstance()->openUserUrl(item.hubUrl, user);	
			}
			else if (item.type == DetailsItem::TYPE_FAV_INFO)
			{
				UsersFrame::openWindow();
				if (UsersFrame::g_frame)
					UsersFrame::g_frame->showUser(item.cid);
			}
			else if (item.type == DetailsItem::TYPE_QUEUED)
			{
				QueueFrame::openWindow();
				if (QueueFrame::g_frame)
				{
					string target = Text::fromT(item.text);
					QueueFrame::g_frame->showQueueItem(target, false);
				}
			}
			break;
		}
	detailsItems.clear();
}

void UserInfoGuiTraits::addSummaryMenu(const OnlineUserPtr& ou)
{
	UINT idc = IDC_USER_INFO;
	const UserPtr& user = ou->getUser();
	userSummaryMenu.InsertSeparatorLast(Text::toT(ou->getIdentity().getNick()));

	ClientManager::UserParams params;
	if (ClientManager::getUserParams(user, params))
	{
		tstring userInfo = TSTRING(SLOTS) + _T(": ") + Util::toStringT(params.slots) + _T(", ") + TSTRING(SHARED) + _T(": ") + Util::formatBytesT(params.bytesShared);
		
		if (params.limit)
			userInfo += _T(", ") + TSTRING(UPLOAD_SPEED_LIMIT) + _T(": ") + Util::formatBytesT(params.limit) + _T('/') + TSTRING(DATETIME_SECONDS);

		userSummaryMenu.AppendMenu(MF_STRING, (UINT_PTR) 0, userInfo.c_str());

		uint64_t slotTick = UploadManager::getInstance()->getReservedSlotTick(user);
		if (slotTick)
		{
			uint64_t currentTick = GET_TICK();
			if (slotTick >= currentTick + 1000)
			{
				const tstring note = TSTRING(EXTRA_SLOT_TIMEOUT) + _T(": ") + Util::formatSecondsT((slotTick-currentTick)/1000);
				userSummaryMenu.AppendMenu(MF_STRING, (UINT_PTR) 0, note.c_str());
			}
		}

		bool hasIp4 = Util::isValidIp4(params.ip4);
		bool hasIp6 = Util::isValidIp6(params.ip6);
		if (hasIp4 || hasIp6)
		{
			userSummaryMenu.AppendMenu(MF_STRING, (UINT_PTR) 0, UserInfoSimple::getTagIP(params).c_str());
			IPInfo ipInfo;
			IpAddress ip;
			if (hasIp4)
			{
				ip.data.v4 = params.ip4;
				ip.type = AF_INET;
			}
			else
			{
				memcpy(ip.data.v6.data, params.ip6.data, 16);
				ip.type = AF_INET6;
			}
			Util::getIpInfo(ip, ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION, true); // get it from cache
			if (!ipInfo.country.empty() || !ipInfo.location.empty())
			{
				tstring text = TSTRING(LOCATION_BARE) + _T(": ");
				if (!ipInfo.country.empty() && !ipInfo.location.empty())
				{
					text += Text::toT(ipInfo.country);
					text += _T(", ");
				}
				text += Text::toT(Util::getDescription(ipInfo));
				userSummaryMenu.AppendMenu(MF_STRING, (UINT_PTR) 0, text.c_str());
			}
		}
		else
		{
			tstring tagIp = UserInfoSimple::getTagIP(params);
			if (!tagIp.empty())
				userSummaryMenu.AppendMenu(MF_STRING, (UINT_PTR) 0, tagIp.c_str());
		}
		HubFrame::addDupUsersToSummaryMenu(params, detailsItems, idc);
	}

	bool hasCaption = false;
	{
		UploadManager::LockInstanceQueue lockedInstance;
		const auto& users = lockedInstance->getUploadQueueL();
		for (auto uit = users.cbegin(); uit != users.cend(); ++uit)
		{
			if (uit->getUser() == user)
			{
				int countAdded = 0;
				const auto& waitingFiles = uit->getWaitingFiles();
				for (auto i = waitingFiles.cbegin(); i != waitingFiles.cend(); ++i)
				{
					if (!hasCaption)
					{
						userSummaryMenu.InsertSeparatorLast(TSTRING(USER_WAIT_MENU));
						hasCaption = true;
					}
					const tstring note =
					    Text::toT(Util::ellipsizePath((*i)->getFile())) +
					    _T("\t[") +
					    Util::toStringT((double)(*i)->getPos() * 100.0 / (double)(*i)->getSize()) +
					    _T("% ") +
					    Util::formatSecondsT(GET_TIME() - (*i)->getTime()) +
					    _T(']');
					userSummaryMenu.AppendMenu(MF_STRING, (UINT_PTR) 0, note.c_str());
					if (countAdded++ == 10)
					{
						userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, (UINT_PTR) 0, _T("..."));
						break;
					}
				}
			}
		}
	}
	hasCaption = false;
	{
		int countAdded = 0;
		DetailsItem item;
		item.type = DetailsItem::TYPE_QUEUED;
		item.flags = 0;
		QueueRLock(*QueueItem::g_cs);
		QueueManager::LockFileQueueShared fileQueue;
		const auto& downloads = fileQueue.getQueueL();
		for (auto j = downloads.cbegin(); j != downloads.cend(); ++j)
		{
			const QueueItemPtr& qi = j->second;
			const bool src = qi->isSourceL(user);
			bool badsrc = false;
			if (!src)
			{
				badsrc = qi->isBadSourceL(user);
			}
			if (src || badsrc)
			{
				if (!hasCaption)
				{
					userSummaryMenu.InsertSeparatorLast(TSTRING(NEED_USER_FILES_MENU));
					hasCaption = true;
				}
				item.text = Text::toT(qi->getTarget());
				tstring note = item.text;
				if (qi->getSize() > 0)
				{
					note += _T("\t(");
					note += Util::toStringT((double)qi->getDownloadedBytes() * 100.0 / (double)qi->getSize());
					note += _T("%)");
				}
				if (!badsrc)
					item.id = idc++;
				else
					item.id = 0;
				const UINT flags = MF_STRING | (badsrc ? MF_GRAYED : 0);
				userSummaryMenu.AppendMenu(flags, (UINT_PTR) item.id, note.c_str());
				if (!badsrc) detailsItems.push_back(item);
				if (countAdded++ == 10)
				{
					userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, (UINT_PTR) 0, _T("..."));
					break;
				}
			}
		}
	}
	detailsItemMaxId = idc - 1;
}

void UserInfoGuiTraits::copyUserInfo(WORD idc, const Identity& id)
{
	string sCopy;
	switch (idc)
	{
		case IDC_COPY_NICK:
			sCopy += id.getNick();
			break;
		case IDC_COPY_EXACT_SHARE:
			sCopy += Identity::formatShareBytes(id.getBytesShared());
			break;
		case IDC_COPY_DESCRIPTION:
			sCopy += id.getDescription();
			break;
		case IDC_COPY_APPLICATION:
			sCopy += id.getApplication();
			break;
		case IDC_COPY_TAG:
			sCopy += id.getTag();
			break;
		case IDC_COPY_CID:
			sCopy += id.getCID();
			break;
		case IDC_COPY_EMAIL_ADDRESS:
			sCopy += id.getEmail();
			break;
		case IDC_COPY_GEO_LOCATION:
		{
			IPInfo ipInfo;
			Util::getIpInfo(id.getConnectIP(), ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
			sCopy += Util::getDescription(ipInfo);
			break;
		}
		case IDC_COPY_IP:
			sCopy += Util::printIpAddress(id.getConnectIP());
			break;
		case IDC_COPY_NICK_IP:
		{
			// TODO translate
			sCopy += "User Info:\r\n"
			         "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n";
			Ip4Address ip4 = id.getIP4();
			if (Util::isValidIp4(ip4))
			{
				IpAddress ip;
				ip.type = AF_INET;
				ip.data.v4 = ip4;
				sCopy += "\tIPv4: " + Identity::formatIpString(ip) + "\r\n";
			}
			Ip6Address ip6 = id.getIP6();
			if (Util::isValidIp6(ip6))
			{
				IpAddress ip;
				ip.type = AF_INET6;
				ip.data.v6 = ip6;
				sCopy += "\tIPv6: " + Identity::formatIpString(ip) + "\r\n";
			}
			break;
		}
		case IDC_COPY_ALL:
		{
			// TODO: Use Identity::getReport ?
			const auto& u = id.getUser();
			bool isNMDC = (u->getFlags() & User::NMDC) != 0;
			sCopy += "User info:\r\n"
			         "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n" +
			         "\tNicks: " + Util::toString(ClientManager::getNicks(u->getCID(), Util::emptyString)) + "\r\n" +
			         "\tTag: " + id.getTag() + "\r\n" +
			         "\t" + STRING(HUBS) + ": " + Util::toString(ClientManager::getHubs(u->getCID(), Util::emptyString)) + "\r\n" +
			         "\t" + STRING(SHARED) + ": " + Identity::formatShareBytes(id.getBytesShared()) + (isNMDC ? Util::emptyString : "(" + STRING(SHARED_FILES) +
			                 ": " + Util::toString(id.getSharedFiles()) + ")") + "\r\n" +
			         "\t" + STRING(DESCRIPTION) + ": " + id.getDescription() + "\r\n" +
			         "\t" + STRING(APPLICATION) + ": " + id.getApplication() + "\r\n";
			const auto con = Identity::formatSpeedLimit(id.getDownloadSpeed());
			if (!con.empty())
			{
				sCopy += "\t";
				sCopy += isNMDC ? STRING(CONNECTION) : "Download speed";
				sCopy += ": " + con + "\r\n";
			}
			const auto lim = Identity::formatSpeedLimit(u->getLimit());
			if (!lim.empty())
			{
				sCopy += "\tUpload limit: " + lim + "\r\n";
			}
			sCopy += "\tE-Mail: " + id.getEmail() + "\r\n" +
			         "\tClient Type: " + Util::toString(id.getClientType()) + "\r\n" +
			         "\tMode: " + (id.isTcpActive() ? 'A' : 'P') + "\r\n" +
			         "\t" + STRING(SLOTS) + ": " + Util::toString(id.getSlots()) + "\r\n";
			Ip4Address ip4 = id.getIP4();
			if (Util::isValidIp4(ip4))
			{
				IpAddress ip;
				ip.type = AF_INET;
				ip.data.v4 = ip4;
				sCopy += "\tIPv4: " + Identity::formatIpString(ip) + "\r\n";
			}
			Ip6Address ip6 = id.getIP6();
			if (Util::isValidIp6(ip6))
			{
				IpAddress ip;
				ip.type = AF_INET6;
				ip.data.v6 = ip6;
				sCopy += "\tIPv6: " + Identity::formatIpString(ip) + "\r\n";
			}
			const auto su = id.getSupports();
			if (!su.empty())
			{
				sCopy += "\tKnown supports: " + su;
			}
			break;
		}
		default:
			dcassert(0);
			return;
	}
	WinUtil::setClipboard(sCopy);
}

void UserInfoGuiTraits::showFavUser(const UserPtr& user)
{
	UsersFrame::openWindow();
	if (UsersFrame::g_frame && user)
		UsersFrame::g_frame->showUser(user->getCID());
}

void FavUserTraits::init(const UserInfoSimple& ui)
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
		
		int type;
		if (UserManager::getInstance()->isInIgnoreList(ui.getHintedUser().getNick(), &type))
		{
			isIgnoredByName = type == UserManager::IGNORE_NICK;
			isIgnoredByWildcard = !isIgnoredByName;
		}
		else
			isIgnoredByName = isIgnoredByWildcard = false;
		isOnline = ui.getUser()->isOnline();
		if (!ui.hintedUser.hint.empty())
			isHubConnected = ClientManager::isConnected(ui.hintedUser.hint);
		else
		{
			string url = FavoriteManager::getInstance()->getUserUrl(ui.getUser());
			isHubConnected = url.empty() ? false : ClientManager::isConnected(url);
		}
		isEmpty = false;
	}
}
