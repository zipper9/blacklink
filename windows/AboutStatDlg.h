/*
 * Copyright (C) 2016 FlylinkDC++ Team
 */

#ifndef ABOUT_STAT_DLG_H
#define ABOUT_STAT_DLG_H

#include "wtl_flylinkdc.h"
#include "../client/NmdcHub.h"
#include "../client/CompatibilityManager.h"
#include <boost/algorithm/string/replace.hpp>

class AboutStatDlg : public CDialogImpl<AboutStatDlg>
{
	public:
		enum { IDD = IDD_ABOUTSTAT };
		
		BEGIN_MSG_MAP(AboutStatDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
			CFlylinkDBManager::getInstance()->loadGlobalRatio();
			const CFlylinkDBManager::GlobalRatio& ratio = CFlylinkDBManager::getInstance()->getGlobalRatio();
			double r = ratio.download > 0 ? (double) ratio.upload / (double) ratio.download : 0;
			SetDlgItemText(IDC_TOTAL_UPLOAD, (TSTRING(UPLOADED) + _T(": ") +
				Util::formatBytesT(ratio.upload)).c_str());
			SetDlgItemText(IDC_TOTAL_DOWNLOAD, (TSTRING(DOWNLOADED) + _T(": ") +
				Util::formatBytesT(ratio.download)).c_str());
			SetDlgItemText(IDC_RATIO, (TSTRING(RATIO) + _T(": ") + Util::toStringT(r)).c_str());
#else
			SetDlgItemText(IDC_TOTAL_UPLOAD, (TSTRING(UPLOADED) + _T(": ") + TSTRING(NONE)).c_str());
			SetDlgItemText(IDC_TOTAL_DOWNLOAD, (TSTRING(DOWNLOADED) + _T(": ") + TSTRING(NONE)).c_str());
			SetDlgItemText(IDC_RATIO, (TSTRING(RATING) + _T(": ") + TSTRING(NONE)).c_str());
#endif
			
			CEdit ctrlUDPStat(GetDlgItem(IDC_UDP_DHT_SSL_STAT));
			ctrlUDPStat.AppendText(Text::toT(NmdcHub::get_all_unknown_command()).c_str());
			auto stat = CompatibilityManager::generateProgramStats();
			boost::replace_all(stat, "\t", "");
#ifdef _DEBUG

			stat += "\r\n" + ConnectionManager::g_tokens_manager.toString();
#endif
			ctrlUDPStat.AppendText(Text::toT(stat).c_str());
			return TRUE;
		}
};

#endif // !defined(ABOUT_STAT_DLG_H)
