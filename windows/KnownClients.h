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
}

#endif /* KNOWN_CLIENTS_H_ */
