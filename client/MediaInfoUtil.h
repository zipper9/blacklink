#ifndef MEDIA_INFO_UTIL_H_
#define MEDIA_INFO_UTIL_H_

#include "typedefs.h"

namespace MediaInfoUtil
{

	struct Info
	{
		uint16_t width;
		uint16_t height;
		uint16_t bitrate;
		string audio;
		string video;

		uint32_t getSize() const
		{
			return (uint32_t) width << 16 | height;
		}

		void clear()
		{
			width = height = bitrate = 0;
			audio.clear();
			video.clear();
		}

		bool hasData() const
		{
			return !audio.empty() || !video.empty();
		}
	};

	class Parser
	{
		public:
			Parser() : handle(nullptr) {}
			~Parser() { uninit(); }

			Parser(const Parser&) = delete;
			Parser& operator= (const Parser&) = delete;

			bool init();
			void uninit();

			bool parseFile(const string& path, Info& info);
#ifdef _UNICODE
			bool parseFile(const wstring& path, Info& info);
#endif

		private:
			bool getInfo(Info& info);
			string getAudioStreamInfo(size_t index);
			string getVideoStreamInfo(size_t index);
			bool addStream(const string& s, size_t index);
			string getStreams();

			void* handle;

			struct StreamItem
			{
				size_t index;
				unsigned count;
			};

			struct StreamInfo
			{
				string info;
				StreamItem si;
			};

			uint16_t maxBitRate;
			boost::unordered_map<string, StreamItem> dup;
			std::vector<StreamInfo> v;
	};

	bool parseDuration(const string& s, unsigned& msec);
	uint16_t getMediaInfoFileTypes();

}

#endif // MEDIA_INFO_UTIL_H_
