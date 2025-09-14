#include "stdafx.h"

#include "SlotPage.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "../client/Text.h"
#include "../client/SettingsManager.h"
#include "../client/IpGrant.h"
#include "../client/File.h"
#include "../client/ConfCore.h"

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
	{ IDC_SLOTS, Conf::SLOTS, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ IDC_MIN_UPLOAD_SPEED, Conf::AUTO_SLOT_MIN_UL_SPEED, PropPage::T_INT },
	{ IDC_EXTRA_SLOTS, Conf::EXTRA_SLOTS, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ IDC_SMALL_FILE_SIZE, Conf::MINISLOT_SIZE, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ IDC_EXTRA_SLOTS2, Conf::HUB_SLOTS, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ IDC_SLOT_DL, Conf::EXTRA_SLOT_TO_DL, PropPage::T_BOOL },
	{ IDC_AUTO_SLOTS, Conf::AUTO_SLOTS, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ IDC_PARTIAL_SLOTS, Conf::EXTRA_PARTIAL_SLOTS, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
#ifdef SSA_IPGRANT_FEATURE
	{ IDC_EXTRA_SLOT_BY_IP, Conf::EXTRA_SLOT_BY_IP, PropPage::T_BOOL },
#endif
	{ 0, 0, PropPage::T_END }
};

LRESULT SlotPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::initControls(*this, items);
	PropPage::read(*this, items);

	// FIXME
	CUpDownCtrl spin3(GetDlgItem(IDC_MIN_UPLOAD_SPIN));
	spin3.SetRange32(0, UD_MAXVAL);
	spin3.SetBuddy(GetDlgItem(IDC_MIN_UPLOAD_SPEED));

#ifdef SSA_IPGRANT_FEATURE
	try
	{
		auto ss = SettingsManager::instance.getCoreSettings();
		ss->lockRead();
		ipGrantEnabled = ss->getBool(Conf::EXTRA_SLOT_BY_IP);
		ss->unlockRead();
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
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	bool extraSlotByIP = ss->getBool(Conf::EXTRA_SLOT_BY_IP);
	ss->unlockRead();
	if (extraSlotByIP)
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
