
#ifndef __DEBUGMANAGER_H
#define __DEBUGMANAGER_H

#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION

#include "TimerManager.h"
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
	DebugTask() : m_type(LAST), m_time(0)   { }
	DebugTask(const string& message, Type type, const string& p_ip_and_port = Util::emptyString);
	static string format(const DebugTask& task);
	string m_message;
	string m_ip_and_port;
	time_t m_time;
	Type m_type;
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
		void SendCommandMessage(const string& command, DebugTask::Type type, const string& ip) noexcept;
		void SendDetectionMessage(const string& mess) noexcept;
		static bool g_isCMDDebug;
};

static inline bool CMD_DEBUG_ENABLED()
{
	return DebugManager::g_isCMDDebug && !ClientManager::isBeforeShutdown();
}

static inline void COMMAND_DEBUG(const string& text, DebugTask::Type type, const string& ip)
{
	DebugManager::getInstance()->SendCommandMessage(text, type, ip);
}

static inline void DETECTION_DEBUG(const string& text)
{
	DebugManager::getInstance()->SendDetectionMessage(text);
}

#else

#define CMD_DEBUG_ENABLED() (0)

#define COMMAND_DEBUG(message, direction, ip)
#define DETECTION_DEBUG(message)

#endif // IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION

#endif
