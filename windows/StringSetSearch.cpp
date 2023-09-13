#include <stdafx.h>
#include "StringSetSearch.h"
#include "../client/Text.h"

using std::vector;

bool StringSetSearch::Node::findChar(size_t& pos, TCHAR c) const
{
	Link sl;
	sl.c = c;
	auto i = std::lower_bound(links.cbegin(), links.cend(), sl, [](const Link &x, const Link &y) -> bool { return x.c < y.c; });
	pos = i - links.cbegin();
	return i != links.cend() && i->c == c;
}

StringSetSearch::Set::Set()
{
	root = nullptr;
}

bool StringSetSearch::Set::addString(const tstring& s, uint64_t data)
{
	struct CharItem
	{
		Node *node;
		size_t suffix;
	};
	if (!root)
	{
		root = new Node;
		root->hasData = false;
		root->data = 0;
		root->back = nullptr;
	}

	vector<CharItem> v;
	v.resize(s.length());
	v[0].suffix = 0;
	v[0].node = root;
	size_t k = 0;
	for (size_t i = 1; i < s.length(); i++)
	{
		while (k > 0 && s[i] != s[k]) k = v[k-1].suffix;
		if (s[i] == s[k]) k++;
		v[i].suffix = k;
	}

	Node* node = root;
	for (size_t i = 0; i < s.length(); i++)
	{
		Node* nextNode;
		size_t pos;
		if (node->findChar(pos, s[i]))
		{
			nextNode = node->links[pos].p;
		}
		else
		{
			nextNode = new Node;
			nextNode->hasData = false;
			nextNode->back = nullptr;
			node->links.emplace(node->links.begin() + pos, Link{s[i], nextNode});
		}
		v[i].node = node = nextNode;
	}

	if (node->hasData) return false; // duplicate string
	node->hasData = true;
	node->data = data;

	for (size_t i = 0; i < s.length(); i++)
	{
		node = v[i].node;
		if (!node->back)
			node->back = v[i].suffix ? v[v[i].suffix-1].node : root;
	}
	return true;
}

void StringSetSearch::Set::clearNode(Node* node)
{
	for (auto& l : node->links) clearNode(l.p);
	delete node;
}

void StringSetSearch::Set::clear()
{
	if (root)
	{
		clearNode(root);
		root = nullptr;
	}
}

bool StringSetSearch::SearchContext::search(const tstring& s, const Set& ss)
{
	if (!ss.root) return false;
	if (!current) current = ss.root;
	if (found)
	{
		current = current->back;
		found = nullptr;
	}
	while (pos < s.length())
	{
		TCHAR ch = s[pos];
		if (ignoreCase) ch = Text::asciiToLower(ch);
		size_t index;
		if (current->findChar(index, ch))
		{
			current = current->links[index].p;
			if (current->hasData) found = current;
			pos++;
			continue;
		}
		if (found) return true;
		if (current != ss.root)
		{
			current = current->back;
			continue;
		}
		pos++;
	}
	return found != nullptr;
}

StringSetSearch::SearchContext::SearchContext()
{
	pos = 0;
	current = nullptr;
	found = nullptr;
	ignoreCase = false;
}

void StringSetSearch::SearchContext::reset(const Set& ss, size_t newPos)
{
	pos = newPos;
	current = ss.root;
	found = nullptr;
}
