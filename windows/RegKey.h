#ifndef REG_KEY_H_
#define REG_KEY_H_

#include "../client/w.h"
#include "../client/typedefs.h"

namespace WinUtil
{

	class RegKey
	{
		public:
			RegKey() noexcept : hKey(nullptr) {}
			~RegKey() noexcept { close(); }
			RegKey(const RegKey&) = delete;
			RegKey& operator= (const RegKey&) = delete;
			RegKey(RegKey&& x)
			{
				hKey = x.hKey;
				x.hKey = nullptr;
			}
			RegKey& operator= (RegKey&& x) noexcept
			{
				close();
				hKey = x.hKey;
				x.hKey = nullptr;
			}
			void close() noexcept;
			void attach(HKEY hKey) noexcept
			{
				close();
				this->hKey = hKey;
			}
			HKEY detach() noexcept
			{
				HKEY result = hKey;
				hKey = nullptr;
				return result;
			}
			bool open(HKEY parent, const TCHAR* name, REGSAM access) noexcept;
			bool create(HKEY parent, const TCHAR* name, REGSAM access) noexcept;
			HKEY getKey() const noexcept { return hKey; }
			bool readString(const TCHAR* name, tstring& value, bool expandEnv = true) noexcept;
			bool writeString(const TCHAR* name, const TCHAR* value, size_t len) noexcept;
			bool writeString(const TCHAR* name, const tstring& value) noexcept
			{
				return writeString(name, value.data(), value.length());
			}

		private:
			HKEY hKey;
	};

	bool expandEnvStrings(tstring& s);

}

#endif // REG_KEY_H_
