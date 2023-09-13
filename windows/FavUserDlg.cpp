#include "stdafx.h"
#include "FavUserDlg.h"
#include "WinUtil.h"
#include "../client/FavoriteUser.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_CAPTION_DESCRIPTION,  ResourceManager::DESCRIPTION          },
	{ IDC_AUTO_GRANT,           ResourceManager::FAVUSER_AUTO_GRANT   },
	{ IDC_CAPTION_UPLOAD_SPEED, ResourceManager::FAVUSER_UPLOAD_SPEED },
	{ IDC_KBPS,                 ResourceManager::KBPS                 },
	{ IDC_CAPTION_SHARE_GROUP,  ResourceManager::SHARE_GROUP          },
	{ IDC_CAPTION_PM_HANDLING,  ResourceManager::PM_HANDLING          },
	{ IDOK,                     ResourceManager::OK                   },
	{ IDCANCEL,                 ResourceManager::CANCEL               },
	{ 0,                        ResourceManager::Strings()            }
};

LRESULT FavUserDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(title.c_str());
	WinUtil::translate(*this, texts);

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::USER, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);	
	
	ctrlDesc.Attach(GetDlgItem(IDC_DESCRIPTION));
	ctrlDesc.SetWindowText(description.c_str());

	ctrlAutoGrant.Attach(GetDlgItem(IDC_AUTO_GRANT));
	ctrlAutoGrant.SetCheck((flags & FavoriteUser::FLAG_GRANT_SLOT) ? BST_CHECKED : BST_UNCHECKED);

	ctrlUpload.Attach(GetDlgItem(IDC_UPLOAD_SPEED));
	ctrlUpload.AddString(CTSTRING(NORMAL));
	ctrlUpload.AddString(CTSTRING(SPEED_SUPER_USER));
	ctrlUpload.AddString(CTSTRING(SPEED_LIMITED));
	ctrlUpload.AddString(CTSTRING(BAN_USER));
	int selIndex;
	switch (speedLimit)
	{
		case FavoriteUser::UL_NONE:
			selIndex = 0;
			break;
		case FavoriteUser::UL_SU:
			selIndex = 1;
			break;
		case FavoriteUser::UL_BAN:
			selIndex = 3;
			break;
		default:
			selIndex = speedLimit > 0 ? 2 : 0;
	}
	ctrlUpload.SetCurSel(selIndex);
	ctrlSpeedValue.Attach(GetDlgItem(IDC_UL_VALUE));
	if (speedLimit > 0)
		ctrlSpeedValue.SetWindowText(Util::toStringT(speedLimit).c_str());
	updateSpeedCtrl();

	ctrlShareGroup.Attach(GetDlgItem(IDC_SHARE_GROUP));
	shareGroups.init();
	shareGroups.fillCombo(ctrlShareGroup, shareGroup, (flags & FavoriteUser::FLAG_HIDE_SHARE) != 0);

	ctrlPMHandling.Attach(GetDlgItem(IDC_PM_HANDLING));
	ctrlPMHandling.AddString(CTSTRING(NORMAL));
	ctrlPMHandling.AddString(CTSTRING(IGNORE_PRIVATE));
	ctrlPMHandling.AddString(CTSTRING(FREE_PM_ACCESS));
	if (flags & FavoriteUser::FLAG_IGNORE_PRIVATE)
		selIndex = 1;
	else if (flags & FavoriteUser::FLAG_FREE_PM_ACCESS)
		selIndex = 2;
	else
		selIndex = 0;
	ctrlPMHandling.SetCurSel(selIndex);

	CenterWindow(GetParent());
	return 0;
}

void FavUserDlg::updateSpeedCtrl()
{
	int vis = ctrlUpload.GetCurSel() == 2 ? SW_SHOW : SW_HIDE;
	ctrlSpeedValue.ShowWindow(vis);
	GetDlgItem(IDC_KBPS).ShowWindow(vis);
}

LRESULT FavUserDlg::onSelChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	updateSpeedCtrl();
	return 0;
}

LRESULT FavUserDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		WinUtil::getWindowText(ctrlDesc, description);
		flags = 0;
		if (ctrlAutoGrant.GetCheck() == BST_CHECKED)
			flags |= FavoriteUser::FLAG_GRANT_SLOT;
		switch (ctrlPMHandling.GetCurSel())
		{
			case 1:
				flags |= FavoriteUser::FLAG_IGNORE_PRIVATE;
				break;
			case 2:
				flags |= FavoriteUser::FLAG_FREE_PM_ACCESS;
		}
		switch (ctrlUpload.GetCurSel())
		{
			case 0:
				speedLimit = FavoriteUser::UL_NONE;
				break;
			case 1:
				speedLimit = FavoriteUser::UL_SU;
				break;
			case 3:
				speedLimit = FavoriteUser::UL_BAN;
				break;
			default:
			{
				tstring ts;
				WinUtil::getWindowText(ctrlSpeedValue, ts);
				speedLimit = Util::toInt(ts);
				if (speedLimit < 0) speedLimit = 0;
			}
		}
		int selIndex = ctrlShareGroup.GetCurSel();
		if (selIndex >= 0 && selIndex < (int) shareGroups.v.size())
		{
			const auto& sg = shareGroups.v[selIndex];
			if (sg.def == ShareGroupList::HIDE_SHARE_DEF)
				flags |= FavoriteUser::FLAG_HIDE_SHARE;
			shareGroup = sg.id;
		}
		else
			shareGroup.init();
	}
	EndDialog(wID);
	return 0;
}
