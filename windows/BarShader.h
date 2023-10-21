#pragma once

#include "../Client/typedefs.h"
#include "atlapp.h"
#include "atlwin.h"
#include "atlctrls.h"
#include "atlcoll.h"

class CBarShader
{
	public:
		CBarShader(uint32_t height, uint32_t width, COLORREF color = 0, uint64_t fileSize = 1);
		~CBarShader(void);
		
		//set the width of the bar
		void SetWidth(uint32_t width);
		
		//set the height of the bar
		void SetHeight(uint32_t height);
		
		//returns the width of the bar
		int GetWidth() const
		{
			return m_iWidth;
		}
		
		//returns the height of the bar
		int GetHeight() const
		{
			return m_iHeight;
		}
		
		//sets new file size and resets the shader
		void SetFileSize(uint64_t qwFileSize);
		
		//fills in a range with a certain color, new ranges overwrite old
		void FillRange(uint64_t qwStart, uint64_t qwEnd, COLORREF crColor);
		
		//fills in entire range with a certain color
		void Fill(COLORREF crColor);
		
		//draws the bar
		void Draw(HDC hdc, int iLeft, int iTop, int);
		
	protected:
		void BuildModifiers();
		void FillRect(HDC hdc, LPCRECT rectSpan, COLORREF crColor);
		
		uint64_t    m_qwFileSize;
		uint32_t    m_iWidth;
		uint32_t    m_iHeight;
		double      m_dblPixelsPerByte;
		double      m_dblBytesPerPixel;
		
	private:
		CRBMap<uint64_t, COLORREF> m_Spans;
		vector<double> m_pdblModifiers;
		byte    m_used3dlevel;
		bool    m_bIsPreview;
		void CalcPerPixelandPerByte();
};

class OperaColors
{
	public:
		static inline COLORREF blendColors(COLORREF cr1, COLORREF cr2, double balance = 0.5)
		{
			unsigned r1 = GetRValue(cr1);
			unsigned g1 = GetGValue(cr1);
			unsigned b1 = GetBValue(cr1);
			unsigned r2 = GetRValue(cr2);
			unsigned g2 = GetGValue(cr2);
			unsigned b2 = GetBValue(cr2);
			unsigned r = unsigned((r1 * (balance * 2) + (r2 * ((1 - balance) * 2))) / 2);
			unsigned g = unsigned((g1 * (balance * 2) + (g2 * ((1 - balance) * 2))) / 2);
			unsigned b = unsigned((b1 * (balance * 2) + (b2 * ((1 - balance) * 2))) / 2);
			return RGB(r, g, b);
		}

		static inline COLORREF brightenColor(COLORREF c, double brightness = 0)
		{
			if (abs(brightness) < 1e-6)
				return c;

			unsigned r = GetRValue(c);
			unsigned g = GetGValue(c);
			unsigned b = GetBValue(c);
			if (brightness > 0)
			{
				r += (255 - r) * brightness;
				g += (255 - g) * brightness;
				b += (255 - b) * brightness;
				if (r > 255) r = 255;
				if (g > 255) g = 255;
				if (b > 255) b = 255;
			}
			else
			{
				r *= (1 + brightness);
				g *= (1 + brightness);
				b *= (1 + brightness);
			}
			return RGB(r, g, b);
		}

		static void drawBar(HDC hDC, int x1, int y1, int x2, int y2, COLORREF c1, COLORREF c2, bool light = true);
		static void getBarColors(COLORREF clr, COLORREF& a, COLORREF& b);
		static void clearCache();

	private:
		struct FloodCacheKey
		{
			COLORREF c1;
			COLORREF c2;
			bool light;
		};

		struct FloodCacheItem
		{
			FloodCacheItem() : w(0), h(0), bitmap(nullptr), dibBuffer(nullptr) {}

			FloodCacheItem(const FloodCacheItem&) = delete;
			FloodCacheItem& operator= (const FloodCacheItem&) = delete;

			void cleanup()
			{
				if (bitmap)
				{
					DeleteObject(bitmap);
					bitmap = nullptr;					
					dibBuffer = nullptr;
				}
			}
			~FloodCacheItem() { cleanup(); }

			int w;
			int h;
			HBITMAP bitmap;
			void* dibBuffer;
		};

		template<bool b64>
		struct fci_hash
		{
			size_t operator()(const FloodCacheKey& x) const;
		};

		template<>
		struct fci_hash<false>
		{
			size_t operator()(const FloodCacheKey& x) const
			{
				return (((x.c1 & 0xFFFFFF) << 7) ^ (x.c2 & 0xFFFFFF)) | (x.light ? 1u : 0u) << 31;
			}
		};

		template<>
		struct fci_hash<true>
		{
			size_t operator()(const FloodCacheKey& x) const
			{
				return (uint64_t)(x.c1 & 0xFFFFFF) | ((uint64_t)(x.c2 & 0xFFFFFF) << 24) | (x.light ? 1ull : 0ull) << 48;
			}
		};
		
		struct fci_equal_to
		{
			bool operator()(const FloodCacheKey& x, const FloodCacheKey& y) const
			{
				return x.c1 == y.c1 && x.c2 == y.c2 && x.light == y.light;
			}
		};
		
		typedef std::unordered_map<FloodCacheKey, FloodCacheItem*, fci_hash<sizeof(size_t) == 8>, fci_equal_to> CacheMap;

		static CacheMap cache;
};

class ProgressBar
{
	public:
		struct Settings
		{
			bool odcStyle;
			bool odcBumped;
			bool setTextColor;
			int depth; // !odcStyle
			COLORREF clrBackground;
			COLORREF clrText;
			COLORREF clrEmptyBackground; // !odcStyle

			bool operator== (const Settings& rhs) const;
			bool operator!= (const Settings& rhs) const { return !operator==(rhs); }
		};

		ProgressBar() : backBrush(nullptr), framePen(nullptr), colorsInitialized(false) {}
		~ProgressBar() { cleanup(); }
		ProgressBar(const ProgressBar&) = delete;
		ProgressBar& operator= (const ProgressBar&) = delete;

		void set(const Settings& s);
		const Settings& get() const { return settings; }
		void draw(HDC hdc, const RECT& rc, int pos, const tstring& text, int iconIndex);
		void setWindowBackground(COLORREF clr);
		void setEmptyBarBackground(COLORREF clr);

	private:
		Settings settings;
		COLORREF windowBackground; // odcStyle, used to derive background colors
		COLORREF textColor[2];
		bool colorsInitialized;
		// odcStyle, cached GDI objects
		HBRUSH backBrush;
		HPEN framePen;

		void cleanup();
};
