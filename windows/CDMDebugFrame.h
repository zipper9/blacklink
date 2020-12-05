#ifndef __CDMDEBUGFRAME_H
#define __CDMDEBUGFRAME_H

#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION

#include "../client/DebugManager.h"
#include "../client/WinEvent.h"
#include "../client/StringTokenizer.h"
#include "FlatTabCtrl.h"
#include "WinUtil.h"
#include <atomic>

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
	protected Thread,
	private DebugManagerListener
{
	public:
		static CFrameWndClassInfo& GetWndClassInfo();
		
		CDMDebugFrame() : showCommands(true), showHubCommands(false), showDetection(false), enableFilterIp(false),
			detectionContainer(WC_BUTTON, this, DETECTION_MESSAGE_MAP),
			hubCommandContainer(WC_BUTTON, this, HUB_COMMAND_MESSAGE_MAP),
			commandContainer(WC_BUTTON, this, COMMAND_MESSAGE_MAP),
			cFilterContainer(WC_BUTTON, this, DEBUG_FILTER_MESSAGE_MAP),
			eFilterContainer(WC_EDIT, this, DEBUG_FILTER_IP_MESSAGE_MAP),
			clearContainer(WC_BUTTON, this, CLEAR_MESSAGE_MAP),
			statusContainer(STATUSCLASSNAME, this, CLEAR_MESSAGE_MAP),
			includeFilterContainer(WC_EDIT, this, DEBUG_FILTER_INCLUDE_TEXT_MESSAGE_MAP),
			excludeFilterContainer(WC_EDIT, this, DEBUG_FILTER_EXCLUDE_TEXT_MESSAGE_MAP),
			filterDirection(DIRECTION_BOTH), stopFlag(false)
		{
			event.create();
		}
		
		typedef MDITabChildWindowImpl<CDMDebugFrame> baseClass;
		BEGIN_MSG_MAP(CDMDebugFrame)
		MESSAGE_HANDLER(WM_SETFOCUS, OnFocus)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
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
			showDetection = wParam == BST_CHECKED;
			bHandled = FALSE;
			return 0;
		}
		LRESULT onSetCheckCommand(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			showCommands = wParam == BST_CHECKED;
			bHandled = FALSE;
			return 0;
		}
		LRESULT onSetCheckHubCommand(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			showHubCommands = wParam == BST_CHECKED;
			bHandled = FALSE;
			return 0;
		}
		LRESULT onSetCheckFilter(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			enableFilterIp = wParam == BST_CHECKED;
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
		
		std::atomic_bool stopFlag;
		WinEvent<FALSE> event;
		deque<DebugTask> cmdList;
		FastCriticalSection cs;
		
		int run();
		
		void addCmd(const DebugTask& task);
		void stopThread();
		void clearCmd();
		void moveCheckBox(int index, CButton& ctrl);
		
		CEdit ctrlCMDPad;
		CEdit ctrlIPFilter;
		CStatusBarCtrl ctrlStatus;
		CComboBox ctrlDirection;
		CButton ctrlClear, ctrlCommands, ctrlHubCommands, ctrlDetection, ctrlFilterIp;		
		CEdit ctrlIncludeFilter;
		CEdit ctrlExcludeFilter;
		CContainedWindow clearContainer, statusContainer, detectionContainer, commandContainer, hubCommandContainer, cFilterContainer, eFilterContainer;
		CContainedWindow includeFilterContainer;
		CContainedWindow excludeFilterContainer;
		
		int filterDirection;
		bool showCommands, showHubCommands, showDetection, enableFilterIp;
		string filterIp;
		string filterInclude;
		string filterExclude;
		StringTokenizer<string> includeTokens;
		StringTokenizer<string> excludeTokens;
		StringTokenizer<string> ipTokens;

		void on(DebugManagerListener::DebugEvent, const DebugTask& task) noexcept override;
};

#endif // IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION

#endif // __CDMDEBUGFRAME_H
