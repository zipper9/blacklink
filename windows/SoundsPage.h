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

#ifndef SOUNDS_PAGE_H_
#define SOUNDS_PAGE_H_

#include "PropPage.h"
#include "ExListViewCtrl.h"

class Sounds : public CPropertyPage<IDD_SOUNDS_PAGE>, public PropPage
{
	public:
		explicit Sounds() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_SOUNDS))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~Sounds()
		{
			ctrlSounds.Detach();
		}
		
		BEGIN_MSG_MAP(Sounds)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_BROWSE, BN_CLICKED, onBrowse)
		COMMAND_HANDLER(IDC_PLAY, BN_CLICKED, onPlay)
		COMMAND_ID_HANDLER(IDC_NONE, onClickedNone)
		COMMAND_ID_HANDLER(IDC_DEFAULT, onDefault)
		COMMAND_ID_HANDLER(IDC_SOUND_ENABLE, onClickedActive)
		COMMAND_ID_HANDLER(IDC_SOUNDS_COMBO, onDefaultAll)
		NOTIFY_HANDLER(IDC_SOUNDLIST, NM_CUSTOMDRAW, ctrlSounds.onCustomDraw)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onBrowse(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onPlay(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onClickedNone(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onDefault(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onDefaultAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
		{
			setAllToDefault();
			return 0;
		}
		LRESULT onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_SOUNDS; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	private:
		void fixControls();
		
	protected:	
		typedef boost::unordered_map<tstring, string> SoundThemeMap;
		CComboBox ctrlSoundTheme;
		SoundThemeMap soundThemes;
		
		void getSoundThemeList();
		void setAllToDefault();
		ExListViewCtrl ctrlSounds;
};

#endif // SOUNDS_PAGE_H_
