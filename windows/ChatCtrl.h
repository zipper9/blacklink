/*
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

#ifndef CHAT_CTRL_H
#define CHAT_CTRL_H

#include "../client/w.h"
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include "../client/typedefs.h"
#include "../client/Locks.h"
#include "ChatTextParser.h"
#include "UserMessages.h"
#include "resource.h"

class Identity;
class Client;

class ChatCtrl: public CWindowImpl<ChatCtrl, CRichEditCtrl>
#ifdef BL_UI_FEATURE_EMOTICONS
	, public IRichEditOleCallback
#endif
{
		typedef ChatCtrl thisClass;

	public:
		ChatCtrl();
		~ChatCtrl();

		ChatCtrl(const ChatCtrl&) = delete;
		ChatCtrl& operator= (const ChatCtrl&) = delete;
		
		BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, onMouseWheel)
		NOTIFY_HANDLER(IDC_CLIENT, EN_LINK, onEnLink)
		COMMAND_ID_HANDLER(IDC_COPY_ACTUAL_LINE, onCopyActualLine)
		COMMAND_ID_HANDLER(IDC_COPY_URL, onCopyURL)
		COMMAND_ID_HANDLER(IDC_REPORT_CHAT, onDumpUserInfo)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, onEditCopy)
		COMMAND_ID_HANDLER(ID_EDIT_SELECT_ALL, onEditSelectAll)
		COMMAND_ID_HANDLER(ID_EDIT_CLEAR_ALL, onEditClearAll)
		END_MSG_MAP()
		
		LRESULT onMouseWheel(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onEnLink(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onDumpUserInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditSelectAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditClearAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyActualLine(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyURL(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onRButtonDown(POINT pt);
		
		struct Message
		{
			const tstring nick;
			tstring msg;
			tstring extra;
			const bool myMessage;
			const bool isRealUser;
			const bool thirdPerson;
			bool useEmoticons;
			bool isFavorite;
			bool isBanned;
			bool isOp;
			const int textStyle;

			Message(const Identity* id, bool myMessage, bool thirdPerson,
			        const tstring& extra, const tstring& msg, int textStyle,
			        bool useEmoticons, bool removeLineBreaks = true);
			size_t length() const
			{
				return extra.length() + msg.length() + nick.length() + 30;
			}
		};

		void insertAndFormat(const tstring& text, CHARFORMAT2 cf, LONG& startPos, LONG& endPos, unsigned addFlags = 0);
		void appendText(const Message& message, unsigned maxSmiles, bool highlightNick);
		void adjustTextSize();
		bool findText();
		const tstring& getNeedle() const { return currentNeedle; }
		void setNeedle(const tstring& needle);
		DWORD getFindFlags() const { return currentFindFlags; }
		void setFindFlags(DWORD flags) { currentFindFlags = flags; }
		void resetFindPos() { findInit = true; }
		static void removeHiddenText(tstring& s);

		static const tstring nickBoundaryChars;

	protected:
		bool hitNick(POINT p, tstring& nick, int& startPos, int& endPos);
		bool hitIP(POINT p, tstring& result, int& startPos, int& endPos);
		bool hitText(tstring& text, int selBegin, int selEnd) const;
		long findAndSelect(DWORD flags, FINDTEXTEX& ft);

	private:
		// Chat cache
		list<Message> chatCache;
		size_t chatCacheSize;
		bool useChatCacheFlag;

		bool autoScroll;
		string hubHint;
		tstring myNick;
		int ignoreLinkStart, ignoreLinkEnd;
		int selectedLine;
		IRichEditOle* pRichEditOle;

		// Find text
		bool findInit;
		long findRangeStart, findRangeEnd;
		tstring currentNeedle;
		DWORD currentFindFlags;

#ifdef BL_UI_FEATURE_EMOTICONS
		IStorage* pStorage;
#endif

		void initRichEditOle();
		void appendTextInternal(tstring& text, const Message& message, unsigned maxEmoticons, bool highlightNick);
		void appendTextInternal(tstring&& text, const Message& message, unsigned maxEmoticons, bool highlightNick);
		void appendText(tstring& text, const Message& message, unsigned maxEmoticons, bool highlightNick);
		tstring getUrl(LONG start, LONG end, bool keepSelected);
		tstring getUrl(const ENLINK* el, bool keepSelected);
		tstring getUrlHiddenText(LONG end);
	
	public:
		IRichEditOle* getRichEditOle();
		void disableChatCache() { useChatCacheFlag = false; }
		void restoreChatCache();
		
		void goToEnd(bool force);
		void goToEnd(POINT& scrollPos, bool force);
		bool getAutoScroll() const { return autoScroll; }
		void invertAutoScroll();
		void setAutoScroll(bool flag);
		
		void setHubParam(const string& url, const string& nick);
		const string& getHubHint() const { return hubHint; }
		void setHubHint(const string& hint) { hubHint = hint; }

		void Clear();

		static tstring g_sSelectedText;
		static tstring g_sSelectedIP;
		static tstring g_sSelectedUserName;
		static tstring g_sSelectedURL;
		static string g_sSelectedHostname;

#ifdef BL_UI_FEATURE_EMOTICONS
	protected:
		volatile LONG refs;
		
		void replaceObjects(tstring& s, int startIndex) const;

		// IRichEditOleCallback implementation
		
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE QueryInterface(THIS_ REFIID riid, LPVOID FAR * lplpObj);
		COM_DECLSPEC_NOTHROW ULONG STDMETHODCALLTYPE AddRef(THIS);
		COM_DECLSPEC_NOTHROW ULONG STDMETHODCALLTYPE Release(THIS);
		
		// *** IRichEditOleCallback methods ***
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetNewStorage(THIS_ LPSTORAGE FAR * lplpstg);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetInPlaceContext(THIS_ LPOLEINPLACEFRAME FAR * lplpFrame,
		                                                                 LPOLEINPLACEUIWINDOW FAR * lplpDoc,
		                                                                 LPOLEINPLACEFRAMEINFO lpFrameInfo);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ShowContainerUI(THIS_ BOOL fShow);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE QueryInsertObject(THIS_ LPCLSID lpclsid, LPSTORAGE lpstg,
		                                                                 LONG cp);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE DeleteObject(THIS_ LPOLEOBJECT lpoleobj);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE QueryAcceptData(THIS_ LPDATAOBJECT lpdataobj,
		                                                               CLIPFORMAT FAR * lpcfFormat, DWORD reco,
		                                                               BOOL fReally, HGLOBAL hMetaPict);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(THIS_ BOOL fEnterMode);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetClipboardData(THIS_ CHARRANGE FAR * lpchrg, DWORD reco,
		                                                                LPDATAOBJECT FAR * lplpdataobj);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetDragDropEffect(THIS_ BOOL fDrag, DWORD grfKeyState,
		                                                                 LPDWORD pdwEffect);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetContextMenu(THIS_ WORD seltype, LPOLEOBJECT lpoleobj,
		                                                              CHARRANGE FAR * lpchrg,
		                                                              HMENU FAR * lphmenu);
	private:
		unsigned totalEmoticons;
#endif // BL_UI_FEATURE_EMOTICONS
};

#endif // CHAT_CTRL_H
