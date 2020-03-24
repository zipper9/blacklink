/*
 * Copyright (C) 2001-2017 Jacek Sieka, j_s@telia.com
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

#ifndef OPERA_COLORS_PAGE_H_
#define OPERA_COLORS_PAGE_H_

#include "PropPage.h"
#include "ExListViewCtrl.h"
#include "PropPageTextStyles.h"

class OperaColorsPage : public CPropertyPage<IDD_OPERACOLORS_PAGE>, public PropPage
{
	public:
		explicit OperaColorsPage() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_TEXT_STYLES) + _T('\\') + TSTRING(SETTINGS_OPERACOLORS)), bDoProgress(false)
		{
			SetTitle(m_title.c_str());
			depth = SETTING(PROGRESS_3DDEPTH);
			m_psp.dwFlags |= PSP_RTLREADING;
			bDoProgress = false;
			bDoLeft = false;
			bDoSegment = false;
			odcStyle = false;
			stealthyStyleIco = false;
			stealthyStyleIcoSpeedIgnore = false;
		}
		
		~OperaColorsPage()
		{
			ctrlList.Detach();
		}
		
		BEGIN_MSG_MAP(OperaColorsPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_OPERACOLORS_BOOLEANS, NM_CUSTOMDRAW, ctrlList.onCustomDraw)
		COMMAND_HANDLER(IDC_FLAT, EN_UPDATE, On3DDepth)
		COMMAND_HANDLER(IDC_PROGRESS_OVERRIDE, BN_CLICKED, onClickedProgressOverride)
		COMMAND_HANDLER(IDC_PROGRESS_OVERRIDE2, BN_CLICKED, onClickedProgressOverride)
		COMMAND_HANDLER(IDC_PROGRESS_SEGMENT_SHOW, BN_CLICKED, onClickedProgressOverride)
		COMMAND_HANDLER(IDC_ODC_STYLE, BN_CLICKED, onClickedProgressOverride)
		COMMAND_HANDLER(IDC_STEALTHY_STYLE_ICO, BN_CLICKED, onClickedProgressOverride)
		COMMAND_HANDLER(IDC_STEALTHY_STYLE_ICO_SPEEDIGNORE, BN_CLICKED, onClickedProgressOverride)
		COMMAND_HANDLER(IDC_PROGRESS_BUMPED, BN_CLICKED, onClickedProgressOverride)
		COMMAND_HANDLER(IDC_SETTINGS_DOWNLOAD_BAR_COLOR, BN_CLICKED, onClickedProgress)
		COMMAND_HANDLER(IDC_SETTINGS_UPLOAD_BAR_COLOR, BN_CLICKED, onClickedProgress)
		COMMAND_HANDLER(IDC_SETTINGS_SEGMENT_BAR_COLOR, BN_CLICKED, onClickedProgress)
		COMMAND_HANDLER(IDC_PROGRESS_TEXT_COLOR_DOWN, BN_CLICKED, onClickedProgressTextDown)
		COMMAND_HANDLER(IDC_PROGRESS_TEXT_COLOR_UP, BN_CLICKED, onClickedProgressTextUp)
		MESSAGE_HANDLER(WM_DRAWITEM, onDrawItem)
		
		COMMAND_HANDLER(IDC_SETTINGS_ODC_MENUBAR_LEFT, BN_CLICKED, onMenubarClicked)
		COMMAND_HANDLER(IDC_SETTINGS_ODC_MENUBAR_RIGHT, BN_CLICKED, onMenubarClicked)
		COMMAND_HANDLER(IDC_SETTINGS_ODC_MENUBAR_USETWO, BN_CLICKED, onMenubarClicked)
		COMMAND_HANDLER(IDC_SETTINGS_ODC_MENUBAR_BUMPED, BN_CLICKED, onMenubarClicked)
		
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onDrawItem(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onMenubarClicked(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onClickedProgressOverride(WORD /* wNotifyCode */, WORD /* wID */, HWND /* hWndCtl */, BOOL& /* bHandled */)
		{
			updateProgress();
			return S_OK;
		}
		LRESULT onClickedProgress(WORD /* wNotifyCode */, WORD /* wID */, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onClickedProgressTextDown(WORD /* wNotifyCode */, WORD /* wID */, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onClickedProgressTextUp(WORD /* wNotifyCode */, WORD /* wID */, HWND /* hWndCtl */, BOOL& /* bHandled */);
		
		LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		LRESULT On3DDepth(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		void updateProgress(bool bInvalidate = true)
		{
			stealthyStyleIco = (IsDlgButtonChecked(IDC_STEALTHY_STYLE_ICO) != 0);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_STEALTHY_STYLE_ICO_SPEEDIGNORE), stealthyStyleIco);
			stealthyStyleIcoSpeedIgnore = stealthyStyleIco ? (IsDlgButtonChecked(IDC_STEALTHY_STYLE_ICO_SPEEDIGNORE) != 0) : 0;
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_TOP_SPEED), stealthyStyleIcoSpeedIgnore);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_SPEED_STR), stealthyStyleIcoSpeedIgnore);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_KBPS), stealthyStyleIcoSpeedIgnore);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_TOP_UP_SPEED), stealthyStyleIcoSpeedIgnore);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_SPEED_UP_STR), stealthyStyleIcoSpeedIgnore);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_KBPS2), stealthyStyleIcoSpeedIgnore);
			
			odcStyle = IsDlgButtonChecked(IDC_ODC_STYLE) != 0;
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_PROGRESS_BUMPED), odcStyle);
			
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_FLAT), !odcStyle);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_DEPTH_STR), !odcStyle);
			
			bool state = (IsDlgButtonChecked(IDC_PROGRESS_OVERRIDE) != 0);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_PROGRESS_OVERRIDE2), state);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_SETTINGS_DOWNLOAD_BAR_COLOR), state);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_SETTINGS_UPLOAD_BAR_COLOR), state);
			
			state = IsDlgButtonChecked(IDC_PROGRESS_OVERRIDE) != 0 && IsDlgButtonChecked(IDC_PROGRESS_OVERRIDE2) != 0;
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_PROGRESS_TEXT_COLOR_DOWN), state);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_PROGRESS_TEXT_COLOR_UP), state);
			if (bInvalidate)
			{
				if (ctrlProgressDownDrawer.m_hWnd)
					ctrlProgressDownDrawer.Invalidate();
				if (ctrlProgressUpDrawer.m_hWnd)
					ctrlProgressUpDrawer.Invalidate();
			}
		}
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_WINDOW_DETAILS; }
		void write();
		void cancel()
		{
			cancel_check();
		}
	private:
		friend UINT_PTR CALLBACK MenuBarCommDlgProc(HWND, UINT, WPARAM, LPARAM);
		friend LRESULT PropPageTextStyles::onImport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		bool bDoProgress;
		bool bDoLeft;
		bool bDoSegment;
		bool odcStyle;
		bool stealthyStyleIco;
		bool stealthyStyleIcoSpeedIgnore;
		
		ExListViewCtrl ctrlList;
		
		typedef CButton CCheckBox;
		
		COLORREF crProgressDown;
		COLORREF crProgressUp;
		COLORREF crProgressTextDown;
		COLORREF crProgressTextUp;
		COLORREF crProgressSegment;
		CCheckBox ctrlProgressOverride1;
		CCheckBox ctrlProgressOverride2;
		CButton ctrlProgressDownDrawer;
		CButton ctrlProgressUpDrawer;
		
		bool getCheckbox(int id)
		{
			return (BST_CHECKED == IsDlgButtonChecked(id));
		}
		
		COLORREF crMenubarLeft;
		COLORREF crMenubarRight;
		CButton ctrlLeftColor;
		CButton ctrlRightColor;
		CCheckBox ctrlTwoColors;
		CCheckBox ctrlBumped;
		CStatic ctrlMenubarDrawer;
		
		int depth;		
};

#endif // OPERA_COLORS_PAGE_H_
