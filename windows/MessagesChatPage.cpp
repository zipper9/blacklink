/*
 * FlylinkDC++ // Chat Settings Page
 */

#include "stdafx.h"
#include "MessagesChatPage.h"
#include "WinUtil.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_PROTECT_PRIVATE, ResourceManager::SETTINGS_PROTECT_PRIVATE },
	{ IDC_SETTINGS_PASSWORD, ResourceManager::SETTINGS_PASSWORD },
	{ IDC_SETTINGS_PASSWORD_HINT, ResourceManager::SETTINGS_PASSWORD_HINT },
	{ IDC_SETTINGS_PASSWORD_OK_HINT, ResourceManager::SETTINGS_PASSWORD_OK_HINT },
	{ IDC_PROTECT_PRIVATE_RND, ResourceManager::SETTINGS_PROTECT_PRIVATE_RND },
	{ IDC_PROTECT_PRIVATE_SAY, ResourceManager::SETTINGS_PROTECT_PRIVATE_SAY },
	{ IDC_PM_PASSWORD_GENERATE, ResourceManager::WIZARD_NICK_RND },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_PROTECT_PRIVATE, SettingsManager::PROTECT_PRIVATE, PropPage::T_BOOL },
	{ IDC_PASSWORD, SettingsManager::PM_PASSWORD, PropPage::T_STR },
	{ IDC_PASSWORD_HINT, SettingsManager::PM_PASSWORD_HINT, PropPage::T_STR },
	{ IDC_PASSWORD_OK_HINT, SettingsManager::PM_PASSWORD_OK_HINT, PropPage::T_STR },
	{ IDC_PROTECT_PRIVATE_RND, SettingsManager::PROTECT_PRIVATE_RND, PropPage::T_BOOL },
	{ IDC_PROTECT_PRIVATE_SAY, SettingsManager::PROTECT_PRIVATE_SAY, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ SettingsManager::SHOW_SEND_MESSAGE_BUTTON, ResourceManager::SHOW_SEND_MESSAGE_BUTTON},
	{ SettingsManager::SHOW_MULTI_CHAT_BTN, ResourceManager::SHOW_MULTI_CHAT_BTN },
#ifdef IRAINMAN_INCLUDE_SMILE
	{ SettingsManager::SHOW_EMOTICONS_BTN, ResourceManager::SHOW_EMOTIONS_BTN },
#endif
	{ SettingsManager::SHOW_BBCODE_PANEL, ResourceManager::SHOW_BBCODE_PANEL },
	{ SettingsManager::MULTILINE_CHAT_INPUT, ResourceManager::MULTILINE_CHAT_INPUT },
	{ SettingsManager::MULTILINE_CHAT_INPUT_BY_CTRL_ENTER, ResourceManager::MULTILINE_CHAT_INPUT_BY_CTRL_ENTER },
#ifdef IRAINMAN_USE_BB_CODES
	{ SettingsManager::FORMAT_BB_CODES, ResourceManager::FORMAT_BB_CODES },
	{ SettingsManager::FORMAT_BB_CODES_COLORS, ResourceManager::FORMAT_BB_CODES_COLORS },
	{ SettingsManager::FORMAT_BOT_MESSAGE, ResourceManager::FORMAT_BOT_MESSAGE },
#endif
#ifdef IRAINMAN_INCLUDE_SMILE
	{ SettingsManager::CHAT_ANIM_SMILES, ResourceManager::CHAT_ANIM_SMILES },
	{ SettingsManager::SMILE_SELECT_WND_ANIM_SMILES, ResourceManager::SMILE_SELECT_WND_ANIM_SMILES },
#endif
	{ SettingsManager::CHAT_PANEL_SHOW_INFOTIPS, ResourceManager::CHAT_PANEL_SHOW_INFOTIPS },
#ifdef SCALOLAZ_CHAT_REFFERING_TO_NICK
	{ SettingsManager::CHAT_REFFERING_TO_NICK, ResourceManager::CHAT_REFFERING_TO_NICK },
#endif
	{ 0, ResourceManager::Strings() }
};

LRESULT MessagesChatPage::onInitDialog_chat(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlList.Attach(GetDlgItem(IDC_MESSAGES_CHAT_BOOLEANS));
	PropPage::read(*this, items, listItems, ctrlList);
	
	ctrlTooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
	ctrlTooltip.SetDelayTime(TTDT_AUTOPOP, 15000);
	ATLASSERT(ctrlTooltip.IsWindow());
	ctrlSee.Attach(GetDlgItem(IDC_PROTECT_PRIVATE_SAY));
	ctrlTooltip.AddTool(ctrlSee, ResourceManager::PROTECT_PRIVATE_SAY_TOOLTIP);
	ctrlProtect.Attach(GetDlgItem(IDC_PROTECT_PRIVATE));
	ctrlTooltip.AddTool(ctrlProtect, ResourceManager::PROTECT_PRIVATE_TOOLTIP);
	ctrlRnd.Attach(GetDlgItem(IDC_PROTECT_PRIVATE_RND));
	ctrlTooltip.AddTool(ctrlRnd, ResourceManager::PROTECT_PRIVATE_RND_TOOLTIP);
	ctrlTooltip.SetMaxTipWidth(256);
	ctrlTooltip.Activate(TRUE);
	
	PropPage::translate(*this, texts);
	fixControls();
	return TRUE;
}

void MessagesChatPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_MESSAGES_CHAT_BOOLEANS));
}

LRESULT MessagesChatPage::onEnablePassword(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

void MessagesChatPage::fixControls()
{
	BOOL enabled = ctrlProtect.GetCheck() == BST_CHECKED;
	GetDlgItem(IDC_PASSWORD).EnableWindow(enabled);
	GetDlgItem(IDC_PM_PASSWORD_GENERATE).EnableWindow(enabled);
	ctrlRnd.EnableWindow(enabled);
	GetDlgItem(IDC_PASSWORD_HINT).EnableWindow(enabled);
	GetDlgItem(IDC_PM_PASSWORD_HELP).EnableWindow(enabled);
	GetDlgItem(IDC_PASSWORD_OK_HINT).EnableWindow(enabled);
	ctrlSee.EnableWindow(enabled);
}

LRESULT MessagesChatPage::onRandomPassword(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SetDlgItemText(IDC_PASSWORD, Text::toT(Util::getRandomNick()).c_str());
	return 0;
}

LRESULT MessagesChatPage::onClickedHelp(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	MessageBox(CTSTRING(PRIVATE_PASSWORD_HELP), CTSTRING(PRIVATE_PASSWORD_HELP_DESC), MB_OK | MB_ICONINFORMATION);
	return 0;
}
