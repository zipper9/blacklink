/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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

#ifndef MESSAGE_PANEL_H
#define MESSAGE_PANEL_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "../client/typedefs.h"

#ifdef BL_UI_FEATURE_EMOTICONS
#include "UserMessages.h"
#endif

#ifdef OSVER_WIN_XP
#include "ImageButton.h"
#endif

class MessagePanel
{
		BEGIN_MSG_MAP(MessagePanel)
#ifdef BL_UI_FEATURE_EMOTICONS
		MESSAGE_HANDLER(WMU_PASTE_TEXT, onPasteText)
		COMMAND_ID_HANDLER(IDC_EMOT, onEmoticons)
#endif
		END_MSG_MAP()

	public:
		static const int MIN_INPUT_BOX_HEIGHT = 26;

		enum
		{
			BUTTON_SEND,
			BUTTON_MULTILINE,
#ifdef BL_UI_FEATURE_EMOTICONS
			BUTTON_EMOTICONS,
#endif
			BUTTON_TRANSCODE,
			BUTTON_BOLD,
			BUTTON_ITALIC,
			BUTTON_UNDERLINE,
			BUTTON_STRIKETHROUGH,
			BUTTON_LINK,
			BUTTON_COLOR,
			BUTTON_FIND,
			BUTTON_SELECT_HUB,
			BUTTON_CCPM,
			MAX_BUTTONS
		};

		enum
		{
			CCPM_STATE_DISCONNECTED,
			CCPM_STATE_CONNECTED,
			CCPM_STATE_CONNECTING
		};

		explicit MessagePanel(CEdit& ctrlMessage);
		void initPanel(HWND hWnd);
		void destroyPanel();
		void updatePanel(const CRect& rect);
		void setCCPMState(int state);
		int getPanelWidth() const;
#ifdef BL_UI_FEATURE_EMOTICONS
		LRESULT onEmoticons(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEmoPackChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		void pasteText(const tstring& text);
#endif
		CButton& getButton(int index) { return ctrlButtons[index]; }
		BOOL onContextMenu(POINT& pt, WPARAM& wParam);

		bool initialized;
		bool showSelectHubButton;
		bool showCCPMButton;
		bool disableChat;

	private:
		CToolTipCtrl tooltip;
		CEdit& ctrlMessage;

		CButton ctrlShowUsers;
		CButton ctrlButtons[MAX_BUTTONS];
#ifdef OSVER_WIN_XP
		ImageButton imageButtons[MAX_BUTTONS];
#endif
		CButton ctrlTransCodeBtn;
		HWND m_hWnd;
		int ccpmState;

#ifdef BL_UI_FEATURE_EMOTICONS
		static void showEmoticonsConfig(const POINT& pt, HWND hWnd);
		LRESULT onPasteText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
#endif
		void createButton(int index, int image, int idc, ResourceManager::Strings caption);
		void updateButton(HDWP dwp, bool show, int index, CRect& rc);
};

#endif //MESSAGE_PANEL_H
