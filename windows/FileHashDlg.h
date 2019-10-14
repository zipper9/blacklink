#ifndef FILE_HASH_DLG_H
#define FILE_HASH_DLG_H

#include <atlctrls.h>
#include <atldlgs.h>
#include "../client/Text.h"
#include "resource.h"
#include <atomic>

class FileHashDlg : public CDialogImpl<FileHashDlg>
{
	public:
		enum { IDD = IDD_FILE_HASH };
		enum { FHD_THREAD_RESULT = WM_USER + 5 };

		FileHashDlg(HICON icon): icon(icon), hashThreadHandle(NULL), fileSize(0)
		{
		}

		BEGIN_MSG_MAP(FileHashDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(FHD_THREAD_RESULT, OnThreadResult)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_OPEN, OnOpen)
		COMMAND_ID_HANDLER(IDC_COPY, OnCopy)
		COMMAND_ID_HANDLER(IDC_COPY_WEB_LINK, OnCopyWebLink)
		END_MSG_MAP()

		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD, WORD, HWND, BOOL&);
		LRESULT OnOpen(WORD, WORD, HWND, BOOL&);
		LRESULT OnCopy(WORD, WORD, HWND, BOOL&);
		LRESULT OnCopyWebLink(WORD, WORD, HWND, BOOL&);
		LRESULT OnThreadResult(UINT, WPARAM, LPARAM lParam, BOOL&);

		tstring filename;
		tstring lastDir;

	private:
		HICON icon;
		HANDLE hashThreadHandle;
		std::atomic_bool stopFlag;
		tstring filenameWithoutPath;
		int64_t fileSize;
		string magnet;

		static DWORD WINAPI hashThreadProc(void* param);
		void hashThreadProc();
		void stopThread();
		void startThread();
		bool initFileInfo();
		void clearMagnet();
};

#endif // FILE_HASH_DLG_H
