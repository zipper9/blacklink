#ifndef __CDMDEBUGFRAME_H
#define __CDMDEBUGFRAME_H

#pragma once

#include "../client/Semaphore.h"

#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION

#include "../client/DebugManager.h"
#include "FlatTabCtrl.h"
#include "WinUtil.h"

#define COMMAND_MESSAGE_MAP 14
#define DETECTION_MESSAGE_MAP 15
#define HUB_COMMAND_MESSAGE_MAP 16
#define DEBUG_FILTER_MESSAGE_MAP 17
#define DEBUG_FILTER_IP_MESSAGE_MAP 18
#define CLEAR_MESSAGE_MAP 19
#define DEBUG_FILTER_INCLUDE_TEXT_MESSAGE_MAP 20
#define DEBUG_FILTER_EXCLUDE_TEXT_MESSAGE_MAP 21

class CDMDebugFrame : public MDITabChildWindowImpl<CDMDebugFrame>,
	public StaticFrame<CDMDebugFrame, ResourceManager::MENU_CDMDEBUG_MESSAGES>,
	public Thread,
	private CFlyStopThread,
	private DebugManagerListener
{
	public:
		DECLARE_FRAME_WND_CLASS_EX(_T("CDMDebugFrame"), IDR_CDM, 0, COLOR_3DFACE);
		
		CDMDebugFrame() : m_showCommands(true), m_showHubCommands(false), m_showDetection(false), m_bFilterIp(false),
			detectionContainer(WC_BUTTON, this, DETECTION_MESSAGE_MAP),
			HubCommandContainer(WC_BUTTON, this, HUB_COMMAND_MESSAGE_MAP),
			commandContainer(WC_BUTTON, this, COMMAND_MESSAGE_MAP),
			cFilterContainer(WC_BUTTON, this, DEBUG_FILTER_MESSAGE_MAP),
			eFilterContainer(WC_EDIT, this, DEBUG_FILTER_IP_MESSAGE_MAP),
			clearContainer(WC_BUTTON, this, CLEAR_MESSAGE_MAP),
			statusContainer(STATUSCLASSNAME, this, CLEAR_MESSAGE_MAP),
			m_eIncludeFilterContainer(WC_EDIT, this, DEBUG_FILTER_INCLUDE_TEXT_MESSAGE_MAP),
			m_eExcludeFilterContainer(WC_EDIT, this, DEBUG_FILTER_EXCLUDE_TEXT_MESSAGE_MAP),
			filterDirection(DIRECTION_BOTH)
		{
		}
		
		~CDMDebugFrame()
		{
		}
		
		typedef MDITabChildWindowImpl<CDMDebugFrame> baseClass;
		BEGIN_MSG_MAP(CDMDebugFrame)
		MESSAGE_HANDLER(WM_SETFOCUS, OnFocus)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(DETECTION_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onSetCheckDetection)
		ALT_MSG_MAP(COMMAND_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onSetCheckCommand)
		ALT_MSG_MAP(HUB_COMMAND_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onSetCheckHubCommand)
		ALT_MSG_MAP(DEBUG_FILTER_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onSetCheckFilter)
		ALT_MSG_MAP(DEBUG_FILTER_IP_MESSAGE_MAP)
		COMMAND_HANDLER(IDC_DEBUG_IP_FILTER_TEXT, EN_CHANGE, onChange)
		ALT_MSG_MAP(DEBUG_FILTER_INCLUDE_TEXT_MESSAGE_MAP)
		COMMAND_HANDLER(IDC_DEBUG_INCLUDE_FILTER_TEXT, EN_CHANGE, onChange)
		ALT_MSG_MAP(DEBUG_FILTER_EXCLUDE_TEXT_MESSAGE_MAP)
		COMMAND_HANDLER(IDC_DEBUG_EXCLUDE_FILTER_TEXT, EN_CHANGE, onChange)
		COMMAND_CODE_HANDLER(CBN_SELCHANGE, onSelChange)

		ALT_MSG_MAP(CLEAR_MESSAGE_MAP)
		COMMAND_ID_HANDLER(IDC_CLEAR, onClear)
		END_MSG_MAP()
		
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClear(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		void UpdateLayout(BOOL bResizeBars = TRUE);
		LRESULT onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT OnFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlCMDPad.SetFocus();
			return 0;
		}
		LRESULT onSetCheckDetection(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			m_showDetection = wParam == BST_CHECKED;
			bHandled = FALSE;
			return 0;
		}
		LRESULT onSetCheckCommand(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			m_showCommands = wParam == BST_CHECKED;
			bHandled = FALSE;
			return 0;
		}
		LRESULT onSetCheckHubCommand(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			m_showHubCommands = wParam == BST_CHECKED;
			bHandled = FALSE;
			return 0;
		}
		LRESULT onSetCheckFilter(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			m_bFilterIp = wParam == BST_CHECKED;
			UpdateLayout();
			bHandled = FALSE;
			return 0;
		}
		LRESULT onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSelChange(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/);
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		
	private:
		enum
		{
			DIRECTION_OUT  = 1,
			DIRECTION_IN   = 2,
			DIRECTION_BOTH = DIRECTION_OUT | DIRECTION_IN
		};
	
		void addLine(const DebugTask& task);
		
		FastCriticalSection m_cs;
		Semaphore m_semaphore;
		deque<DebugTask> m_cmdList;
		
		int run();
		
		void addCmd(const DebugTask& task);
		void clearCmd();
		void moveCheckBox(int index, CButton& ctrl);
		
		CEdit ctrlCMDPad;
		CEdit ctrlIPFilter;
		CStatusBarCtrl ctrlStatus;
		CComboBox ctrlDirection;
		CButton ctrlClear, ctrlCommands, ctrlHubCommands, ctrlDetection, ctrlFilterIp;		
		CEdit ctrlIncludeFilter;
		CEdit ctrlExcludeFilter;
		CContainedWindow clearContainer, statusContainer, detectionContainer, commandContainer, HubCommandContainer, cFilterContainer, eFilterContainer;
		CContainedWindow m_eIncludeFilterContainer;
		CContainedWindow m_eExcludeFilterContainer;
		
		int filterDirection;
		bool m_showCommands, m_showHubCommands, m_showDetection, m_bFilterIp;
		string m_sFilterIp;
		string m_sFilterInclude;
		string m_sFilterExclude;
		StringTokenizer<string> m_IncludeTokens;
		StringTokenizer<string> m_ExcludeTokens;
		StringTokenizer<string> m_IPTokens;

		static HIconWrapper frameIcon;
		
		void on(DebugManagerListener::DebugEvent, const DebugTask& task) noexcept override;
};

#endif // IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION

#endif // __CDMDEBUGFRAME_H
