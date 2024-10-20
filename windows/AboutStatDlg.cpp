#include "stdafx.h"
#include "AboutStatDlg.h"
#include "DialogLayout.h"
#include "../client/AppStats.h"
#include "../client/SimpleStringTokenizer.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_AUTO_REFRESH, FLAG_TRANSLATE, AUTO, UNSPEC },
};

LRESULT AboutStatDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	showText();
	return TRUE;
}

void AboutStatDlg::showText()
{
	string formattedText("{\\urtf8\\tx2500\n{\\colortbl \\red0\\green0\\blue0;\\red0\\green128\\blue255;\\red128\\green128\\blue128;}\n");
	string text = AppStats::getStats();
	SimpleStringTokenizer<char> st(text, '\n');
	string line;
	while (st.getNextToken(line))
	{
		if (!line.empty())
		{
			auto pos = line.find('\t');
			if (pos == string::npos)
			{
				formattedText += "{\\cf1\\b ";
				formattedText += line;
				formattedText += "\\b0}";
			}
			else
			{
				formattedText += "\\b ";
				formattedText += line.substr(0, pos);
				formattedText += "\\b0 ";
				formattedText += line.substr(pos);
			}
		}
		formattedText += "\\line\n";
	}
	formattedText += "\\par}\n";
	CRichEditCtrl ctrlText(GetDlgItem(IDC_COMMANDS));
	ctrlText.SetTextEx((LPCTSTR) formattedText.c_str());
	ctrlText.PostMessage(EM_SCROLL, SB_TOP);
}

LRESULT AboutStatDlg::onDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	destroyTimer();
	return 0;
}

LRESULT AboutStatDlg::onAutoRefresh(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	if (IsDlgButtonChecked(IDC_AUTO_REFRESH))
		createTimer(2000);
	else
		destroyTimer();
	return 0;
}

LRESULT AboutStatDlg::onTimer(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (checkTimerID(wParam))
		showText();
	else
		bHandled = FALSE;
	return 0;
}
