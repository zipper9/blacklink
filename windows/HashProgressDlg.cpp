#include "stdafx.h"
#include "HashProgressDlg.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "../client/ShareManager.h"
#include "../client/HashManager.h"
#include "../client/FormatUtil.h"
#include "../client/TimeUtil.h"

#ifdef OSVER_WIN_XP
#include "../client/SysVersion.h"
#endif

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

int HashProgressDlg::instanceCounter = 0;

static const WinUtil::TextItem texts[] =
{
	{ IDOK,                      ResourceManager::HASH_PROGRESS_BACKGROUND  },
	{ IDC_STATISTICS,            ResourceManager::HASH_PROGRESS_STATS       },
	{ IDC_HASH_INDEXING,         ResourceManager::HASH_PROGRESS_TEXT        },
	{ IDC_BTN_ABORT,             ResourceManager::HASH_ABORT_TEXT           },
	{ IDC_BTN_REFRESH_FILELIST,  ResourceManager::HASH_REFRESH_FILE_LIST    },
	{ IDC_BTN_EXIT_ON_DONE,      ResourceManager::EXIT_ON_HASHING_DONE_TEXT },
	{ IDC_CHANGE_HASH_SPEED,     ResourceManager::CHANGE_HASH_SPEED_TEXT    },
	{ IDC_CURRENT_HASH_SPEED,    ResourceManager::CURRENT_HASH_SPEED_TEXT   },
	{ 0,                         ResourceManager::Strings()                 }
};

static const DialogLayout::Align align1 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { -2, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_HASH_SCAN1_LABEL, FLAG_TRANSLATE, AUTO,   UNSPEC, 1          },
	{ IDC_HASH_SCAN2_LABEL, FLAG_TRANSLATE, AUTO,   UNSPEC, 1          },
	{ IDC_HASH_SCAN1_TEXT,  0,              UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_HASH_SCAN2_TEXT,  0,              UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_HASH_SPEED_LABEL, FLAG_TRANSLATE, AUTO,   UNSPEC, 2          },
	{ IDC_HASH_ETA_LABEL,   FLAG_TRANSLATE, AUTO,   UNSPEC, 2          },
	{ IDC_HASH_SPEED_TEXT,  0,              UNSPEC, UNSPEC, 0, &align2 },
	{ IDC_HASH_ETA_TEXT,    0,              UNSPEC, UNSPEC, 0, &align2 }
};

LRESULT HashProgressDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(HASH_PROGRESS));
	
	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::HASH_PROGRESS, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	WinUtil::translate(*this, texts);
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	currentFile.Attach(GetDlgItem(IDC_CURRENT_FILE));
	infoState.Attach(GetDlgItem(IDC_HASH_STATE));
	infoSpeedLabel.Attach(GetDlgItem(IDC_HASH_SPEED_LABEL));
	infoSpeedText.Attach(GetDlgItem(IDC_HASH_SPEED_TEXT));
	infoTimeLabel.Attach(GetDlgItem(IDC_HASH_ETA_LABEL));
	infoTimeText.Attach(GetDlgItem(IDC_HASH_ETA_TEXT));
	infoDirsLabel.Attach(GetDlgItem(IDC_HASH_SCAN1_LABEL));
	infoDirsText.Attach(GetDlgItem(IDC_HASH_SCAN1_TEXT));
	infoFilesLabel.Attach(GetDlgItem(IDC_HASH_SCAN2_LABEL));
	infoFilesText.Attach(GetDlgItem(IDC_HASH_SCAN2_TEXT));

	pauseButton.Attach(GetDlgItem(IDC_PAUSE));
	exitOnDoneButton.Attach(GetDlgItem(IDC_BTN_EXIT_ON_DONE));
	exitOnDoneButton.SetCheck(exitOnDone ? BST_CHECKED : BST_UNCHECKED);

	if (!exitOnDone)
		exitOnDoneButton.ShowWindow(SW_HIDE);

	int hashSpeed = SETTING(MAX_HASH_SPEED);
	slider.Attach(GetDlgItem(IDC_EDIT_MAX_HASH_SPEED_SLIDER));
	slider.SetRange(0, 100);
	slider.SetPos(hashSpeed);

	updatingEditBox++;
	SetDlgItemInt(IDC_EDIT_MAX_HASH_SPEED, hashSpeed, FALSE);
	updatingEditBox--;
	
	progress.Attach(GetDlgItem(IDC_HASH_PROGRESS));
	progress.SetRange(0, MAX_PROGRESS_VALUE);
	updateStats();
	
	CenterWindow(GetParent());
	
	HashManager::getInstance()->setThreadPriority(Thread::NORMAL);
	
	createTimer(1000);
	return TRUE;
}

LRESULT HashProgressDlg::onPause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto hashManager = HashManager::getInstance();
	if (paused)
		hashManager->setMaxHashSpeed(tempHashSpeed);
	else
		hashManager->setMaxHashSpeed(-1);
	updateStats();
	return 0;
}

LRESULT HashProgressDlg::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	destroyTimer();
	auto hashManager = HashManager::getInstance();
	hashManager->setThreadPriority(Thread::IDLE);
	if (!paused)
		hashManager->setMaxHashSpeed(0);
	return 0;
}

void HashProgressDlg::showSpeedInfo(int state)
{
	dcassert(state == SW_SHOW || state == SW_HIDE);
	infoSpeedLabel.ShowWindow(state);
	infoSpeedText.ShowWindow(state);
	infoTimeLabel.ShowWindow(state);
	infoTimeText.ShowWindow(state);
}

void HashProgressDlg::showScanInfo(int state)
{
	dcassert(state == SW_SHOW || state == SW_HIDE);
	infoDirsLabel.ShowWindow(state);
	infoDirsText.ShowWindow(state);
	infoFilesLabel.ShowWindow(state);
	infoFilesText.ShowWindow(state);
}

void HashProgressDlg::updateStats()
{
	auto hashManager = HashManager::getInstance();
	
	paused = hashManager->getHashSpeed() < 0;
	pauseButton.SetWindowText(paused ? CTSTRING(RESUME) : CTSTRING(PAUSE));

	HashManager::Info info;
	hashManager->getInfo(info);
	if (info.filesLeft == 0)
	{
		pauseButton.EnableWindow(FALSE);
		if (exitOnDoneButton.GetCheck())
		{
			EndDialog(IDC_BTN_EXIT_ON_DONE);
			return;
		}
		if (autoClose)
		{
			PostMessage(WM_CLOSE);
			return;
		}
		GetDlgItem(IDC_BTN_REFRESH_FILELIST).EnableWindow(!ShareManager::getInstance()->isRefreshing());
		currentFile.ShowWindow(SW_HIDE);
		ResourceManager::Strings stateText;
		auto sm = ShareManager::getInstance();
		switch (sm->getState())
		{
			case ShareManager::STATE_SCANNING_DIRS:
			{
				stateText = ResourceManager::SCANNING_DIRS;
				int64_t progress[2];
				sm->getScanProgress(progress);
				showSpeedInfo(SW_HIDE);
				infoDirsText.SetWindowText(Util::toStringT(progress[0]).c_str());
				infoFilesText.SetWindowText(Util::toStringT(progress[1]).c_str());
				showScanInfo(SW_SHOW);
				setProgressState(PBST_NORMAL);
				setProgressMarquee(true);
				break;
			}
			case ShareManager::STATE_CREATING_FILELIST:
				stateText = ResourceManager::CREATING_FILELIST;
				showSpeedInfo(SW_HIDE);
				showScanInfo(SW_HIDE);
				setProgressState(PBST_NORMAL);
				setProgressMarquee(true);
				break;
			default:
				stateText = ResourceManager::DONE;
				showSpeedInfo(SW_HIDE);
				showScanInfo(SW_HIDE);
				setProgressState(PBST_NORMAL);
				setProgressMarquee(false);
				progress.SetPos(0);
		}
		infoState.SetWindowText(CTSTRING_I(stateText));
		return;
	}

	pauseButton.EnableWindow(TRUE);
	showScanInfo(SW_HIDE);

	GetDlgItem(IDC_BTN_REFRESH_FILELIST).EnableWindow(FALSE);
	currentFile.SetWindowText(Text::toT(info.filename).c_str());
	currentFile.ShowWindow(SW_SHOW);

	const uint64_t tick = GET_TICK();
	const int64_t bytesLeft = info.sizeToHash - info.sizeHashed;
	tstring fileStr = TPLURAL_F(PLURAL_FILES_LEFT, info.filesLeft);
	fileStr += _T(" (");
	fileStr += Util::formatBytesT(bytesLeft);
	fileStr += _T(")");
	infoState.SetWindowText(fileStr.c_str());

	setProgressMarquee(false);
	if (paused)
	{
		tstring timeStr = _T("(") + TSTRING(PAUSED) + _T(")");
		infoTimeText.SetWindowText(timeStr.c_str());
		infoTimeLabel.ShowWindow(SW_SHOW);
		infoTimeText.ShowWindow(SW_SHOW);
		infoSpeedLabel.ShowWindow(SW_HIDE);
		infoSpeedText.ShowWindow(SW_HIDE);
		setProgressState(PBST_PAUSED);
	}
	else
	{
		setProgressState(PBST_NORMAL);
		const int64_t diff = (int64_t) tick - info.startTick;
		int64_t sizeHashed = info.sizeHashed - info.startTickSavedSize;
		tstring speedStr, timeStr;
		if (diff < 1000 || sizeHashed < 1024)
		{
			speedStr = _T("-.--");
			timeStr = _T("-:--:--");
		}
		else
		{
			speedStr = Util::formatBytesT(sizeHashed * 1000 / diff) + _T('/') + TSTRING(S);
			timeStr = Util::formatSecondsT(bytesLeft * diff / (sizeHashed * 1000));
		}
		infoSpeedText.SetWindowText(speedStr.c_str());
		infoTimeText.SetWindowText(timeStr.c_str());
		showSpeedInfo(SW_SHOW);
	}

	int progressValue;
	if (info.sizeHashed >= info.sizeToHash)
		progressValue = MAX_PROGRESS_VALUE;
	else
		progressValue = (info.sizeHashed * MAX_PROGRESS_VALUE) / info.sizeToHash;
	progress.SetPos(progressValue);
}

LRESULT HashProgressDlg::onSliderChangeMaxHashSpeed(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	tempHashSpeed = slider.GetPos();
	updatingEditBox++;
	SetDlgItemInt(IDC_EDIT_MAX_HASH_SPEED, tempHashSpeed, FALSE);
	updatingEditBox--;
	HashManager::getInstance()->setMaxHashSpeed(tempHashSpeed);
	return 0;
}

LRESULT HashProgressDlg::onRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ShareManager::getInstance()->refreshShare();
	return 0;
}

LRESULT HashProgressDlg::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!checkTimerID(wParam))
	{
		bHandled = FALSE;
		return 0;
	}
	updateStats();
	return 0;
}

LRESULT HashProgressDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LRESULT HashProgressDlg::onClickedAbort(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HashManager::getInstance()->stopHashing(Util::emptyString);
	return 0;
}

LRESULT HashProgressDlg::onChangeMaxHashSpeed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (updatingEditBox) return 0;
	tempHashSpeed = GetDlgItemInt(IDC_EDIT_MAX_HASH_SPEED, nullptr, FALSE);
	slider.SetPos(tempHashSpeed);
	HashManager::getInstance()->setMaxHashSpeed(tempHashSpeed);
	return 0;
}

void HashProgressDlg::setProgressMarquee(bool enable)
{
	if (progressMarquee == enable) return;
	if (enable)
	{
		progress.ModifyStyle(0, PBS_MARQUEE);
		progress.SetMarquee(TRUE);
	}
	else
	{
		progress.SetMarquee(FALSE);
		progress.ModifyStyle(PBS_MARQUEE, 0);
	}
	progressMarquee = enable;
}

void HashProgressDlg::setProgressState(int state)
{
#ifdef OSVER_WIN_XP
	if (!SysVersion::isOsVistaPlus()) return;
#endif
	if (progressState == state) return;
	progress.SendMessage(PBM_SETSTATE, state);
	progressState = state;
}
