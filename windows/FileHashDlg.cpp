#include "stdafx.h"
#include "FileHashDlg.h"
#include "../client/HashManager.h"
#include "../client/Util.h"
#include "../client/HashUtil.h"
#include "../client/File.h"
#include "DialogLayout.h"
#include "WinUtil.h"

#ifdef OSVER_WIN_XP
#include "../client/CompatibilityManager.h"
#endif

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::AUTO;
using DialogLayout::UNSPEC;

static const size_t HASH_BUF_SIZE = 512 * 1024;

struct ThreadResult
{
	TTHValue tthValue;
	bool result;
	bool stopped;
};

static const DialogLayout::Align align1 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { 1, DialogLayout::SIDE_RIGHT, U_DU(0) };
static const DialogLayout::Align align3 = { 0, DialogLayout::SIDE_RIGHT, U_DU(26) };
static const DialogLayout::Align align4 = { 0, DialogLayout::SIDE_RIGHT, U_DU(8) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_PATH,              0,              UNSPEC, UNSPEC                      },
	{ IDC_OPEN,              FLAG_TRANSLATE, UNSPEC, UNSPEC                      },
	{ IDC_FILE_SIZE,         0,              UNSPEC, UNSPEC                      },
	{ IDC_CAPTION_FILE_NAME, FLAG_TRANSLATE, AUTO,   UNSPEC, 1                   },
	{ IDC_CAPTION_TTH,       FLAG_TRANSLATE, AUTO,   UNSPEC, 1                   },
	{ IDC_CAPTION_NTFS_TTH,  FLAG_TRANSLATE, AUTO,   UNSPEC, 1                   },
	{ IDC_CAPTION_MAGNET,    FLAG_TRANSLATE, AUTO,   UNSPEC, 1                   },
	{ IDC_FILE_NAME,         0,              UNSPEC, UNSPEC, 0, &align1, &align2 },
	{ IDC_FILE_TTH,          0,              UNSPEC, UNSPEC, 0, &align1, &align3 },
	{ IDC_NTFS_TTH,          0,              UNSPEC, UNSPEC, 0, &align1, &align4 },
	{ IDC_MAGNET,            0,              UNSPEC, UNSPEC, 0, &align1, &align4 },
	{ IDC_COPY,              FLAG_TRANSLATE, UNSPEC, UNSPEC, 0, &align1          },
	{ IDCANCEL,              FLAG_TRANSLATE, UNSPEC, UNSPEC                      }
};

LRESULT FileHashDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(FILE_TTH_TITLE));
	HICON icon = g_iconBitmaps.getIcon(IconBitmaps::TTH, 0);
	SetIcon(icon, FALSE);
	SetIcon(icon, TRUE);

	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	CButton ctrlCopyTTH(GetDlgItem(IDC_COPY_TTH));
#ifdef OSVER_WIN_XP
	if (!CompatibilityManager::isOsVistaPlus())
		imageButton.SubclassWindow(ctrlCopyTTH);
#endif
	ctrlCopyTTH.SetIcon(g_iconBitmaps.getIcon(IconBitmaps::COPY_TO_CLIPBOARD, 0));

	if (!filename.empty() && initFileInfo())
		startThread();

	CenterWindow(GetParent());
	return 0;
}

LRESULT FileHashDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&)
{
	stopThread();
	EndDialog(wID);
	return 0;
}

LRESULT FileHashDlg::OnOpen(WORD, WORD, HWND, BOOL&)
{
	tstring file;
	if (WinUtil::browseFile(file, m_hWnd, false, lastDir, WinUtil::getFileMaskString(WinUtil::allFilesMask).c_str(), nullptr, &WinUtil::guidGetTTH))
	{
		stopThread();
		filename = std::move(file);
		if (initFileInfo())
			startThread();
	}
	return 0;
}

LRESULT FileHashDlg::OnCopy(WORD, WORD, HWND, BOOL&)
{
	if (!magnet.empty()) WinUtil::setClipboard(magnet);
	return 0;
}

LRESULT FileHashDlg::OnCopyTTH(WORD, WORD, HWND, BOOL&)
{
	tstring text;
	WinUtil::getWindowText(GetDlgItem(IDC_FILE_TTH), text);
	WinUtil::setClipboard(text);
	return 0;
}

#if 0
LRESULT FileHashDlg::OnCopyWebLink(WORD, WORD, HWND, BOOL&)
{
	if (!magnet.empty() && !filenameWithoutPath.empty())
	{
		StringMap params;
		params["magnet"] = magnet;
		params["size"] = Util::formatBytes(fileSize);
		params["TTH"] = tthStr;
		params["name"] = Text::fromT(filenameWithoutPath);
		WinUtil::setClipboard(Util::formatParams(SETTING(WMLINK_TEMPLATE), params, false));
	}
	return 0;
}
#endif

LRESULT FileHashDlg::OnThreadResult(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	ThreadResult* tr = reinterpret_cast<ThreadResult*>(lParam);
	if (!tr->stopped)
	{
		CEdit edit(GetDlgItem(IDC_FILE_TTH));
		if (tr->result)
		{
			tthStr = tr->tthValue.toBase32();
			edit.SetWindowText(Text::toT(tthStr).c_str());
			if (fileSize > 0)
			{
				magnet = Util::getMagnet(tthStr, Text::fromT(filenameWithoutPath), fileSize);
				SetDlgItemText(IDC_MAGNET, Text::toT(magnet).c_str());
				GetDlgItem(IDC_COPY_TTH).EnableWindow(TRUE);
				GetDlgItem(IDC_COPY).EnableWindow(TRUE);
			}
			else
				clearMagnet();
		} else
		{
			edit.SetWindowText(CTSTRING(FILE_TTH_ERROR));
		}
	}
	delete tr;
	return 0;
}

DWORD WINAPI FileHashDlg::hashThreadProc(void* param)
{
	static_cast<FileHashDlg*>(param)->hashThreadProc();
	return 0;
}

void FileHashDlg::hashThreadProc()
{
	ThreadResult* tr = new ThreadResult;
	TigerTree tree;
	tr->result = Util::getTTH(Text::fromT(filename), true, HASH_BUF_SIZE, stopFlag, tree, 1);
	tr->tthValue = tree.getRoot();
	tr->stopped = stopFlag.load();
	PostMessage(FHD_THREAD_RESULT, 0, reinterpret_cast<LPARAM>(tr));
}

void FileHashDlg::stopThread()
{
	if (!hashThreadHandle) return;
	stopFlag.store(true);
	WaitForSingleObject(hashThreadHandle, INFINITE);
	CloseHandle(hashThreadHandle);
	hashThreadHandle = NULL;
}

void FileHashDlg::startThread()
{
	if (hashThreadHandle) return;
	stopFlag.store(false);
	SetDlgItemText(IDC_FILE_TTH, CTSTRING(FILE_TTH_CALCULATING));
	clearMagnet();
	hashThreadHandle = CreateThread(nullptr, 0, hashThreadProc, this, 0, nullptr);
}

bool FileHashDlg::initFileInfo()
{
	filenameWithoutPath = Util::getFileName(filename);
	SetDlgItemText(IDC_PATH, filename.c_str());
	SetDlgItemText(IDC_FILE_NAME, filenameWithoutPath.c_str());
	fileSize = File::getSize(filename);
	if (fileSize < 0)
	{
		SetDlgItemText(IDC_FILE_TTH, CTSTRING(FILE_TTH_ERROR));
		SetDlgItemText(IDC_FILE_SIZE, _T(""));
		SetDlgItemText(IDC_NTFS_TTH, _T(""));
		clearMagnet();
		return false;
	}
	SetDlgItemTextW(IDC_FILE_SIZE, Util::formatBytesW(fileSize).c_str());
	TigerTree tree;
	if (HashManager::doLoadTree(Text::fromT(filename), tree, fileSize, false))
	{
		string tthStr = tree.getRoot().toBase32();
		SetDlgItemText(IDC_NTFS_TTH, Text::toT(tthStr).c_str());
	}
	else
		SetDlgItemText(IDC_NTFS_TTH, CTSTRING(NA));
	return true;
}

void FileHashDlg::clearMagnet()
{
	magnet.clear();
	tthStr.clear();
	SetDlgItemText(IDC_MAGNET, _T(""));
	GetDlgItem(IDC_COPY_TTH).EnableWindow(FALSE);
	GetDlgItem(IDC_COPY).EnableWindow(FALSE);
}
