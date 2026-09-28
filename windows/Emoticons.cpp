#include "stdafx.h"

#ifdef BL_UI_FEATURE_EMOTICONS

#include "Emoticons.h"
#include "../client/File.h"
#include "../client/SimpleXML.h"
#include "../client/AppPaths.h"
#include "../client/LogManager.h"
#include "../GdiOle/GDIImageOle.h"
#include <boost/algorithm/string/trim.hpp>

EmoticonPackList emoticonPackList;
static int nextId = 0;

Emoticon::Emoticon() : alias(nullptr), flags(0), id(0)
{
	for (int i = 0; i < MAX_IMAGES; ++i)
		images[i].obj = nullptr;
}

Emoticon::~Emoticon()
{
	for (int i = 0; i < MAX_IMAGES; ++i)
		if (images[i].obj) images[i].obj->release();
}

void Emoticon::setText(const tstring& text)
{
	this->text = text;
}

void Emoticon::setImagePath(int type, const string& path)
{
	images[type].path = path;
}

const string& Emoticon::getImagePath(int type) const
{
	return images[type].path;
}

void Emoticon::setHidden(bool enable)
{
	if (enable)
		flags |= FLAG_HIDDEN;
	else
		flags &= ~FLAG_HIDDEN;
}

int Emoticon::getEffectiveId() const
{
	return alias ? alias->id : id;
}

CGDIImage* Emoticon::getImage(int flags, int* oleFlags, HWND callbackWnd, UINT callbackMsg)
{
	if (alias)
		return alias->getImage(flags, oleFlags, callbackWnd, callbackMsg);

	int types[MAX_IMAGES];
	int count = 0;
	int gif1 = IMAGE_LIGHT_GIF;
	int gif2 = IMAGE_DARK_GIF;
	if (flags & FLAG_DARK_BACKGROUND)
		std::swap(gif1, gif2);
	if (flags & FLAG_PREFER_GIF)
	{
		types[count++] = gif1;
		types[count++] = gif2;
		if (!(flags & FLAG_NO_FALLBACK))
			types[count++] = IMAGE_BMP;
	}
	else
	{
		types[count++] = IMAGE_BMP;
		if (!(flags & FLAG_NO_FALLBACK))
		{
			types[count++] = gif1;
			types[count++] = gif2;
		}
	}
	for (int i = 0; i < count; i++)
	{
		int type = types[i];
		tryLoadImage(type, callbackWnd, callbackMsg);
		if (images[type].obj)
		{
			if (oleFlags)
			{
				int outFlags = 0;
				if (type != IMAGE_BMP)
				{
					outFlags = getOleImageFlags();
					if (outFlags && type == IMAGE_DARK_GIF)
						outFlags |= OLE_IMAGE_DARK_VARIANT;
				}
				*oleFlags = outFlags;
			}
			return images[type].obj;
		}
	}
	if (oleFlags)
		*oleFlags = 0;
	return nullptr;
}

void Emoticon::tryLoadImage(int type, HWND callbackWnd, UINT callbackMsg)
{
	if (images[type].obj || images[type].path.empty()) return;
	int errorMask = 1 << (ERROR_BIT + type);
	if (flags & errorMask) return;
	const string path = Util::getEmoPacksPath() + images[type].path;
	CGDIImage* obj = CGDIImage::createInstance(Text::toT(path).c_str());
	if (obj->isInitialized())
	{
		obj->setCallback(callbackWnd, callbackMsg);
		images[type].obj = obj;
		return;
	}
	LogManager::message("Can't load image of type " + Util::toString(type) + " from " + path, false);
	obj->release();
	flags |= errorMask;
}

IOleObject* Emoticon::getImageObject(int flags, IOleClientSite* pOleClientSite, IStorage* pStorage, HWND hCallbackWnd, UINT callbackMsg, COLORREF clrBackground, const tstring& text)
{
	IOleObject* pObject = nullptr;
	int oleFlags;
	CGDIImage* image = getImage(flags, &oleFlags, hCallbackWnd, callbackMsg);
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
		pImageObject->put_Flags(oleFlags);
		pImageObject->QueryInterface(IID_IOleObject, (void**)&pObject);
		if (pObject)
		{
			pImageObject->put_Text(const_cast<TCHAR*>(text.c_str()));
			pImageObject->put_EmoticonId(getEffectiveId());
			pObject->SetClientSite(pOleClientSite);
			return pObject;
		}
	}
	return nullptr;
}

void Emoticon::checkDuplicate(Emoticon* prev)
{
	for (int i = 0; i < MAX_IMAGES; i++)
		if (images[i].path != prev->images[i].path)
			return;
	alias = prev->alias ? prev->alias : prev;
}

int Emoticon::getOleImageFlags() const
{
	int flags = 0;
	if (!images[IMAGE_LIGHT_GIF].path.empty() && !images[IMAGE_DARK_GIF].path.empty() &&
	    images[IMAGE_LIGHT_GIF].path != images[IMAGE_DARK_GIF].path)
		flags |= OLE_IMAGE_MULTIPLE_VARIANTS;
	return flags;
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
	const string attrNames[] = { "Bitmap", "Gif", "DarkGif" };
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
					Emoticon* emoticon = new Emoticon;
					emoticon->setText(Text::toT(iconText));
					for (int type = 0; type < Emoticon::MAX_IMAGES; type++)
					{
						const string& path = xml.getChildAttrib(attrNames[type]);
						emoticon->setImagePath(type, path);
					}
					emoticon->setHidden(xml.getBoolChildAttrib("Hidden"));
					if (!emoticons.empty())
						emoticon->checkDuplicate(emoticons.back());
					emoticons.push_back(emoticon);
					if (!emoticon->isAlias() && (emoticon->getOleImageFlags() & Emoticon::OLE_IMAGE_MULTIPLE_VARIANTS))
						emoticon->setId(++nextId);
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
		string text = Text::fromT(icon->getText());
		bool first = true;
		for (int i = 0; i < Emoticon::MAX_IMAGES; i++)
		{
			const string& path = icon->getImagePath(i);
			if (path.empty()) continue;
			text += first ? " (" : ", ";
			text += "type " + Util::toString(i) + ": ";
			text += path;
			first = false;
		}
		if (!first) text += ')';
		dcdebug("%s\n", text.c_str());
	}
#endif
}

void EmoticonPackList::clear() noexcept
{
	for (EmoticonPack* pack : packs) delete pack;
	packs.clear();
	idMap.clear();
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

	idMap.clear();
	for (const EmoticonPack* p : packs)
	{
		const auto& v = p->getEmoticons();
		for (Emoticon* emoticon : v)
		{
			int id = emoticon->getId();
			if (id)
				idMap.insert(make_pair(id, emoticon));
		}
	}
}

Emoticon* EmoticonPackList::findById(int id) const
{
	auto i = idMap.find(id);
	return i == idMap.end() ? nullptr : i->second;
}

#endif // BL_UI_FEATURE_EMOTICONS
