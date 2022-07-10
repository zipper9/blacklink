/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdafx.h"
#include "UserInfo.h"
#include "resource.h"
#include "LocationUtil.h"
#include "../client/CFlyProfiler.h"

int UserInfo::compareItems(const UserInfo* a, const UserInfo* b, int col)
{
	if (a == nullptr || b == nullptr)
		return 0;
	if (col == COLUMN_NICK)
	{
		PROFILE_THREAD_SCOPED_DESC("COLUMN_NICK")
		const bool a_isOp = a->isOP(),
		           b_isOp = b->isOP();
		if (a_isOp && !b_isOp)
			return -1;
		if (!a_isOp && b_isOp)
			return 1;
		if (BOOLSETTING(SORT_FAVUSERS_FIRST))
		{
			const bool a_isFav = (a->getUser()->getFlags() & User::FAVORITE) != 0;
			const bool b_isFav = (b->getUser()->getFlags() & User::FAVORITE) != 0;
			if (a_isFav && !b_isFav)
				return -1;
			if (!a_isFav && b_isFav)
				return 1;
		}
		// workaround for faster hub loading
		// lstrcmpiA(a->m_identity.getNick().c_str(), b->m_identity.getNick().c_str());
	}
	switch (col)
	{
		case COLUMN_NICK:
		{
			PROFILE_THREAD_SCOPED_DESC("COLUMN_NICK")
			return Util::defaultSort(Text::toT(a->getIdentity().getNick()), Text::toT(b->getIdentity().getNick()));
		}
		case COLUMN_SHARED:
		case COLUMN_EXACT_SHARED:
		{
			PROFILE_THREAD_SCOPED_DESC("COLUMN_SHARED")
			return compare(a->getIdentity().getBytesShared(), b->getIdentity().getBytesShared());
		}
		case COLUMN_SLOTS:
		{
			PROFILE_THREAD_SCOPED_DESC("COLUMN_SLOTS")
			return compare(a->getIdentity().getSlots(), b->getIdentity().getSlots());
		}
		case COLUMN_HUBS:
		{
			PROFILE_THREAD_SCOPED_DESC("COLUMN_HUBS")
			return compare(a->getIdentity().getHubNormalRegOper(), b->getIdentity().getHubNormalRegOper());
		}
#ifdef BL_FEATURE_IP_DATABASE
		case COLUMN_UPLOAD:
		{
			PROFILE_THREAD_SCOPED_DESC("COLUMN_UPLOAD")
			return compare(a->getUser()->getBytesUploaded(), b->getUser()->getBytesUploaded());
		}
		case COLUMN_DOWNLOAD:
		{
			PROFILE_THREAD_SCOPED_DESC("COLUMN_DOWNLOAD")
			return compare(a->getUser()->getBytesDownloaded(), b->getUser()->getBytesDownloaded());
		}
		case COLUMN_MESSAGES:
		{
			PROFILE_THREAD_SCOPED_DESC("COLUMN_MESSAGES")
			return compare(a->getUser()->getMessageCount(), b->getUser()->getMessageCount());
		}
#endif
		case COLUMN_P2P_GUARD:
		{
			PROFILE_THREAD_SCOPED_DESC("COLUMN_P2P_GUARD")
			const_cast<UserInfo*>(a)->loadP2PGuard();
			const_cast<UserInfo*>(b)->loadP2PGuard();
			return compare(a->getIdentity().getP2PGuard(), b->getIdentity().getP2PGuard());
		}
		case COLUMN_IP:
		{
			PROFILE_THREAD_SCOPED_DESC("COLUMN_IP")
			return compare(a->getIdentity().getConnectIP(), b->getIdentity().getConnectIP());
		}
		case COLUMN_GEO_LOCATION:
		{
			PROFILE_THREAD_SCOPED_DESC("COLUMN_GEO_LOCATION")
			const_cast<UserInfo*>(a)->loadLocation();
			const_cast<UserInfo*>(b)->loadLocation();
			return Util::defaultSort(a->getText(col), b->getText(col));
		}
		case COLUMN_FLY_HUB_GENDER:
		{
			PROFILE_THREAD_SCOPED_DESC("COLUMN_FLY_HUB_GENDER")
			return compare(a->getIdentity().getGenderType(), b->getIdentity().getGenderType());
		}
		case COLUMN_FLY_HUB_COUNT_FILES:
		{
			return compare(a->getIdentity().getExtJSONCountFiles(), b->getIdentity().getExtJSONCountFiles());
		}
		case COLUMN_FLY_HUB_LAST_SHARE_DATE:
		{
			return compare(a->getIdentity().getExtJSONLastSharedDate(), b->getIdentity().getExtJSONLastSharedDate());
		}
		case COLUMN_FLY_HUB_RAM:
		{
			return compare(a->getIdentity().getExtJSONRAMWorkingSet(), b->getIdentity().getExtJSONRAMWorkingSet());
		}
		case COLUMN_FLY_HUB_SQLITE_DB_SIZE:
		{
			return compare(a->getIdentity().getExtJSONSQLiteDBSize(), b->getIdentity().getExtJSONSQLiteDBSize());
		}
		case COLUMN_FLY_HUB_QUEUE:
		{
			return compare(a->getIdentity().getExtJSONQueueFiles(), b->getIdentity().getExtJSONQueueFiles());
		}
		case COLUMN_FLY_HUB_TIMES:
		{
			return compare(a->getIdentity().getExtJSONTimesStartGUI() + a->getIdentity().getExtJSONTimesStartCore(),
			               b->getIdentity().getExtJSONTimesStartGUI() + b->getIdentity().getExtJSONTimesStartCore());
		}
		case COLUMN_FLY_HUB_SUPPORT_INFO:
		{
			return compare(a->getIdentity().getExtJSONSupportInfo(), b->getIdentity().getExtJSONSupportInfo());
		}
	}
	{
		PROFILE_THREAD_SCOPED_DESC("COLUMN_DEFAULT")
		return Util::defaultSort(a->getText(col), b->getText(col));
	}
}

tstring UserInfo::getText(int col) const
{
#ifdef IRAINMAN_USE_HIDDEN_USERS
	// dcassert(isHidden() == false);
#endif
	switch (col)
	{
		case COLUMN_NICK:
		{
			return Text::toT(getIdentity().getNick());
		}
		case COLUMN_SHARED:
		{
			return Util::formatBytesT(getIdentity().getBytesShared());
		}
		case COLUMN_EXACT_SHARED:
		{
			return Util::formatExactSizeT(getIdentity().getBytesShared());
		}
		case COLUMN_DESCRIPTION:
		{
			return Text::toT(getIdentity().getDescription());
		}
		case COLUMN_APPLICATION:
		{
			return Text::toT(getIdentity().getApplication());
		}
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
		case COLUMN_CONNECTION:
		{
			return getDownloadSpeed();
		}
#endif
#ifdef BL_FEATURE_IP_DATABASE
		case COLUMN_UPLOAD:
		{
			const UserPtr& user = getUser();
			user->loadIPStat();
			auto value = user->getBytesUploaded();
			return value ? Util::formatBytesT(value) : Util::emptyStringT;
		}
		case COLUMN_DOWNLOAD:
		{
			const UserPtr& user = getUser();
			user->loadIPStat();
			auto value = user->getBytesDownloaded();
			return value ? Util::formatBytesT(value) : Util::emptyStringT;
		}
		case COLUMN_P2P_GUARD:
		{
			return Text::toT(getIdentity().getP2PGuard());
		}
		case COLUMN_MESSAGES:
		{
			const UserPtr& user = getUser();
			user->loadUserStat();
			auto value = user->getMessageCount();
			return value ? Util::toStringT(value) : Util::emptyStringT;
		}
#endif // BL_FEATURE_IP_DATABASE
		case COLUMN_IP:
		{
			return Util::printIpAddressT(getIdentity().getConnectIP());
		}
		case COLUMN_GEO_LOCATION:
		{
			return Text::toT(Util::getDescription(getIpInfo()));
		}
		case COLUMN_EMAIL:
		{
			return Text::toT(getIdentity().getEmail());
		}
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
		case COLUMN_MODE:
		{
			return (getIdentity().isTcpActive(getClient())) ? _T("A") : _T("P");
		}
#endif
		case COLUMN_HUBS:
		{
			return getHubs();
		}
		case COLUMN_SLOTS:
		{
			const auto slots = getIdentity().getSlots();
			if (slots)
				return Util::toStringW(slots);
			return Util::emptyStringT;
		}
		case COLUMN_CID:
		{
			return Text::toT(getIdentity().getCID());
		}
		case COLUMN_TAG:
		{
			return Text::toT(getIdentity().getTag());
		}
#ifdef FLYLINKDC_USE_EXT_JSON
		case COLUMN_FLY_HUB_GENDER:
		{
			return getIdentity().getGenderTypeAsString();
		}
		case COLUMN_FLY_HUB_COUNT_FILES:
		{
			return Text::toT(getIdentity().getExtJSONCountFilesAsText());
		}
		case COLUMN_FLY_HUB_LAST_SHARE_DATE:
		{
			return Text::toT(getIdentity().getExtJSONLastSharedDateAsText());
		}
		case COLUMN_FLY_HUB_RAM:
		{
			return Text::toT(getIdentity().getExtJSONHubRamAsText());
		}
		case COLUMN_FLY_HUB_SQLITE_DB_SIZE:
		{
			return Text::toT(getIdentity().getExtJSONSQLiteDBSizeAsText());
		}
		case COLUMN_FLY_HUB_QUEUE:
		{
			return Text::toT(getIdentity().getExtJSONQueueFilesText());
		}
		case COLUMN_FLY_HUB_TIMES:
		{
			return Text::toT(getIdentity().getExtJSONTimesStartCoreText());
		}
		case COLUMN_FLY_HUB_SUPPORT_INFO:
		{
			return Text::toT(getIdentity().getExtJSONSupportInfo());
		}
		
#endif
		default:
		{
			dcassert(0);
			return Util::emptyStringT;
		}
	}
}


tstring UserInfo::formatSpeedLimit(const uint32_t limit)
{
	return limit ? Util::formatBytesT(limit) + _T('/') + TSTRING(S) : Util::emptyStringT;
}

string UserInfo::getSpeedLimitText(int lim)
{
	switch (lim)
	{
		case FavoriteUser::UL_SU:
			return STRING(SPEED_SUPER_USER);
		case FavoriteUser::UL_BAN:
			return "BAN";
	}
	if (lim > 0)
		return Util::formatBytes(int64_t(lim) << 10) + '/' + STRING(S);

	return Util::emptyString;
}

tstring UserInfo::getLimit() const
{
	return formatSpeedLimit(getIdentity().getLimit());
}

tstring UserInfo::getDownloadSpeed() const
{
	return formatSpeedLimit(getIdentity().getDownloadSpeed());
}

void UserInfo::loadP2PGuard()
{
	getIdentityRW().loadP2PGuard();
	stateP2PGuard = STATE_DONE;
}

void UserInfo::loadLocation()
{
	IpAddress ip = getIp();
	if (Util::isValidIp(ip))
		Util::getIpInfo(ip, ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
	else
	{
		ipInfo.clearCountry();
		ipInfo.clearLocation();
	}
	stateLocation = STATE_DONE;
}

void UserInfo::clearLocation()
{
	ipInfo.clearCountry();
	ipInfo.clearLocation();
	stateLocation = STATE_DONE;
}

uint8_t UserInfo::getImageIndex() const
{
	const auto& id = ou->getIdentity();
	uint8_t image;
	if (id.isOp())
	{
		image = 0;
	}
	else if (id.getStatusBit(Identity::SF_SERVER))
	{
		image = 1;
	}
	else
	{
		auto speed = id.getLimit();
		if (!speed)
		{
			speed = id.getDownloadSpeed();
		}
		if (speed >= 10 * 1024 * 1024) // over 10 MB
			image = 2; // fast user
		else if (speed > 1024 * 1024 / 10) // over 100 KB
			image = 3; // normal user
		else
			image = 4; // slow user
	}
	
	if (id.getStatusBit(Identity::SF_AWAY))
		image += 5;
	if (id.getStatusBit(Identity::SF_PASSIVE))
		image += 10;
	return image;
}
