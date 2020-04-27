#ifndef PARAM_EXPANDER_H_
#define PARAM_EXPANDER_H_

#include "typedefs.h"
#include <time.h>

namespace Util
{
	class ParamExpander
	{
		public:
			virtual const string& expandBracket(const string& param) noexcept = 0;
			virtual const string& expandCharSequence(const string& str, string::size_type pos, string::size_type& usedChars) noexcept = 0;
	};

	class TimeParamExpander : public ParamExpander
	{
			static const size_t BUF_SIZE = 256;

			tm lt;
			time_t t;
			bool initialized;
#ifdef _WIN32
			wchar_t buf[BUF_SIZE];
#endif
			string result;

		public:
			TimeParamExpander(time_t t) : t(t), initialized(false) {}
			virtual const string& expandBracket(const string& param) noexcept;
			virtual const string& expandCharSequence(const string& str, string::size_type pos, string::size_type& usedChars) noexcept;

		private:
			bool initialize() noexcept;
			bool strftime(char c) noexcept;
	};

	class MapParamExpander : public TimeParamExpander
	{
			const StringMap& m;

		public:
			MapParamExpander(const StringMap& m, time_t t) : TimeParamExpander(t), m(m) {}
			virtual const string& expandBracket(const string& param) noexcept override;
	};

	string formatParams(const string& s, ParamExpander* ex, bool filter) noexcept;
	
	// Note: a value expanded from %[param] will not be further processed by strftime.
	// This is an intentional change from original formatParams.
	string formatParams(const string& s, const StringMap& params, bool filter, time_t t = time(nullptr)) noexcept;
}

#endif // PARAM_EXPANDER_H_
