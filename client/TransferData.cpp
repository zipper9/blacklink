//-----------------------------------------------------------------------------
//(c) 2007-2017 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#include "stdinc.h"
#include <ctime>

#include "ResourceManager.h"
#ifdef FLYLINKDC_USE_TORRENT
#include "libtorrent/torrent_status.hpp"
#endif
#include "TransferData.h"


#ifdef FLYLINKDC_USE_TORRENT
void TransferData::init(libtorrent::torrent_status const& s)
{
	//l_td.m_hinted_user = d->getHintedUser();
	//m_token = s.info_hash.to_string(); для токена используется m_sha1
	
	sha1 = s.info_hash;
	pos = 1;
	speed = s.download_payload_rate;
	actual = s.total_done;
	secondsLeft = 0;
	startTime = 0;
	size = s.total_wanted;
	type = 0;
	path = s.save_path + s.name;
	numSeeds = s.num_seeds;
	numPeers = s.num_peers;
	isTorrent = true;
	isSeeding = s.state == libtorrent::torrent_status::seeding || s.state == libtorrent::torrent_status::finished;
	isPaused = (s.flags & libtorrent::torrent_flags::paused) == libtorrent::torrent_flags::paused;
	//calcPercent();
	///l_td.statusString += _T("[Torrent] Peers:") + Util::toStringT(s.num_peers) + _T(" Seeds:") + Util::toStringT(s.num_seeds) + _T(" ");
	//l_td.statusString += Text::tformat(TSTRING(DOWNLOADED_BYTES), Util::formatBytesW(l_td.m_pos).c_str(),
	//  l_td.m_percent, l_td.get_elapsed(aTick).c_str());

#if 0 // FIXME FIXME FIXME
	statusString = _T("[Torrent] ");
	switch (s.state)
	{
		case  libtorrent::torrent_status::checking_files:
			statusString += Text::tformat(TSTRING(CHECKED_BYTES), "", percent, "");
			break;
		case  libtorrent::torrent_status::downloading_metadata:
			statusString += TSTRING(DOWNLOADING_METADATA);
			break;
		case  libtorrent::torrent_status::downloading:
			statusString += TSTRING(DOWNLOADING);
			break;
		case  libtorrent::torrent_status::finished:
			statusString += TSTRING(FINISHED);
			break;
		case  libtorrent::torrent_status::seeding:
			statusString += TSTRING(SEEDING);
			break;
		case  libtorrent::torrent_status::allocating:
			statusString += TSTRING(ALLOCATING);
			break;
		case  libtorrent::torrent_status::checking_resume_data:
			statusString += TSTRING(CHECKING_RESUME_DATA);
			break;
		default:
			dcassert(0);
			break;
	}
	
	if (isPaused && s.state == libtorrent::torrent_status::downloading_metadata)
	{
		statusString += TSTRING(PLEASE_WAIT);
	}
	if (s.state == libtorrent::torrent_status::seeding)
	{
		statusString +=  +_T(" (Download: ") + Text::toT(Util::formatSeconds(s.active_duration.count() - s.finished_duration.count())) + _T(")")
		                    + _T("(Seedind: ") + Text::toT(Util::formatSeconds(s.seeding_duration.count())) + _T(")");
	}
	if (s.state == libtorrent::torrent_status::downloading ||
	        s.state == libtorrent::torrent_status::finished)
	{
		const tstring l_peer_seed = _T(" Peers:") + Util::toStringT(s.num_peers) + _T(" Seeds:") + Util::toStringT(s.num_seeds) + _T(" ");
		if (s.state == libtorrent::torrent_status::seeding)
		{
			statusString = l_peer_seed + _T("  Download: ") +
			               Util::formatBytesW(s.total_download) + _T(" Upload: ") +
			               Util::formatBytesW(s.total_upload) + _T(" Time: ") +
			               Text::toT(Util::formatSeconds(s.seeding_duration.count())).c_str();
		}
		else
		{
			statusString += l_peer_seed + Text::tformat(TSTRING(DOWNLOADED_BYTES), Util::formatBytesW(s.total_done).c_str(),
			                                            percent,
			                                            Text::toT(Util::formatSeconds(s.active_duration.count())).c_str());
		}
	}
#endif
}
#endif
