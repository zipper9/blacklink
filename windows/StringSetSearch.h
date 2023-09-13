#ifndef STRING_SET_SEARCH_H_
#define STRING_SET_SEARCH_H_

#include "../client/typedefs.h"
#include "../client/w.h"

namespace StringSetSearch
{

	struct Node;
	class SearchContext;

	struct Link
	{
		TCHAR c;
		Node* p;
	};

	struct Node
	{
		bool hasData;
		uint64_t data;
		std::vector<Link> links;
		Node* back;

		bool findChar(size_t& pos, TCHAR c) const;
	};

	class Set
	{
		friend class SearchContext;

		public:
			Set();
			~Set() { clear(); }
			void clear();
			bool addString(const tstring& s, uint64_t data);

		private:
			Node* root;
			void clearNode(Node *node);
	};

	class SearchContext
	{
		friend class Set;

		public:
			SearchContext();
			size_t getCurrentPos() const { return pos; }
			uint64_t getCurrentData() const { return current ? current->data : 0; }
			bool search(const tstring& s, const Set& ss);
			void reset(const Set& ss, size_t newPos = 0);
			void setIgnoreCase(bool flag) { ignoreCase = flag; }

		private:
			size_t pos;
			const Node* current;
			const Node* found;
			bool ignoreCase;
	};

}

#endif // STRING_SET_SEARCH_H_
