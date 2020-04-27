#include "stdafx.h"
#include "FileHashDlg.h"
#include "../client/HashManager.h"
#include "../client/Util.h"
#include "../client/ParamExpander.h"
#include "WinUtil.h"

static const size_t HASH_BUF_SIZE = 512 * 1024;

struct ThreadResult
{
	TTHValue tthValue;
	bool result;
	bool stopped;
};

static const WinUtil::TextItem texts[] =
{
	{ IDC_CAPTION_PATH,      ResourceManager::FILE_TTH_PATH          },
	{ IDC_OPEN,              ResourceManager::OPEN                   },
	{ IDC_CAPTION_FILE_NAME, ResourceManager::FILE_TTH_FILE_NAME     },
	{ IDC_CAPTION_TTH,       ResourceManager::FILE_TTH_FILE_TTH      },
	{ IDC_CAPTION_NTFS_TTH,  ResourceManager::FILE_TTH_NTFS_TTH      },
	{ IDC_CAPTION_MAGNET,    ResourceManager::FILE_TTH_MAGNET        },
	{ IDC_COPY,              ResourceManager::COPY                   },
	{ IDC_COPY_WEB_LINK,     ResourceManager::FILE_TTH_COPY_WEB_LINK },
	{ IDCANCEL,              ResourceManager::CLOSE                  },
	{ 0,                     ResourceManager::Strings()              }
};

LRESULT FileHashDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(FILE_TTH_TITLE));
	WinUtil::translate(*this, texts);
	SetIcon(icon, FALSE);
	SetIcon(icon, TRUE);
	
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
	if (WinUtil::browseFile(file, m_hWnd, false, lastDir))
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
				magnet = "magnet:?xt=urn:tree:tiger:" + tthStr +
					"&xl=" + Util::toString(fileSize) + 
					"&dn=" + Util::encodeURI(Text::fromT(filenameWithoutPath));
				SetDlgItemText(IDC_MAGNET, Text::toT(magnet).c_str());
				GetDlgItem(IDC_COPY).EnableWindow(TRUE);
				GetDlgItem(IDC_COPY_WEB_LINK).EnableWindow(TRUE);
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
		SetDlgItemText(IDC_NTFS_TTH, CTSTRING(FILE_TTH_NA));
	return true;
}

void FileHashDlg::clearMagnet()
{
	magnet.clear();
	tthStr.clear();
	SetDlgItemText(IDC_MAGNET, _T(""));
	GetDlgItem(IDC_COPY).EnableWindow(FALSE);
	GetDlgItem(IDC_COPY_WEB_LINK).EnableWindow(FALSE);
}
