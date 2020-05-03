#ifndef IP_LIST_H_
#define IP_LIST_H_

#include <stdint.h>
#include <string>
#include <map>
#include <list>

class IpList
{
	private:
		struct Range
		{
			uint32_t end;
			uint64_t payload;
		};
		typedef std::list<Range> RangeList;
		typedef std::map<uint32_t, RangeList> IpListMap;
		IpListMap m;

	public:
		enum
		{
			ERR_LINE_SKIPPED = 1,
			ERR_BAD_RANGE,
			ERR_ALREADY_EXISTS,
			ERR_BAD_FORMAT,
			ERR_BAD_NETMASK
		};
		
		struct ParseLineOptions
		{
			char specialChars[8];
			int  specialCharCount;
		};
		
		struct ParseLineResult
		{
			uint32_t start;
			uint32_t end;
			std::string::size_type pos;
			char specialChar;
		};

		bool addRange(uint32_t start, uint32_t end, uint64_t payload, int& error);
		bool find(uint32_t addr, uint64_t& payload) const;
		void clear() { m.clear(); }

		static int parseLine(const std::string& s, ParseLineResult& res, const ParseLineOptions* options = nullptr, string::size_type startPos = 0);
		static std::string getErrorText(int error);
};

#endif // IP_LIST_H_
