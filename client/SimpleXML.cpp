/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdinc.h"
#include "SimpleXML.h"

#ifdef NO_RESOURCE_MANAGER
enum
{
	SXML_EMPTY_TAG_NAME = 1,
	SXML_INVALID_FILE,
	SXML_NO_TAG_SELECTED,
	SXML_ONLY_ONE_ROOT,
	SXML_ALREADY_LOWEST
};

static const char* STRING(int error)
{
	switch (error)
	{
		case SXML_EMPTY_TAG_NAME:
			return "Empty tag names not allowed";
		case SXML_INVALID_FILE:
			return "Invalid XML file, missing or multiple root tags";
		case SXML_NO_TAG_SELECTED:
			return "No tag is currently selected";
		case SXML_ONLY_ONE_ROOT:
			return "Only one root tag allowed";
		case SXML_ALREADY_LOWEST:
			return "Already at lowest level";
	}
	return "Unknown error";
}
#else
#include "ResourceManager.h"
#endif

const string SimpleXML::utf8Header = "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>\r\n";

string& SimpleXML::escape(string& str, bool isAttrib, bool isLoading, int encoding)
{
	string::size_type i = 0;
	const char* chars = isAttrib ? "<&>'\"" : "<&>";

	if (isLoading)
	{
		while ((i = str.find('&', i)) != string::npos)
		{
			if (str.compare(i + 1, 3, "lt;", 3) == 0)
				str.replace(i, 4, 1, '<');
			else if (str.compare(i + 1, 4, "amp;", 4) == 0)
				str.replace(i, 5, 1, '&');
			else if (str.compare(i + 1, 3, "gt;", 3) == 0)
				str.replace(i, 4, 1, '>');
			else if (isAttrib)
			{
				if (str.compare(i + 1, 5, "apos;", 5) == 0)
					str.replace(i, 6, 1, '\'');
				else if (str.compare(i + 1, 5, "quot;", 5) == 0)
					str.replace(i, 6, 1, '"');
			}
			i++;
		}
		if (encoding != Text::CHARSET_UTF8)
			str = Text::toUtf8(str, encoding);
	}
	else
	{
		while ((i = str.find_first_of(chars, i)) != string::npos)
		{
			switch (str[i])
			{
				case '<':
					str.replace(i, 1, "&lt;");
					i += 4;
					break;
				case '&':
					str.replace(i, 1, "&amp;");
					i += 5;
					break;
				case '>':
					str.replace(i, 1, "&gt;");
					i += 4;
					break;
				case '\'':
					str.replace(i, 1, "&apos;");
					i += 6;
					break;
				case '"':
					str.replace(i, 1, "&quot;");
					i += 6;
					break;
				default:
					dcassert(0);
			}
		}
		// No need to convert back to acp since our utf8Header denotes we
		// should store it as utf8.
	}
	return str;
}

void SimpleXML::Tag::appendAttribString(string& tmp) const
{
	for (auto i = attribs.cbegin(); i != attribs.cend(); ++i)
	{
		tmp.append(i->first);
		tmp.append("=\"", 2);
		if (needsEscape(i->second, true, false))
		{
			string tmp2(i->second);
			escape(tmp2, true, false);
			tmp.append(tmp2);
		}
		else
			tmp.append(i->second);
		tmp.append("\" ", 2);
	}
	tmp.erase(tmp.size() - 1);
}

void SimpleXML::Tag::toXML(int indent, OutputStream* f) const
{
	if (children.empty() && data.empty())
	{
		string tmp;
		tmp.reserve(indent + name.length() + 30);
		tmp.append(indent, '\t');
		tmp.append(1, '<');
		tmp.append(name);
		tmp.append(1, ' ');
		appendAttribString(tmp);
		tmp.append("/>\r\n", 4);
		f->write(tmp);
	}
	else
	{
		string tmp;
		tmp.append(indent, '\t');
		tmp.append(1, '<');
		tmp.append(name);
		tmp.append(1, ' ');
		appendAttribString(tmp);
		if (children.empty())
		{
			tmp.append(1, '>');
			if (needsEscape(data, false, false))
			{
				string tmp2(data);
				escape(tmp2, false, false);
				tmp.append(tmp2);
			}
			else
				tmp.append(data);
		}
		else
		{
			tmp.append(">\r\n", 3);
			f->write(tmp);
			tmp.clear();
			for (auto i = children.cbegin(); i != children.cend(); ++i)
				(*i)->toXML(indent + 1, f);
			tmp.append(indent, '\t');
		}
		tmp.append("</", 2);
		tmp.append(name);
		tmp.append(">\r\n", 3);
		f->write(tmp);
	}
}

bool SimpleXML::findChild(const string& name) noexcept
{
	dcassert(current != nullptr);
	if (!current)
		return false;

	if (found && currentChild != current->children.end())
		++currentChild;

	while (currentChild != current->children.end())
	{
		if ((*currentChild)->name == name)
		{
			found = true;
			return true;
		}
		++currentChild;
	}
	return false;
}

bool SimpleXML::getNextChild() noexcept
{
	dcassert(current != nullptr);
	if (!current)
		return false;

	if (found && currentChild != current->children.end())
		++currentChild;

	found = currentChild != current->children.end();
	return found;
}

void SimpleXML::addTag(const string& name, const string& data /* = "" */)
{
	if (name.empty())
		throw SimpleXMLException(STRING(SXML_EMPTY_TAG_NAME));

	if (current == &root && !current->children.empty())
		throw SimpleXMLException(STRING(SXML_ONLY_ONE_ROOT));

	current->children.push_back(new Tag(name, data, current));
	currentChild = current->children.end() - 1;
}

void SimpleXML::addAttrib(const string& name, const string& data)
{
	if (current == &root)
		throw SimpleXMLException(STRING(SXML_NO_TAG_SELECTED));

	current->attribs.push_back(make_pair(name, data));
}

void SimpleXML::addChildAttrib(const string& name, const string& data)
{
	checkChildSelected();
	(*currentChild)->attribs.push_back(make_pair(name, data));
}

void SimpleXML::fromXML(const string& xml)
{
	if (!root.children.empty())
	{
		delete root.children[0];
		root.children.clear();
	}

	TagReader t(&root);
	SimpleXMLReader(&t).parse(xml.c_str(), xml.size(), false);

	if (root.children.size() != 1)
		throw SimpleXMLException(STRING(SXML_INVALID_FILE));

	current = &root;
	resetCurrentChild();
}

void SimpleXML::stepIn()
{
	checkChildSelected();
	current = *currentChild;
	currentChild = current->children.begin();
	found = false;
}

void SimpleXML::stepOut()
{
	if (current == &root)
		throw SimpleXMLException(STRING(SXML_ALREADY_LOWEST));

	dcassert(current && current->parent);
	if (!(current && current->parent))
		return;
	currentChild = find(current->parent->children.begin(), current->parent->children.end(), current);

	current = current->parent;
	found = true;
}

void SimpleXML::resetCurrentChild()
{
	found = false;
	dcassert(current != nullptr);
	if (!current)
		return;

	currentChild = current->children.begin();
}
