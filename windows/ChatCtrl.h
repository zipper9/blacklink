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
#include "UserMessages.h"
#include "resource.h"

class Identity;
class Client;

class ChatCtrl: public CWindowImpl<ChatCtrl, CRichEditCtrl>
#ifdef IRAINMAN_INCLUDE_SMILE
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
#ifdef IRAINMAN_ENABLE_WHOIS
		COMMAND_ID_HANDLER(IDC_WHOIS_IP, onWhoisIP)
		COMMAND_ID_HANDLER(IDC_WHOIS_IP2, onWhoisIP)
		COMMAND_ID_HANDLER(IDC_WHOIS_URL, onWhoisURL)
		COMMAND_ID_HANDLER(IDC_REPORT_CHAT, onDumpUserInfo)
#endif
		COMMAND_ID_HANDLER(ID_EDIT_COPY, onEditCopy)
		COMMAND_ID_HANDLER(ID_EDIT_SELECT_ALL, onEditSelectAll)
		COMMAND_ID_HANDLER(ID_EDIT_CLEAR_ALL, onEditClearAll)
		END_MSG_MAP()
		
		LRESULT onMouseWheel(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onEnLink(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
#ifdef IRAINMAN_ENABLE_WHOIS
		LRESULT onWhoisIP(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onWhoisURL(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDumpUserInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
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
			const CHARFORMAT2 cf;

			Message(const Identity* id, bool myMessage, bool thirdPerson,
			        const tstring& extra, const tstring& msg, const CHARFORMAT2& cf,
			        bool useEmoticons, bool removeLineBreaks = true);
			size_t length() const
			{
				return extra.length() + msg.length() + nick.length() + 30;
			}
		};

		void appendText(const Message& message, unsigned maxSmiles, bool highlightNick);
		void adjustTextSize();

	protected:
		bool hitNick(const POINT& p, tstring& nick, int& startPos, int& endPos);
		bool hitIP(const POINT& p, tstring& result, int& startPos, int& endPos);
		bool hitText(tstring& text, int selBegin, int selEnd) const;

	private:
		list<Message> chatCache;
		size_t chatCacheSize;
		bool disableChatCacheFlag;
		FastCriticalSection csChatCache;
		bool autoScroll;
		string hubHint;
		tstring myNick;
		int ignoreLinkStart, ignoreLinkEnd;
		int selectedLine;
		IRichEditOle* pRichEditOle;

#ifdef IRAINMAN_INCLUDE_SMILE
		IStorage* pStorage;
#endif

		struct TagItem
		{
			int type;
			tstring::size_type openTagStart;
			tstring::size_type openTagEnd;
			tstring::size_type closeTagStart;
			tstring::size_type closeTagEnd;
			CHARFORMAT2 fmt;
		};		

		vector<TagItem> tags;
		
		struct LinkItem
		{
			int type;
			tstring::size_type start;
			tstring::size_type end;
			tstring updatedText;
			int hiddenTextLen;
		};

		vector<LinkItem> links;

		void initRichEditOle();
		void insertAndFormat(const tstring& text, CHARFORMAT2 cf, LONG& startPos, LONG& endPos);
		void appendTextInternal(tstring& text, const Message& message, unsigned maxSmiles, bool highlightNick);
		void appendTextInternal(tstring&& text, const Message& message, unsigned maxSmiles, bool highlightNick);
		void parseText(tstring& text, const Message& message, unsigned maxSmiles, bool highlightNick);
		void applyShift(size_t tagsStartIndex, size_t linksStartIndex, tstring::size_type start, int shift);		
		static bool processTag(TagItem& item, tstring& tag, tstring::size_type start, tstring::size_type end, const CHARFORMAT2& prevFmt);
		static void processLink(const tstring& text, LinkItem& li);
		void findSubstringAvoidingLinks(tstring::size_type& pos, tstring& text, const tstring& str, size_t& currentLink) const;
		tstring getUrl(LONG start, LONG end, bool keepSelected);
		tstring getUrl(const ENLINK* el, bool keepSelected);
		tstring getUrlHiddenText(LONG end);
	
	public:
		IRichEditOle* getRichEditOle();
		void disableChatCache() { disableChatCacheFlag = true; }
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
		static void SetTextStyleMyNick(const CHARFORMAT2& ts);
		
		static tstring g_sSelectedText;
		static tstring g_sSelectedIP;
		static tstring g_sSelectedUserName;
		static tstring g_sSelectedURL;

#ifdef IRAINMAN_INCLUDE_SMILE
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
		bool outOfMemory;
		unsigned totalEmoticons;
#endif // IRAINMAN_INCLUDE_SMILE
};

#endif // CHAT_CTRL_H
