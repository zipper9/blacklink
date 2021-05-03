#include "stdafx.h"

#ifdef IRAINMAN_INCLUDE_SMILE
#include "../client/Util.h"
#include "../client/SimpleXML.h"
#include "../client/LogManager.h"
#include "../client/SettingsManager.h"
#include "../GdiOle/GDIImageOle.h"
#include "Emoticons.h"
#include <boost/algorithm/string/replace.hpp>

#ifdef FLYLINKDC_USE_ZIP_EMOTIONS
#include "../client/zip/zip.h"
#endif

Emoticon::Emoticon() :	
	imageGif(nullptr),
	imageBmp(nullptr),
	duplicate(false),
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

string Emoticon::unzipIcons(const string& fileName)
{
#ifdef FLYLINKDC_USE_ZIP_EMOTIONS
	//struct buffer_t {
	//  char *data;
	//  size_t size;
	//};
	string l_result;
	CFlyLog l_log("[Unzip EmoPacks.zip]" + fileName);
	//buffer_t buf = {0};
	const string l_path = Util::getEmoPacksPath() + "EmoPacks.zip";
	const string l_tmp_path = Util::getTempPath(); // +"FlylinkDC++EmoPacks";
	//File::ensureDirectory(l_tmp_path); // TODO first
	auto l_copy_path = m_EmotionBmp;
	boost::replace_all(l_copy_path, "\\", "-");
	const string l_path_target = l_tmp_path + l_copy_path;
	zip_t *zip = zip_open(l_path.c_str(), 0, 'r');
	if (zip)
	{
		auto l_copy = m_EmotionBmp;
		boost::replace_all(l_copy, "\\", "/");
		if (!zip_entry_open(zip, l_copy.c_str()))
		{
			if (!zip_entry_fread(zip, l_path_target.c_str()))
			{
				l_result = l_path_target;
			}
			zip_entry_close(zip);
		}
		zip_close(zip);
	}
	
	//assert(0 == zip_entry_extract(zip, on_extract, &buf));
	
//		free(buf.data);
//		buf.data = NULL;
//		buf.size = 0;
	return l_result;
#else
	return fileName;
#endif // FLYLINKDC_USE_ZIP_EMOTIONS
}

void Emoticon::setIcon(const tstring& text, const string& iconBmpFile, const string& iconGifFile)
{
	this->text = text;
	fileBmp = iconBmpFile;
	fileGif = iconGifFile;
}

CGDIImage* Emoticon::getImage(int flags, HWND callbackWnd, UINT callbackMsg)
{
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
	const string path = unzipIcons(getGifPath());
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
	const string path = unzipIcons(getBmpPath());
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

IOleObject* Emoticon::getImageObject(int flags, IOleClientSite* pOleClientSite, IStorage* pStorage, HWND hCallbackWnd, UINT callbackMsg, COLORREF clrBackground)
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
		if (pImageObject->put_SetImage(image, clrBackground, hCallbackWnd, callbackMsg) != S_OK)
		{
			pImageObject->Release();
			return nullptr;
		}
		pImageObject->QueryInterface(IID_IOleObject, (void**)&pObject);
		if (pObject)
		{
			pObject->SetClientSite(pOleClientSite);
			return pObject;
		}
	}
	return nullptr;
}

void Emoticon::checkDuplicate(const Emoticon* prev)
{
	if (fileBmp == prev->fileBmp && fileGif == prev->fileGif)
		duplicate = true;
}

EmoticonPack* EmoticonPack::current = nullptr;

void EmoticonPack::destroy()
{
	delete current;
	current = nullptr;
}

bool EmoticonPack::reCreate()
{
	destroy();
	current = new EmoticonPack;
	if (!current->initialize())
	{
		delete current;
		current = nullptr;
		SET_SETTING(EMOTICONS_FILE, "Disabled");
		dcassert(FALSE);
		return false;
	}
	return true;
}

EmoticonPack::~EmoticonPack()
{
	cleanup();
}

void EmoticonPack::cleanup()
{
	for_each(emoticons.begin(), emoticons.end(), [](auto p) { delete p; });
	emoticons.clear();
	sortedEmoticons.clear();
	nameSet.clear();
	packSize = 0;
}

bool EmoticonPack::loadXMLFile(const string& name)
{
	const string path = Util::getEmoPacksPath() + name + ".xml";
	if (!File::isExist(path))
		return true;
	try
	{
		SimpleXML xml;
		xml.fromXML(File(path, File::READ, File::OPEN).read());
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
					packSize++;
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
	return true;
}

static bool sortFunc(const Emoticon* l, const Emoticon* r)
{
	const tstring& lt = l->getText();
	const tstring& rt = r->getText();
	if (lt.length() != rt.length()) 
		return lt.length() > rt.length();
	return lt.compare(0, rt.length(), rt) > 0;
}

bool EmoticonPack::initialize()
{
	static const string defName = "FlylinkSmilesInternational";
	static const string defNameOld = "FlylinkSmiles";

	setUseEmoticons(false);
	cleanup();
	const string currentName = SETTING(EMOTICONS_FILE);
	if (currentName == "Disabled")
		return true;
	loadXMLFile(currentName);
	size_t savedSize = packSize;
	if (currentName != defName)
		loadXMLFile(defName);
	if (currentName != defNameOld)
		loadXMLFile(defNameOld);
	packSize = savedSize;
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

	setUseEmoticons(true);
	return true;
}

#endif  // IRAINMAN_INCLUDE_SMILE
