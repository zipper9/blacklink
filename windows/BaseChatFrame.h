/*
 * Copyright (C) 2012-2017 FlylinkDC++ Team http://flylinkdc.com
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

#pragma once

#include "FlatTabCtrl.h"
#include "ChatCtrl.h"
#include "MessagePanel.h"

class BaseChatFrame : public InternetSearchBaseHandler
{
		BEGIN_MSG_MAP(BaseChatFrame)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_FORWARDMSG, OnForwardMsg)
		COMMAND_ID_HANDLER(IDC_WINAMP_SPAM, onWinampSpam)
		NOTIFY_CODE_HANDLER(TTN_GETDISPINFO, onGetToolTip)
		CHAIN_COMMANDS(InternetSearchBaseHandler)
		CHAIN_MSG_MAP_MEMBER(ctrlClient)
		if (!ClientManager::isStartup()) // try fix https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=38156
		{
			if (msgPanel && msgPanel->ProcessWindowMessage(hWnd, uMsg, wParam, lParam, lResult))
			{
				return TRUE;
			}
		}
		else
		{
			// dcassert(0);
		}
		COMMAND_ID_HANDLER(IDC_MESSAGEPANEL, onMultilineChatInputButton)
		COMMAND_ID_HANDLER(ID_TEXT_TRANSCODE, OnTextTranscode)
		COMMAND_RANGE_HANDLER(IDC_BOLD, IDC_STRIKE, onTextStyleSelect)
#ifdef SCALOLAZ_BB_COLOR_BUTTON
		COMMAND_ID_HANDLER(IDC_COLOR, onTextStyleSelect)
#endif
		COMMAND_HANDLER(IDC_CHAT_MESSAGE_EDIT, EN_CHANGE, onChange)
		END_MSG_MAP()

	public:
		void createMessagePanel();
		void destroyMessagePanel(bool isShutdown);
		string getHubHint() const { return ctrlClient.getHubHint(); }

	private:
		void createChatCtrl();

	protected:
		OMenu* userMenu;
		OMenu* createUserMenu();
		void destroyUserMenu();
		void createMessageCtrl(ATL::CMessageMap* messageMap, DWORD messageMapID, bool suppressChat);
		void destroyMessageCtrl(bool isShutdown);

		BaseChatFrame() :
			curCommandPosition(0),
			tempUseMultiChat(false),
			multiChatLines(0),
			processNextChar(false),
			showTimestamps(BOOLSETTING(CHAT_TIME_STAMPS)),
			currentNeedlePos(-1),
			msgPanel(nullptr),
			messagePanelHwnd(nullptr),
			messagePanelRect{},
			ctrlMessageContainer(nullptr),
			lastMessageSelPos(0),
			userMenu(nullptr),
			suppressChat(false)
		{
		}

		BaseChatFrame(const BaseChatFrame&) = delete;
		BaseChatFrame& operator= (const BaseChatFrame&) = delete;

		virtual ~BaseChatFrame() {}
		virtual void doDestroyFrame() = 0;
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE; // ќб€зательно чтобы продолжить обработку дальше. http://www.rsdn.ru/forum/atl/633568.1
			doDestroyFrame();
			return 0;
		}

		LRESULT onCreate(HWND hWnd, RECT &rc);
		bool processControlKey(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		void processHotKey(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT OnForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onSendMessage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			onEnter();
			return 0;
		}
		void onEnter();
		LRESULT onWinampSpam(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTextStyleSelect(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnTextTranscode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onSearchFileOnInternet(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMultilineChatInputButton(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetToolTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		tstring findTextPopup();
		void findText(const tstring & needle) noexcept;
		
		virtual void processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText) = 0;
		virtual void processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText) = 0;
		
		virtual void sendMessage(const tstring& msg, bool thirdperson = false) = 0;
		void addLine(const tstring& line, unsigned maxSmiles, CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		virtual void addLine(const Identity& ou, const bool myMessage, const bool thirdPerson, const tstring& line, unsigned maxSmiles, const CHARFORMAT2& cf, tstring& extra);
		virtual void addStatus(const tstring& line, const bool inChat = true, const bool history = true, const CHARFORMAT2& cf = Colors::g_ChatTextSystem);
		virtual void UpdateLayout(BOOL bResizeBars = TRUE) = 0;
		
		static tstring getIpCountry(const string& ip, bool ts, bool ipInChat, bool countryInChat, bool locationInChat);
		static TCHAR getChatRefferingToNick();
		
		void appendChatCtrlItems(OMenu& menu, const Client* client = nullptr);
		
		void appendNickToChat(const tstring& nick);
		void appendLogToChat(const string& path, const size_t linesCount);
		virtual void readFrameLog() = 0;
		ChatCtrl ctrlClient;
		
		CEdit ctrlMessage;
		tstring lastMessage;
		DWORD lastMessageSelPos;
		MessagePanel* msgPanel;
		CContainedWindow* ctrlMessageContainer;
		CFlyToolTipCtrl ctrlLastLinesToolTip;
		CStatusBarCtrl ctrlStatus;
		bool suppressChat;
		
		void createStatusCtrl(HWND hWnd);
		void destroyStatusCtrl();
		std::vector<tstring> ctrlStatusCache; // Temp storage until ctrlStatus is created
		void setStatusText(unsigned char index, const tstring& text);
		void restoreStatusFromCache();
		void destroyStatusbar();
		
		enum { MAX_CLIENT_LINES = 10 };
		std::list<tstring> lastLinesList;
		tstring lastLines;
		StringMap ucLineParams;
		
		TStringList prevCommands;
		tstring currentCommand;
		size_t curCommandPosition;
		bool tempUseMultiChat;
		bool isMultiChat(int& height) const;
		void clearMessageWindow();
		unsigned multiChatLines;

	private:
		bool processNextChar;
		bool showTimestamps;
		tstring currentNeedle;
		LONG currentNeedlePos;
		RECT messagePanelRect;
		HWND messagePanelHwnd;
		
		bool adjustChatInputSize(BOOL& bHandled);
		void checkMultiLine();
		void insertLineHistoryToChatInput(const WPARAM wParam, BOOL& bHandled);
};
