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
			FLAG_PREFER_GIF      = 1,
			FLAG_NO_FALLBACK     = 2,
			FLAG_DARK_BACKGROUND = 4
		};

		enum
		{
			IMAGE_BMP,
			IMAGE_LIGHT_GIF,
			IMAGE_DARK_GIF,
			MAX_IMAGES
		};

		enum
		{
			OLE_IMAGE_MULTIPLE_VARIANTS = 1,
			OLE_IMAGE_DARK_VARIANT      = 2
		};

		Emoticon();
		~Emoticon();

		Emoticon(const Emoticon&) = delete;
		Emoticon& operator= (const Emoticon&) = delete;

		void setImagePath(int type, const string& path);
		const string& getImagePath(int type) const;
		void setText(const tstring& text);
		const tstring& getText() const { return text; }
		bool isDuplicate() const { return alias != nullptr; }
		void checkDuplicate(Emoticon* prev);
		bool isHidden() const { return (flags & FLAG_HIDDEN) != 0; }
		void setHidden(bool enable);
		void setId(int id) { this->id = id; }
		int getId() const { return id; }
		int getEffectiveId() const;
		bool isAlias() const { return alias != nullptr; }

		CGDIImage* getImage(int flags, int* oleFlags, HWND callbackWnd, UINT callbackMsg);
		IOleObject* getImageObject(int flags, IOleClientSite* pOleClientSite, IStorage* pStorage, HWND hCallbackWnd, UINT callbackMsg, COLORREF clrBackground, const tstring& text);
		int getOleImageFlags() const;

	protected:
		struct ImageInfo
		{
			string path;
			CGDIImage* obj;
		};

		enum
		{
			FLAG_HIDDEN = 1,
			ERROR_BIT = 4
		};

		tstring text;
		int flags;
		int id;

		Emoticon* alias;
		ImageInfo images[MAX_IMAGES];

	private:
		void tryLoadImage(int type, HWND callbackWnd, UINT callbackMsg);
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
		Emoticon* findById(int id) const;

	protected:
		std::vector<EmoticonPack*> packs;
		boost::unordered_map<int, Emoticon*> idMap;
};

extern EmoticonPackList emoticonPackList;

#endif // EMOTICONS_H_
