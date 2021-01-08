#ifndef RANGES_PAGE_H
#define RANGES_PAGE_H

#include "PropPage.h"
#include "RichTextLabel.h"

class RangesPageIPGuard : public CDialogImpl<RangesPageIPGuard>
{
	public:
		enum { IDD = IDD_IPFILTER_PAGE1 };

		RangesPageIPGuard() : ipGuardEnabled(false) {}

		BEGIN_MSG_MAP(RangesPageIPGuard)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_ENABLE_IPGUARD, onFixControls)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD, WORD, HWND, BOOL&) { fixControls(); return 0; }
		void fixControls();
		void write();

	private:
		CComboBox ctrlMode;
		string ipGuardData;
		string ipGuardPath;
		bool ipGuardEnabled;
};

class RangesPageIPTrust : public CDialogImpl<RangesPageIPTrust>
{
	public:
		enum { IDD = IDD_IPFILTER_PAGE2 };

		RangesPageIPTrust() : ipTrustEnabled(false) {}

		BEGIN_MSG_MAP(RangesPageIPTrust)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_ENABLE_IPTRUST, onFixControls)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD, WORD, HWND, BOOL&) { fixControls(); return 0; }
		void fixControls();
		void write();

	private:
		string ipTrustData;
		string ipTrustPath;
		bool ipTrustEnabled;
};

class RangesPageP2PGuard: public CDialogImpl<RangesPageP2PGuard>
{
	public:
		enum { IDD = IDD_IPFILTER_PAGE3 };

		BEGIN_MSG_MAP(RangesPageP2PGuard)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WMU_LINK_ACTIVATED, onLinkActivated)
		COMMAND_ID_HANDLER(IDC_ENABLE_P2P_GUARD, onFixControls)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemoveBlocked)
		COMMAND_HANDLER(IDC_MANUAL_P2P_GUARD, LBN_SELCHANGE, onSelChange)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onLinkActivated(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD, WORD, HWND, BOOL&) { fixControls(); return 0; }
		LRESULT onSelChange(WORD, WORD, HWND, BOOL&);
		LRESULT onRemoveBlocked(WORD, WORD, HWND, BOOL&);
		void fixControls();
		void loadBlocked();
		void write();

	private:
		CButton checkBox;
		CListBox listBox;
		RichTextLabel infoLabel;
};

class RangesPage : public CPropertyPage<IDD_IPFILTER_PAGE>, public PropPage
{
	public:
		explicit RangesPage() : PropPage(TSTRING(IPGUARD))
		{
			SetTitle(m_title.c_str());
		}

		BEGIN_MSG_MAP_EX(RangesPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_TABS, TCN_SELCHANGE, onChangeTab)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onChangeTab(int idCtrl, LPNMHDR pnmh, BOOL& bHandled) { changeTab(); return 1; }

		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_RESTRICTED; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	private:
		CTabCtrl ctrlTabs;
		std::unique_ptr<RangesPageIPGuard> pageIpGuard;
		std::unique_ptr<RangesPageIPTrust> pageIpTrust;
		std::unique_ptr<RangesPageP2PGuard> pageP2PGuard;

		void changeTab();
};

#endif // RANGES_PAGE_H
