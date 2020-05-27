#include "stdafx.h"
#include "Resource.h"
#include "WinUtil.h"
#include "HashProgressDlg.h"
#include "../client/ShareManager.h"
#include "../client/HashManager.h"

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

LRESULT HashProgressDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(HASH_PROGRESS));
	
	if (icon)
	{
		SetIcon(icon, FALSE);
		SetIcon(icon, TRUE);
	}

	WinUtil::translate(*this, texts);

	currentFile.Attach(GetDlgItem(IDC_CURRENT_FILE));
	infoFiles.Attach(GetDlgItem(IDC_HASH_FILES));
	infoSpeed.Attach(GetDlgItem(IDC_HASH_SPEED));
	infoTime.Attach(GetDlgItem(IDC_TIME_LEFT));
	
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

void HashProgressDlg::updateStats()
{
	auto hashManager = HashManager::getInstance();
	
	paused = hashManager->getHashSpeed() < 0;

	SetDlgItemText(IDC_PAUSE, paused ? CTSTRING(RESUME) : CTSTRING(PAUSE));

	HashManager::Info info;
	hashManager->getInfo(info);
	if (info.filesLeft == 0)
	{
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
		switch (ShareManager::getInstance()->getState())
		{
			case ShareManager::STATE_SCANNING_DIRS:
				stateText = ResourceManager::SCANNING_DIRS;
				break;
			case ShareManager::STATE_CREATING_FILELIST:
				stateText = ResourceManager::CREATING_FILELIST;
				break;
			default:
				stateText = ResourceManager::DONE;
		}
		infoFiles.SetWindowText(CTSTRING_I(stateText));
		infoSpeed.ShowWindow(SW_HIDE);
		infoTime.ShowWindow(SW_HIDE);
		progress.SetPos(0);
		return;
	}
	
	GetDlgItem(IDC_BTN_REFRESH_FILELIST).EnableWindow(FALSE);
	currentFile.SetWindowText(Text::toT(info.filename).c_str());
	currentFile.ShowWindow(SW_SHOW);

	const uint64_t tick = GET_TICK();
	const int64_t bytesLeft = info.sizeToHash - info.sizeHashed;
	tstring sizeStr = Util::formatBytesT(bytesLeft);
	tstring filesStr = Util::toStringT(info.filesLeft);
	infoFiles.SetWindowText(TSTRING_F(HASH_INFO_FILES, filesStr % sizeStr).c_str());

	if (paused)
	{
		tstring timeStr = _T("(") + TSTRING(PAUSED) + _T(")");;
		infoTime.SetWindowText(timeStr.c_str());
		infoTime.ShowWindow(SW_SHOW);
		infoSpeed.ShowWindow(SW_HIDE);
	}
	else
	{
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
		infoSpeed.SetWindowText(CTSTRING_F(HASH_INFO_SPEED, speedStr));
		infoSpeed.ShowWindow(SW_SHOW);
		infoTime.SetWindowText(CTSTRING_F(HASH_INFO_ETA, timeStr));
		infoTime.ShowWindow(SW_SHOW);
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
