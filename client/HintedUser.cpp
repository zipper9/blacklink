#include "stdinc.h"
#include "HintedUser.h"
#include "ClientManager.h"

string HintedUser::toString() const
{
	string s = getNick();
	if (!hint.empty())
	{
		s += " - ";
		s += hint;
	}
	return s;
}

string HintedUser::getNick() const
{
	if (user) return ClientManager::getNick(user, hint);
	return Util::emptyString;
}

string HintedUser::getNickAndHub() const
{
	string s = getNick();
	if (!hint.empty())
	{
		s += " - ";
		string hubName = ClientManager::getOnlineHubName(hint);
		s += hubName.empty() ? hint : hubName;
	}
	return s;
}

