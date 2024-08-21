#ifndef PROGRESS_BAR_TAB_H_
#define PROGRESS_BAR_TAB_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "SettingsStore.h"
#include "PropPageCallback.h"
#include "BarShader.h"
#include "../client/BaseSettingsImpl.h"

class ProgressBarTab : public CDialogImpl<ProgressBarTab>
{
	public:
		enum { IDD = IDD_PROGRESSBAR_TAB };

		ProgressBarTab() : initializing(true), callback(nullptr)
		{
			percent[0] = percent[1] = 100;
			speedIcon[0] = speedIcon[1] = 4;
		}

		BEGIN_MSG_MAP(ProgressBarTab)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_DRAWITEM, onDrawItem)
		COMMAND_HANDLER(IDC_SHOW_PROGRESSBARS, BN_CLICKED, onEnable)
		COMMAND_HANDLER(IDC_SET_DL_BACKGROUND, BN_CLICKED, onChooseColor)
		COMMAND_HANDLER(IDC_SET_DL_TEXT, BN_CLICKED, onChooseColor)
		COMMAND_HANDLER(IDC_SET_UL_BACKGROUND, BN_CLICKED, onChooseColor)
		COMMAND_HANDLER(IDC_SET_UL_TEXT, BN_CLICKED, onChooseColor)
		COMMAND_HANDLER(IDC_SET_DL_DEFAULT, BN_CLICKED, onSetDefaultColor)
		COMMAND_HANDLER(IDC_SET_UL_DEFAULT, BN_CLICKED, onSetDefaultColor)
		COMMAND_HANDLER(IDC_PROGRESSBAR_ODC, BN_CLICKED, onStyleOption)
		COMMAND_HANDLER(IDC_PROGRESSBAR_CLASSIC, BN_CLICKED, onStyleOption)
		COMMAND_HANDLER(IDC_PROGRESS_OVERRIDE, BN_CLICKED, onStyleOption)
		COMMAND_HANDLER(IDC_PROGRESS_OVERRIDE2, BN_CLICKED, onStyleOption)
		COMMAND_HANDLER(IDC_PROGRESS_BUMPED, BN_CLICKED, onStyleOption)
		COMMAND_HANDLER(IDC_SHOW_SPEED_ICON, BN_CLICKED, onSpeedIconOption)
		COMMAND_HANDLER(IDC_USE_CUSTOM_SPEED, BN_CLICKED, onSpeedIconOption)
		COMMAND_HANDLER(IDC_DEPTH, EN_UPDATE, onStyleOption)
		COMMAND_HANDLER(IDC_PROGRESS_COLOR_DOWN_SHOW, STN_CLICKED, onClickProgress)
		COMMAND_HANDLER(IDC_PROGRESS_COLOR_UP_SHOW, STN_CLICKED, onClickProgress)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onEnable(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChooseColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSetDefaultColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onStyleOption(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSpeedIconOption(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickProgress(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		void loadSettings(const BaseSettingsImpl* ss);
		void saveSettings(BaseSettingsImpl* ss) const;
		void getValues(SettingsStore& ss) const;
		void setValues(const SettingsStore& ss);
		void updateTheme();
		void setBackgroundColor(COLORREF clr);
		void setEmptyBarBackground(COLORREF clr);
		void setCallback(PropPageCallback* p) { callback = p; }

	private:
		bool initializing;
		COLORREF progressText[2];
		COLORREF progressBackground[2];
		COLORREF windowBackground;
		COLORREF emptyBarBackground;
		ProgressBar progress[2];
		COLORREF* selColor;
		int selIndex;
		int percent[2];
		int speedIcon[2];
		bool progressOdc;
		bool progressBumped;
		bool progressOverrideBackground;
		bool progressOverrideText;
		int progressDepth;
		bool speedIconEnable;
		bool speedIconCustom;
		PropPageCallback* callback;

		void updateControls();
		void redrawBars();
		void setEnabled(bool enable);
		void getStyleOptions();
		void updateStyleOptions();
		void getSpeedIconOptions();
		void updateSpeedIconOptions();
		void applySettings(int index, bool check);
		void notifyColorChange(int id, int value);
		static UINT_PTR CALLBACK hookProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif // PROGRESS_BAR_TAB_H_
