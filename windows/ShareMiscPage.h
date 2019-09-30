/*
 * FlylinkDC++ // Share Misc Settings Page
 */

#if !defined(SHARE_MISC_PAGE_H)
#define SHARE_MISC_PAGE_H

#pragma once

#include <atlcrack.h>
#include "PropPage.h"
#include "wtl_flylinkdc.h"

class ShareMiscPage : public CPropertyPage<IDD_SHARE_MISC_PAGE>, public PropPage
{
	public:
		explicit ShareMiscPage() : PropPage(TSTRING(SETTINGS_UPLOADS) + _T('\\') + TSTRING(SETTINGS_ADVANCED))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~ShareMiscPage()
		{
		}
		
		BEGIN_MSG_MAP(ShareMiscPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_TTH_IN_STREAM, onFixControls)
#ifdef FLYLINKDC_USE_GPU_TTH
		COMMAND_ID_HANDLER(IDC_TTH_USE_GPU, onTTHUseGPUToggle)
#endif
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // [+]NightOrion
#ifdef FLYLINKDC_USE_GPU_TTH
		LRESULT onTTHUseGPUToggle(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif

		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_UPLOAD_ADVANCED; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	private:
		void fixControls();
#ifdef FLYLINKDC_USE_GPU_TTH
		void fixGPUTTHControls();
#endif

	protected:	
		static Item items[];
		static TextItem texts[];
		static ListItem listItems[];
#ifdef FLYLINKDC_USE_GPU_TTH
		CComboBox ctrlTTHGPUDevices;
#endif
};

#endif //SHARE_MISC_PAGE_H
