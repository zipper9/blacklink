#ifndef MESSAGE_EDIT_H_
#define MESSAGE_EDIT_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include "../client/typedefs.h"

class MessageEdit: public CWindowImpl<MessageEdit, CEdit>
{
	public:
		struct Callback
		{
			virtual bool processEnter() = 0;
			virtual void updateEditHeight() = 0;
			virtual bool handleKey(int key) = 0;
			virtual bool handleAutoComplete() = 0;
			virtual void clearAutoComplete() = 0;
			virtual void typingNotification() = 0;
		};

		void setCallback(Callback* cb) { callback = cb; }
		void saveCommand(const tstring& text);
		
		BEGIN_MSG_MAP(MessageEdit)
		MESSAGE_HANDLER(WM_CHAR, onChar)
		MESSAGE_HANDLER(WM_KEYDOWN, onKeyDown)
		END_MSG_MAP()

		LRESULT onChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	private:
		Callback* callback = nullptr;
		TStringList prevCommands;
		tstring currentCommand;
		size_t curCommandPosition = 0;

		bool insertHistoryLine(UINT chr);
};

#endif // MESSAGE_EDIT_H_
