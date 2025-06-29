#include "stdafx.h"

#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION

#include "Resource.h"
#include "CDMDebugFrame.h"
#include "Fonts.h"
#include "../client/File.h"
#include "../client/DatabaseManager.h"

#define MAX_TEXT_LEN 131072

static const size_t MAX_CMD_LIST_SIZE = 10000;

LRESULT CDMDebugFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ctrlCMDPad.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                  WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_READONLY, WS_EX_CLIENTEDGE);
	ctrlCMDPad.LimitText(0);
	ctrlCMDPad.SetFont(Fonts::g_font);

	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	ctrlStatus.ModifyStyleEx(0, WS_EX_COMPOSITED);
	statusContainer.SubclassWindow(ctrlStatus.m_hWnd);

	ctrlClear.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_PUSHBUTTON, 0, IDC_CLEAR);
	ctrlClear.SetWindowText(CTSTRING(CDM_CLEAR));
	ctrlClear.SetFont(Fonts::g_systemFont);
	clearContainer.SubclassWindow(ctrlClear.m_hWnd);

	ctrlDirection.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE);
	ctrlDirection.SetFont(Fonts::g_systemFont);
	ctrlDirection.AddString(_T("Both"));
	ctrlDirection.AddString(_T("Outgoing"));
	ctrlDirection.AddString(_T("Incoming"));
	ctrlDirection.AddString(_T("None"));
	ctrlDirection.SetCurSel(0);
	
	DBRegistryMap values;
	auto conn = DatabaseManager::getInstance()->getDefaultConnection();
	if (conn) conn->loadRegistry(values, e_CMDDebugFilterState);
	showHubCommands = values["showHubCommands"];
	
	ctrlHubCommands.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(CDM_HUB_COMMANDS), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_AUTOCHECKBOX, 0);
	ctrlHubCommands.SetFont(Fonts::g_systemFont);
	ctrlHubCommands.SetCheck(showHubCommands ? BST_CHECKED : BST_UNCHECKED);
	hubCommandContainer.SubclassWindow(ctrlHubCommands.m_hWnd);
	
	showCommands = values["showCommands"];
	ctrlCommands.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(CDM_CLIENT_COMMANDS), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_AUTOCHECKBOX, 0);
	ctrlCommands.SetFont(Fonts::g_systemFont);
	ctrlCommands.SetCheck(showCommands ? BST_CHECKED : BST_UNCHECKED);
	commandContainer.SubclassWindow(ctrlCommands.m_hWnd);
	
	showDetection = values["showDetection"];
	ctrlDetection.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(CDM_DETECTION), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_AUTOCHECKBOX, 0);
	ctrlDetection.SetFont(Fonts::g_systemFont);
	ctrlDetection.SetCheck(showDetection ? BST_CHECKED : BST_UNCHECKED);
	detectionContainer.SubclassWindow(ctrlDetection.m_hWnd);
	
	enableFilterIp = values["enableFilterIp"];
	ctrlFilterIp.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(CDM_FILTER), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_AUTOCHECKBOX, 0);
	ctrlFilterIp.SetFont(Fonts::g_systemFont);
	ctrlFilterIp.SetCheck(enableFilterIp ? BST_CHECKED : BST_UNCHECKED);
	cFilterContainer.SubclassWindow(ctrlFilterIp.m_hWnd);
	// add ES_AUTOHSCROLL - fix
	ctrlIPFilter.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | ES_NOHIDESEL | ES_AUTOHSCROLL, WS_EX_STATICEDGE, IDC_DEBUG_IP_FILTER_TEXT);
	ctrlIPFilter.SetLimitText(22); // для IP+Port
	ctrlIPFilter.SetFont(Fonts::g_font);
	eFilterContainer.SubclassWindow(ctrlStatus.m_hWnd);
	
	ctrlIncludeFilter.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | ES_NOHIDESEL | ES_AUTOHSCROLL, WS_EX_STATICEDGE, IDC_DEBUG_INCLUDE_FILTER_TEXT);
	ctrlIncludeFilter.SetLimitText(100);
	ctrlIncludeFilter.SetFont(Fonts::g_font);
	includeFilterContainer.SubclassWindow(ctrlStatus.m_hWnd);
	
	ctrlExcludeFilter.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | ES_NOHIDESEL | ES_AUTOHSCROLL, WS_EX_STATICEDGE, IDC_DEBUG_EXCLUDE_FILTER_TEXT);
	ctrlExcludeFilter.SetLimitText(100);
	ctrlExcludeFilter.SetFont(Fonts::g_font);
	excludeFilterContainer.SubclassWindow(ctrlStatus.m_hWnd);
	
	m_hWndClient = ctrlCMDPad;
	m_hMenu = MenuHelper::mainMenu;
	
	start(64, "CDMDebugFrame");
	DebugManager::newInstance();
	DebugManager::getInstance()->addListener(this);
	
	ctrlIPFilter.SetWindowText(Text::toT(values["filterIp"].sval).c_str());
	ctrlIncludeFilter.SetWindowText(Text::toT(values["filterInclude"].sval).c_str());
	ctrlExcludeFilter.SetWindowText(Text::toT(values["filterExclude"].sval).c_str());
	
	bHandled = FALSE;
	DebugManager::g_isCMDDebug = true;
	return 1;
}

LRESULT CDMDebugFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::CDM_DEBUG, 0);
	opt->isHub = false;
	return TRUE;
}

LRESULT CDMDebugFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	DebugManager::g_isCMDDebug = false;
	if (!closed)
	{
		stopThread();
		closed = true;

		DBRegistryMap values;
		if (!filterIp.empty())
			values["filterIp"] = filterIp;
		if (!filterInclude.empty())
			values["filterInclude"] = filterInclude;
		if (!filterExclude.empty())
			values["filterExclude"] = filterExclude;
		if (showCommands)
			values["showCommands"] = DBRegistryValue(showCommands);
		if (showHubCommands)
			values["showHubCommands"] = DBRegistryValue(showHubCommands);
		if (showDetection)
			values["showDetection"] = DBRegistryValue(showDetection);
		if (enableFilterIp)
			values["enableFilterIp"] = DBRegistryValue(enableFilterIp);
		auto conn = DatabaseManager::getInstance()->getDefaultConnection();
		if (conn) conn->saveRegistry(values, e_CMDDebugFilterState, true);
		
		DebugManager::getInstance()->removeListener(this);
		DebugManager::deleteInstance();
		PostMessage(WM_CLOSE);
	}
	else
	{
		bHandled = FALSE;
	}
	return 0;
}

void CDMDebugFrame::moveCheckBox(int index, CButton& ctrl)
{
	CRect sr;
	ctrlStatus.GetRect(index, sr);
	ctrl.MoveWindow(sr);
}

void CDMDebugFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	RECT rect = { 0 };
	GetClientRect(&rect);
	
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[10];
		ctrlStatus.GetClientRect(sr);
		w[0] = 64;
		w[1] = w[0] + 120;
		w[2] = w[1] + 120;
		w[3] = w[2] + 120;
		w[4] = w[3] + 100;
		w[5] = w[4] + 170;
		w[6] = w[5] + 100;
		w[7] = w[6] + 100;
		w[8] = w[7] + 100;
		w[9] = sr.Width() - 4;
		ctrlStatus.SetParts(_countof(w), w);
		
		ctrlStatus.GetRect(0, sr);
		ctrlClear.MoveWindow(sr);
		ctrlStatus.GetRect(1, sr);
		ctrlDirection.MoveWindow(sr);
		moveCheckBox(2, ctrlCommands);
		moveCheckBox(3, ctrlHubCommands);
		moveCheckBox(4, ctrlDetection);
		moveCheckBox(5, ctrlFilterIp);
		ctrlStatus.GetRect(6, sr);
		ctrlIPFilter.MoveWindow(sr);
		ctrlStatus.GetRect(7, sr);
		ctrlIncludeFilter.MoveWindow(sr);
		ctrlStatus.GetRect(8, sr);
		ctrlExcludeFilter.MoveWindow(sr);
		
		tstring msg;
		if (enableFilterIp)
		{
			if (!filterIp.empty())
				msg += _T(" IP:Port = ") + Text::toT(filterIp); // TODO - ругаться если Ip и порт не указан. не найдет иначе.
			if (!filterInclude.empty())
			{
				if (!msg.empty()) msg += _T(", ");
				msg += _T("Include ") + Text::toT(filterInclude);
			}
			if (!filterExclude.empty())
			{
				if (!msg.empty()) msg += _T(", ");
				msg += _T("Exclude ") + Text::toT(filterExclude);
			}
		}
		else
		{
			msg = TSTRING(CDM_WATCHING_ALL);
		}
		ctrlIPFilter.EnableWindow(enableFilterIp);
		ctrlIncludeFilter.EnableWindow(enableFilterIp);
		ctrlExcludeFilter.EnableWindow(enableFilterIp);
		ctrlStatus.SetText(9, msg.c_str());
	}
	
	// resize client window
	if (m_hWndClient)
		::SetWindowPos(m_hWndClient, NULL, rect.left, rect.top,
		               rect.right - rect.left, rect.bottom - rect.top,
		               SWP_NOZORDER | SWP_NOACTIVATE);
}

void CDMDebugFrame::addLine(const DebugTask& task)
{
	if (!isClosedOrShutdown())
	{
		if (ctrlCMDPad.GetWindowTextLength() > MAX_TEXT_LEN)
		{
			CLockRedraw<> lockRedraw(ctrlCMDPad);
			ctrlCMDPad.SetSel(0, ctrlCMDPad.LineIndex(ctrlCMDPad.LineFromChar(2000)));
			ctrlCMDPad.ReplaceSel(_T(""));
		}
		bool noScroll = true;
		POINT p = ctrlCMDPad.PosFromChar(ctrlCMDPad.GetWindowTextLength() - 1);
		CRect r;
		ctrlCMDPad.GetClientRect(r);
		
		if (r.PtInRect(p) || MDIGetActive() != m_hWnd)
		{
			noScroll = false;
		}
		else
		{
			ctrlCMDPad.SetRedraw(FALSE); // Strange!! This disables the scrolling...????
		}
		
		auto message = DebugTask::format(task);
		message += "\r\n";
		ctrlCMDPad.AppendText(Text::toT(message).c_str());
		if (noScroll)
			ctrlCMDPad.SetRedraw(TRUE);
	}
}

LRESULT CDMDebugFrame::onClear(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlCMDPad.SetWindowText(_T(""));
	ctrlCMDPad.SetFocus();
	return 0;
}

LRESULT CDMDebugFrame::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const HWND hWnd = (HWND)lParam;
	const HDC hDC = (HDC)wParam;
	if (hWnd == ctrlCMDPad.m_hWnd)
	{
		return Colors::setColor(hDC);
	}
	bHandled = FALSE;
	return FALSE;
}

LRESULT CDMDebugFrame::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring tmp;
	switch (wID)
	{
		case IDC_DEBUG_IP_FILTER_TEXT:
			WinUtil::getWindowText(ctrlIPFilter, tmp);
			filterIp = Text::fromT(tmp);
			ipTokens = StringTokenizer<string>(filterIp, ',');
			clearCmd();
			break;
		case IDC_DEBUG_INCLUDE_FILTER_TEXT:
			WinUtil::getWindowText(ctrlIncludeFilter, tmp);
			filterInclude = Text::fromT(tmp);
			includeTokens = StringTokenizer<string>(filterInclude, ',');
			clearCmd();
			break;
		case IDC_DEBUG_EXCLUDE_FILTER_TEXT:
			WinUtil::getWindowText(ctrlExcludeFilter, tmp);
			filterExclude = Text::fromT(tmp);
			excludeTokens = StringTokenizer<string>(filterExclude, ',');
			clearCmd();
			break;
		default:
			dcassert(0);
	}
	UpdateLayout();
	return 0;
}

LRESULT CDMDebugFrame::onSelChange(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
	if (hWndCtl == ctrlDirection.m_hWnd)
		switch (ctrlDirection.GetCurSel())
		{
			case 0: filterDirection = DIRECTION_BOTH; break;
			case 1: filterDirection = DIRECTION_OUT; break;
			case 2: filterDirection = DIRECTION_IN; break;
			default: filterDirection = 0;
		}
	return 0;
}

int CDMDebugFrame::run()
{
	deque<DebugTask> cmdToProcess;
	while (!stopFlag)
	{
		event.wait();
		if (stopFlag) break;
		{
			LOCK(cs);
			std::swap(cmdToProcess, cmdList);
		}
		for (const DebugTask& task : cmdToProcess)
		{
			if (task.type != DebugTask::LAST)
				addLine(task);
			if (stopFlag) break;
		}
		cmdToProcess.clear();
	}
	return 0;
}

static inline bool hasToken(const vector<string>& tokens, const string& tok)
{
	return std::find(tokens.begin(), tokens.end(), tok) != tokens.end();
}

static inline bool hasToken(const vector<string>& tokens, const DebugTask& task)
{
	for (const string& tok : tokens)
		if (task.ipAndPort.find(tok) != string::npos || task.message.find(tok) != string::npos)
			return true;
	return false;
}

void CDMDebugFrame::on(DebugManagerListener::DebugEvent, const DebugTask& task) noexcept
{
	int direction;
	if (task.type == DebugTask::HUB_IN || task.type == DebugTask::CLIENT_IN)
		direction = DIRECTION_IN;
	else
	if (task.type == DebugTask::HUB_OUT || task.type == DebugTask::CLIENT_OUT)
		direction = DIRECTION_OUT;
	else
		direction = DIRECTION_IN | DIRECTION_OUT;
	if (!(filterDirection & direction)) return;
	switch (task.type)
	{
		case DebugTask::HUB_IN:
		case DebugTask::HUB_OUT:
			if (!showHubCommands)
				return;
			if (enableFilterIp && !filterIp.empty() && !hasToken(ipTokens.getTokens(), task.ipAndPort))
				return;
			break;
		case DebugTask::CLIENT_IN:
		case DebugTask::CLIENT_OUT:
			if (!showCommands)
				return;
			if (enableFilterIp && !filterIp.empty() && !hasToken(ipTokens.getTokens(), task.ipAndPort))
				return;
			break;
		case DebugTask::DETECTION:
			if (!showDetection)
				return;
			break;
#ifdef _DEBUG
		default:
			dcassert(0);
			return;
#endif
	}
	if (enableFilterIp && !filterInclude.empty())
	{
		if (!hasToken(includeTokens.getTokens(), task))
			return;
	}
	if (enableFilterIp && !filterExclude.empty())
	{
		if (hasToken(excludeTokens.getTokens(), task))
			return;
	}
	addCmd(task);
}

void CDMDebugFrame::clearCmd()
{
	LOCK(cs);
	cmdList.clear();
}

void CDMDebugFrame::addCmd(const DebugTask& task)
{
	{
		LOCK(cs);
		cmdList.push_back(task);
		if (cmdList.size() > MAX_CMD_LIST_SIZE)
			cmdList.pop_front();
	}
	event.notify();
}

void CDMDebugFrame::stopThread()
{
	stopFlag.store(true);
	event.notify();
	join();
}

CFrameWndClassInfo& CDMDebugFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("CDMDebugFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::CDM_DEBUG, 0);

	return wc;
}

#endif // IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
