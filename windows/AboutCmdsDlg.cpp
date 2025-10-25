#include "stdafx.h"
#include "AboutCmdsDlg.h"
#include "../client/Commands.h"
#include "../client/Text.h"
#include "../client/SimpleStringTokenizer.h"

static inline bool isAlpha(char c)
{
	return (c>='a' && c<='z') || (c>='A' && c<='Z');
}

static void replaceHash(string& s)
{
	string::size_type pos = 0;
	while (true)
	{
		string::size_type startPos = s.find('#', pos);
		if (startPos == string::npos) break;
		string::size_type endPos = startPos + 1;
		while (endPos < s.length() && isAlpha(s[endPos])) ++endPos;
		s.insert(endPos, "}");
		s.erase(startPos, 1);
		s.insert(startPos, "{\\cf2 ");
		pos = endPos + 5;
	}
}

static void replaceAsterisk(string& s)
{
	string::size_type pos = 0;
	while (true)
	{
		string::size_type startPos = s.find('*', pos);
		if (startPos == string::npos) break;
		string::size_type endPos = s.find('*', startPos + 1);
		if (endPos == string::npos) break;
		s[endPos] = '}';
		s.erase(startPos, 1);
		s.insert(startPos, "{\\b ");
		pos = endPos + 3;
	}
}

LRESULT AboutCmdsDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
	string formattedHelp("{\\urtf8\\tx2000\n{\\colortbl \\red0\\green0\\blue0;\\red0\\green128\\blue255;\\red128\\green128\\blue128;}\n");
	string help = Commands::getHelpText(Commands::GHT_MARK_ALIASES);
	SimpleStringTokenizer<char> st(help, '\n');
	string line;
	while (st.getNextToken(line))
	{
		if (!line.empty())
		{
			auto pos = line.find('\t');
			if (pos == string::npos)
			{
				formattedHelp += "{\\cf1\\b ";
				formattedHelp += line;
				formattedHelp += "\\b0}";
			}
			else
			{
				pos = line.find_first_of("\t ");
				formattedHelp += "\\b ";
				formattedHelp += line.substr(0, pos);
				formattedHelp += "\\b0 ";
				formattedHelp += line.substr(pos);
				replaceAsterisk(formattedHelp);
				replaceHash(formattedHelp);
			}
		}
		formattedHelp += "\\line\n";
	}
	formattedHelp += "\\par}\n";
	CRichEditCtrl ctrlCommands(GetDlgItem(IDC_COMMANDS));
	ctrlCommands.SetTextEx((LPCTSTR) formattedHelp.c_str());
	ctrlCommands.PostMessage(EM_SCROLL, SB_TOP);
	ctrlCommands.PostMessage(EM_SETSEL, (WPARAM) -1, 0);
	return TRUE;
}
