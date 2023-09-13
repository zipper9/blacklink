#ifndef EMOTICONS_H_
#define EMOTICONS_H_

#include <boost/unordered/unordered_set.hpp>
#include "../client/BaseUtil.h"

class CGDIImage;

// Emoticon

class Emoticon
{
	public:
		enum
		{
			FLAG_PREFER_GIF  = 1,
			FLAG_NO_FALLBACK = 2
		};

		Emoticon();
		~Emoticon();

		Emoticon(const Emoticon&) = delete;
		Emoticon& operator= (const Emoticon&) = delete;

		void setIcon(const tstring& text, const string& iconBmpFile, const string& iconGifFile) noexcept;
		const tstring& getText() const { return text; }
		string getBmpPath() const;
		string getGifPath() const;
		bool isDuplicate() const { return alias != nullptr; }
		void checkDuplicate(Emoticon* prev) noexcept;
		bool isHidden() const { return hidden; }
		void setHidden(bool flag) { hidden = flag; }

		CGDIImage* getImage(int flags, HWND callbackWnd, UINT callbackMsg);
		IOleObject* getImageObject(int flags, IOleClientSite* pOleClientSite, IStorage* pStorage, HWND hCallbackWnd, UINT callbackMsg, COLORREF clrBackground, const tstring& text);

	protected:
		enum
		{
			MASK_BMP = 1,
			MASK_GIF = 2
		};

		tstring text;
		string  fileBmp;
		string  fileGif;
		bool    hidden;
		int     loadingError;

		Emoticon *alias;
		CGDIImage *imageGif;
		CGDIImage *imageBmp;

	private:
		void tryLoadGif(HWND callbackWnd, UINT callbackMsg);
		void tryLoadBmp(HWND callbackWnd, UINT callbackMsg);
};

// EmoticonPack

class EmoticonPack
{
	public:
		EmoticonPack() noexcept : timestamp(0) {}
		~EmoticonPack() noexcept;

		EmoticonPack(const EmoticonPack&) = delete;
		EmoticonPack& operator= (const EmoticonPack&) = delete;

		bool loadXMLFile(const string& name) noexcept;
		const std::vector<Emoticon*>& getEmoticons() const { return emoticons; }
		const std::vector<Emoticon*>& getSortedEmoticons() const { return sortedEmoticons; }
		const string& getName() const { return name; }
		uint64_t getTimeStamp() const { return timestamp; }

	protected:
		std::vector<Emoticon*> emoticons;
		std::vector<Emoticon*> sortedEmoticons;
		string name;
		uint64_t timestamp;

	private:
		void cleanup() noexcept;
		void initialize() noexcept;
};

// EmoticonPackList

class EmoticonPackList
{
	public:
		EmoticonPackList() noexcept  {}
		~EmoticonPackList() noexcept { clear(); }

		EmoticonPackList(const EmoticonPackList&) = delete;
		EmoticonPackList& operator= (const EmoticonPackList&) = delete;

		void setConfig(const StringList& names) noexcept;
		void clear() noexcept;
		const std::vector<EmoticonPack*>& getPacks() const { return packs; }

	protected:
		std::vector<EmoticonPack*> packs;
};

extern EmoticonPackList emoticonPackList;

#endif // EMOTICONS_H_
