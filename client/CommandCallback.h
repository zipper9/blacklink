#ifndef COMMAND_CALLBACK_H_
#define COMMAND_CALLBACK_H_

#include "typedefs.h"
#include "Severity.h"

class CommandCallback
{
public:
	virtual void onCommandCompleted(int type, uint64_t frameId, const string& text) noexcept = 0;
};

#endif // COMMAND_CALLBACK_H_
