/*
 * FlylinkDC++ // Chat Settings Page
 */

#ifndef MESSAGES_CHAT_PAGE_H
#define MESSAGES_CHAT_PAGE_H

#include "PropPage.h"
#include "wtl_flylinkdc.h"

class MessagesChatPage : public CPropertyPage<IDD_MESSAGES_CHAT_PAGE>, public PropPage
{
	public:
		explicit MessagesChatPage() : PropPage(TSTRING(SETTINGS_MESSAGES) + _T('\\') + TSTRING(SETTINGS_ADVANCED))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}

		BEGIN_MSG_MAP(MessagesChatPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog_chat)
		COMMAND_ID_HANDLER(IDC_PROTECT_PRIVATE, onEnablePassword)
		COMMAND_ID_HANDLER(IDC_PM_PASSWORD_GENERATE, onRandomPassword)
		COMMAND_HANDLER(IDC_PM_PASSWORD_HELP, BN_CLICKED, onClickedHelp)
		END_MSG_MAP()
		
		LRESULT onInitDialog_chat(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onEnablePassword(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRandomPassword(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedHelp(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */);
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_CHAT; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	private:
		CFlyToolTipCtrl ctrlTooltip;
		CButton ctrlSee;
		CButton ctrlProtect;
		CButton ctrlRnd;
		
	protected:
		CListViewCtrl ctrlList;
		void fixControls();
};

#endif //MESSAGES_CHAT_PAGE_H
