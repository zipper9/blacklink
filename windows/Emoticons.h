#ifndef EMOTICONS_H_
#define EMOTICONS_H_

#ifdef IRAINMAN_INCLUDE_SMILE

#include "ImageDataObject.h"
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
		
		void setIcon(const tstring& text, const string& iconBmpFile, const string& iconGifFile);
		const tstring& getText() const { return text; }
		string getBmpPath() const;
		string getGifPath() const;
		bool isDuplicate() const { return duplicate; }
		void checkDuplicate(const Emoticon* prev);
		bool isHidden() const { return hidden; }
		void setHidden(bool flag) { hidden = flag; }

		CGDIImage* getImage(int flags, HWND callbackWnd, UINT callbackMsg);
		IOleObject* getImageObject(int flags, IOleClientSite* pOleClientSite, IStorage* pStorage, HWND hCallbackWnd, UINT callbackMsg, COLORREF clrBackground);
		
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
		bool    duplicate;
		int     loadingError;

		CGDIImage *imageGif;
		CGDIImage *imageBmp;

	private:
		string unzipIcons(const string& fileName);
		void tryLoadGif(HWND callbackWnd, UINT callbackMsg);
		void tryLoadBmp(HWND callbackWnd, UINT callbackMsg);
};

// EmoticonPack

class EmoticonPack
{
	public:
		EmoticonPack() : useEmoticons(false), packSize(0) { }
		~EmoticonPack();

		EmoticonPack(const EmoticonPack&) = delete;
		EmoticonPack& operator= (const EmoticonPack&) = delete;

		bool loadXMLFile(const string& name);
		bool initialize();

		static bool reCreate();
		static void destroy();
		static EmoticonPack* current;

		GETSET(bool, useEmoticons, UseEmoticons);

		const vector<Emoticon*>& getEmoticonsArray() const { return emoticons; }
		const vector<Emoticon*>& getSortedEmoticons() const { return sortedEmoticons; }
		size_t getPackSize() const { return packSize; }

	protected:
		vector<Emoticon*> emoticons;
		vector<Emoticon*> sortedEmoticons;
		size_t packSize;

	private:
		boost::unordered_set<string> nameSet;
		void cleanup();
};

#endif // IRAINMAN_INCLUDE_SMILE

#endif // EMOTICONS_H_
