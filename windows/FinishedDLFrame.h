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

#ifndef FINISHED_DL_FRAME_H
#define FINISHED_DL_FRAME_H

#include "FinishedFrameBase.h"
#include "ConfUI.h"

class FinishedDLFrame : public FinishedFrame<FinishedDLFrame, ResourceManager::FINISHED_DOWNLOADS, IDC_FINISHED, IconBitmaps::FINISHED_DOWNLOADS>
{
	public:
		FinishedDLFrame(): FinishedFrame(e_TransferDownload)
		{
			boldFinished = Conf::BOLD_FINISHED_DOWNLOADS;
			columnOrder = Conf::FINISHED_DL_FRAME_ORDER;
			columnWidth = Conf::FINISHED_DL_FRAME_WIDTHS;
			columnVisible = Conf::FINISHED_DL_FRAME_VISIBLE;
			columnSort = Conf::FINISHED_DL_FRAME_SORT;
		}

		static CFrameWndClassInfo& GetWndClassInfo()
		{
			static CFrameWndClassInfo wc =
			{
				{
					sizeof(WNDCLASSEX), 0, StartWindowProc,
					0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("FinishedDLFrame"), NULL
				},
				NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
			};

			if (!wc.m_wc.hIconSm)
				wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::FINISHED_DOWNLOADS, 0);

			return wc;
		}

	private:
		void on(AddedDl, bool isFile, const FinishedItemPtr& entry) noexcept override
		{
			WinUtil::postSpeakerMsg(m_hWnd, SPEAK_ADD_ITEM, new FinishedItemPtr(entry));
		}
		
		void on(RemovedDl, const FinishedItemPtr& entry) noexcept override
		{
			WinUtil::postSpeakerMsg(m_hWnd, SPEAK_REMOVE_ITEM, new FinishedItemPtr(entry));
		}
};

#endif // FINISHED_DL_FRAME_H
