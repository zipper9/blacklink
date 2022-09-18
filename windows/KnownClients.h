#ifndef KNOWN_CLIENTS_H_
#define KNOWN_CLIENTS_H_

namespace KnownClients
{

	struct Client
	{
		const char* name;
		const char* version;
	};

	extern const Client clients[];
	extern const char* userAgents[];
}

#endif /* KNOWN_CLIENTS_H_ */
