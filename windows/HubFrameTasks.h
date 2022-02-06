#ifndef HUB_FRAME_TASKS_H_
#define HUB_FRAME_TASKS_H_

#include "../client/TaskQueue.h"
#include "../client/ChatMessage.h"

enum Tasks
{
	ADD_STATUS_LINE,
	ADD_CHAT_LINE,
	REMOVE_USER,
	REMOVE_USERS,
	UPDATE_USER_JOIN,
	UPDATE_USER,
	UPDATE_USERS,
	PRIVATE_MESSAGE,
	GET_PASSWORD,
	DISCONNECTED,
	CONNECTED,
	CHEATING_USER,
	USER_REPORT,
	LOAD_IP_INFO,
	SETTINGS_LOADED
};

class OnlineUserTask : public Task
{
	public:
		explicit OnlineUserTask(const OnlineUserPtr& ou) : ou(ou) {}
		const OnlineUserPtr ou;
};

class OnlineUsersTask : public Task
{
	public:
		explicit OnlineUsersTask(const vector<OnlineUserPtr>& userList) : userList(userList) {}
		vector<OnlineUserPtr> userList;
};

class MessageTask : public Task
{
	public:
		explicit MessageTask(const ChatMessage* messagePtr) : messagePtr(messagePtr) {}
		virtual ~MessageTask() { delete messagePtr; }
		const ChatMessage* getMessage() const { return messagePtr; }

	private:
		const ChatMessage* const messagePtr;
};

#endif // HUB_FRAME_TASKS_H_
