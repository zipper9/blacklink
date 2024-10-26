#include "stdinc.h"
#include "DebugManager.h"
#include "TimeUtil.h"
#include "Util.h"

bool DebugManager::g_isCMDDebug = false;

void DebugManager::sendCommandMessage(const string& command, DebugTask::Type type, const string& ip) noexcept
{
	fire(DebugManagerListener::DebugEvent(), DebugTask(command, type, ip));
}

void DebugManager::sendDetectionMessage(const string& mess) noexcept
{
	fire(DebugManagerListener::DebugEvent(), DebugTask(mess, DebugTask::DETECTION));
}

DebugTask::DebugTask(const string& message, Type type, const string& ipAndPort /*= Util::emptyString */) :
	message(message), ipAndPort(ipAndPort), time(GET_TIME()), type(type)
{
}

string DebugTask::format(const DebugTask& task)
{
	string out = Util::getShortTimeString(task.time) + ' ';
	switch (task.type)
	{
		case DebugTask::HUB_IN:
			out += "Hub:\t[Incoming][" + task.ipAndPort + "]\t \t";
			break;
		case DebugTask::HUB_OUT:
			out += "Hub:\t[Outgoing][" + task.ipAndPort + "]\t \t";
			break;
		case DebugTask::CLIENT_IN:
			out += "Client:\t[Incoming][" + task.ipAndPort + "]\t \t";
			break;
		case DebugTask::CLIENT_OUT:
			out += "Client:\t[Outgoing][" + task.ipAndPort + "]\t \t";
			break;
#ifdef _DEBUG
		case DebugTask::DETECTION:
			break;
		default:
			dcassert(0);
#endif
	}
	// FIXME
	out += !Text::validateUtf8(task.message) ? Text::toUtf8(task.message) : task.message;
	return out;
}

