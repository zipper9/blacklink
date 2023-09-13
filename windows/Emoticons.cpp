#include <stdafx.h>

#ifdef BL_UI_FEATURE_EMOTICONS

#include "Emoticons.h"
#include "../client/File.h"
#include "../client/SimpleXML.h"
#include "../client/Util.h"
#include "../client/LogManager.h"
#include "../GdiOle/GDIImageOle.h"

EmoticonPackList emoticonPackList;

Emoticon::Emoticon() :
	imageGif(nullptr),
	imageBmp(nullptr),
	alias(nullptr),
	hidden(false),
	loadingError(0)
{
}

Emoticon::~Emoticon()
{
	if (imageGif) imageGif->Release();
	if (imageBmp) imageBmp->Release();
}

string Emoticon::getGifPath() const
{
	return Util::getEmoPacksPath() + fileGif;
}

string Emoticon::getBmpPath() const
{
	return Util::getEmoPacksPath() + fileBmp;
}

void Emoticon::setIcon(const tstring& text, const string& iconBmpFile, const string& iconGifFile) noexcept
{
	this->text = text;
	fileBmp = iconBmpFile;
	fileGif = iconGifFile;
}

CGDIImage* Emoticon::getImage(int flags, HWND callbackWnd, UINT callbackMsg)
{
	if (alias) return alias->getImage(flags, callbackWnd, callbackMsg);
	if (flags & FLAG_PREFER_GIF)
	{
		tryLoadGif(callbackWnd, callbackMsg);
		if (imageGif) return imageGif;
		if (!(flags & FLAG_NO_FALLBACK))
		{
			tryLoadBmp(callbackWnd, callbackMsg);
			if (imageBmp) return imageBmp;
		}
	}
	else
	{
		tryLoadBmp(callbackWnd, callbackMsg);
		if (imageBmp) return imageBmp;
		if (!(flags & FLAG_NO_FALLBACK))
		{
			tryLoadGif(callbackWnd, callbackMsg);
			if (imageGif) return imageGif;
		}
	}
	return nullptr;
}

void Emoticon::tryLoadGif(HWND callbackWnd, UINT callbackMsg)
{
	if (imageGif || fileGif.empty() || (loadingError & MASK_GIF)) return;
	const string path = getGifPath();
	imageGif = CGDIImage::createInstance(Text::toT(path).c_str());
	if (imageGif->isInitialized())
	{
		imageGif->setCallback(callbackWnd, callbackMsg);
		return;
	}
	LogManager::message("Can't load GIF image from " + path, false);
	imageGif->Release();
	imageGif = nullptr;
	loadingError |= MASK_GIF;
}

void Emoticon::tryLoadBmp(HWND callbackWnd, UINT callbackMsg)
{
	if (imageBmp || fileBmp.empty() || (loadingError & MASK_BMP)) return;
	const string path = getBmpPath();
	imageBmp = CGDIImage::createInstance(Text::toT(path).c_str());
	if (imageBmp->isInitialized())
	{
		imageBmp->setCallback(callbackWnd, callbackMsg);
		return;
	}
	LogManager::message("Can't load BMP image from " + path, false);
	imageBmp->Release();
	imageBmp = nullptr;
	loadingError |= MASK_BMP;
}

IOleObject* Emoticon::getImageObject(int flags, IOleClientSite* pOleClientSite, IStorage* pStorage, HWND hCallbackWnd, UINT callbackMsg, COLORREF clrBackground, const tstring& text)
{
	IOleObject* pObject = nullptr;
	CGDIImage* image = getImage(flags, hCallbackWnd, callbackMsg);
	if (!image) return nullptr;

	CComObject<CGDIImageOle>* pImageObject = nullptr;
	// Create view instance of single graphic object (m_pGifImage).
	// Instance will be automatically removed after releasing last pointer;
	CComObject<CGDIImageOle>::CreateInstance(&pImageObject);
	if (pImageObject)
	{
		if (!pImageObject->SetImage(image, clrBackground, hCallbackWnd, callbackMsg))
		{
			pImageObject->Release();
			return nullptr;
		}
		pImageObject->QueryInterface(IID_IOleObject, (void**)&pObject);
		if (pObject)
		{
			pImageObject->put_Text(const_cast<TCHAR*>(text.c_str()));
			pObject->SetClientSite(pOleClientSite);
			return pObject;
		}
	}
	return nullptr;
}

void Emoticon::checkDuplicate(Emoticon* prev) noexcept
{
	if (fileBmp == prev->fileBmp && fileGif == prev->fileGif)
		alias = prev->alias ? prev->alias : prev;
}

EmoticonPack::~EmoticonPack() noexcept
{
	cleanup();
}

void EmoticonPack::cleanup() noexcept
{
	for_each(emoticons.begin(), emoticons.end(), [](auto p) { delete p; });
	emoticons.clear();
	sortedEmoticons.clear();
}

bool EmoticonPack::loadXMLFile(const string& name) noexcept
{
	const string path = Util::getEmoPacksPath() + name + ".xml";
	uint64_t timestamp = File::getTimeStamp(path);
	if (!timestamp) return false;
	try
	{
		boost::unordered_set<string> nameSet;
		SimpleXML xml;
		xml.fromXML(File(path, File::READ, File::OPEN, false).read());
		emoticons.reserve(200);
		if (xml.findChild("Emoticons"))
		{
			xml.stepIn();
			string iconText;
			while (xml.findChild("Emoticon"))
			{
				iconText = xml.getChildAttrib("PasteText");
				if (iconText.empty())
					iconText = xml.getChildAttrib("Expression");
				boost::algorithm::trim(iconText);
				dcassert(!iconText.empty());
				if (!iconText.empty())
				{
					if (!nameSet.insert(iconText).second) continue;
					const string& iconBmpFile = xml.getChildAttrib("Bitmap");
					const string& iconGifFile = xml.getChildAttrib("Gif");
					Emoticon* emoticon = new Emoticon();
					emoticon->setIcon(Text::toT(iconText), iconBmpFile, iconGifFile);
					emoticon->setHidden(xml.getBoolChildAttrib("Hidden"));
					if (!emoticons.empty()) emoticon->checkDuplicate(emoticons.back());
					emoticons.push_back(emoticon);
				}
			}
			xml.stepOut();
		}
	}
	catch (const Exception& e)
	{
		dcdebug("EmoticonPack::loadXMLFile: %s\n", e.getError().c_str());
		return false;
	}
	this->name = name;
	this->timestamp = timestamp;
	initialize();
	return true;
}

static bool sortFunc(const Emoticon* l, const Emoticon* r) noexcept
{
	const tstring& lt = l->getText();
	const tstring& rt = r->getText();
	if (lt.length() != rt.length())
		return lt.length() > rt.length();
	return lt.compare(0, rt.length(), rt) > 0;
}

void EmoticonPack::initialize() noexcept
{
	sortedEmoticons = emoticons;
	std::sort(sortedEmoticons.begin(), sortedEmoticons.end(), sortFunc);

#if 0
	dcdebug("-=[Dumping emoticons]=-\n");
	for (const Emoticon* icon : sortedEmoticons)
	{
		string text = Text::fromT(icon->getText()) + " (bmp: " + icon->getBmpPath() + "   gif: " + icon->getGifPath() + ")";
		dcdebug("%s\n", text.c_str());
	}
#endif
}

void EmoticonPackList::clear() noexcept
{
	for (EmoticonPack* pack : packs) delete pack;
	packs.clear();
}

void EmoticonPackList::setConfig(const StringList& names) noexcept
{
	vector<EmoticonPack*> newPacks;
	for (const string& name : names)
	{
		const string path = Util::getEmoPacksPath() + name + ".xml";
		bool found = false;
		for (EmoticonPack* &oldPack : packs)
			if (oldPack && oldPack->getName() == name)
			{
				uint64_t timestamp = File::getTimeStamp(path);
				if (oldPack->getTimeStamp() == timestamp)
				{
					newPacks.push_back(oldPack);
					oldPack = nullptr;
					found = true;
				}
				break;
			}
		if (!found)
		{
			EmoticonPack* newPack = new EmoticonPack;
			if (newPack->loadXMLFile(name))
				newPacks.push_back(newPack);
			else
				delete newPack;
		}
	}
	for_each(packs.begin(), packs.end(), [](auto p) { delete p; });
	packs = std::move(newPacks);
}

#endif // BL_UI_FEATURE_EMOTICONS
