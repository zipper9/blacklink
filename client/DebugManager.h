#ifndef __DEBUGMANAGER_H
#define __DEBUGMANAGER_H

#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION

#include "ClientManager.h"
#include "BaseUtil.h"

struct DebugTask
{
	enum Type
	{
		HUB_IN,
		HUB_OUT,
		CLIENT_IN,
		CLIENT_OUT,
		DETECTION,
		LAST
	};
	DebugTask() : type(LAST), time(0)   { }
	DebugTask(const string& message, Type type, const string& ipAndPort = Util::emptyString);
	static string format(const DebugTask& task);
	string message;
	string ipAndPort;
	time_t time;
	Type type;
};

class DebugManagerListener
{
	public:
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> DebugEvent;
		
		virtual void on(DebugEvent, const DebugTask&) noexcept { }
};

class DebugManager : public Singleton<DebugManager>, public Speaker<DebugManagerListener>
{
		friend class Singleton<DebugManager>;
		DebugManager() { }
		~DebugManager() { }

	public:
		void sendCommandMessage(const string& command, DebugTask::Type type, const string& ip) noexcept;
		void sendDetectionMessage(const string& mess) noexcept;
		static bool g_isCMDDebug;
};

static inline bool CMD_DEBUG_ENABLED()
{
	return DebugManager::g_isCMDDebug && !ClientManager::isBeforeShutdown();
}

static inline void COMMAND_DEBUG(const string& text, DebugTask::Type type, const string& ip)
{
	DebugManager::getInstance()->sendCommandMessage(text, type, ip);
}

static inline void DETECTION_DEBUG(const string& text)
{
	DebugManager::getInstance()->sendDetectionMessage(text);
}

#else

#define CMD_DEBUG_ENABLED() (0)

#define COMMAND_DEBUG(message, direction, ip)
#define DETECTION_DEBUG(message)

#endif // IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION

#endif
