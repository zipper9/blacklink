#ifndef RANGES_PAGE_H
#define RANGES_PAGE_H

#include "PropPage.h"
#include "ExListViewCtrl.h"

class RangesPage : public CPropertyPage<IDD_RANGES_PAGE>, public PropPage
{
	public:
		explicit RangesPage() : PropPage(TSTRING(IPGUARD)), ipGuardEnabled(false), ipTrustEnabled(false)
		{
			SetTitle(m_title.c_str());
		}
		
		BEGIN_MSG_MAP_EX(RangesPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_ENABLE_IPGUARD, onFixControls)
		COMMAND_ID_HANDLER(IDC_ENABLE_IPTRUST, onFixControls)
		COMMAND_ID_HANDLER(IDC_FLYLINK_MANUAL_P2P_GUARD_IP_LIST_REMOVE_BUTTON, onRemoveP2PManual)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		
		LRESULT onItemchangedDirectories(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onRemoveP2PManual(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			fixControls();
			return 0;
		}
		
		LRESULT onDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
			
			if (item->iItem >= 0)
			{
				PostMessage(WM_COMMAND, IDC_CHANGE, 0);
			}
			else if (item->iItem == -1)
			{
				PostMessage(WM_COMMAND, IDC_ADD, 0);
			}
			
			return 0;
		}
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_RESTRICTED; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	protected:
		CComboBox ctrlPolicy;
		CListBox p2pGuardListBox;

		void loadManualP2PGuard();
		void fixControls();

	private:
		string ipGuardData;
		string ipGuardPath;
		bool ipGuardEnabled;
		string ipTrustData;
		string ipTrustPath;
		bool ipTrustEnabled;
};

#endif // RANGES_PAGE_H
