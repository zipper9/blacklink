#include "stdafx.h"

#include "SlotPage.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "../client/IpGrant.h"
#include "../client/File.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { 9, DialogLayout::SIDE_RIGHT, U_DU(4) };
static const DialogLayout::Align align3 = { -2, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align4 = { 17, DialogLayout::SIDE_RIGHT, U_DU(4) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_SLOT_CONTROL_GROUP, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SETTINGS_UPLOADS_SLOTS, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_SETTINGS_SLOTS_PER_HUB, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_SETTINGS_UPLOADS_MIN_SPEED, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_SETTINGS_AUTO_SLOTS, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_SETTINGS_PARTIAL_SLOTS, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_SLOTS, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_EXTRA_SLOTS2, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_MIN_UPLOAD_SPEED, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_AUTO_SLOTS, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_PARTIAL_SLOTS, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_SLOT_DL, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SETTINGS_KBPS, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_MINISLOT_CONTROL_GROUP, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CZDC_SMALL_SLOTS, FLAG_TRANSLATE, AUTO, UNSPEC, 2 },
	{ IDC_CZDC_SMALL_SIZE, FLAG_TRANSLATE, AUTO, UNSPEC, 2 },
	{ IDC_EXTRA_SLOTS, 0, UNSPEC, UNSPEC, 0, &align3 },
	{ IDC_SMALL_FILE_SIZE, 0, UNSPEC, UNSPEC, 0, &align3 },
	{ IDC_SETTINGS_KB, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align4 },
	{ IDC_CZDC_NOTE_SMALL, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_EXTRA_SLOT_BY_IP, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_GRANTIP_INI, FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

static const PropPage::Item items[] =
{
	{ IDC_SLOTS, SettingsManager::SLOTS, PropPage::T_INT },
	{ IDC_MIN_UPLOAD_SPEED, SettingsManager::AUTO_SLOT_MIN_UL_SPEED, PropPage::T_INT },
	{ IDC_EXTRA_SLOTS, SettingsManager::EXTRA_SLOTS, PropPage::T_INT },
	{ IDC_SMALL_FILE_SIZE, SettingsManager::MINISLOT_SIZE, PropPage::T_INT },
	{ IDC_EXTRA_SLOTS2, SettingsManager::HUB_SLOTS, PropPage::T_INT },
	{ IDC_SLOT_DL, SettingsManager::EXTRA_SLOT_TO_DL, PropPage::T_BOOL },
	{ IDC_AUTO_SLOTS, SettingsManager::AUTO_SLOTS, PropPage::T_INT  },
	{ IDC_PARTIAL_SLOTS, SettingsManager::EXTRA_PARTIAL_SLOTS, PropPage::T_INT  },
#ifdef SSA_IPGRANT_FEATURE
	{ IDC_EXTRA_SLOT_BY_IP, SettingsManager::EXTRA_SLOT_BY_IP, PropPage::T_BOOL },
#endif
	{ 0, 0, PropPage::T_END }
};

LRESULT SlotPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items);

	CUpDownCtrl spin1(GetDlgItem(IDC_SLOTSPIN));
	spin1.SetRange32(1, 500);
	spin1.SetBuddy(GetDlgItem(IDC_SLOTS));

	CUpDownCtrl spin2(GetDlgItem(IDC_EXTRASPIN));
	spin2.SetRange32(0, 10);
	spin2.SetBuddy(GetDlgItem(IDC_EXTRA_SLOTS2));

	CUpDownCtrl spin3(GetDlgItem(IDC_MIN_UPLOAD_SPIN));
	spin3.SetRange32(0, UD_MAXVAL);
	spin3.SetBuddy(GetDlgItem(IDC_MIN_UPLOAD_SPEED));

	CUpDownCtrl spin4(GetDlgItem(IDC_AUTO_SLOTS_SPIN));
	spin4.SetRange32(0, 100);
	spin4.SetBuddy(GetDlgItem(IDC_AUTO_SLOTS));

	CUpDownCtrl spin5(GetDlgItem(IDC_PARTIAL_SLOTS_SPIN));
	spin5.SetRange32(0, 10);
	spin5.SetBuddy(GetDlgItem(IDC_PARTIAL_SLOTS));

	CUpDownCtrl spin6(GetDlgItem(IDC_EXTRA_SLOTS_SPIN));
	spin6.SetRange32(0, 100);
	spin6.SetBuddy(GetDlgItem(IDC_EXTRA_SLOTS));

	CUpDownCtrl spin7(GetDlgItem(IDC_SMALL_FILE_SIZE_SPIN));
	spin7.SetRange32(16, 32768);
	spin7.SetBuddy(GetDlgItem(IDC_SMALL_FILE_SIZE));

#ifdef SSA_IPGRANT_FEATURE
	try
	{
		ipGrantEnabled = BOOLSETTING(EXTRA_SLOT_BY_IP);
		ipGrantPath = IpGrant::getFileName();
		ipGrantData = File(ipGrantPath, File::READ, File::OPEN).read();
		SetDlgItemText(IDC_GRANTIP_INI, Text::toT(ipGrantData).c_str());
	}
	catch (const FileException&)
	{
		SetDlgItemText(IDC_GRANTIP_INI, _T(""));
	}
	
	fixControls();
#else
	::EnableWindow(GetDlgItem(IDC_EXTRA_SLOT_BY_IP), FALSE);
	GetDlgItem(IDC_EXTRA_SLOT_BY_IP).ShowWindow(FALSE);
	
	::EnableWindow(GetDlgItem(IDC_GRANT_IP_GROUP), FALSE);
	GetDlgItem(IDC_GRANT_IP_GROUP).ShowWindow(FALSE);
	
	::EnableWindow(GetDlgItem(IDC_GRANTIP_INI_STAIC), FALSE);
	GetDlgItem(IDC_GRANTIP_INI_STAIC).ShowWindow(FALSE);
	
	::EnableWindow(GetDlgItem(IDC_GRANTIP_INI), FALSE);
	GetDlgItem(IDC_GRANTIP_INI).ShowWindow(FALSE);
#endif // SSA_IPGRANT_FEATURE
	
	return TRUE;
}

void SlotPage::write()
{
	PropPage::write(*this, items);
#ifdef SSA_IPGRANT_FEATURE
	bool changed = false;
	tstring buf;
	WinUtil::getWindowText(GetDlgItem(IDC_GRANTIP_INI), buf);
	string newVal = Text::fromT(buf);
	if (newVal != ipGrantData)
	{
		try
		{
			File fout(ipGrantPath, File::WRITE, File::CREATE | File::TRUNCATE);
			fout.write(newVal);
			fout.close();
			ipGrantData = std::move(newVal);
			changed = true;
		}
		catch (const FileException&)
		{
		}
	}
	if (BOOLSETTING(EXTRA_SLOT_BY_IP))
	{
		if (changed || !ipGrantEnabled)
			ipGrant.load();
	}
	else
		ipGrant.clear();
#endif // SSA_IPGRANT_FEATURE
}

#ifdef SSA_IPGRANT_FEATURE
void SlotPage::fixControls()
{
	bool state = (IsDlgButtonChecked(IDC_EXTRA_SLOT_BY_IP) != 0);
	::EnableWindow(GetDlgItem(IDC_CAPTION_GRANTIP_INI), state);
	::EnableWindow(GetDlgItem(IDC_GRANTIP_INI), state);
}
#endif
