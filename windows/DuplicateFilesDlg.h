#ifndef DUPLICATE_FILES_DLG_H
#define DUPLICATE_FILES_DLG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "../client/DirectoryListing.h"

class DuplicateFilesDlg : public CDialogImpl<DuplicateFilesDlg>, public CDialogResize<DuplicateFilesDlg>
{
	public:
		enum { IDD = IDD_DUPLICATE_FILES };

		DuplicateFilesDlg(const DirectoryListing* dl, const std::list<DirectoryListing::File*>& files, const DirectoryListing::File* selFile);

		BEGIN_MSG_MAP(DuplicateFilesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		COMMAND_ID_HANDLER(IDC_GOTO, onGoTo)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		NOTIFY_HANDLER(IDC_LIST1, LVN_ITEMCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_LIST1, NM_DBLCLK, onDoubleClick)
		NOTIFY_HANDLER(IDC_LIST1, NM_RETURN, onEnter)
		CHAIN_MSG_MAP(CDialogResize<DuplicateFilesDlg>)
		END_MSG_MAP()

		BEGIN_DLGRESIZE_MAP(DuplicateFilesDlg)
		BEGIN_DLGRESIZE_GROUP()
		DLGRESIZE_CONTROL(IDC_LIST1, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		END_DLGRESIZE_GROUP()
		DLGRESIZE_CONTROL(IDC_GOTO, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		END_DLGRESIZE_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onGoTo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSize(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onDoubleClick(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onEnter(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
	
	private:
		CListViewCtrl ctrlList;
		const DirectoryListing* const dl;
		const DirectoryListing::File* const selFile;
		vector<const DirectoryListing::File*> files;

		void goToSelectedFile();

	public:
		const DirectoryListing::File* goToFile;
};

#endif // DUPLICATE_FILES_DLG_H
