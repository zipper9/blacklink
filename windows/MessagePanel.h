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

#include "wtl_flylinkdc.h"
#include "HIconWrapper.h"

#ifdef IRAINMAN_INCLUDE_SMILE
#include "OMenu.h"
#include "UserMessages.h"
#include "resource.h"
#endif

#ifdef OSVER_WIN_XP
#include "ImageButton.h"
#endif

class MessagePanel
{
		BEGIN_MSG_MAP(MessagePanel)
#ifdef IRAINMAN_INCLUDE_SMILE
		MESSAGE_HANDLER(WMU_PASTE_TEXT, onPasteText)
		COMMAND_ID_HANDLER(IDC_EMOT, onEmoticons)
		COMMAND_RANGE_HANDLER(IDC_EMOMENU, IDC_EMOMENU + emoMenuItemCount, onEmoPackChange)
#endif
		END_MSG_MAP()

		enum
		{
			BUTTON_SEND,
			BUTTON_MULTILINE,
#ifdef IRAINMAN_INCLUDE_SMILE
			BUTTON_EMOTICONS,
#endif
			BUTTON_TRANSCODE,
			BUTTON_BOLD,
			BUTTON_ITALIC,
			BUTTON_UNDERLINE,
			BUTTON_STRIKETHROUGH,
			BUTTON_COLOR,
			MAX_BUTTONS
		};

	public:
		static const int MIN_MULTI_HEIGHT = 22 + 26 + 4;

		explicit MessagePanel(CEdit& ctrlMessage);
		void initPanel(HWND hWnd);
		void destroyPanel();
		void updatePanel(const CRect& rect);
		static int getPanelWidth();
#ifdef IRAINMAN_INCLUDE_SMILE
		LRESULT onEmoticons(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEmoPackChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		void pasteText(const tstring& text);
#endif
		BOOL onContextMenu(POINT& pt, WPARAM& wParam);

		bool initialized;

	private:
		CFlyToolTipCtrl tooltip;
		CEdit& ctrlMessage;
		
		CButton ctrlShowUsers;
		CButton ctrlButtons[MAX_BUTTONS];
#ifdef OSVER_WIN_XP
		ImageButton imageButtons[MAX_BUTTONS];
#endif
#ifdef FLYLINKDC_USE_BB_SIZE_CODE
		CComboBox ctrlSizeSel;
#endif
		CButton ctrlTransCodeBtn;
#ifdef IRAINMAN_INCLUDE_SMILE
		static OMenu g_emoMenu;
		static int emoMenuItemCount;
#endif
		HWND m_hWnd;

		static HIconWrapper g_hSendMessageIco;
		static HIconWrapper g_hMultiChatIco;
#ifdef IRAINMAN_INCLUDE_SMILE
		static HIconWrapper g_hEmoticonIco;
#endif
		static HIconWrapper g_hBoldIco;
		static HIconWrapper g_hUndelineIco;
		static HIconWrapper g_hStrikeIco;
		static HIconWrapper g_hItalicIco;
		static HIconWrapper g_hTransCodeIco;
		static HIconWrapper g_hColorIco;

#ifdef IRAINMAN_INCLUDE_SMILE
		static void showEmoticonsMenu(OMenu& menu, const POINT& pt, HWND hWnd, int idc, int& count);
		LRESULT onPasteText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
#endif
		void createButton(int index, HICON icon, int idc, ResourceManager::Strings caption);
		void updateButton(HDWP dwp, bool show, int index, CRect& rc);
};

#endif //MESSAGE_PANEL_H
