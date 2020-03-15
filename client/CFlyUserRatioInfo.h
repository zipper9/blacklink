#ifndef CFlyUserRatioInfo_H
#define CFlyUserRatioInfo_H

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO

#include <boost/unordered/unordered_map.hpp>
#include <boost/asio/ip/address_v4.hpp>

template <class T> class CFlyUploadDownloadPair
{
	private:
		T  m_upload;
		T  m_download;
		bool m_is_dirty;
	public:
		CFlyUploadDownloadPair() : m_upload(0), m_download(0), m_is_dirty(false)
		{
		}
		~CFlyUploadDownloadPair()
		{
		}
		bool is_dirty() const
		{
			return m_is_dirty;
		}
		void set_dirty(bool p_value)
		{
			m_is_dirty = p_value;
		}
		const T& get_upload() const
		{
			return m_upload;
		}
		const T& get_download() const
		{
			return m_download;
		}
		void add_upload(const T& p_size)
		{
			m_upload += p_size;
			if (p_size)
				m_is_dirty = true;
		}
		void add_download(const T& p_size)
		{
			m_download += p_size;
			if (p_size)
				m_is_dirty = true;
		}
		void set_upload(const T& p_size)
		{
			if (m_upload != p_size)
				m_is_dirty = true;
			m_upload = p_size;
		}
		void set_download(const T& p_size)
		{
			if (m_download != p_size)
				m_is_dirty = true;
			m_download = p_size;
		}
		void reset_dirty()
		{
			m_is_dirty = false;
		}
		T get_ratio() const
		{
			if (m_download > 0)
				return m_upload / m_download;
			else
				return 0;
		}
		
};

template <class T> class CFlyDirtyValue
{
	private:
		bool m_is_dirty;
		T m_value;

	public:
		CFlyDirtyValue(const T& p_value = T()) : m_value(p_value), m_is_dirty(false)
		{
		}
		CFlyDirtyValue(const CFlyDirtyValue&) = delete;
		CFlyDirtyValue& operator= (const CFlyDirtyValue &) = delete;
		const T& get() const
		{
			return m_value;
		}
		bool set(const T& p_value)
		{
			if (p_value != m_value)
			{
				m_value = p_value;
				m_is_dirty = true;
				return true;
			}
			return false;
		}
		bool is_dirty() const
		{
			return m_is_dirty;
		}
		void reset_dirty()
		{
			m_is_dirty = false;
		}
};

typedef CFlyUploadDownloadPair<double> CFlyGlobalRatioItem;

typedef boost::unordered_map<unsigned long, CFlyUploadDownloadPair<uint64_t> > CFlyUploadDownloadMap; // TODO кей boost::asio::ip::address_v4

struct CFlyUserRatioInfo : public CFlyUploadDownloadPair<uint64_t>
{
	public:
		CFlyUserRatioInfo() : ipMap(nullptr) {}
		CFlyUserRatioInfo(const CFlyUserRatioInfo& src) : CFlyUploadDownloadPair<uint64_t>(src)
		{
			ipMap = src.ipMap ? new CFlyUploadDownloadMap(*src.ipMap) : nullptr;
			lastIp = src.lastIp;
		}

		CFlyUserRatioInfo(CFlyUserRatioInfo&& src) : CFlyUploadDownloadPair<uint64_t>(src)
		{
			ipMap = src.ipMap;
			src.ipMap = nullptr;
			lastIp = src.lastIp;
		}

		CFlyUserRatioInfo& operator= (const CFlyUserRatioInfo&) = delete;

		~CFlyUserRatioInfo() { delete ipMap; }
		
		CFlyUploadDownloadPair<uint64_t>& findIpInMap(boost::asio::ip::address_v4 ip)
		{
			if (!ipMap)
				ipMap = new CFlyUploadDownloadMap;
			return (*ipMap)[ip.to_ulong()];
		}

		void addUpload(boost::asio::ip::address_v4 ip, uint64_t size)
		{
			if (size)
			{
				add_upload(size);
				if (!lastIp.is_unspecified())
				{
					if (lastIp != ip)
						findIpInMap(ip).add_upload(size);
				}
				else
				{
					lastIp = ip;
				}
			}
		}

		void addDownload(boost::asio::ip::address_v4 ip, uint64_t size)
		{
			if (size)
			{
				add_download(size);
				if (!lastIp.is_unspecified())
				{
					if (lastIp != ip)
						findIpInMap(ip).add_download(size);
				}
				else
				{
					lastIp = ip;
				}
			}
		}
		
		void resetDirty()
		{
			reset_dirty();
			if (ipMap)
			{
				for (auto i = ipMap->begin(); i != ipMap->end(); ++i)
					i->second.reset_dirty();
			}
		}
		
		CFlyUploadDownloadMap* getUploadDownloadMap()
		{
			return ipMap;
		}

		boost::asio::ip::address_v4 getIp() const { return lastIp; }

	private:
		CFlyUploadDownloadMap* ipMap;
		boost::asio::ip::address_v4 lastIp;
};
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO


#endif

