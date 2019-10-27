/*
 * Copyright (C) 2016 FlylinkDC++ Team
 */

#ifndef ABOUT_STAT_DLG_H
#define ABOUT_STAT_DLG_H

#include "wtl_flylinkdc.h"
#include "../client/NmdcHub.h"
#include "boost/algorithm/string/replace.hpp"

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
			CFlylinkDBManager::getInstance()->load_global_ratio();
			SetDlgItemText(IDC_TOTAL_UPLOAD, (TSTRING(UPLOADED) + _T(": ") +
			                                  Text::toT(Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.get_upload()))).c_str());
			SetDlgItemText(IDC_TOTAL_DOWNLOAD, (TSTRING(DOWNLOADED) + _T(": ") +
			                                    Text::toT(Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.get_download()))).c_str());
			SetDlgItemText(IDC_RATIO, (TSTRING(RATIO) + _T(": ") + (CFlylinkDBManager::getInstance()->get_ratioW())).c_str());
#else
			SetDlgItemText(IDC_TOTAL_UPLOAD, (TSTRING(UPLOADED) + _T(": ") + TSTRING(NONE));
			               SetDlgItemText(IDC_TOTAL_DOWNLOAD, (TSTRING(DOWNLOADED) + _T(": ") + TSTRING(NONE));
			                              SetDlgItemText(IDC_RATIO, (TSTRING(RATING) + _T(": ") + TSTRING(NONE));
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
