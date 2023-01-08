#ifndef PREVIEW_APPLICATION_H_
#define PREVIEW_APPLICATION_H_

#include "Text.h"

class PreviewApplication
{
	public:
		typedef vector<PreviewApplication*> List;

		PreviewApplication() noexcept {}
		PreviewApplication(const string& n, const string& a, const string& r, const string& e) noexcept : name(n), application(a), arguments(r), extension(Text::toLower(e))
		{
		}

		PreviewApplication(const PreviewApplication &) = delete;
		PreviewApplication& operator= (const PreviewApplication &) = delete;

		string name;
		string application;
		string arguments;
		string extension;

		static void clearList(List& l) noexcept
		{
			for (PreviewApplication* item : l) delete item;
			l.clear();
		}
};

#endif
