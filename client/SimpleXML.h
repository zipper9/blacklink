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

#ifndef DCPLUSPLUS_DCPP_SIMPLE_XML_H
#define DCPLUSPLUS_DCPP_SIMPLE_XML_H

#include "Streams.h"
#include "SimpleXMLReader.h"
#include "SimpleXMLException.h"
#include "StrUtil.h"
#include "BaseUtil.h"
#include "Text.h"

/**
 * A simple XML class that loads an XML-ish structure into an internal tree
 * and allows easy access to each element through a "current location".
 */
class SimpleXML
{
	public:
		SimpleXML() : root("BOGUSROOT", Util::emptyString, nullptr), current(&root), found(false)
		{
			resetCurrentChild();
		}

		void addTag(const string& name, const string& data = Util::emptyString);
		void addTag(const string& name, int data) { addTag(name, Util::toString(data)); }
		void addTag(const string& name, int64_t data) { addTag(name, Util::toString(data)); }

		template<typename T>
		void addAttrib(const string& name, const T& data) { addAttrib(name, Util::toString(data)); }

		void addAttrib(const string& name, const string& data);

		void addAttrib(const string& name, bool data) { addAttrib(name, string(data ? "1" : "0")); }

		template <typename T>
		void addChildAttrib(const string& name, const T& data)
		{
			addChildAttrib(name, Util::toString(data));
		}

		void addChildAttrib(const string& name, const string& data);

		void addChildAttribIfNotEmpty(const string& name, const string& data)
		{
			if (!data.empty()) addChildAttrib(name, data);
		}

		void addChildAttrib(const string& name, bool data) { addChildAttrib(name, string(data ? "1" : "0")); }

		const string& getData() const
		{
			dcassert(current);
			return current->data;
		}

		void stepIn();

		void stepOut();
		void resetCurrentChild();

		bool findChild(const string& name) noexcept;
		bool getNextChild() noexcept;

		const string& getChildTag() const
		{
			checkChildSelected();
			return (*currentChild)->name;
		}

		const string& getChildData() const
		{
			checkChildSelected();
			return (*currentChild)->data;
		}

		const string& getChildAttrib(const string& name, const string& defValue = Util::emptyString) const
		{
			checkChildSelected();
			return (*currentChild)->getAttrib(name, defValue);
		}

		int getIntChildAttrib(const string& name,  const string& defValue) const
		{
			checkChildSelected();
			return Util::toInt(getChildAttrib(name, defValue));
		}

		int getIntChildAttrib(const string& name) const
		{
			checkChildSelected();
			return Util::toInt(getChildAttrib(name));
		}

		int64_t getInt64ChildAttrib(const string& name, const string& defValue) const
		{
			checkChildSelected();
			return Util::toInt64(getChildAttrib(name, defValue));
		}

		int64_t getInt64ChildAttrib(const string& name) const
		{
			checkChildSelected();
			return Util::toInt64(getChildAttrib(name));
		}

		bool getBoolChildAttrib(const string& name) const
		{
			checkChildSelected();
			const string& tmp = getChildAttrib(name);
			return !tmp.empty() && tmp[0] == '1';
		}

		void fromXML(const string& xml);

		string toXML() const
		{
			string tmp;
			StringOutputStream os(tmp);
			toXML(&os);
			return tmp;
		}

		void toXML(OutputStream* f) const
		{
			if (!root.children.empty())
				root.children[0]->toXML(0, f);
		}

		static const string& escapeAttrib(const string& str, string& tmp)
		{
			if (needsEscapeAttrib(str))
			{
				tmp = str;
				return escape(tmp, true, false);
			}
			return str;
		}

		static const string& escape(const string& str, string& tmp, bool isAttrib, bool isLoading = false, int encoding = Text::CHARSET_UTF8)
		{
			if (needsEscape(str, isAttrib, isLoading, encoding))
			{
				tmp = str;
				return escape(tmp, isAttrib, isLoading, encoding);
			}
			return str;
		}

		static string& escape(string& str, bool isAttrib, bool isLoading = false, int encoding = Text::CHARSET_UTF8);

		/**
		 * This is a heuristic for whether escape needs to be called or not. The results are
		 * only guaranteed for false, i e sometimes true might be returned even though escape
		 * was not needed...
		 */
		inline static bool needsEscape(const string& str, bool isAttrib, bool isLoading, int encoding = Text::CHARSET_UTF8)
		{
			return encoding != Text::CHARSET_UTF8 ||
				((isLoading ? str.find('&') : str.find_first_of(isAttrib ? "<&>'\"" : "<&>")) != string::npos);
		}

		inline static bool needsEscapeAttrib(const string& str)
		{
			return str.find_first_of("<&>'\"") != string::npos;
		}

		static const string utf8Header;

		SimpleXML(const SimpleXML&) = delete;
		SimpleXML& operator= (const SimpleXML&) = delete;

	private:
		class Tag
		{
			public:
				typedef Tag* Ptr;
				typedef std::vector<Ptr> List;
				typedef List::const_iterator Iter;

				/**
				 * A simple list of children. To find a tag, one must search the entire list.
				 */
				List children;
				/**
				 * Attributes of this tag. According to the XML standard the names
				 * must be unique (case-sensitive). (Assuming that we have few attributes here,
				 * we use a vector instead of a (hash)map to save a few bytes of memory and unnecessary
				 * calls to the memory allocator...)
				 */
				StringPairList attribs;

				/** Tag name */
				string name;

				/** Tag data, may be empty. */
				string data;

				/** Parent tag, for easy traversal */
				Ptr parent;

				Tag(const string& name, const StringPairList& a, Ptr parent) : attribs(a), name(name), data(), parent(parent)
				{
				}

				Tag(const string& name, const string& d, Ptr parent) : name(name), data(d), parent(parent)
				{
				}

				const string& getAttrib(const string& name, const string& defValue = Util::emptyString) const
				{
					StringPairList::const_iterator i = find_if(attribs.begin(), attribs.end(), [&](const auto& p) { return p.first == name; });
					return i == attribs.end() ? defValue : i->second;
				}

				void toXML(int indent, OutputStream* f) const;

				void appendAttribString(string& tmp) const;

				/** Delete all children! */
				~Tag()
				{
					for (auto i = children.cbegin(); i != children.cend(); ++i)
						delete *i;
				}

				Tag(const Tag&) = delete;
				Tag& operator= (const Tag&) = delete;
		};

		class TagReader : public SimpleXMLReader::CallBack
		{
			public:
				explicit TagReader(Tag* root) : cur(root) { }
				virtual bool getData(string&) const
				{
					return false;
				}
				virtual void startTag(const string& name, StringPairList& attribs, bool simple)
				{
					cur->children.push_back(new Tag(name, attribs, cur));
					if (!simple)
						cur = cur->children.back();
				}
				virtual void endTag(const string&, const string& d)
				{
					cur->data = d;
					if (cur->parent == nullptr)
						throw SimpleXMLException("Invalid end tag");
					cur = cur->parent;
				}

				Tag* cur;
		};

		/** Bogus root tag, should have only one child! */
		Tag root;

		/** Current position */
		Tag::Ptr current;

		Tag::Iter currentChild;

		void checkChildSelected() const noexcept
		{
			dcassert(current);
			if (!current)
				return;
			dcassert(currentChild != current->children.end());
		}

		bool found;
};

#endif // DCPLUSPLUS_DCPP_SIMPLE_XML_H
