#include "stdinc.h"

#include "MediaInfoUtil.h"
#include "MediaInfoLib.h"
#include "Text.h"
#include "SettingsManager.h"
#include "FileTypes.h"
#include "StrUtil.h"
#include "ConfCore.h"
#include <algorithm>

#define MI MediaInfoLib::instance

#ifdef MEDIA_INFO_UNICODE
#define MC(x) L ## x
#else
#define MC(x) x
#endif

using MediaInfoUtil::Info;
using MediaInfoUtil::Parser;

bool Parser::init()
{
	if (!handle)
	{
		MI.init();
#ifdef MEDIA_INFO_UNICODE
		if (!MI.pMediaInfo_New) return false;
		handle = MI.pMediaInfo_New();
#else
		if (!MI.pMediaInfoA_New) return false;
		handle = MI.pMediaInfoA_New();
#endif
	}
	return handle != nullptr;
}

void Parser::uninit()
{
	if (handle)
	{
#ifdef MEDIA_INFO_UNICODE
		MI.pMediaInfo_Delete(handle);
#else
		MI.pMediaInfoA_Delete(handle);
#endif
		handle = nullptr;
	}
}

static inline string getParameter(void* handle, int streamType, size_t index, const MediaInfoLib::MediaInfoChar* param)
{
#ifdef MEDIA_INFO_UNICODE
	string s;
	const wchar_t* ws = MI.pMediaInfo_Get(handle, streamType, index, param, MediaInfoLib::Info_Text, MediaInfoLib::Info_Name);
	if (ws) s = Text::wideToUtf8(ws);
	return s;
#else
	const char* s = MI.pMediaInfoA_Get(handle, streamType, index, param, MediaInfoLib::Info_Text, MediaInfoLib::Info_Name);
	if (s) return string(s);
	return string();
#endif
}

static inline const MediaInfoLib::MediaInfoChar* getRawParameter(void* handle, int streamType, size_t index, const MediaInfoLib::MediaInfoChar* param)
{
#ifdef MEDIA_INFO_UNICODE
	return MI.pMediaInfo_Get(handle, streamType, index, param, MediaInfoLib::Info_Text, MediaInfoLib::Info_Name);
#else
	return MI.pMediaInfoA_Get(handle, streamType, index, param, MediaInfoLib::Info_Text, MediaInfoLib::Info_Name);
#endif
}

static inline size_t getStreamCount(void* handle, int streamType)
{
#ifdef MEDIA_INFO_UNICODE
	return MI.pMediaInfo_Count_Get(handle, streamType, (size_t) -1);
#else
	return MI.pMediaInfoA_Count_Get(handle, streamType, (size_t) -1);
#endif
}

string Parser::getAudioStreamInfo(size_t index)
{
	string result = getParameter(handle, MediaInfoLib::Stream_Audio, index, MC("Format"));
	if (result.empty()) return result;
	string val = getParameter(handle, MediaInfoLib::Stream_Audio, index, MC("Channel(s)"));
	if (!val.empty())
	{
		uint32_t number = Util::toUInt32(val);
		if (number)
		{
			if (!result.empty()) result += ", ";
			if (getParameter(handle, MediaInfoLib::Stream_Audio, index, MC("ChannelPositions")).find("LFE") != string::npos)
			{
				result += Util::toString(number - 1);
				result += ".1";
			}
			else
			{
				result += val;
				result += ".0";
			}
		}
	}
	val = getParameter(handle, MediaInfoLib::Stream_Audio, index, MC("BitRate/String"));
	if (!val.empty())
	{
		if (!result.empty()) result += ", ";
		result += val;
	}
	val = getParameter(handle, MediaInfoLib::Stream_Audio, index, MC("Language/String1"));
	if (!val.empty())
	{
		if (!result.empty()) result += ", ";
		result += val;
	}
	return result;
}

string Parser::getVideoStreamInfo(size_t index)
{
	string result = getParameter(handle, MediaInfoLib::Stream_Video, index, MC("Format"));
	if (result.empty()) return result;
	string val = getParameter(handle, MediaInfoLib::Stream_Video, index, MC("BitRate/String"));
	if (!val.empty())
	{
		if (!result.empty()) result += ", ";
		result += val;
	}
	val = getParameter(handle, MediaInfoLib::Stream_Video, index, MC("FrameRate/String"));
	if (!val.empty())
	{
		if (!result.empty()) result += ", ";
		result += val;
	}
	return result;
}

bool Parser::getInfo(Info& info)
{
	info.clear();
	maxBitRate = 0;

	size_t count = getStreamCount(handle, MediaInfoLib::Stream_Audio);
	if (count)
	{
		dup.clear();
		bool hasDuplicate = false;
		for (size_t i = 0; i < count; i++)
		{
			string s = getAudioStreamInfo(i);
			if (s.empty()) continue;
			if (count > 1 && addStream(s, i)) hasDuplicate = true;
			if (!hasDuplicate)
			{
				if (!info.audio.empty()) info.audio += " | ";
				info.audio += s;
			}
			const MediaInfoLib::MediaInfoChar* param = getRawParameter(handle, MediaInfoLib::Stream_Audio, i, MC("BitRate"));
			uint16_t bitRate = (uint16_t) (Util::toUInt32(param) / 1000.0 + 0.5);
			if (bitRate > maxBitRate) maxBitRate = bitRate;
		}
		if (hasDuplicate)
			info.audio = getStreams();
	}
	count = getStreamCount(handle, MediaInfoLib::Stream_Video);
	if (count)
	{
		const MediaInfoLib::MediaInfoChar* param = getRawParameter(handle, MediaInfoLib::Stream_Video, 0, MC("Width"));
		if (param) info.width = Util::toUInt32(param);
		param = getRawParameter(handle, MediaInfoLib::Stream_Video, 0, MC("Height"));
		if (param) info.height = Util::toUInt32(param);
		dup.clear();
		bool hasDuplicate = false;
		for (size_t i = 0; i < count; i++)
		{
			string s = getVideoStreamInfo(i);
			if (s.empty()) continue;
			if (count > 1 && addStream(s, i)) hasDuplicate = true;
			if (!hasDuplicate)
			{
				if (!info.video.empty()) info.video += " | ";
				info.video += s;
			}
		}
		if (hasDuplicate)
			info.video = getStreams();
	}
	string duration = getParameter(handle, MediaInfoLib::Stream_General, 0, MC("Duration/String"));
	if (!duration.empty())
	{
		duration += " | ";
		info.audio.insert(0, duration);
	}

	v.clear();
	info.bitrate = maxBitRate;
	return !info.audio.empty() || !info.video.empty();
}

bool Parser::addStream(const string& s, size_t index)
{
	StreamItem si = { index, 1 };
	auto result = dup.insert(make_pair(s, si));
	if (result.second) return false;
	++result.first->second.index;
	return true;
}

string Parser::getStreams()
{
	v.clear();
	v.reserve(dup.size());
	for (auto i = dup.begin(); i != dup.end(); ++i)
		v.emplace_back(StreamInfo{i->first, i->second});
	std::sort(v.begin(), v.end(),
		[](const StreamInfo& a, const StreamInfo& b)
		{
			return a.si.index < b.si.index;
		});
	string s;
	for (const auto& item : v)
	{
		if (!s.empty()) s += " | ";
		s += item.info;
		if (item.si.count > 1)
		{
			s += " (x";
			s += Util::toString(item.si.count);
			s += ')';
		}
	}
	return s;
}

bool Parser::parseFile(const string& path, Info& info)
{
	if (!handle) return false;
#ifdef MEDIA_INFO_UNICODE
	if (!MI.pMediaInfo_Open) return false;
	wstring s = Text::utf8ToWide(path);
	if (!MI.pMediaInfo_Open(handle, s.c_str())) return false;
	bool result = getInfo(info);
	MI.pMediaInfo_Close(handle);
#else
	if (!MI.pMediaInfoA_Open) return false;
	if (!MI.pMediaInfoA_Open(handle, path.c_str())) return false;
	bool result = getInfo(info);
	MI.pMediaInfoA_Close(handle);
#endif
	return result;
}

#ifdef _UNICODE
bool Parser::parseFile(const wstring& path, Info& info)
{
	if (!handle) return false;
#ifdef MEDIA_INFO_UNICODE
	if (!MI.pMediaInfo_Open) return false;
	if (!MI.pMediaInfo_Open(handle, path.c_str())) return false;
	bool result = getInfo(info);
	MI.pMediaInfo_Close(handle);
#else
	if (!MI.pMediaInfoA_Open) return false;
	string s = Text::wideToUtf8(path);
	if (!MI.pMediaInfoA_Open(handle, s.c_str())) return false;
	bool result = getInfo(info);
	MI.pMediaInfoA_Close(handle);
#endif
	return result;
}
#endif

static unsigned getUnit(const string& s)
{
	if (s == "ms" || s == "msec") return 1;
	if (s == "s" || s == "sec") return 1000;
	if (s == "mn" || s == "min") return 60000;
	if (s == "h" || s == "hr") return 3600000;
	return 0;
}

bool MediaInfoUtil::parseDuration(const string& s, unsigned& msec)
{
	msec = 0;
	bool getNum = true;
	bool hasNum = false;
	bool found = false;
	unsigned val = 0;
	string::size_type i = 0;
	string::size_type j = string::npos;
	while (i < s.length())
	{
		if (getNum)
		{
			if (s[i] == ' ')
			{
				++i;
				continue;
			}
			if (s[i] >= '0' && s[i] <= '9')
			{
				val = val*10 + s[i]-'0';
				hasNum = true;
				++i;
				continue;
			}
			if (!hasNum) return false;
			getNum = false;
		}
		if (s[i] != ' ' && j == string::npos) j = i;
		if (j != string::npos && (s[i] == ' ' || (s[i] >= '0' && s[i] <= '9')))
		{
			unsigned unit = getUnit(s.substr(j, i-j));
			if (!unit) return false;
			msec += val * unit;
			val = 0;
			getNum = true;
			hasNum = false;
			found = true;
			j = string::npos;
		}
		++i;
	}
	if (j != string::npos)
	{
		unsigned unit = getUnit(s.substr(j));
		if (!unit) return false;
		msec += val * unit;
		getNum = true;
		hasNum = false;
		found = true;
	}
	if (getNum && hasNum) return false;
	return found;
}

uint16_t MediaInfoUtil::getMediaInfoFileTypes()
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	unsigned mediaInfoOptions = ss->getInt(Conf::MEDIA_INFO_OPTIONS);
	uint16_t mask = 0;
	if (mediaInfoOptions & Conf::MEDIA_INFO_OPTION_ENABLE)
	{
		if (mediaInfoOptions & Conf::MEDIA_INFO_OPTION_SCAN_AUDIO)
			mask |= 1<<FILE_TYPE_AUDIO;
		if (mediaInfoOptions & Conf::MEDIA_INFO_OPTION_SCAN_VIDEO)
			mask |= 1<<FILE_TYPE_VIDEO;
	}
	ss->unlockRead();
	return mask;
}
