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

#ifndef BASE_CHAT_FRAME_H_
#define BASE_CHAT_FRAME_H_

#include "ChatCtrl.h"
#include "MessageEdit.h"
#include "MessagePanel.h"
#include "BaseHandlers.h"
#include "Colors.h"
#include "../client/ClientManager.h"
#include "../client/Commands.h"

class BaseChatFrame : public InternetSearchBaseHandler, protected MessageEdit::Callback
{
		BEGIN_MSG_MAP(BaseChatFrame)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_FORWARDMSG, onForwardMsg)
		MESSAGE_HANDLER(WMU_CHAT_LINK_CLICKED, onChatLinkClicked)
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
		COMMAND_ID_HANDLER(IDC_SAVE, onSaveToFile)
		COMMAND_ID_HANDLER(IDC_MESSAGEPANEL, onMultilineChatInputButton)
		COMMAND_ID_HANDLER(ID_TEXT_TRANSCODE, OnTextTranscode)
		COMMAND_RANGE_HANDLER(IDC_BOLD, IDC_STRIKE, onTextStyleSelect)
		COMMAND_ID_HANDLER(IDC_COLOR, onTextStyleSelect)
		COMMAND_HANDLER(IDC_CHAT_MESSAGE_EDIT, EN_CHANGE, onChange)
		END_MSG_MAP()

	public:
		void createMessagePanel(bool showSelectHubButton, bool showCCPMButton);
		void destroyMessagePanel();

	private:
		void createChatCtrl();

	protected:
		OMenu* userMenu;
		OMenu* createUserMenu();
		void destroyUserMenu();
		void createMessageCtrl(ATL::CMessageMap* messageMap, DWORD messageMapID, bool suppressChat);
		void destroyMessageCtrl(bool isShutdown);

		BaseChatFrame() :
			multiChatLines(0),
			showTimestamps(BOOLSETTING(CHAT_TIME_STAMPS)),
			currentNeedlePos(-1),
			msgPanel(nullptr),
			messagePanelHwnd(nullptr),
			messagePanelRect{},
			lastMessageSelPos(0),
			userMenu(nullptr),
			suppressChat(false),
			ctrlStatusOwnerDraw(0)
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
		bool processHotKey(int key);
		LRESULT onForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
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
		LRESULT onSaveToFile(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMultilineChatInputButton(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetToolTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onChatLinkClicked(UINT, WPARAM, LPARAM, BOOL&);
		tstring findTextPopup();
		void findText(const tstring & needle) noexcept;

		virtual bool processFrameCommand(const Commands::ParsedCommand& pc, Commands::Result& res);
		virtual void processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText) = 0;
		virtual void onTextEdited() {}

		virtual bool sendMessage(const string& msg, bool thirdPerson = false) = 0;
		virtual void addStatus(const tstring& line, bool inChat = true, bool history = true, const CHARFORMAT2& cf = Colors::g_ChatTextSystem);
		virtual void UpdateLayout(BOOL bResizeBars = TRUE) = 0;

		void addLine(const tstring& line, unsigned maxSmiles, const CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		void addLine(const Identity& ou, bool myMessage, bool thirdPerson, const tstring& line, unsigned maxSmiles, const CHARFORMAT2& cf, string& extra);
		void addSystemMessage(const tstring& line, const CHARFORMAT2& cf);

		static TCHAR getChatRefferingToNick();

		void appendChatCtrlItems(OMenu& menu, bool isOp);

		void appendNickToChat(const tstring& nick);
		void appendLogToChat(const string& path, const size_t linesCount);
		virtual void readFrameLog() = 0;
		ChatCtrl ctrlClient;
		
		MessageEdit ctrlMessage;
		tstring lastMessage;
		DWORD lastMessageSelPos;
		MessagePanel* msgPanel;
		CFlyToolTipCtrl ctrlLastLinesToolTip;
		CStatusBarCtrl ctrlStatus;
		bool suppressChat;

		std::vector<tstring> ctrlStatusCache; // Temp storage until ctrlStatus is created
		unsigned ctrlStatusOwnerDraw;

		void createStatusCtrl(HWND hWnd);
		void destroyStatusCtrl();
		void setStatusText(int index, const tstring& text);
		void restoreStatusFromCache();
		void destroyStatusbar();
		
		enum { MAX_CLIENT_LINES = 10 };
		std::list<tstring> lastLinesList;
		tstring lastLines;
		StringMap ucLineParams;
		
		unsigned multiChatLines;

		int getInputBoxHeight() const;
		void clearInputBox();

	private:
		bool showTimestamps;
		tstring currentNeedle;
		LONG currentNeedlePos;
		RECT messagePanelRect;
		HWND messagePanelHwnd;

		void checkMultiLine();
		void insertBBCode(WORD wID, HWND hwndCtl);

	private:
		bool processEnter() override;
		void updateEditHeight() override;
		bool handleKey(int key) override;
		void typingNotification() override;

	protected:
		bool handleAutoComplete() override { return false; }
		void clearAutoComplete() override {}
		void sendCommandResult(Commands::Result& res);
};

#endif // BASE_CHAT_FRAME_H_
