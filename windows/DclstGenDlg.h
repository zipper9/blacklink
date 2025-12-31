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

#ifndef DCLST_GEN_DLG_H
#define DCLST_GEN_DLG_H

#include "../client/DirectoryListing.h"
#include "../client/Thread.h"
#include "resource.h"
#include "TimerHelper.h"
#include <atomic>

#define WM_FINISHED  WM_USER+102

class DclstGenDlg : public CDialogImpl< DclstGenDlg >, public Thread, private TimerHelper
{
	public:
		enum { IDD = IDD_DCLS_GENERATOR };

		DclstGenDlg(const DirectoryListing::Directory* dir, const UserPtr& user) :
			TimerHelper(m_hWnd),
			dir(dir), user(user), filesProcessed(0), foldersProcessed(0),
			sizeProcessed(0), sizeTotal(0), abortFlag(false), calculatingSize(false),
			includeSelf(false)
		{
		}

		DclstGenDlg(const string& path) :
			TimerHelper(m_hWnd),
			dirToHash(path),
			dir(nullptr), user(nullptr), filesProcessed(0), foldersProcessed(0),
			sizeProcessed(0), sizeTotal(0), abortFlag(false),
			includeSelf(false)
		{
		}

		BEGIN_MSG_MAP(DclstGenDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_ID_HANDLER(IDC_DCLSTGEN_COPYMAGNET, onCopyMagnet)
		COMMAND_ID_HANDLER(IDC_DCLSTGEN_SHARE, onShareOrOpen)
		COMMAND_ID_HANDLER(IDC_DCLSTGEN_SAVEAS, onSaveAs)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_FINISHED, onFinished)
		END_MSG_MAP();

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTimer(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onFinished(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onShareOrOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		int run();

		static DclstGenDlg* instance;
		static void showDialog(const string& path, HWND hWndParent);

	private:
		void updateDialogItems();
		void writeXMLStart();
		void writeXMLEnd();
		void writeFolder(const DirectoryListing::Directory* dir);
		void writeFile(const DirectoryListing::File* file);
		void hashFolder(const string& path, const string& dirName);
		void hashFile(const string& path);
		size_t packAndSave();
		bool calculateTTH();
		void makeMagnet();
		static void progressFunc(void *ctx, int64_t fileSize);
		static const string& getDcLstDirectory();

	private:
		const DirectoryListing::Directory* const dir;
		const UserPtr user;
		std::atomic_bool abortFlag;
		FastCriticalSection cs;
		size_t filesProcessed;
		size_t foldersProcessed;
		int64_t sizeProcessed;
		int64_t sizeTotal;
		bool calculatingSize;
		bool includeSelf;
		string xml;
		string listName;
		string magnet;
		string dirToHash;
		TigerTree listTree;
};

#endif // !defined(DCLST_GEN_DLG_H)
