#ifndef DWM_API_LIB_H_
#define DWM_API_LIB_H_

#include "../client/w.h"
#include <dwmapi.h>

class DwmApiLib
{
	public:
		typedef HRESULT (STDAPICALLTYPE *fnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

		fnDwmSetWindowAttribute pDwmSetWindowAttribute;

		DwmApiLib() : hModule(nullptr), initialized(false) { clearPointers(); }
		~DwmApiLib() { uninit(); }

		DwmApiLib(const DwmApiLib&) = delete;
		DwmApiLib& operator= (const DwmApiLib&) = delete;

		void init();
		void uninit();

		static DwmApiLib instance;

	private:
		void clearPointers();

		HMODULE hModule;
		bool initialized;
};

#endif // DWM_API_LIB_H_
