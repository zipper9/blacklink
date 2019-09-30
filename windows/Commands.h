#ifndef COMMANDS_H
#define COMMANDS_H

#include "typedefs.h"

namespace Commands
{
	tstring help();
	tstring helpForCEdit();
	
	/**
	 * Check if this is a common /-command.
	 * @param cmd The whole text string, will be updated to contain only the command.
	 * @param param Set to any parameters.
	 * @param message Message that should be sent to the chat.
	 * @param status Message that should be shown in the status line.
	 * @return True if the command was processed, false otherwise.
	 */
	bool processCommand(tstring& cmd, tstring& param, tstring& message, tstring& status, tstring& localMessage);
}

#endif /* COMMANDS_H */
