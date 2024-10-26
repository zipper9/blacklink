#include "stdafx.h"

#ifdef BL_UI_FEATURE_EMOTICONS
#include "EmoticonsDlg.h"
#include "Emoticons.h"
#include "WinUtil.h"
#include "MainFrm.h"
#include "../client/Util.h"
#include "../GdiOle/GDIImage.h"

#define SUBCLASS_BUTTON

static const int BUTTON_SPACE = 8;
static const int BUTTON_PADDING = 3;

WNDPROC EmoticonsDlg::g_MFCWndProc = nullptr;
EmoticonsDlg* EmoticonsDlg::g_pDialog = nullptr;

LRESULT EmoticonsDlg::onIconClick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
	tstring text;
	WinUtil::getWindowText(hWndCtl, text);
	if (WinUtil::isShift() && hWndNotif)
	{
		tstring* msgParam = new tstring(std::move(text));
		if (!::PostMessage(hWndNotif, WMU_PASTE_TEXT, 0, reinterpret_cast<LPARAM>(msgParam)))
			delete msgParam;
	}
	else
	{
		result = std::move(text);
		PostMessage(WM_CLOSE);
	}
	return 0;
}

LRESULT EmoticonsDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	WNDPROC temp = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(EmoticonsDlg::m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(NewWndProc)));
	if (!g_MFCWndProc)
		g_MFCWndProc = temp;
	g_pDialog = this;

	ShowWindow(SW_HIDE);
	::EnableWindow(WinUtil::g_mainWnd, TRUE);

	const auto& packList = emoticonPackList.getPacks();
	if (!packList.empty() && packList[0])
	{
		const EmoticonPack* pack = packList[0];
		const int useAnimation = SettingsManager::instance.getUiSettings()->getBool(Conf::SMILE_SELECT_WND_ANIM_SMILES) ?
			Emoticon::FLAG_PREFER_GIF : Emoticon::FLAG_NO_FALLBACK;
		const vector<Emoticon*>& icons = pack->getEmoticons();
		size_t packSize = icons.size();
		size_t numberOfIcons = 0;
		int sw = 0, sh = 0;
		for (size_t i = 0; i < icons.size(); ++i)
		{
			if (i == packSize) break;
			Emoticon* icon = icons[i];
			if (icon->isDuplicate() || icon->isHidden()) continue;
			CGDIImage* pImage = icon->getImage(useAnimation, MainFrame::getMainFrame()->m_hWnd, WM_ANIM_CHANGE_FRAME);
			if (pImage)
			{
				int w = pImage->GetWidth();
				int h = pImage->GetHeight();
				if (w && h)
				{
					sw += w * w;
					sh += h * h;
					numberOfIcons++;
				}
			}
		}

		if (!numberOfIcons)
		{
			isError = true;
			EndDialog(IDCANCEL);
			return 0;
		}
		
		int i = (int) sqrt(double(numberOfIcons));
		int nXfor = i;
		int nYfor = i;
		
		if (i * i != numberOfIcons)
		{
			nXfor = i + 1;
			if (i * nXfor < numberOfIcons) nYfor++;
		}

		// Get mean square of all icon dimensions
		sw = (int) sqrt((double) sw / numberOfIcons);
		sh = (int) sqrt((double) sh / numberOfIcons);
		
		pos.bottom = pos.top - 3;
		pos.left = pos.right - nXfor * (sw + BUTTON_SPACE) - 2;
		pos.top = pos.bottom - nYfor * (sh + BUTTON_SPACE) - 2;
		
		// [+] brain-ripper
		// Fit window in screen's work area
		RECT rcWorkArea;
		SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
		if (pos.right > rcWorkArea.right)
		{
			pos.left -= pos.right - rcWorkArea.right;
			pos.right = rcWorkArea.right;
		}
		if (pos.bottom > rcWorkArea.bottom)
		{
			pos.top -= pos.bottom - rcWorkArea.bottom;
			pos.bottom = rcWorkArea.bottom;
		}
		if (pos.left < rcWorkArea.left)
		{
			pos.right += rcWorkArea.left - pos.left;
			pos.left = rcWorkArea.left;
		}
		if (pos.top < rcWorkArea.top)
		{
			pos.bottom += rcWorkArea.top - pos.top;
			pos.top = rcWorkArea.top;
		}
		
		MoveWindow(pos);

		tooltip.Create(EmoticonsDlg::m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
		tooltip.SetDelayTime(TTDT_AUTOMATIC, 1000);

		int x = 0, y = 0;
		for (size_t i = 0; i < icons.size(); ++i)
		{
			if (i == packSize) break;
			Emoticon* icon = icons[i];
			if (icon->isDuplicate() || icon->isHidden()) continue;
			CGDIImage *pImage = icon->getImage(useAnimation, MainFrame::getMainFrame()->m_hWnd, WM_ANIM_CHANGE_FRAME);
			if (pImage && pImage->GetWidth() && pImage->GetHeight())
			{
				pos.left = x * (sw + BUTTON_SPACE);
				pos.top = y * (sh + BUTTON_SPACE);
				pos.right = pos.left + sw + BUTTON_SPACE;
				pos.bottom = pos.top + sh + BUTTON_SPACE;
				const tstring& text = icon->getText();
				CAnimatedButton* button = new CAnimatedButton(pImage);
#ifdef SUBCLASS_BUTTON
				CButton nativeButton;
				nativeButton.Create(m_hWnd, pos, text.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0);
				button->SubclassWindow(nativeButton.m_hWnd);
				BOOL unused;
				button->onCreate(0, 0, 0, unused);
#else
				button->Create(m_hWnd, pos, text.c_str(), WS_CHILD | WS_VISIBLE, 0);
#endif
				buttonList.push_back(button);
				CToolInfo ti(TTF_SUBCLASS, button->m_hWnd, 0, nullptr, const_cast<TCHAR*>(text.c_str()));
				tooltip.AddTool(&ti);
				if (++x == nXfor)
				{
					x = 0;
					++y;
				}
			}
		}
		ShowWindow(SW_SHOW);
		RedrawWindow(nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
		isError = false;
	}
	else
	{
		isError = true;
		EndDialog(IDCANCEL);
	}

	return 0;
}

LRESULT EmoticonsDlg::onHotItemChange(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	::InvalidateRect(pnmh->hwndFrom, nullptr, FALSE);
	return 0;
}

LRESULT EmoticonsDlg::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	clearButtons();
	return 0;
}

LRESULT CALLBACK EmoticonsDlg::NewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (g_pDialog && message == WM_ACTIVATE && wParam == 0)
	{
		g_pDialog->EndDialog(IDCANCEL);
		return FALSE;
	}
	return ::CallWindowProc(g_MFCWndProc, hWnd, message, wParam, lParam);
}

void EmoticonsDlg::clearButtons()
{
	for (CAnimatedButton* button : buttonList)
	{
#ifdef SUBCLASS_BUTTON
		button->UnsubclassWindow();
#endif
		delete button;
	}
	buttonList.clear();
}

EmoticonsDlg::~EmoticonsDlg()
{
	clearButtons();
}

CAnimatedButton::CAnimatedButton(CGDIImage *image):
	image(image), initialized(false), hBackDC(nullptr)
{
	xBk = 0;
	yBk = 0;
	xSrc = 0;
	ySrc = 0;
	wSrc = 0;
	hSrc = 0;
	height = width = 0;
	hTheme = nullptr;

	if (image)
		image->AddRef();
}

CAnimatedButton::~CAnimatedButton()
{
	BOOL unused;
	onClose(0, 0, 0, unused);
	if (hTheme) CloseThemeData(hTheme);
}

bool CAnimatedButton::OnFrameChanged(CGDIImage *pImage, LPARAM lParam)
{
	ASSERT_MAIN_THREAD();
	CAnimatedButton *pBtn = (CAnimatedButton*)lParam;
	pBtn->RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	return true;
}

LRESULT CAnimatedButton::onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(&ps);
	draw(hdc);
	EndPaint(&ps);
	return 0;
}

LRESULT CAnimatedButton::onCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (image)
	{
		RECT rect;
		GetClientRect(&rect);

		width = rect.right - rect.left;
		height = rect.bottom - rect.top;

		int imageWidth = image->GetWidth();
		int imageHeight = image->GetHeight();

		int offsetX = (width - imageWidth) / 2;
		int offsetY = (height - imageHeight) / 2;

		xBk = offsetX;
		yBk = offsetY;
		xSrc = ySrc = 0;
		wSrc = imageWidth;
		hSrc = imageHeight;
		
		if (offsetX < BUTTON_PADDING)
		{
			xSrc = BUTTON_PADDING - offsetX;
			wSrc -= xSrc;
			xBk = BUTTON_PADDING;
		}

		if (offsetY < BUTTON_PADDING)
		{
			ySrc = BUTTON_PADDING - offsetY;
			hSrc -= ySrc;
			yBk = BUTTON_PADDING;
		}

		int xright = width - BUTTON_PADDING;
		if (xBk + wSrc > xright)
			wSrc -= xBk + wSrc - xright;

		int ybottom = height - BUTTON_PADDING;
		if (yBk + hSrc > ybottom)
			hSrc -= yBk + hSrc - ybottom;

		if (!hTheme)
			hTheme = OpenThemeData(m_hWnd, L"BUTTON");

		image->RegisterCallback(OnFrameChanged, reinterpret_cast<LPARAM>(this));
		initialized = true;
	}
	
	return 0;
}

LRESULT CAnimatedButton::onErase(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	return 1;
}

LRESULT CAnimatedButton::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (image)
	{
		if (initialized)
		{
			image->UnregisterCallback(OnFrameChanged, (LPARAM)this);
			image->DeleteBackDC(hBackDC);
		}
		image->Release();
		image = nullptr;
	}
	return 0;
}

LRESULT CAnimatedButton::onThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (hTheme) CloseThemeData(hTheme);
	hTheme = OpenThemeData(m_hWnd, L"BUTTON");
	Invalidate();
	return 0;
}

void CAnimatedButton::drawBackground(HDC hdc)
{
	UINT state = GetState();
	RECT rect = { 0, 0, width, height };
	if (hTheme)
	{
		int flags;
		if (state & BST_PUSHED) flags = PBS_PRESSED;
		else if (state & BST_HOT) flags = PBS_HOT;
		else flags = PBS_NORMAL;
		DrawThemeBackground(hTheme, hdc, BP_PUSHBUTTON, flags, &rect, nullptr);
	}
	else
	{
		UINT flags = DFCS_BUTTONPUSH;
		if (state & BST_PUSHED) flags |= DFCS_PUSHED;
		if (state & BST_HOT) flags |= DFCS_HOT;
		DrawFrameControl(hdc, &rect, DFC_BUTTON, flags);
	}
	if (state & BST_FOCUS)
	{
		InflateRect(&rect, -BUTTON_PADDING, -BUTTON_PADDING);
		DrawFocusRect(hdc, &rect);
	}
}

void CAnimatedButton::drawImage(HDC hdc)
{
	if (wSrc > 0 && hSrc > 0)
		image->Draw(hdc, xBk, yBk, wSrc, hSrc, xSrc, ySrc, nullptr, 0, 0, 0, 0);
}

void CAnimatedButton::draw(HDC hdc)
{
	if (!hBackDC && image)
		hBackDC = image->CreateBackDC(hdc, GetSysColor(COLOR_BTNFACE), width, height);
	if (hBackDC)
	{
		BitBlt(hBackDC, 0, 0, width, height, nullptr, 0, 0, PATCOPY);
		drawBackground(hBackDC);
		drawImage(hBackDC);
		BitBlt(hdc, 0, 0, width, height, hBackDC, 0, 0, SRCCOPY);
	}
	else
	{
		drawBackground(hdc);
		drawImage(hdc);
	}
}

#endif // BL_UI_FEATURE_EMOTICONS
